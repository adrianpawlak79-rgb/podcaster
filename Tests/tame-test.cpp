/*  Sprawdzenie Tame na sygnalach, o ktorych wiem, co maja dac.  */
#include "../Source/DSP/Mix/Tame.h"
#include "../Source/DSP/Mix/Fft.h"
#include <cstdio>
#include <random>

static int fails = 0;
static void check(bool ok,const char*what,const char*got){
    std::printf("  %-5s %-46s %s\n", ok?"OK":"ZLE", what, got); if(!ok) ++fails; }

static double bandDb(const std::vector<float>&x,double sr,double lo,double hi){
    const size_t N=1<<15; if(x.size()<N) return -300;
    std::vector<double> re(N),im(N),w; mix::hann(w,(int)N);
    double s=0; int frames=0;
    for(size_t p=0;p+N<=x.size();p+=N/2){
        for(size_t i=0;i<N;++i){re[i]=x[p+i]*w[i];im[i]=0;}
        mix::Fft f; f.prepare((int)N); f.run(re,im,false);
        for(size_t i=0;i<N/2;++i){double fr=i*sr/N; if(fr>=lo&&fr<hi) s+=re[i]*re[i]+im[i]*im[i];}
        ++frames;
    }
    return 10*std::log10(s/std::max(1,frames)+1e-30);
}

int main(){
    const double sr=48000; const size_t N=(size_t)(sr*8);
    std::mt19937 rng(11); std::normal_distribution<float> g(0,0.03f);

    std::printf("\nTame — tlumienie tego, co wystaje ponad wlasna srednia\n");
    std::printf("=====================================================\n\n");

    // 1. staly szum pasmowy: NIC nie ma prawa sie zmienic
    {
        std::printf("1. staly jasny szum — barwa stala, wiec nie ma odchylenia\n");
        std::vector<float> x(N); for(auto&v:x) v=g(rng);
        mix::Tame t; t.prepare(sr); std::vector<float> y; t.render(x,y);
        double a=bandDb(x,sr,5000,10000), b=bandDb(y,sr,5000,10000);
        char s[64]; std::snprintf(s,64,"zmiana %+.2f dB",b-a);
        check(std::fabs(b-a)<1.0,"staly szum przechodzi bez zmian",s);
    }

    // 2. ten sam szum, ale z doklejonym wybuchem 6-9 kHz: wybuch ma byc sciety
    {
        std::printf("\n2. ten sam szum + krotkie wybuchy w 6-9 kHz (udawany sybilant)\n");
        std::vector<float> x(N); for(auto&v:x) v=g(rng);
        // wybuchy co sekunde, po 120 ms, +14 dB w pasmie 6-9 kHz
        std::vector<float> burst(N,0.0f);
        for(size_t s0=(size_t)sr; s0<N; s0+=(size_t)sr){
            for(size_t i=0;i<(size_t)(0.12*sr) && s0+i<N;++i){
                double ph=2*M_PI*7500*(double)i/sr;
                burst[s0+i]=(float)(0.12*std::sin(ph)*std::sin(M_PI*i/(0.12*sr)));
            }
        }
        std::vector<float> xb(N); for(size_t i=0;i<N;++i) xb[i]=x[i]+burst[i];
        mix::Tame t; t.prepare(sr); std::vector<float> y; t.render(xb,y);

        double a=bandDb(xb,sr,6500,8500), b=bandDb(y,sr,6500,8500);
        char s[64]; std::snprintf(s,64,"%+.2f dB w 6,5-8,5 kHz",b-a);
        check(b-a < -2.0,"wybuch zostaje sciety",s);

        double la=bandDb(xb,sr,300,1000), lb=bandDb(y,sr,300,1000);
        char s2[64]; std::snprintf(s2,64,"%+.2f dB w 0,3-1 kHz",lb-la);
        check(std::fabs(lb-la)<0.5,"dol nietkniety",s2);
    }

    // 3. glos jasny od poczatku do konca: barwa ma zostac
    {
        std::printf("\n3. szum z trwalym przechylem +8 dB w gorze\n");
        std::vector<float> x(N);
        double s1=0,s2v=0;
        for(size_t i=0;i<N;++i){ x[i]=g(rng); }
        // podbicie gory: filtr czyta WEJSCIE, nie wlasne wyjscie
        std::vector<float> src=x;
        for(size_t i=1;i<N;++i) x[i]=src[i]+1.2f*(src[i]-src[i-1]);
        (void)s1;(void)s2v;
        mix::Tame t; t.prepare(sr); std::vector<float> y; t.render(x,y);
        double a=bandDb(x,sr,5000,12000), b=bandDb(y,sr,5000,12000);
        char s[64]; std::snprintf(s,64,"zmiana %+.2f dB",b-a);
        check(std::fabs(b-a)<1.0,"trwala jasna barwa NIE jest tlumiona",s);
    }

    // 4. harmoniczne glosu nie moga byc uznane za odchylenie
    {
        std::printf("\n4. ton z harmonicznymi co 120 Hz\n");
        std::vector<float> x(N,0.0f);
        for(size_t i=0;i<N;++i){ double v=0;
            for(int h=1;h<=90;++h) v+=std::sin(2*M_PI*120*h*(double)i/sr)/h;
            x[i]=(float)(0.05*v); }
        mix::Tame t; t.prepare(sr); std::vector<float> y; t.render(x,y);
        double a=bandDb(x,sr,2000,10000), b=bandDb(y,sr,2000,10000);
        char s[64]; std::snprintf(s,64,"zmiana %+.2f dB",b-a);
        check(std::fabs(b-a)<1.5,"harmoniczne przechodza bez tlumienia",s);
    }

    std::printf("\n%s\n\n", fails==0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
