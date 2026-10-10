/*
    prompter-test.cpp — czy prompter nie skacze.

    ---------------------------------------------------------------------------
    SKAD SIE WZIAL TEN TEST

    Prompter dzialal i mial zmierzona dokladnosc: 96% czasu w granicach jednego
    slowa. Czlowiek uruchomil go i powiedzial: "skacze jak chce i zaznacza nie
    te linie ktore czytam".

    Obie rzeczy byly prawdziwe i zadnej nie widac w tamtej liczbie. Dokladnosc
    mowi, GDZIE program jest. Nie mowi nic o tym, jak sie tam dostaje — a tekst,
    ktory trafia we wlasciwy wers po skoku w tyl i z powrotem, jest bezuzyteczny,
    choc statystycznie poprawny.

    Zmierzone przed poprawka: 3 z 5 dosuniec na pauzie COFALO pozycje, bo
    program szukal najblizszego znaku interpunkcyjnego w obie strony, a ten
    czesto lezal juz za czlowiekiem. Czytajacy nie wraca.

    Ten test pilnuje ruchu, nie polozenia.
*/

#include "../Source/DSP/Follower.h"
#include "../Source/DSP/SyllableClock.h"
#include "../Source/Wav.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

int main(int argc,char**argv){
    std::ifstream f(argv[1]); std::stringstream b; b<<f.rdbuf();
    tw::PrompterText t; t.set(b.str());
    wavio::Audio in; wavio::read(argv[2],in);
    auto x=in.toMono(); double sr=in.sampleRate;

    // udawane lamanie na wersy: po 8 slow, jak na ekranie
    auto lineOf=[&](int w){ return w/8; };

    tw::SyllableClock c; c.prepare(sr);
    tw::Follower fo; fo.begin(&t);

    int prevLine=-1, forward=0, back=0, biggestBack=0;
    int snapsBack=0;
    double prevPos=0;
    const int B=512;
    for(size_t p=0;p<x.size();p+=B){
        int n=(int)std::min<size_t>(B,x.size()-p);
        int got=c.process(x.data()+p,n);
        double now=(p+n)/sr;
        double before=fo.syllablePosition();
        fo.advance(now,got,c.pauseLength());
        double after=fo.syllablePosition();
        if(after<before-0.5) ++snapsBack;
        int L=lineOf(fo.currentWord());
        if(prevLine>=0 && L!=prevLine){
            if(L>prevLine) ++forward;
            else { ++back; biggestBack=std::max(biggestBack,prevLine-L); }
        }
        prevLine=L; prevPos=after;
    }
    int fails=0;
    auto check=[&](bool ok,const char*w,const char*g){
        std::printf("  %-5s %-44s %s\n",ok?"OK":"ZLE",w,g); if(!ok)++fails; };

    char s1[64],s2[64],s3[64];
    std::snprintf(s1,64,"%d w przod, %d w tyl",forward,back);
    std::snprintf(s2,64,"%d wersow",biggestBack);
    std::snprintf(s3,64,"%d dosuniec, %d slow",fo.snapCount(),t.wordCount());

    std::printf("\nPrompter — czy tekst jedzie w jedna strone\n");
    std::printf("=========================================\n\n");

    check(back==0,"wers nigdy nie cofa sie sam",s1);
    check(biggestBack==0,"zaden skok wstecz",s2);
    check(forward>=3,"tekst w ogole jedzie do przodu",s1);
    check(fo.snapCount()>0,"dosuwanie na pauzach dziala",s3);

    std::printf("\n%s\n\n",fails==0?"wszystko przeszlo":"SA BLEDY");
    return fails;
}
