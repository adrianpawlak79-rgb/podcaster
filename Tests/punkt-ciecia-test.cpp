/*
    punkt-ciecia-test.cpp — czy ciecie wypada w ciszy przed wersem, a nie w slowie.

    Krok 2 punch and roll (Source/DSP/CutPoint.h). Sygnal sztuczny, ale z ta
    sama budowa co czytanie: sylaby po 150 ms z przerwami 40 ms (za krotkie,
    zeby byc pauza), przecinek w polowie wersu (200 ms), miedzy wersami
    400 ms ciszy, pod spodem tlo pokoju. Poczatki wersow znamy co do probki.

    Pomiar na prawdziwym nagraniu (nic-dwa-razy, Tools/zs-sledzenie.cpp,
    ZS_WERSY=1): 12 z 15 wersow ciete 0,09–0,19 s przed pierwszym slowem;
    3 pozostale to wersy, w ktorych sledzenie bylo w zlym miejscu.
*/

#include "../Source/DSP/CutPoint.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static std::string num (double v) { char b[32]; std::snprintf (b, sizeof (b), "%.2f s", v); return b; }

struct Take { std::vector<float> x; std::vector<double> lineStart; double sr = 16000.0; };

/** wersy: 6 sylab, przecinek po 3., 400 ms miedzy wersami; tlo w dBFS */
static Take reading (int lines, double roomDb, unsigned seed = 3)
{
    Take t;
    std::mt19937 rng (seed);
    std::normal_distribution<float> n (0.0f, 1.0f);
    const float room = (float) std::pow (10.0, roomDb / 20.0);
    const double sr = t.sr;

    auto add = [&] (double sec, bool voiced)
    {
        const size_t len = (size_t) (sec * sr);
        for (size_t i = 0; i < len; ++i)
        {
            const double ph = (double) i / sr;
            float v = room * n (rng);
            if (voiced)
                v += (float) (0.3 * std::sin (2.0 * M_PI * 140.0 * ph) * std::sin (M_PI * (double) i / (double) len))
                   + 0.05f * n (rng);
            t.x.push_back (v);
        }
    };

    add (0.6, false);
    for (int l = 0; l < lines; ++l)
    {
        t.lineStart.push_back ((double) t.x.size() / sr);
        for (int s = 0; s < 6; ++s)
        {
            add (0.15, true);
            add (s == 2 ? 0.20 : 0.04, false);     // przecinek po 3. sylabie
        }
        add (0.40, false);
    }
    return t;
}

int main()
{
    std::printf ("\nPunkt ciecia\n============\n\n");

    // ---- 1. typowe spoznienie zapisu ----------------------------------------------
    {
        auto t = reading (8, -60.0);
        tw::CutFinder f; f.analyse (t.x.data(), t.x.size(), t.sr);

        int good = 0; std::string worst;
        for (double lag : { 0.3, 0.78, 1.0 })
            for (double s : t.lineStart)
            {
                const auto c = f.find (s + lag);
                const bool ok = c.inPause && c.sec <= s && c.sec >= s - 0.40;
                if (ok) ++good; else worst = "spoznienie " + num (lag) + ", ciecie " + num (c.sec - s);
            }
        check (good == 24, "spoznienie 0,3 / 0,78 / 1,0 s: ciecie w ciszy przed wersem",
               std::to_string (good) + "/24 " + worst);

        const auto c = f.find (t.lineStart[3] + 0.78);
        check (c.sec <= t.lineStart[3] - 0.10 && c.sec >= t.lineStart[3] - 0.20,
               "ciecie ok. 0,15 s przed pierwsza sylaba", num (c.sec - t.lineStart[3]));
    }

    // ---- 2. co NIE jest pauza ---------------------------------------------------------
    {
        auto t = reading (3, -60.0);
        tw::CutFinder f; f.analyse (t.x.data(), t.x.size(), t.sr);

        bool gapIsPause = false;
        for (const auto& p : f.pauses())
            if (p.second - p.first < 0.10) gapIsPause = true;
        check (! gapIsPause, "przerwa 40 ms miedzy sylabami to nie pauza");

        // przecinek po 3 sylabach (~0,57 s od poczatku wersu); zapis spozniony
        // o 0,3 s — przecinek konczy sie po zapisanym czasie + 0,25 s, wiec
        // nie wchodzi w gre
        const auto c = f.find (t.lineStart[1] + 0.3);
        check (c.sec <= t.lineStart[1], "przecinek po zapisanym czasie nie myli", num (c.sec - t.lineStart[1]));
    }

    // ---- 3. glosny pokoj -----------------------------------------------------------------
    {
        auto t = reading (6, -32.0);
        tw::CutFinder f; f.analyse (t.x.data(), t.x.size(), t.sr);
        int good = 0;
        for (double s : t.lineStart)
        {
            const auto c = f.find (s + 0.78);
            if (c.inPause && c.sec <= s && c.sec >= s - 0.40) ++good;
        }
        check (good == 6, "tlo -32 dBFS: prog idzie za tlem, ciecie dalej w ciszy",
               std::to_string (good) + "/6, prog " + std::to_string ((int) f.thresholdDb()) + " dB");
    }

    // ---- 4. bez pauzy ------------------------------------------------------------------------
    {
        Take t;
        for (int i = 0; i < 16000 * 6; ++i)
            t.x.push_back ((float) (0.3 * std::sin (2.0 * M_PI * 140.0 * i / 16000.0)));
        tw::CutFinder f; f.analyse (t.x.data(), t.x.size(), 16000.0);
        const auto c = f.find (3.0);
        check (! c.inPause && std::fabs (c.sec - (3.0 - 0.78)) < 1e-9,
               "mowa bez pauzy: czas minus mediana spoznienia, oznaczony jako niepewny", num (c.sec));
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
