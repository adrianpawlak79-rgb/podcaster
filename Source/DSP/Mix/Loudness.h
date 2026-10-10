/*
    Loudness.h — glosnosc wedlug ITU-R BS.1770-4 i szczyt miedzyprobkowy.

    Po co to jest. Podcast wrzucony do Spotify, Apple Podcasts czy YouTube'a
    zostanie tam wyrownany do wlasnej normy platformy. Jesli oddamy plik za
    glosny, platforma go SCISZY — a razem z nim scisza wszystko, co zrobilismy
    kompresja. Jesli oddamy za cichy, sluchacz podkreca i dostaje nasz szum tla.
    Dlatego koniec lancucha nie moze byc "na oko na wskazniku", tylko liczba.

    Cel dla mowy: -16 LUFS zintegrowane (norma podcastowa) przy szczycie
    miedzyprobkowym nie wyzej niz -1 dBTP.

    Dlaczego szczyt MIEDZYPROBKOWY, a nie zwykly. Miedzy probkami przebieg idzie
    wyzej niz w probkach. Plik moze pokazywac -0,1 dBFS i po zamianie na MP3 czy
    AAC trzaskac, bo dekoder odtwarza to, co jest MIEDZY probkami. Mierzymy wiec
    po czterokrotnym nadprobkowaniu.

    Zgodnosc sprawdzona liczbowo wobec niezaleznej implementacji (pyloudnorm) —
    patrz Tests/loudness-check.py.
*/

#pragma once

