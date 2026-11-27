/*
    podklad-ogon-test.cpp — koniec muzyki po glosie (Source/DSP/Mix/MusicBed.h,
    raport audio 27.11, B2).

    Dawniej: dlugosc = dluzsze z dwojga, wiec muzyka grala po glosie do konca
    utworu (przy 6-min utworze i 45 s glosu — 5 min samej muzyki) i konczyla
    sie ucieciem w pol dzwieku (klikniecie). Teraz: glos + tailSec (domyslnie
    8 s, nie dluzej niz muzyka), a ostatnie fadeSec (3 s) muzyki schodza
    lagodnie do zera.
*/

#include "../Source/DSP/Mix/MusicBed.h"

#include <chrono>
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
static std::string f2 (double v) { char b[32]; std::snprintf (b, sizeof b, "%.2f", v); return b; }
static std::string g3 (double v) { char b[32]; std::snprintf (b, sizeof b, "%.3g", v); return b; }

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
    const auto music = [&] (double sec, std::vector<float>& L, std::vector<float>& R)
    {
        L.resize ((size_t) (sec * sr)); R.resize (L.size());
        for (size_t i = 0; i < L.size(); ++i)
        {
            const double t = (double) i / sr;
            L[i] = 0.3f * (float) (std::sin (2 * M_PI * 220 * t) + 0.5 * std::sin (2 * M_PI * 330 * t));
            R[i] = 0.3f * (float) (std::sin (2 * M_PI * 277 * t) + 0.5 * std::sin (2 * M_PI * 440 * t));
        }
    };

    mix::MusicBed::Params p;
    std::printf ("1. Muzyka 30 s, glos 7,5 s (domyslnie ogon %.0f s, wyciszenie %.0f s)\n", p.tailSec, p.fadeSec);
    {
        std::vector<float> L, R; music (30.0, L, R);
        const auto t0 = std::chrono::steady_clock::now();
        const auto r = mix::MusicBed::render (voice, L, R, sr, p);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double len = (double) r.left.size() / sr;
        std::printf ("     dlugosc %.2f s, liczenie %.3f s\n", len, secs);
        check (std::abs (len - (7.5 + p.tailSec)) < 0.01, "dlugosc = glos + ogon", f2 (len) + " s");
        const double last = std::max (std::abs (r.left.back()), std::abs (r.right.back()));
        check (last < 1e-4, "ostatnia probka praktycznie zero", g3 (last));
        const double end = rmsDb (r.left, sr, len - 0.05, len);
        check (end < -60.0, "ostatnie 50 ms ponizej -60 dBFS (bez klikniecia)", f2 (end) + " dB");
        const double mid = rmsDb (r.left, sr, 8.0, 10.0) - rmsDb (r.left, sr, len - 0.5, len - 0.3);
        check (mid > 15.0, "wyciszenie: przy koncu o ponad 15 dB ciszej niz zaraz po glosie", f2 (mid) + " dB");
        const double early = rmsDb (r.left, sr, 8.0, 10.0) - rmsDb (r.left, sr, 10.5, 12.0);
        check (std::abs (early) < 1.0, "przed wyciszeniem muzyka gra rowno", f2 (early) + " dB");
        // dB w kolejnych 0,5 s wyciszenia: bez skokow
        double prev = rmsDb (r.left, sr, len - p.fadeSec - 0.5, len - p.fadeSec), maxStep = 0.0;
        for (double a = len - p.fadeSec; a + 0.5 <= len - 0.5; a += 0.5)
        {
            const double now = rmsDb (r.left, sr, a, a + 0.5);
            maxStep = std::max (maxStep, prev - now); prev = now;
        }
        check (maxStep < 12.0, "wyciszenie lagodne (najwyzej 12 dB na 0,5 s)", f2 (maxStep) + " dB");
    }

    std::printf ("2. Muzyka krotsza niz glos + ogon (10 s)\n");
    {
        std::vector<float> L, R; music (10.0, L, R);
        const auto r = mix::MusicBed::render (voice, L, R, sr, p);
        check (r.left.size() == L.size(), "dlugosc = muzyka (nie dluzej)", f2 ((double) r.left.size() / sr) + " s");
    }

    std::printf ("3. Ogon ustawiany\n");
    {
        std::vector<float> L, R; music (30.0, L, R);
        auto q = p; q.tailSec = 4.0f;
        const auto r = mix::MusicBed::render (voice, L, R, sr, q);
        check (std::abs ((double) r.left.size() / sr - 11.5) < 0.01, "tailSec = 4: dlugosc 11,5 s", f2 ((double) r.left.size() / sr) + " s");
        check (std::abs (r.left.back()) < 1e-4, "i konczy sie na zerze", g3 (std::abs (r.left.back())));
    }

    std::printf ("4. Glos dluzszy niz muzyka: bez zmian\n");
    {
        std::vector<float> L, R; music (5.0, L, R);
        const auto r = mix::MusicBed::render (voice, L, R, sr, p);
        check (r.left.size() == voice.size(), "dlugosc = glos");
    }

    std::printf (fails ? "\nBLEDY: %d\n" : "\nWszystko OK\n", fails);
    return fails ? 1 : 0;
}
