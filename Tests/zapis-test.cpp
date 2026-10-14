/*
    zapis-test.cpp — bezpieczny zapis i odczyt WAV (Wav.h).

    Audyt 14.10 (ChatGPT): zapis ignorowal bledy fwrite/fclose i zwracal
    sukces — przy pelnym dysku ekran pokazywal "Gotowe", a pliku nie bylo.
    Uszkodzony naglowek (bits=1) dawal dzielenie przez zero przy odczycie.
*/

#include "../Source/Wav.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

static int fails = 0;
static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static bool exists (const std::string& p) { std::ifstream f (p, std::ios::binary); return f.good(); }

int main()
{
    std::printf ("\nBezpieczny zapis WAV\n====================\n\n");

    wavio::Audio a;
    a.sampleRate = 48000;
    a.channels.assign (1, std::vector<float> (48000));
    for (size_t i = 0; i < a.channels[0].size(); ++i) a.channels[0][i] = 0.5f * (float) std::sin (2.0 * M_PI * 440.0 * (double) i / 48000.0);

    // 1. zwykly zapis i odczyt
    const std::string dir = "/tmp/zapis-test-katalog";
    (void) std::system (("rm -rf " + dir + " && mkdir -p " + dir).c_str());
    const std::string path = dir + "/odcinek żółty ąę.wav";
    check (wavio::write24 (path, a).empty(), "zapis 24 bit do nazwy z polskimi literami");
    check (! exists (path + ".zapis.tmp"), "po zapisie nie zostaje plik tymczasowy");
    wavio::Audio b;
    const auto rerr = wavio::read (path, b);
    double maxErr = 0.0;
    if (rerr.empty() && b.frames() == a.frames())
        for (size_t i = 0; i < a.frames(); ++i) maxErr = std::max (maxErr, (double) std::fabs (a.channels[0][i] - b.channels[0][i]));
    check (rerr.empty() && b.frames() == a.frames() && maxErr < 1.0e-6, "odczyt daje to samo (24 bit)", std::to_string (maxErr));

    // 2. podmiana istniejacego pliku
    a.channels[0][0] = 0.25f;
    check (wavio::write16 (path, a).empty(), "zapis na istniejacy plik podmienia go");
    check (wavio::read (path, b).empty() && std::fabs (b.channels[0][0] - 0.25f) < 1.0e-4, "nowa tresc po podmianie");

    // 3. pelny dysk: /dev/full odrzuca kazdy zapis. Piszemy do "katalogu"
    // z dowiazaniem: plik tymczasowy = /dev/full nie jest mozliwy, wiec
    // sprawdzamy writePcm na sciezce, gdzie temp jest dowiazaniem do /dev/full.
    if (exists ("/dev/full"))
    {
        const std::string full = dir + "/pelny.wav";
        (void) std::system (("ln -sf /dev/full '" + full + ".zapis.tmp'").c_str());
        const auto err = wavio::write24 (full, a);
        check (! err.empty(), "pelny dysk: zapis zglasza blad", err);
        check (! exists (full), "pelny dysk: pod docelowa nazwa nie ma polowy pliku");
    }

    // 4. brak dostepu do folderu
    check (! wavio::write24 ("/nie-ma-takiego-folderu/x.wav", a).empty(), "brak folderu: blad, nie sukces");

    // 5. puste i dziwne dane
    wavio::Audio empty;
    check (! wavio::write24 (dir + "/pusty.wav", empty).empty(), "nic do zapisania: blad");
    a.channels[0][10] = std::nanf ("");
    check (wavio::write24 (dir + "/nan.wav", a).empty() && wavio::read (dir + "/nan.wav", b).empty()
               && b.channels[0][10] == 0.0f, "NaN zapisany jako cisza, nie smieci");

    // 6. uszkodzony naglowek: bits = 1 (dawniej dzielenie przez zero)
    {
        std::ifstream in (path, std::ios::binary);
        std::string bytes ((std::istreambuf_iterator<char> (in)), {});
        bytes[34] = 1; bytes[35] = 0;                       // bitsPerSample w fmt
        std::ofstream (dir + "/zly.wav", std::ios::binary) << bytes;
        wavio::Audio c;
        const auto err = wavio::read (dir + "/zly.wav", c);
        check (! err.empty(), "naglowek z bits=1: blad zamiast wywrotki", err);
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
