/*
    zs-mp3 — WAV na MP3 tym samym koderem, ktorego uzywa program (Mp3.cpp).

    Po co: sprawdzic zapis MP3 bez okna i bez nagrywania — na Windowsie
    albo pod Wine (tam, gdzie Wine ma koder MP3).

      zs-mp3 wejscie.wav wyjscie.mp3 [kbit/s]

    Budowanie (MinGW na Linuksie):
      x86_64-w64-mingw32-g++ -std=c++17 -O2 -ISource Tools/zs-mp3.cpp Source/Mp3.cpp \
        -lmfuuid -lole32 -static -o zs-mp3.exe
*/

#include "Mp3.h"
#include "Wav.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("uzycie: zs-mp3 wejscie.wav wyjscie.mp3 [kbit/s]\n"); return 2; }

    wavio::Audio a;
    if (auto e = wavio::read (argv[1], a); ! e.empty()) { std::printf ("%s\n", e.c_str()); return 1; }

    std::vector<const float*> ch;
    for (auto& c : a.channels) ch.push_back (c.data());

    std::printf ("koder MP3 w systemie: %s\n", zs::mp3::available() ? "jest" : "BRAK");
    const auto t0 = std::chrono::steady_clock::now();
    const auto err = zs::mp3::write (argv[2], ch, a.frames(), a.sampleRate, argc > 3 ? std::atoi (argv[3]) : 128);
    const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    if (! err.empty()) { std::printf ("BLAD: %s\n", err.c_str()); return 1; }
    std::printf ("zapisano %s: %.1f s nagrania w %.2f s\n", argv[2], (double) a.frames() / a.sampleRate, s);
    return 0;
}
