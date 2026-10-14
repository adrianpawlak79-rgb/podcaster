/*
    odzysk-test.cpp — naprawa naglowka WAV po przerwanym nagraniu (WavRepair.h).

    Audyt 14.10: po awarii programu w trakcie nagrania naglowek mowil
    "0 probek" i nagranie wygladalo na puste, choc dane byly na dysku.
*/

#include "../Source/WavRepair.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

static int fails = 0;
static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static std::string slurp (const std::string& p) { std::ifstream f (p, std::ios::binary); return std::string ((std::istreambuf_iterator<char> (f)), {}); }
static void dump (const std::string& p, const std::string& b) { std::ofstream (p, std::ios::binary) << b; }
static void put32 (std::string& b, size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) b[at + (size_t) i] = (char) ((v >> (8 * i)) & 0xff); }

int main()
{
    std::printf ("\nOdzyskanie przerwanego nagrania\n===============================\n\n");
    const std::string dir = "/tmp/odzysk-test";
    if (std::system (("rm -rf " + dir + " && mkdir -p " + dir).c_str()) != 0) return 1;

    // 1. jak po awarii: naglowek z zerowymi rozmiarami, dane w pliku
    wavio::Audio a;
    a.sampleRate = 48000;
    a.channels.assign (1, std::vector<float> (48000 * 3));
    for (size_t i = 0; i < a.channels[0].size(); ++i) a.channels[0][i] = 0.3f * (float) std::sin (0.01 * (double) i);
    const std::string p1 = dir + "/sesja przerwana ż.wav";
    check (wavio::write24 (p1, a).empty(), "plik testowy");
    auto bytes = slurp (p1);
    put32 (bytes, 4, 36); put32 (bytes, 40, 0);              // "0 probek", jak po awarii
    bytes += std::string (2, '\0');                          // niepelna ramka na koncu
    dump (p1, bytes);
    wavio::Audio b;
    check (wavio::read (p1, b).empty() && b.frames() == 0, "przed naprawa: czytnik widzi puste nagranie");
    const auto r = wavio::repairHeader (p1);
    char s[64]; std::snprintf (s, sizeof (s), "%.2f s", r.seconds);
    check (r.ok && r.changed && std::fabs (r.seconds - 3.0) < 1e-6, "naprawa: odzyskane 3 s", s);
    const bool readOk = wavio::read (p1, b).empty();
    check (readOk && b.frames() == a.frames(), "po naprawie: wszystkie probki", std::to_string (b.frames()));
    check (std::fabs (b.channels[0][1000] - a.channels[0][1000]) < 1e-5, "tresc bez zmian");

    // 2. zdrowy plik: nic nie zmienia
    const auto r2 = wavio::repairHeader (p1);
    check (r2.ok && ! r2.changed, "poprawny plik zostaje bez zmian");

    // 3. jak z JUCE: float 32 bit i fragment JUNK przed danymi, naglowek nieaktualny
    {
        std::string w;
        auto add = [&] (const void* d, size_t n) { w.append ((const char*) d, n); };
        const uint32_t zero = 0, sixteen = 16, rate = 48000, byteRate = 48000 * 4, junk = 28;
        const uint16_t fmt = 3, ch = 1, align = 4, bits = 32;
        add ("RIFF", 4); add (&zero, 4); add ("WAVE", 4);
        add ("JUNK", 4); add (&junk, 4); w.append (28, '\0');
        add ("fmt ", 4); add (&sixteen, 4); add (&fmt, 2); add (&ch, 2); add (&rate, 4); add (&byteRate, 4); add (&align, 2); add (&bits, 2);
        add ("data", 4); const uint32_t stale = 4 * 48000; add (&stale, 4);   // naglowek z chwili 1 s
        for (int i = 0; i < 48000 * 2; ++i) { const float v = 0.1f; add (&v, 4); }   // naprawde 2 s
        const std::string p3 = dir + "/juce.wav";
        dump (p3, w);
        const auto r3 = wavio::repairHeader (p3);
        std::snprintf (s, sizeof (s), "%.2f s", r3.seconds);
        check (r3.ok && r3.changed && std::fabs (r3.seconds - 2.0) < 1e-6, "float 32 + JUNK: naglowek z 1 s poprawiony na 2 s", s);
        check (wavio::read (p3, b).empty() && b.frames() == 96000, "odczyt po naprawie");
    }

    // 4. smieci i za krotkie pliki: blad, nie wywrotka
    dump (dir + "/smiec.wav", "to nie jest wav, tylko tekst, ktory ma ponad czterdziesci cztery bajty");
    check (! wavio::repairHeader (dir + "/smiec.wav").ok, "nie-WAV: blad");
    dump (dir + "/krotki.wav", "RIFF");
    check (! wavio::repairHeader (dir + "/krotki.wav").ok, "za krotki: blad");
    check (! wavio::repairHeader (dir + "/nie-ma.wav").ok, "brak pliku: blad");

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
