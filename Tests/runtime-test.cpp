#include "../Source/Recorder.h"
#include <thread>
#include <cstdio>
#include <cstdlib>

static int failures=0;
static void check(bool ok,const char* label) {
    std::printf("%s %s\n",ok?"OK":"FAIL",label);if(!ok)++failures;
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if(argc<2)return 2;
    const juce::File folder(juce::String::fromUTF8(argv[1]));
    folder.createDirectory();
    Recorder r;r.prepareDevice(48000,512,1);
    float input[512]{},output[512]{};
    const float* in[]={input};float* out[]={output};
    auto block=[&] {r.audioDeviceIOCallbackWithContext(in,1,out,1,512,{});};
    // The device runs long before record; take timestamps must still start at zero.
    for(int k=0;k<3000;++k)block();
    check(r.start(folder).isEmpty(),"start creates a session");
    for(int k=0;k<120;++k) {
        for(int i=0;i<512;++i)input[i]=k<100 ? .05f*std::sin(float(2*M_PI*220*(k*512+i)/48000.)) : 1.f;
        block();std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    r.stop();
    check(!r.recordingDamaged(),"normal disk recording accepts all blocks");
    check(std::abs(r.recordedSeconds()-120.*512/48000)<1e-6,"recording length excludes device warmup");
    tw::Event e;bool clip=false;double latest=0;
    while(r.nextEvent(e))if(e.kind==tw::Kind::Clipping){clip=true;latest=std::max(latest,e.timeSec);}
    check(clip && latest<2,"clipping timestamps are relative to take");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(r.currentFile().createInputStream().release(),true));
    check(reader && reader->lengthInSamples==120*512,"WAV contains every accepted sample");
    const auto firstFile=r.currentFile();reader.reset();
    check(r.start(folder).isEmpty(),"second session starts");
    check(r.currentFile()!=firstFile,"sessions never overwrite earlier takes");
    const float* invalid[]={nullptr};
    r.audioDeviceIOCallbackWithContext(invalid,1,out,1,512,{});r.stop();
    check(r.recordingDamaged() && r.recordedSeconds()==0,"invalid input is reported, not counted as written");
    // Callback runs continuously while GUI attaches/detaches and destroys players.
    std::atomic<bool> done{false};
    std::thread audio([&]{while(!done.load())block();});
    for(int i=0;i<1000;++i) {
        auto player=std::make_unique<zs::Audition>();
        player->setMaterial(std::vector<float>(1024,.1f),std::vector<float>(1024,.2f),48000);
        player->play();r.attachPlayer(player.get());
        std::this_thread::yield();r.attachPlayer(nullptr);player.reset();
    }
    done.store(true);audio.join();
    check(true,"1000 player detach/destroy operations during audio callbacks");
    zs::Audition p;p.setMaterial(std::vector<float>(4096,.1f),std::vector<float>(4096,.2f),48000);
    p.play();p.pull(output,512);p.seekSec(.05);p.pull(output,512);
    check(std::abs(p.positionSec()-(2400+512)/48000.)<1e-6,"seek command applied before next block");
    p.stop();check(p.positionSec()==0,"stop position reports zero immediately");
    // Pre-roll after R: recent audio is aligned with the take, plays once, then silence.
    check(r.start(folder).isEmpty(),"pre-roll session starts");
    for(int k=0;k<40;++k) {
        for(int i=0;i<512;++i)input[i]=float(k*512+i)*1e-6f;
        block();
    }
    double startSec=-1;auto recent=r.recentAudio(30.0,startSec);
    check(recent.size()==40*512 && startSec==0,"recent audio covers the whole short take");
    check(!recent.empty() && recent.back()==float(40*512-1)*1e-6f,"recent audio ends with the newest sample");
    recent=r.recentAudio(0.1,startSec);
    check(recent.size()==4800 && std::abs(startSec-(40*512-4800)/48000.)<1e-9,"recent audio start is take time");
    r.playPreRoll(std::vector<float>(700,.3f));
    for(int i=0;i<512;++i)input[i]=0;
    block();const bool first=output[0]==.3f&&output[511]==.3f;
    block();const bool tail=output[187]==.3f&&output[188]==0&&output[511]==0;
    block();const bool after=output[0]==0;
    check(first&&tail&&after,"pre-roll plays once into the output");
    r.stop();
    check(r.recentAudio(30.0,startSec).empty(),"no recent audio when not recording");
    // Queue never overwrites unread data.
    zs::SpscQueue<int,4> q;int value=0;
    check(q.push(1)&&q.push(2)&&q.push(3)&&!q.push(4)&&q.pop(value)&&value==1,"full event queue preserves oldest unread record");
    std::printf("Failures: %d\n",failures);return failures?1:0;
}
