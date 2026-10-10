/*
    mowa16k-test.cpp — dzwiek dla rozpoznawania mowy (DSP/SpeechFeed.h).

    Dwie rzeczy, ktore musza byc prawda, zanim Vosk dostanie choc jedna
    probke:

    1. zejscie z 48 i 44,1 kHz do 16 kHz nie psuje mowy (ton w pasmie
       przechodzi bez zmiany wysokosci i poziomu), a to, co lezy powyzej
       8 kHz, nie odbija sie w pasmo mowy jako falszywy ton;
    2. kolejka z watku dzwieku nie gubi i nie przestawia probek, a gdy jest
       pelna — gubi nowe i to liczy.
*/

#include "../Source/DSP/SpeechFeed.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got)
{
    std::printf ("  %-5s %-52s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

/** Poziom tonu f w sygnale 16 kHz (korelacja z sinusem i cosinusem), dBFS. */
static double toneDb (const std::vector<int16_t>& x, double f, size_t skip)
{
    double s = 0, c = 0;
    const size_t n = x.size() - skip;
    for (size_t i = skip; i < x.size(); ++i)
    {
        const double v = x[i] / 32767.0, w = 2.0 * M_PI * f * (double) (i - skip) / 16000.0;
        s += v * std::sin (w);
        c += v * std::cos (w);
    }
    const double amp = 2.0 * std::sqrt (s * s + c * c) / (double) n;
    return 20.0 * std::log10 (std::max (amp, 1e-12));
}

static std::vector<int16_t> convert (double rate, double freq, double amp, double seconds, size_t block)
{
    tw::To16k d;
    d.prepare (rate);

    std::vector<float> in ((size_t) (rate * seconds));
    for (size_t i = 0; i < in.size(); ++i) in[i] = (float) (amp * std::sin (2.0 * M_PI * freq * (double) i / rate));

    std::vector<int16_t> out;
    for (size_t i = 0; i < in.size(); i += block)
        d.process (in.data() + i, std::min (block, in.size() - i), out);

    return out;
}

int main()
{
    char b[160];
    std::printf ("\nDzwiek dla rozpoznawania mowy\n=============================\n\n");

    for (double rate : { 48000.0, 44100.0 })
    {
        std::printf ("  z %.1f kHz\n", rate / 1000.0);

        // 1 s w kawalkach po 480 probek, jak z karty
        const auto out = convert (rate, 1000.0, 0.5, 1.0, 480);
        std::snprintf (b, sizeof (b), "%zu probek (oczekiwane 16000)", out.size());
        check (std::abs ((long) out.size() - 16000) <= 2, "dlugosc po zmianie sie zgadza", b);

        const double lvl = toneDb (out, 1000.0, 800);
        std::snprintf (b, sizeof (b), "%.2f dB (wejscie %.2f)", lvl, 20.0 * std::log10 (0.5));
        check (std::fabs (lvl - 20.0 * std::log10 (0.5)) < 0.5, "ton 1 kHz przechodzi bez zmiany poziomu", b);

        // 3 kHz: gorna czesc pasma mowy (sybilanty zaczynaja sie wyzej)
        const auto mid = convert (rate, 3000.0, 0.5, 1.0, 480);
        std::snprintf (b, sizeof (b), "%.2f dB", toneDb (mid, 3000.0, 800) - 20.0 * std::log10 (0.5));
        check (std::fabs (toneDb (mid, 3000.0, 800) - 20.0 * std::log10 (0.5)) < 1.0, "ton 3 kHz tez", b);

        // 12 kHz odbija sie przy 16 kHz w 4 kHz — ma byc zgaszone
        const auto high = convert (rate, 12000.0, 0.5, 1.0, 480);
        const double alias = toneDb (high, 4000.0, 800) - 20.0 * std::log10 (0.5);
        std::snprintf (b, sizeof (b), "odbicie w 4 kHz: %.1f dB", alias);
        check (alias < -40.0, "12 kHz nie odbija sie w pasmo mowy", b);
    }

    // ---- kolejka ------------------------------------------------------------------
    std::printf ("\n  kolejka z watku dzwieku\n");
    {
        tw::SampleRing r;
        r.allocate (1000);

        std::vector<float> x (300);
        float next = 0.0f, expect = 0.0f;
        bool inOrder = true;
        std::vector<float> got (256);

        for (int round = 0; round < 100; ++round)
        {
            for (auto& v : x) v = next++;
            r.push (x.data(), (int) x.size());

            size_t n;
            while ((n = r.pop (got.data(), got.size())) > 0)
                for (size_t i = 0; i < n; ++i) { if (got[i] != expect) inOrder = false; ++expect; }
        }

        std::snprintf (b, sizeof (b), "%.0f probek, zgubionych %llu", expect, (unsigned long long) r.droppedSamples());
        check (inOrder && expect == 30000.0f && r.droppedSamples() == 0, "nic nie ginie i nic sie nie przestawia", b);

        tw::SampleRing full;
        full.allocate (1000);
        std::vector<float> y (700, 1.0f);
        full.push (y.data(), 700);
        full.push (y.data(), 700);
        std::snprintf (b, sizeof (b), "zgubionych %llu z 1400 (miejsce na 1000)", (unsigned long long) full.droppedSamples());
        check (full.droppedSamples() == 400, "pelna kolejka gubi nowe i to liczy", b);
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
