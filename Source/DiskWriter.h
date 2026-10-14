#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>

// Producer: audio only. Consumer: this disk thread only. The audio path has no
// mutex, wakeup syscall, file access or allocation. Disk errors are published.
class DiskWriter final : private juce::Thread {
public:
    explicit DiskWriter(std::unique_ptr<juce::AudioFormatWriter> w, int capacity=262144)
        : juce::Thread("zapis nagrania"), writer(std::move(w)),
          buffer((int)writer->getNumChannels(),capacity), fifo(capacity) { if(!startThread()) failed.store(true); }
    ~DiskWriter() override {
        signalThreadShouldExit();
        waitForThreadToExit(-1); // control thread only; never forcibly kill a file writer
    }
    bool write(const float* const* input,int count) noexcept {
        if(failed.load() || count<=0)return false;
        int a,na,b,nb;fifo.prepareToWrite(count,a,na,b,nb);
        if(na+nb<count)return false;
        for(int c=0;c<buffer.getNumChannels();++c) {
            if(input[c]==nullptr)return false;
        }
        for(int c=0;c<buffer.getNumChannels();++c) {
            buffer.copyFrom(c,a,input[c],na);
            if(nb)buffer.copyFrom(c,b,input[c]+na,nb);
        }
        fifo.finishedWrite(count);return true;
    }
    bool hasFailed() const noexcept { return failed.load(); }
    void finish() {
        signalThreadShouldExit();waitForThreadToExit(-1);
    }
private:
    void run() override {
        while(!threadShouldExit() || fifo.getNumReady()>0) {
            int a,na,b,nb;fifo.prepareToRead(16384,a,na,b,nb);
            if(na+nb==0){ wait(2);continue; }
            if(!failed.load()) {
                if((na && !writer->writeFromAudioSampleBuffer(buffer,a,na)) ||
                   (nb && !writer->writeFromAudioSampleBuffer(buffer,b,nb))) failed.store(true);
            }
            fifo.finishedRead(na+nb);
            // Naglowek WAV z biezaca dlugoscia co ~2 s (flush w JUCE go przepisuje).
            // Bez tego naglowek mowil "0 probek" az do zatrzymania i po awarii
            // programu nagranie wygladalo na puste (audyt 14.10).
            sinceHeader+=na+nb;
            if(!failed.load() && sinceHeader>=(long long)(writer->getSampleRate()*2.0)) {
                sinceHeader=0;
                if(!writer->flush())failed.store(true);
            }
        }
        if(!writer->flush())failed.store(true);
    }
    std::unique_ptr<juce::AudioFormatWriter> writer;
    juce::AudioBuffer<float> buffer;
    juce::AbstractFifo fifo;
    std::atomic<bool> failed{false};
    long long sinceHeader=0;     // tylko watek zapisu
};
