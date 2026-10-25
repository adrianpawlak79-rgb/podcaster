/*
    podklad-miks-test.cpp — glos z podkladem w jeden plik (Source/DSP/Mix/MusicBed.h).

    Sztuczne dane: glos = trzy "frazy" (szum pasmowy 1,5 s) z przerwami 1 s,
    muzyka = akord stereo przez cale 10 s (dluzsza niz glos).
*/

#include "../Source/DSP/Mix/MusicBed.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static int fails = 0;
static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-60s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}
static std::string f1 (double v) { char b[32]; std::snprintf (b, sizeof b, "%.2f", v); return b; }

static double rmsDb (const std::vector<float>& x, double sr, double a, double b)
{
    double s = 0; size_t n = 0;
    for (size_t i = (size_t) (a * sr); i < (size_t) (b * sr) && i < x.size(); ++i, ++n) s += (double) x[i] * x[i];
    return n ? 10.0 * std::log10 (s / (double) n + 1e-20) : -200.0;
}

int main()
{
    const double sr = 48000.0;
    std::mt19937 rng (3); std::normal_distribution<float> n (0.0f, 1.0f);
    std::vector<float> voice ((size_t) (7.5 * sr), 0.0f);
    for (double start : { 0.5, 3.0, 5.5 })
        for (size_t i = (size_t) (start * sr); i < (size_t) ((start + 1.5) * sr); ++i)
        {
            const double t = (double) i / sr;
            voice[i] = 0.2f * (float) std::sin (2 * M_PI * 180 * t) * (0.6f + 0.4f * (float) std::sin (2 * M_PI * 4 * t)) + 0.02f * n (rng);
        }
    std::vector<float> mL ((size_t) (10.0 * sr)), mR (mL.size());
    for (size_t i = 0; i < mL.size(); ++i)
    {
        const double t = (double) i / sr;
        mL[i] = 0.3f * (float) (std::sin (2 * M_PI * 220 * t) + 0.5 * std::sin (2 * M_PI * 330 * t));
        mR[i] = 0.3f * (float) (std::sin (2 * M_PI * 277 * t) + 0.5 * std::sin (2 * M_PI * 440 * t));
    }

    std::printf ("1. Dlugosc, glosnosc, szczyt\n");
    mix::MusicBed::Params p;
    const auto r = mix::MusicBed::render (voice, mL, mR, sr, p);
    check (r.left.size() == mL.size() && r.right.size() == mL.size(), "dlugosc = muzyka (dluzsza od glosu)");
    check (std::abs (r.lufs - p.targetLufs) < 0.5, "calosc na -16 LUFS", f1 (r.lufs));
    check (r.peakDbtp <= p.ceilingDb + 0.1, "szczyt najwyzej -1 dBTP", f1 (r.peakDbtp));

    std::printf ("2. Poziom muzyki wzgledem glosu\n");
    {
        mix::MusicBed::Params q; q.duck = false;
        const auto a = mix::MusicBed::render (voice, mL, mR, sr, q);
        q.musicDb = -6.0f;
        const auto b = mix::MusicBed::render (voice, mL, mR, sr, q);
        // muzyka sama (po glosie, 8-10 s) w stosunku do calosci
        const double qa = rmsDb (a.left, sr, 8.0, 10.0) - rmsDb (a.left, sr, 0.6, 1.9);
        const double qb = rmsDb (b.left, sr, 8.0, 10.0) - rmsDb (b.left, sr, 0.6, 1.9);
        check (qb - qa > 3.0, "\"glosniej\" (-6 dB) slychac glosniej niz -11 dB", f1 (qb - qa) + " dB");
        check (qa < -3.0, "domyslnie muzyka wyraznie pod glosem", f1 (qa) + " dB");
    }

    std::printf ("3. Sciszanie pod glosem\n");
    {
        mix::MusicBed::Params q; q.duck = false;
        const auto flat = mix::MusicBed::render (voice, mL, mR, sr, q);
        // muzyka sama = wynik minus glos przeskalowany; prosciej: prawy kanal
        // ma inna muzyke, glos jest w obu — roznica L-R zawiera tylko muzyke
        auto diff = [] (const mix::MusicBed::Result& x) { std::vector<float> d (x.left.size()); for (size_t i = 0; i < d.size(); ++i) d[i] = x.left[i] - x.right[i]; return d; };
        const auto dd = diff (r), df = diff (flat);
        const double under = (rmsDb (dd, sr, 3.3, 4.2) - rmsDb (dd, sr, 8.0, 10.0)) - (rmsDb (df, sr, 3.3, 4.2) - rmsDb (df, sr, 8.0, 10.0));
        check (under < -3.0 && under > -5.0, "pod mowa muzyka nizej o ok. 4 dB", f1 (under) + " dB");
        const double gap = (rmsDb (dd, sr, 2.8, 2.85) - rmsDb (dd, sr, 8.0, 10.0)) - (rmsDb (df, sr, 2.8, 2.85) - rmsDb (df, sr, 8.0, 10.0));
        check (gap > -1.5, "w przerwie 1 s muzyka wraca (ponad 2/3 drogi)", f1 (gap) + " dB");
        const double before = (rmsDb (dd, sr, 2.93, 2.99) - rmsDb (dd, sr, 8.0, 10.0)) - (rmsDb (df, sr, 2.93, 2.99) - rmsDb (df, sr, 8.0, 10.0));
        check (before < -0.5, "zejscie zaczyna sie przed fraza", f1 (before) + " dB");
    }

    std::printf ("4. Nic sie nie przesuwa\n");
    {
        // glos bez muzyki: wynik = glos * stala (po limiterze bez zmian w czasie)
        std::vector<float> silent;
        mix::MusicBed::Params q; q.ceilingDb = 0.0f;
        const auto v = mix::MusicBed::render (voice, silent, {}, sr, q);
        size_t firstV = 0, firstO = 0;
        while (firstV < voice.size() && std::abs (voice[firstV]) < 1e-3f) ++firstV;
        while (firstO < v.left.size() && std::abs (v.left[firstO]) < 1e-3f * 1.5f) ++firstO;
        check (v.left.size() == voice.size(), "bez muzyki dlugosc = glos");
        check (std::abs ((long) firstV - (long) firstO) <= 2, "pierwsza gloska w tej samej probce",
               std::to_string ((long) firstO - (long) firstV));
    }

    std::printf (fails ? "\nBLEDY: %d\n" : "\nWszystko OK\n", fails);
    return fails ? 1 : 0;
}
