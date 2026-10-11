/*
    preroll-test.cpp — co gra w sluchawkach po R i kiedy pada "Czytaj".

    Krok 5 punch and roll (Source/DSP/PreRoll.h). Najwazniejsze sprawdzenie:
    pre-roll konczy sie DOKLADNIE tam, gdzie po zatrzymaniu bedzie szew
    (Punch.h). Gdyby konczyl sie gdzie indziej, czlowiek dopasowywalby ton
    i tempo do czegos, czego w czystym pliku nie ma.

    Sztuczne czytanie jak w punch-test: sylaby po 150 ms, pauzy 400 ms miedzy
    wersami, tlo -60 dBFS.
*/

#include "../Source/DSP/PreRoll.h"
#include "../Source/DSP/Punch.h"

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

static std::string num (double v, const char* u = " s") { char b[40]; std::snprintf (b, sizeof (b), "%.3f%s", v, u); return b; }

struct Gen
{
    std::vector<float> x;
    double sr = 16000.0;
    std::mt19937 rng { 11 };
    std::normal_distribution<float> n { 0.0f, 1.0f };

    double now() const { return (double) x.size() / sr; }

    void silence (double sec) { for (size_t i = 0, e = (size_t) (sec * sr); i < e; ++i) x.push_back (0.001f * n (rng)); }

    void syllables (int k, double hz)
    {
        for (int s = 0; s < k; ++s)
        {
            const size_t len = (size_t) (0.15 * sr);
            for (size_t i = 0; i < len; ++i)
                x.push_back ((float) (0.3 * std::sin (2.0 * M_PI * hz * (double) i / sr)
                                      * std::sin (M_PI * (double) i / (double) len)) + 0.001f * n (rng));
            silence (0.04);
        }
    }
};

static tw::LineMark mark (int line, double sec, tw::LineSource s)
{
    tw::LineMark m; m.firstWord = line * 6; m.lastWord = line * 6 + 5; m.shown = line; m.sec = sec; m.source = s;
    return m;
}

static std::vector<float> slice (const std::vector<float>& x, double sr, double from)
{
    const size_t a = std::min (x.size(), (size_t) std::lround (from * sr));
    return std::vector<float> (x.begin() + (long) a, x.end());
}

