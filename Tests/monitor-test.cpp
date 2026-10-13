/*
    monitor-test.cpp — kalibracja odsluchu.

    ---------------------------------------------------------------------------
    SKAD SIE WZIAL TEN KROK

    Czlowiek, ktory zna LUFS, ma wlasny miks na dysku i czytal moje tabele
    pomiarow, przez cztery rundy odsluchu mowil przy kazdym wariancie "za
    glosne". Szukalem przyczyny w kompresji, w de-esserze, w barwie i w celu
    glosnosci. Na koncu sam ja znalazl: mial przykrecony YouTube i rozkrecona
    karte. Podsumowal to tak: "widzisz, ja mam wiedze i i tak sie lapie".

    Jesli lapie sie on, to ktos nagrywajacy pierwszy podcast nie ma szans — i
    nawet sie nie dowie, ze nie ma, bo wszystko bedzie brzmialo "jakos".

    Kalibracja uzywa wlasnego glosu czlowieka zamiast szumu i miernika SPL,
    bo miernika nikt nie ma, a wlasny glos ma kazdy i kazdy zna jego glosnosc.
*/
#include "../Source/DSP/Setup.h"
#include "../Source/Wav.h"
#include <cstdio>
#include <random>
static int fails=0;
static void check(bool ok,const char*w,const char*g){
    std::printf("  %-5s %-44s %s\n",ok?"OK":"ZLE",w,g); if(!ok)++fails; }

// przepusc kreator przez wszystkie kroki, podajac zadany material
static void run(tw::SetupWizard& z,double sr,const std::vector<float>& mat,float gain=1.0f){
    size_t i=0;
    while(!z.stepComplete()){
        float b[256];
        for(int k=0;k<256;++k){ b[k]= mat.empty()?0.0f: mat[(i++)%mat.size()]*gain; }
        z.feed(b,256);
    }
}
int main(int argc,char**argv){
    const double sr=48000;
    wavio::Audio sp;
    if(!wavio::read(argv[1],sp).empty()){std::printf("brak mowy\n");return 1;}
    auto mowa=sp.toMono();
    std::mt19937 rng(3); std::normal_distribution<float> n(0,0.0008f);
    std::vector<float> cisza((size_t)(sr*7)); for(auto&v:cisza) v=n(rng);

    std::printf("\nKalibracja odsluchu\n===================\n\n");

    auto przejdz=[&](tw::SetupWizard& z,const std::vector<float>& monitorMat,float mg){
        z.begin(sr);
        run(z,sr,mowa);     z.evaluate(); z.advance();   // Device
        run(z,sr,cisza);    z.evaluate(); z.advance();   // Room
        run(z,sr,mowa);     z.evaluate(); z.advance();   // Level
        run(z,sr,mowa);     z.evaluate(); z.advance();   // Plosive
        run(z,sr,cisza);    z.evaluate(); z.advance();   // Bleed
        run(z,sr,monitorMat,mg);
        return z.evaluate();
    };

    // 1. sluchawki: mikrofon slyszy tylko pokoj
    { tw::SetupWizard z; auto f=przejdz(z,cisza,1.0f);
      char s[96]; std::snprintf(s,96,"%s",f.measured.c_str());
      check(!z.result().monitorMeasured,"cisza w mikrofonie = sluchawki",s);
      check(f.passed,"krok nie jest porazka, tylko bez pomiaru","nie blokuje"); }

    // 2. glosniki ustawione dobrze: odtwarzanie tak glosne jak glos
    { tw::SetupWizard z; auto f=przejdz(z,mowa,1.0f);
      char s[96]; std::snprintf(s,96,"%+.1f dB",z.result().monitorVsVoiceDb);
      check(z.result().monitorMeasured,"mikrofon slyszy odsluch",s);
      check(std::fabs(z.result().monitorVsVoiceDb)<2.0,"rowny poziom = okolo zera",s);
      check(f.passed,"zaliczone",f.headline.c_str()); }

    // 3. za glosno: +12 dB
    { tw::SetupWizard z; auto f=przejdz(z,mowa,4.0f);
      char s[96]; std::snprintf(s,96,"%+.1f dB",z.result().monitorVsVoiceDb);
      check(z.result().monitorVsVoiceDb>10.0,"+12 dB wykryte jako za glosno",s);
      check(f.passed && f.headline.find("Odsłuch gra")==0,"mowi, ale nie zatrzymuje (to ustawienie do miksu)",f.headline.c_str()); }

    // 4. za cicho: -12 dB
    { tw::SetupWizard z; auto f=przejdz(z,mowa,0.25f);
      char s[96]; std::snprintf(s,96,"%+.1f dB",z.result().monitorVsVoiceDb);
      check(z.result().monitorVsVoiceDb<-10.0,"-12 dB wykryte jako za cicho",s);
      check(f.passed && f.headline.find("Odsłuch gra")==0,"mowi, ale nie zatrzymuje (to ustawienie do miksu)",f.headline.c_str()); }

    // 5. material kalibracyjny to wlasny glos, wyrownany
    { tw::SetupWizard z; przejdz(z,cisza,1.0f);
      auto a=z.calibrationAudio();
      double pk=0,sum=0; int cnt=0;
      for(float v:a) pk=std::max(pk,(double)std::fabs(v));
      for(float v:a) if(std::fabs(v)>pk*0.05){sum+=(double)v*v;++cnt;}
      double rms=cnt?std::sqrt(sum/cnt):0;
      char s[96]; std::snprintf(s,96,"%.4f (cel 0,0500), %.1f s",rms,a.size()/sr);
      check(!a.empty()&&std::fabs(rms-0.05)<0.004,"wyrownany do zadanej wartosci",s);
      check(pk<=1.0,"nic sie nie obcina","szczyt w normie"); }

    std::printf("\n%s\n\n",fails==0?"wszystko przeszlo":"SA BLEDY");
    return fails;
}