#include "Biquad.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mix
{

//==============================================================================
/** Filtr K: polka wysokotonowa modelujaca glowe sluchacza + gornoprzepustowy
    RLB. Wspolczynniki z BS.1770-4, przeliczane na dowolna czestotliwosc
    probkowania — tablica ze standardu jest podana tylko dla 48 kHz. */
inline Coeffs kWeightShelf (double sr) noexcept
{
    const double db = 3.999843853973347;
    const double f0 = 1681.974450955533;
    const double Q  = 0.7071752369554196;

    const double K  = std::tan (M_PI * f0 / sr);
    const double Vh = std::pow (10.0, db / 20.0);
    const double Vb = std::pow (Vh, 0.4996667741545416);
    const double a0 = 1.0 + K / Q + K * K;

    return { (Vh + Vb * K / Q + K * K) / a0,
             2.0 * (K * K - Vh) / a0,
             (Vh - Vb * K / Q + K * K) / a0,
             2.0 * (K * K - 1.0) / a0,
             (1.0 - K / Q + K * K) / a0 };
}

inline Coeffs kWeightHighpass (double sr) noexcept
{
    const double f0 = 38.13547087602444;
    const double Q  = 0.5003270373238773;

    const double K  = std::tan (M_PI * f0 / sr);
    const double a0 = 1.0 + K / Q + K * K;

    return { 1.0, -2.0, 1.0,
             2.0 * (K * K - 1.0) / a0,
             (1.0 - K / Q + K * K) / a0 };
}

//==============================================================================
/** Pomiar glosnosci jednego przebiegu. Zbiera bloki 400 ms z zakladka 75%,
    tak jak kaze standard, i bramkuje je dwustopniowo:

      brama bezwzgledna  -70 LUFS   — wycina cisze miedzy zdaniami
      brama wzgledna     -10 LU     — wycina to, co ciche wzgledem reszty

    Bez tych bram cisza miedzy frazami zanizalaby wynik i program podkrecalby
    mowe za wysoko. To jest ta sama zasada, co przy lampce: cisza nie jest
    materialem do oceny.
*/
class LoudnessMeter
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate;
        chans = std::max (1, numChannels);

        shelf.assign ((size_t) chans, Biquad());
        hipass.assign ((size_t) chans, Biquad());

        for (int c = 0; c < chans; ++c)
        {
            shelf[(size_t) c].setCoeffs (kWeightShelf (sr));
            hipass[(size_t) c].setCoeffs (kWeightHighpass (sr));
        }

        blockLen = (int) std::llround (sr * 0.4);       // 400 ms
        hop      = std::max (1, blockLen / 4);          // zakladka 75%

        reset();
    }

    void reset()
    {
        for (auto& b : shelf)  b.reset();
        for (auto& b : hipass) b.reset();

        ring.assign ((size_t) (blockLen * chans), 0.0);
        write = 0;
        filled = 0;
        sinceHop = 0;
        blocks.clear();
        shortRing.assign ((size_t) std::max (1, (int) std::llround (sr * 3.0)), 0.0);
        shortWrite = shortFilled = 0;
        shortEnergy = 0.0;
    }

    /** @param in   [kanal][probka] */
    void process (const float* const* in, int numChannels, int numSamples)
    {
        const int nc = std::min (numChannels, chans);

        for (int n = 0; n < numSamples; ++n)
        {
            for (int c = 0; c < nc; ++c)
            {
                const float k = hipass[(size_t) c].process (shelf[(size_t) c].process (in[c][n]));
                ring[(size_t) (write * chans + c)] = (double) k * (double) k;
            }

            for (int c = nc; c < chans; ++c)
                ring[(size_t) (write * chans + c)] = 0.0;

            double energy = 0.0;
            for (int c = 0; c < chans; ++c) energy += ring[(size_t) (write * chans + c)];
            shortEnergy += energy - shortRing[shortWrite];
            shortRing[shortWrite] = energy;
            shortWrite = (shortWrite + 1) % shortRing.size();
            shortFilled = std::min (shortFilled + 1, shortRing.size());
            write = (write + 1) % blockLen;
            if (filled < blockLen) ++filled;

            if (++sinceHop >= hop && filled >= blockLen)
            {
                sinceHop = 0;
                blocks.push_back (blockLoudness());
            }
        }
    }

    /** Glosnosc zintegrowana calego materialu, w LUFS. */
    double integrated() const
    {
        if (blocks.empty())
            return -70.0;

        // brama bezwzgledna
        double sum = 0.0;
        int    n   = 0;

        for (double l : blocks)
            if (l > -70.0) { sum += std::pow (10.0, (l + 0.691) / 10.0); ++n; }

        if (n == 0)
            return -70.0;

        const double relative = -0.691 + 10.0 * std::log10 (sum / n) - 10.0;

        // brama wzgledna
        sum = 0.0; n = 0;

        for (double l : blocks)
            if (l > -70.0 && l > relative) { sum += std::pow (10.0, (l + 0.691) / 10.0); ++n; }

        if (n == 0)
            return -70.0;

        return -0.691 + 10.0 * std::log10 (sum / n);
    }

    /** Glosnosc ostatniego bloku 400 ms — do wskaznika na zywo. */
    double momentary() const { return blocks.empty() ? -70.0 : blocks.back(); }

    /** Glosnosc krotkoterminowa (3 s) — usredniona z ostatnich blokow. */
    double shortTerm() const
    {
        if (shortFilled == 0 || shortEnergy <= 1.0e-15) return -70.0;
        return -0.691 + 10.0 * std::log10 (shortEnergy / shortFilled);
    }

    /** Ile decybeli trzeba dodac, zeby trafic w cel. */
    double gainToReach (double targetLufs) const { return targetLufs - integrated(); }

private:
    double blockLoudness() const
    {
        double total = 0.0;

        for (int c = 0; c < chans; ++c)
        {
            double ms = 0.0;

            for (int i = 0; i < blockLen; ++i)
                ms += ring[(size_t) (i * chans + c)];

            // wagi kanalow: L i R po 1,0 (mono i stereo)
            total += ms / blockLen;
        }

        return total > 1.0e-15 ? -0.691 + 10.0 * std::log10 (total) : -140.0;
    }

    double sr = 48000.0;
    int chans = 1;
    int blockLen = 19200, hop = 4800;

    std::vector<Biquad> shelf, hipass;
    std::vector<double> ring;
    std::vector<double> blocks, shortRing;
    size_t shortWrite = 0, shortFilled = 0;
    double shortEnergy = 0.0;
    int write = 0, filled = 0, sinceHop = 0;
};

//==============================================================================
/** Szczyt miedzyprobkowy: 8 faz, 32 odczepy na faze.
    Jest to estymata skonczonego FIR, nie gwarancja bledu < 0,1 dB.
    Kolejne odczepy fazy czytaja KOLEJNE probki oryginalu. */
