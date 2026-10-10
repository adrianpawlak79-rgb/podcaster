/*
    Limiter.h — ostatnia rzecz w lancuchu. Jego jedyne zadanie: zeby nic nie
    wyszlo ponad sufit, i zeby nie bylo tego slychac.

    Dlaczego z wyprzedzeniem. Limiter bez wyprzedzenia dowiaduje sie o szczycie
    dopiero wtedy, gdy szczyt juz wyszedl na wyjscie. Zeby zdazyc, musi miec
    atak liczony w mikrosekundach — a tak szybka zmiana wzmocnienia to jest
    znieksztalcenie, slyszalne jako trzask przy kazdej glosce zwarto-wybuchowej.
    Z wyprzedzeniem gain schodzi PRZED szczytem, po lagodnym zboczu.

    Sufit podajemy w dBTP, nie w dBFS. Miedzy probkami przebieg idzie wyzej niz
    w probkach, wiec plik pokazujacy -0,1 dBFS potrafi trzaskac po zamianie na
    MP3. Standardowy zapas dla podcastu to -1 dBTP i tyle mamy domyslnie.
*/

#pragma once

#include "Loudness.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mix
{

class Limiter
{
public:
    struct Params
    {
        float  ceilingDb   = -1.0f;
        float  ispMarginDb = 0.5f;   ///< zapas na szczyt miedzyprobkowy
        double lookaheadMs = 3.0;
        double releaseMs   = 120.0;
    };

    Params params;

    void prepare (double sr, int numChannels)
    {
        sampleRate = sr;
        chans = std::max (1, numChannels);

        look = std::max (2, (int) std::llround (sr * params.lookaheadMs * 0.001));
        delay.assign ((size_t) (look * chans), 0.0f);
        window.assign ((size_t) look, 1.0f);

        // Zapas na szczyt miedzyprobkowy. Sufit -1 dBTP osiagamy, celujac
        // nizej w PROBKACH: miedzy nimi przebieg idzie wyzej. Wielkosc zapasu
        // nie jest zgadnieta — przy 0,3 dB czterokrotne nadprobkowanie w
        // tescie pokazywalo -0,95 dBTP, czyli tuz nad sufitem. 0,5 dB daje
        // zapas z marginesem i nie slychac tego jako ciszy.
        ceilingLin = dbToGainL (params.ceilingDb - params.ispMarginDb);

        relCoeff = (float) std::exp (-1.0 / (0.001 * params.releaseMs * sr));
        reset();
    }

    void reset()
    {
        std::fill (delay.begin(), delay.end(), 0.0f);
        std::fill (window.begin(), window.end(), 1.0f);
        pos = 0;
        gain = 1.0f;
        lastGr = maxGr = 0.0f;
    }

    /** Przetwarza w miejscu. @param io [kanal][probka] */
    void process (float* const* io, int numChannels, int numSamples)
    {
        const int nc = std::min (numChannels, chans);

        for (int n = 0; n < numSamples; ++n)
        {
            // ---- najglosniejszy kanal w tej probce --------------------------
            float a = 0.0f;

            for (int c = 0; c < nc; ++c)
                a = std::max (a, std::fabs (io[c][n]));

            // wzmocnienie, ktore sprowadzi te probke do sufitu
            const float need = a > ceilingLin ? ceilingLin / a : 1.0f;
            window[(size_t) pos] = need;

            // ---- najmniejsze wzmocnienie w calym oknie wyprzedzenia ---------
            // To jest sedno: patrzymy na to, co DOPIERO nadejdzie, i schodzimy
            // z gainem zawczasu.
            float target = 1.0f;

            for (int i = 0; i < look; ++i)
                target = std::min (target, window[(size_t) i]);

            if (target < gain)
            {
                // Atak musi ZDAZYC zejsc do celu, zanim szczyt dojdzie do
                // wyjscia. Przesuwane minimum trzyma niska wartosc przez cale
                // okno, wiec wystarczy stala czasowa piec razy krotsza niz
                // okno: po jego przejsciu zostaje 0,7% roznicy, czyli 0,06 dB.
                //
                // Przy zbyt wolnym zejsciu (probowalem dwa razy dluzej) gain
                // nie dochodzil na czas, szczyt szedl w twarde obciecie na
                // sufcie, a obciecie samo z siebie rodzi szczyty
                // miedzyprobkowe: w tescie wychodzilo -0,30 dBTP przy sufcie
                // -1 dBTP. Limiter, ktory trzaska, jest gorszy niz jego brak.
                gain += (target - gain) * (5.0f / (float) look);
                gain = std::max (gain, target);
            }
            else
            {
                gain = relCoeff * gain + (1.0f - relCoeff) * target;
            }

            lastGr = 20.0f * std::log10 (std::max (1.0e-6f, gain));
            maxGr = std::min (maxGr, lastGr);

            // ---- wyjscie: probka sprzed okna, przemnozona ------------------
            for (int c = 0; c < nc; ++c)
            {
                const float out = delay[(size_t) (pos * chans + c)] * gain;
                delay[(size_t) (pos * chans + c)] = io[c][n];
                io[c][n] = std::max (-ceilingLin, std::min (ceilingLin, out));
            }

            pos = (pos + 1) % look;
        }
    }

    float gainReductionDb() const noexcept { return lastGr; }
    float maxGainReductionDb() const noexcept { return maxGr; }
    int   latencySamples()  const noexcept { return look; }

private:
    static float dbToGainL (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

    double sampleRate = 48000.0;
    int chans = 1, look = 144, pos = 0;
    float ceilingLin = 0.89f, gain = 1.0f, relCoeff = 0.0f, lastGr = 0.0f, maxGr = 0.0f;
    std::vector<float> delay, window;
};

} // namespace mix
