/*  Wybor kanalu: mikrofon w jednym wejsciu, szum w drugim.

    Logika Recordera siedzi w klasie z JUCE, wiec tu jest odtworzona jej
    arytmetyka — to, co naprawde decyduje. Test pilnuje reguly, nie kodu:
    kanal z glosem ma wygrac, szum ma przegrac, a przy dwoch mikrofonach
    oba maja zostac.
*/
#include "../Source/Wav.h"
#include <cstdio>
#include <random>
#include <cmath>
#include <algorithm>

static int fails=0;
static void check(bool ok,const char*w,const char*g){
    std::printf("  %-5s %-44s %s\n",ok?"OK":"ZLE",w,g); if(!ok)++fails; }

struct Chooser {
    static constexpr int N=8;
    float env[N]{}; int chosen=0; bool both=false;
    void feed(const float* const* in,int nch,int n){
        for(int c=0;c<std::min(nch,N);++c){
            float pk=0; for(int i=0;i<n;++i) pk=std::max(pk,std::fabs(in[c][i]));
            env[c] = pk>env[c] ? pk : env[c]*0.9985f+pk*0.0015f;
        }
        int best=0; float bestE=0;
        for(int c=0;c<std::min(nch,N);++c) if(env[c]>bestE){bestE=env[c];best=c;}
        if(bestE>1e-4f && bestE>env[chosen]*2.0f) chosen=best;
        if(nch>=2 && env[0]>1e-4f && env[1]>1e-4f){
            float lo=std::min(env[0],env[1]), hi=std::max(env[0],env[1]);
            both = lo>hi*0.5f;
        } else both=false;
    }
};

int main(int argc,char**argv){
    wavio::Audio sp; if(!wavio::read(argv[1],sp).empty()){std::printf("brak mowy\n");return 1;}
    auto mowa=sp.toMono(); double sr=sp.sampleRate;
    std::mt19937 rng(9); std::normal_distribution<float> n(0,0.0015f);

    std::printf("\nWybor kanalu wejsciowego\n========================\n\n");

    auto run=[&](bool voiceLeft,bool twoMics,double sec){
        Chooser ch; size_t pos=0;
        const int B=512; std::vector<float> a(B),b(B);
        for(double t=0;t<sec;t+=B/sr){
            for(int i=0;i<B;++i){
                float v=mowa[(pos+i)%mowa.size()];
                float q=twoMics? mowa[(pos+i+mowa.size()/3)%mowa.size()]*0.7f : n(rng);
                a[i]= voiceLeft? v : q;
                b[i]= voiceLeft? q : v;
            }
            pos+=B;
            const float* io[2]={a.data(),b.data()};
            ch.feed(io,2,B);
        }
        return ch; };

    { auto c=run(true,false,20); char s[64]; std::snprintf(s,64,"wybrany kanal %d",c.chosen+1);
      check(c.chosen==0,"mikrofon w lewym — wybiera lewy",s);
      check(!c.both,"nie uznaje szumu za drugie zrodlo","jeden kanal"); }

    { auto c=run(false,false,20); char s[64]; std::snprintf(s,64,"wybrany kanal %d",c.chosen+1);
      check(c.chosen==1,"mikrofon w prawym — wybiera prawy",s);
      check(!c.both,"nie uznaje szumu za drugie zrodlo","jeden kanal"); }

    { auto c=run(true,true,20);
      check(c.both,"dwa mikrofony — zapisuje oba","oba kanaly zyja"); }

    // szum musi byc naprawde odrzucony, nie tylko przegrac o wlos
    { auto c=run(true,false,20);
      char s[80]; std::snprintf(s,80,"glos %.4f, szum %.4f",c.env[0],c.env[1]);
      check(c.env[0]>c.env[1]*4.0f,"glos gorem o ponad 12 dB",s); }

    std::printf("\n%s\n\n",fails==0?"wszystko przeszlo":"SA BLEDY");
    return fails;
}
