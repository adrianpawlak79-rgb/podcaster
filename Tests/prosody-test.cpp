/*
    prosody-test.cpp — czy tempo i melodia sa mierzone, czy zgadywane.

    Ten test ma mocniejszy punkt odniesienia niz wszystkie poprzednie. Mowa
    pochodzi z espeak-ng, a syntezatorowi PODAJE SIE tempo w slowach na minute —
    wiec prawdziwa odpowiedz jest znana z gory i nie pochodzi ode mnie. Liczbe
    sylab w tekscie tez da sie policzyc niezaleznie, ze zrodla.

    Sprawdzamy trzy rzeczy, w kolejnosci waznosci:

      1. czy liczba wykrytych sylab zgadza sie z liczba sylab w tekscie
      2. czy zmierzone tempo rosnie tak, jak kazano rosnac syntezatorowi
      3. czy monotonia odroznia czytanie jednym tonem od mowy z melodia

    Punkt drugi jest wazniejszy niz bezwzgledna dokladnosc: uzytkownikowi mowimy
    "zwolnij", a nie "masz dokladnie 163,4 slowa na minute".

        g++ -O2 -std=c++17 -I../Source prosody-test.cpp -o prosody-test
*/

#include "../Source/DSP/Prosody.h"
#include "../Source/Wav.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;

static void check (bool ok, const std::string& what, const std::string& detail = {})
{
    std::printf ("      %s  %-46s %s\n", ok ? "OK  " : "BLAD", what.c_str(), detail.c_str());
    if (! ok) ++failures;
}

static std::string num (double v, int d = 1)
{
    char b[64]; std::snprintf (b, sizeof (b), "%.*f", d, v); return b;
}

static bool load (const std::string& path, std::vector<float>& out, double& sr)
{
    wavio::Audio a;
    if (! wavio::read (path, a).empty()) return false;
    out = a.toMono();
    sr = a.sampleRate;
    return out.size() > 1000;
}