int main()
{
    std::printf ("\nPunch and roll — pre-roll po R\n==============================\n\n");

    // ---- czytanie: wers 0, wers 1, wers 2 z pomylka, R ----------------------------
    Gen g;
    std::vector<tw::LineMark> marks;
    g.silence (0.6);
    const double l0 = g.now(); g.syllables (6, 140); g.silence (0.4);
    const double l1 = g.now(); g.syllables (6, 140); g.silence (0.4);
    const double l2 = g.now(); g.syllables (3, 300);
    const double key = g.now() + 0.05;
    marks.push_back (mark (0, l0 + 0.78, tw::LineSource::Words));
    marks.push_back (mark (1, l1 + 0.78, tw::LineSource::Words));
    marks.push_back (mark (2, l2 + 0.78, tw::LineSource::Words));
    marks.push_back (mark (2, key, tw::LineSource::Retake));
    g.silence (2.5);                                       // pre-roll i odliczanie

    const double sr = g.sr;
    tw::PreRoll pre;

    // ---- 1. konczy sie na szwie ------------------------------------------------------
    std::printf ("  1. gdzie konczy sie pre-roll\n");
    {
        const auto p = pre.plan (marks, g.x, sr, 0.0);
        check (! p.audio.empty(), "jest co grac");
        check (p.inPause, "szew w pauzie, nie zgadniety");
        check (std::fabs (p.toSec - (l2 - 0.15)) < 0.02, "konczy sie 0,15 s przed wersem 2", num (p.toSec - l2));
        check (std::fabs ((p.toSec - p.fromSec) - 2.0) < 0.01, "trwa 2 s", num (p.toSec - p.fromSec));
        check (std::fabs ((double) p.audio.size() / sr - 2.0) < 0.01, "probek na 2 s");

        // to samo miejsce, w ktorym Punch zrobi szew (powtorka mowiona dalej)
        Gen full = g;
        full.syllables (6, 140); full.silence (0.5);
        tw::CutFinder f; f.analyse (full.x.data(), full.x.size(), sr);
        const auto plan = tw::Punch().plan (marks, f);
        check (plan.size() == 1 && std::fabs (plan[0].from - p.toSec) < 0.011,
               "ten sam punkt co szew w Punch", plan.empty() ? "brak ciecia" : num (plan[0].from - p.toSec));
    }

    // ---- 2. pamiec zaczyna sie pozniej niz nagranie -------------------------------------
    std::printf ("\n  2. pamiec ostatnich sekund\n");
    {
        const double start = 1.0;
        const auto p = pre.plan (marks, slice (g.x, sr, start), sr, start);
        check (std::fabs (p.toSec - (l2 - 0.15)) < 0.02, "czas w nagraniu, nie w pamieci", num (p.toSec - l2));

        const auto late = pre.plan (marks, slice (g.x, sr, l2 - 0.5), sr, l2 - 0.5);
        check (late.audio.empty(), "poczatek wersu poza pamiecia: samo odliczanie");
    }

    // ---- 3. druga powtorka tego samego wersu ---------------------------------------------
    std::printf ("\n  3. druga powtorka\n");
    {
        Gen g2 = g;
        auto m2 = marks;
        g2.syllables (2, 300);                                 // znowu pomylka
        m2.push_back (mark (2, g2.now() + 0.05, tw::LineSource::Retake));
        g2.silence (2.5);
        const auto p1 = pre.plan (marks, g.x, sr, 0.0);
        const auto p2 = pre.plan (m2, g2.x, sr, 0.0);
        check (std::fabs (p2.toSec - p1.toSec) < 0.011, "gra to samo, co za pierwszym razem", num (p2.toSec - p1.toSec));
    }

    // ---- 4. kiedy nie grac -------------------------------------------------------------
    std::printf ("\n  4. kiedy nie grac\n");
    {
        std::vector<tw::LineMark> early { mark (0, 0.3, tw::LineSource::Retake) };
        check (pre.plan (early, g.x, sr, 0.0).audio.empty(), "R zanim zaczal sie jakikolwiek wers");

        auto noKey = marks; noKey.pop_back();
        check (pre.plan (noKey, g.x, sr, 0.0).audio.empty(), "ostatni zapis to nie R");

        std::vector<tw::LineMark> first { mark (0, l0 + 0.78, tw::LineSource::Words), mark (0, l0 + 1.6, tw::LineSource::Retake) };
        check (pre.plan (first, g.x, sr, 0.0).audio.empty(), "R w pierwszym wersie: przed nim cisza, nie ma czego grac");
    }

    // ---- 5. bez trzasku --------------------------------------------------------------
    std::printf ("\n  5. wejscie i wyjscie bez trzasku\n");
    {
        const auto p = pre.plan (marks, g.x, sr, 0.0);
        float peak = 0.0f;
        for (float v : p.audio) peak = std::max (peak, std::fabs (v));
        check (std::fabs (p.audio.front()) < 0.01f * peak + 1e-4f && std::fabs (p.audio.back()) < 0.01f * peak + 1e-4f,
               "pierwsza i ostatnia probka prawie zero");
    }

    // ---- 6. pamiec w watku dzwieku ----------------------------------------------------
    std::printf ("\n  6. HistoryRing\n");
    {
        tw::HistoryRing ring;
        ring.allocate (1000);
        std::vector<float> block (64);
        float v = 0.0f;
        for (int b = 0; b < 40; ++b)                       // 2560 probek, pamiec 1000
        {
            for (auto& s : block) s = v++;
            ring.push (block.data(), (int) block.size());
        }
        uint64_t end = 0;
        const auto last = ring.copyLast (5000, 200, end);
        check (end == 2560, "liczy wszystkie probki", std::to_string (end));
        check (last.size() == 800, "nie wiecej niz pojemnosc minus zapas", std::to_string (last.size()));
        bool ordered = ! last.empty() && last.back() == 2559.0f;
        for (size_t i = 1; i < last.size(); ++i) if (last[i] != last[i - 1] + 1.0f) ordered = false;
        check (ordered, "ostatnie probki, po kolei");

        ring.reset();
        const auto none = ring.copyLast (5000, 200, end);
        check (end == 0 && none.empty(), "nowe nagranie zaczyna od zera");
    }

    // ---- 7. odliczanie -----------------------------------------------------------------
    std::printf ("\n  7. odliczanie\n");
    {
        tw::Countdown c;
        const double t = c.totalFor (2.0);
        check (c.label (0.0, t) == "2" && c.label (1.2, t) == "1", "2, 1", c.label (0.0, t) + " " + c.label (1.2, t));
        check (c.label (2.1, t) == "Czytaj", "potem Czytaj");
        check (c.label (3.0, t).empty(), "potem nic");
        check (c.totalFor (0.0) == 1.5, "bez pre-rollu i tak chwila na oddech");
        check (c.label (0.0, c.totalFor (3.0)) == "3", "najwyzej 3");
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