class TruePeak
{
public:
    void prepare (double /*sr*/, int numChannels)
    {
        chans = std::max (1, numChannels);
        hist.assign ((size_t) (chans * kTaps), 0.0f);
        pos = 0;
        peak = 0.0f;
        buildKernel();
    }

    void reset()
    {
        std::fill (hist.begin(), hist.end(), 0.0f);
        pos = 0;
        peak = 0.0f;
    }

    void process (const float* const* in, int numChannels, int numSamples)
    {
        const int nc = std::min (numChannels, chans);

        for (int n = 0; n < numSamples; ++n)
        {
            for (int c = 0; c < nc; ++c)
            {
                hist[(size_t) (pos * chans + c)] = in[c][n];

                const float a = std::fabs (in[c][n]);
                if (a > peak) peak = a;
            }

            pos = (pos + 1) % kTaps;

            // Fazy filtru polifazowego (historia w czestotliwosci WEJSCIA).
            for (int ph = 0; ph < kPhases; ++ph)
            {
                for (int c = 0; c < nc; ++c)
                {
                    float acc = 0.0f;

                    for (int t = 0; t < kTapsPerPhase; ++t)
                    {
                        const int idx = (pos + kTaps - 1 - t) % kTaps;
                        acc += kernel[(size_t) (ph * kTapsPerPhase + t)]
                             * hist[(size_t) (idx * chans + c)];
                    }

                    const float a = std::fabs (acc);
                    if (a > peak) peak = a;
                }
            }
        }
    }

    // Dopelnienie po OSTATNIM bloku, zeby szczyt opoznionego FIR nie zginal.
    void finish()
    {
        std::vector<float> zeros ((size_t) kTapsPerPhase, 0.0f);
        std::vector<const float*> ptr ((size_t) chans, zeros.data());
        process (ptr.data(), chans, kTapsPerPhase);
    }

    float linear() const noexcept { return peak; }
    double dbTP() const noexcept
    {
        return peak > 1.0e-9f ? 20.0 * std::log10 ((double) peak) : -140.0;
    }

private:
    static constexpr int kPhases = 8;
    static constexpr int kTapsPerPhase = 32;
    static constexpr int kTaps = kPhases * kTapsPerPhase;

    void buildKernel()
    {
        kernel.assign ((size_t) kTaps, 0.0f);

        const double fc = 0.5 / kPhases;          // pasmo oryginalu
        const int    mid = kTaps / 2;

        for (int i = 0; i < kTaps; ++i)
        {
            const double x = (double) (i - mid);
            const double s = std::fabs (x) < 1.0e-9 ? 2.0 * fc
                           : std::sin (2.0 * M_PI * fc * x) / (M_PI * x);

            // okno Blackmana
            const double w = 0.42
                           - 0.5  * std::cos (2.0 * M_PI * i / (kTaps - 1))
                           + 0.08 * std::cos (4.0 * M_PI * i / (kTaps - 1));

            kernel[(size_t) i] = (float) (s * w * kPhases);
        }

        // normalizacja kazdej fazy z osobna, zeby stala skladowa przechodzila 1:1
        for (int ph = 0; ph < kPhases; ++ph)
        {
            double sum = 0.0;
            for (int t = 0; t < kTapsPerPhase; ++t)
                sum += kernel[(size_t) (t * kPhases + ph)];

            if (std::fabs (sum) > 1.0e-9)
                for (int t = 0; t < kTapsPerPhase; ++t)
                    kernel[(size_t) (t * kPhases + ph)] /= (float) sum;
        }

        // przelozenie na uklad [faza][odczep], zeby petla wyzej byla ciagla
        std::vector<float> re ((size_t) kTaps);

        for (int ph = 0; ph < kPhases; ++ph)
            for (int t = 0; t < kTapsPerPhase; ++t)
                re[(size_t) (ph * kTapsPerPhase + t)] = kernel[(size_t) (t * kPhases + ph)];

        kernel.swap (re);
    }

    int chans = 1, pos = 0;
    float peak = 0.0f;
    std::vector<float> hist, kernel;
};

} // namespace mix