//==============================================================================
int main (int argc, char** argv)
{
    // liczba sylab w tekscie, policzona niezaleznie i podana z zewnatrz
    const int trueSyllables = argc > 1 ? std::atoi (argv[1]) : 0;

    std::printf ("\nTempo i melodia — mowa o ZNANYM tempie\n");
    std::printf ("======================================\n");

    struct Case { int setting; const char* file; };
    const Case cases[] = {
        { 110, "/tmp/mowa/tempo110.wav" },
        { 150, "/tmp/mowa/tempo150.wav" },
        { 190, "/tmp/mowa/tempo190.wav" },
    };

    double measured[3] = { 0, 0, 0 };
    int    counted[3]  = { 0, 0, 0 };
    bool   haveAll = true;

    std::printf ("\n  1. liczba sylab (w tekscie jest %d)\n", trueSyllables);

    for (int i = 0; i < 3; ++i)
    {
        std::vector<float> x;
        double sr = 48000.0;

        if (! load (cases[i].file, x, sr)) { haveAll = false; continue; }

        tw::Prosody p;
        const auto r = p.analyse (x, sr);

        measured[i] = r.syllablesPerMin;
        counted[i]  = r.syllables;

        const double err = trueSyllables > 0
                         ? 100.0 * std::fabs (r.syllables - trueSyllables) / trueSyllables
                         : 0.0;

        check (trueSyllables == 0 || err < 25.0,
               "przy -s " + std::to_string (cases[i].setting),
               "wykryte " + std::to_string (r.syllables) + ", blad " + num (err, 0) + "%");
    }

    if (! haveAll)
    {
        std::printf ("\n  (brak plikow testowych — reszta pominieta)\n\n");
        return failures == 0 ? 0 : 1;
    }

    std::printf ("\n  2. czy tempo rosnie tak, jak kazano syntezatorowi\n");

    check (measured[1] > measured[0] * 1.15 && measured[2] > measured[1] * 1.15,
           "kazde szybsze ustawienie mierzy sie szybciej",
           num (measured[0], 0) + " -> " + num (measured[1], 0) + " -> "
           + num (measured[2], 0) + " sylab/min");

    // syntezator dostal 110 i 190, czyli 1,73 raza szybciej
    const double ratio = measured[2] / std::max (1.0, measured[0]);
    check (ratio > 1.4 && ratio < 2.1,
           "stosunek najszybszego do najwolniejszego",
           num (ratio, 2) + " przy zadanym 1,73");

    // Prawdziwe tempo znamy: tekst ma znana liczbe slow, a plik znana dlugosc.
    // To jest jedyny w calym zestawie punkt odniesienia, ktory nie pochodzi
    // ode mnie.
    if (argc > 2)
    {
        std::printf ("\n  2b. tempo bezwzglednie, wobec prawdy z tekstu\n");

        const double trueWpm = std::atof (argv[2]);

        std::vector<float> x;
        double sr = 48000.0;
        load (cases[1].file, x, sr);

        tw::Prosody p;
        const auto r = p.analyse (x, sr);

        const double err = 100.0 * std::fabs (r.wordsPerMin - trueWpm) / trueWpm;

        check (err < 30.0, "slow na minute w granicach 30% prawdy",
               num (r.wordsPerMin, 0) + " zmierzone, " + num (trueWpm, 0) + " naprawde");
    }

    std::printf ("\n  3. melodia\n");

    {
        std::vector<float> x;
        double sr = 48000.0;
        load (cases[1].file, x, sr);

        tw::Prosody p;
        const auto r = p.analyse (x, sr);

        check (r.medianF0Hz > 70.0 && r.medianF0Hz < 220.0,
               "wysokosc glosu w sensownym zakresie",
               num (r.medianF0Hz, 0) + " Hz");

        std::printf ("      ---   rozrzut melodii %s poltonu, %s\n",
                     num (r.f0RangeSemis, 1).c_str(),
                     r.monotone ? "uznane za monotonne" : "uznane za zywe");

        std::printf ("      ---   pauzy: %d, najdluzsza %s s, za dlugich %d\n",
                     r.pauses, num (r.longestPauseSec, 2).c_str(), r.longPauses);

        std::printf ("      ---   podejrzenia o wypelniacz: %d\n",
                     (int) r.fillerAtSec.size());
    }

    // ---- monotonia -----------------------------------------------------------
    // Pierwsza wersja tego testu rozchwiewala melodie mowy o polton i sprawdzala,
    // czy rozrzut urosnie. Urosl o 0,3 poltonu przy naturalnym rozrzucie 4,7 —
    // czyli test mierzyl szum, nie zjawisko. Wlasciwe pytanie brzmi inaczej:
    // czy DECYZJA "to jest czytane jednym tonem" zapada tam, gdzie powinna.
    // Wiec budujemy material naprawde monotonny i sprawdzamy oba przypadki.
    {
        const double sr = 48000.0;
        const size_t n = (size_t) (20.0 * sr);
        std::vector<float> flatVoice (n, 0.0f);

        double phase = 0.0;

        for (size_t i = 0; i < n; ++i)
        {
            const double t = (double) i / sr;

            // sylaby: sto dwadziescia na minute, obwiednia dzwonu
            const double inSyl = std::fmod (t, 0.5);
            const double env = inSyl < 0.3 ? std::sin (M_PI * inSyl / 0.3) : 0.0;

            phase += 2.0 * M_PI * 112.0 / sr;      // wysokosc STALA
            double v = 0.0;
            for (int h = 1; h <= 16; ++h) v += std::sin (phase * h) / (h * h);

            flatVoice[i] = (float) (v * 1.55 * env * 0.25);
        }

        tw::Prosody p;
        const auto flat = p.analyse (flatVoice, sr);

        std::vector<float> x;
        double sr2 = 48000.0;
        load (cases[1].file, x, sr2);
        const auto real = p.analyse (x, sr2);

        check (flat.monotone,
               "staly ton rozpoznany jako monotonny",
               "rozrzut " + num (flat.f0RangeSemis, 2) + " poltonu");

        check (! real.monotone,
               "prawdziwa mowa NIE jest uznana za monotonna",
               "rozrzut " + num (real.f0RangeSemis, 2) + " poltonu");

        check (flat.syllables > 30 && flat.syllables < 50,
               "sylaby policzone w znanym rytmie (jest 40)",
               std::to_string (flat.syllables) + " wykrytych");
    }

    std::printf ("\n======================================\n");
    std::printf ("%s\n\n", failures == 0 ? "wszystko przeszlo"
                                         : (std::to_string (failures) + " nieprzeszlych").c_str());
    return failures == 0 ? 0 : 1;
}
