/*
    przester-test.cpp — Decide::listen zauwaza przesterowane nagranie (raport
    audio 27.11, W3) i prosi prostym zdaniem o nagranie tych miejsc jeszcze
    raz, z czasem pierwszego z nich.

    Mowa z syntezatora (argv[1]) powielona do 20 s. Wersja przesterowana: od
    7. sekundy glos x6 i obciecie na 0,99 (jak za glosno ustawione wejscie).
    Czysta wersja i pojedyncze glosne probki nie moga dac tego zdania.
*/

#include "../Source/DSP/Mix/Decide.h"
#include "../Source/Wav.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

static int fails = 0;
static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static std::string clipLine (const mix::Decision& d)
{
    for (const auto& l : d.because)
        if (l.find ("przester") != std::string::npos) return l;
    return {};
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("uzycie: przester-test mowa.wav\n"); return 1; }
    wavio::Audio a;
    if (! wavio::read (argv[1], a).empty()) { std::printf ("  BLAD  brak mowy\n"); return 1; }
    const auto src = a.toMono();
    const double sr = a.sampleRate;

    std::vector<float> clean ((size_t) (20.0 * sr));
    for (size_t i = 0; i < clean.size(); ++i) clean[i] = src[i % src.size()];
    float mx = 0; for (float x : clean) mx = std::max (mx, std::fabs (x));
    for (auto& x : clean) x *= 0.5f / mx;          // szczyt -6 dBFS

    auto clipped = clean;
    for (size_t i = (size_t) (7.0 * sr); i < clipped.size(); ++i)
        clipped[i] = std::clamp (clipped[i] * 6.0f, -0.99f, 0.99f);

    // pojedyncze probki na 1,0 (trzask, jedna ostra gloska) to nie przester
    auto spikes = clean;
    for (double t : { 3.0, 9.0, 15.0 }) spikes[(size_t) (t * sr)] = 1.0f;

    const mix::Decide::Style styles[] = { mix::Decide::Style::Delikatny, mix::Decide::Style::Studyjny, mix::Decide::Style::Mocny };
    for (auto st : styles)
    {
        mix::Decide::Wish w; w.where = mix::Decide::Destination::Podcast; w.style = st;
        std::printf ("%s\n", mix::Decide::styleName (st));
        const auto line = clipLine (mix::Decide::listen (clipped, sr, w));
        check (! line.empty(), "przesterowane: jest zdanie o przesterze", line);
        check (line.find ("0:07") != std::string::npos, "podaje czas pierwszego miejsca (0:07)");
        check (line.find ("jeszcze raz") != std::string::npos, "prosi o ponowne nagranie");
        check (clipLine (mix::Decide::listen (clean, sr, w)).empty(), "czyste: bez zdania o przesterze");
        check (clipLine (mix::Decide::listen (spikes, sr, w)).empty(), "pojedyncze probki 1,0: bez zdania o przesterze");
    }

    std::printf (fails ? "\nBLEDY: %d\n" : "\nWszystko OK\n", fails);
    return fails ? 1 : 0;
}
