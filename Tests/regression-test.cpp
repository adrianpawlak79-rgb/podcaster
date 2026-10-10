#include "../Source/DSP/Mix/SpeechChain.h"
#include "../Source/DSP/TakeReview.h"
#include <cstdio>
#include <random>
#include <thread>

static int fails = 0;
static void check(bool ok, const char* name, double value = 0) {
    std::printf("%s  %s  %.6f\n", ok ? "OK" : "FAIL", name, value);
    if (!ok) ++fails;
}
static double rms(const std::vector<float>& x, size_t a, size_t b) {
    double e=0; for(size_t i=a;i<b;++i) e+=double(x[i])*x[i];
    return std::sqrt(e/double(b-a));
}
int main() {
    constexpr double sr=48000;
    {
        std::vector<float> x(48000);
        for(size_t i=0;i<x.size();++i) {
            double t=i/sr, env=std::min(1.,std::min(t/.05,(1.-t)/.05));
            x[i]=float(env*std::sin(2*M_PI*12000*t+M_PI/4));
        }
        mix::TruePeak m; m.prepare(sr,1); const float* p[]={x.data()};
        m.process(p,1,(int)x.size());
        check(std::abs(m.dbTP())<.2,"true peak: analytical 12kHz sine",m.dbTP());
    }
    {
        mix::DeEsser d;d.prepare(sr);
        for(int i=0;i<48000;++i)d.process(0);
        check(std::abs(d.gainReductionDb())<.01,"de-esser: silence releases shelf",d.gainReductionDb());
    }
    {
        std::vector<float>x(5000,.125f),y;
        mix::Tame t;t.params.maxCutDb=0;t.prepare(sr);t.render(x,y);
        double error=0;for(size_t i=0;i<x.size();++i)error=std::max(error,std::abs(double(x[i]-y[i])));
        check(error<1e-6,"Tame: neutral reconstruction including both edges",error);
        mix::SpectralDenoise d;d.params.reductionDb=0;d.prepare(sr);d.render(x,y);
        error=0;for(size_t i=0;i<x.size();++i)error=std::max(error,std::abs(double(x[i]-y[i])));
        check(error<1e-6,"denoise: neutral reconstruction including both edges",error);
    }
    {
        // All frequencies are FFT-bin centred; only the level changes.
        std::vector<float>x(48000*5),y;
        for(size_t i=0;i<x.size();++i) {
            double a=i<48000*3 ? .015 : .015*std::pow(10.,12./20);
            x[i]=float(a*(std::sin(2*M_PI*750*i/sr)+std::sin(2*M_PI*6000*i/sr)));
        }
        mix::Tame t;t.prepare(sr);t.render(x,y);
        auto amplitude=[](const std::vector<float>& v,double hz) {
            double re=0,im=0;
            for(int i=147456;i<153600;++i) {
                re+=v[i]*std::cos(2*M_PI*hz*i/48000.);
                im+=v[i]*std::sin(2*M_PI*hz*i/48000.);
            }
            return std::hypot(re,im);
        };
        double change=20*std::log10(amplitude(y,6000)/amplitude(x,6000));
        check(std::abs(change)<.5,"Tame: unchanged spectrum with +12dB level step",change);
    }
    {
        std::vector<float>x(48000*4),y;
        for(size_t i=0;i<x.size();++i)x[i]=float(.1*std::sin(2*M_PI*1500*i/sr));
        mix::SpectralDenoise d;d.prepare(sr);d.render(x,y);
        double change=20*std::log10(rms(y,96000,144000)/rms(x,96000,144000));
        check(std::abs(change)<.2,"denoise: clean sustained sound is not a noise profile",change);
    }
    {
        mix::SpeechChain c;c.setSampleRate(sr);
        c.params.useDenoise=false;c.params.useDeClick=false;c.params.useTame=false;
        c.params.useTiltMatch=false;c.params.useGate=false;c.params.useRider=false;
        c.params.useSaturation=false;c.params.useDeEsser=false;c.params.usePlosive=false;
        c.params.deHum=false;c.params.levelerRatio=1;c.params.peakRatio=1;
        c.params.mudDb=c.params.presenceDb=c.params.airDb=0;
        c.params.targetLufs=-40;c.prepare(sr);
        std::vector<float>x(48000),y; x.back()=.1f;c.render(x,y);
        check(std::abs(y.back())>1e-7,"chain: last input sample is not lost",y.back());
    }
    std::printf("Failures: %d\n",fails);return fails ? 1 : 0;
}
