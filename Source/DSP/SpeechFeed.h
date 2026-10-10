/*
    SpeechFeed.h — dzwiek dla rozpoznawania mowy: z watku dzwieku do Vosk.

    ---------------------------------------------------------------------------
    DWIE RZECZY

    1. Kolejka z watku dzwieku. Watek dzwieku nie moze czekac ani przydzielac
       pamieci (patrz Recorder.h), a Vosk liczy wolno i nierowno — wiec
       probki ida przez pierscien o stalej wielkosci: dzwiek tylko dopisuje,
       watek Vosk tylko czyta. Gdy Vosk nie nadaza i pierscien sie zapelni,
       NOWE probki przepadaja (i sa liczone), a nie stare: lepiej zgubic
       kawalek mowy, niz podmienic probki, ktore ktos wlasnie czyta.

    2. Zejscie do 16 kHz. Model small-pl jest uczony na 16 kHz. Interfejs
       daje 44,1 albo 48. Najpierw filtr dolnoprzepustowy (osiem biegunow
       Butterwortha na 6,8 kHz), zeby to, co lezy powyzej 8 kHz, nie odbilo
       sie w pasmo mowy, potem interpolacja liniowa do nowego kroku. Dla
       rozpoznawania mowy to wystarcza: liczy sie pasmo 100 Hz - 7 kHz, a nie
       ostatni decybel. Mix/Colour.h ma dokladniejszy Resampler, ale on
       pracuje na calym pliku naraz, a tu dzwiek przychodzi kawalkami.
*/

#pragma once

#include "Mix/Biquad.h"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tw
{

//==============================================================================
/** Jeden piszacy (dzwiek), jeden czytajacy (Vosk). Pamiec przydzielona raz. */
class SampleRing
{
public:
    void allocate (size_t capacity)       // NIE z watku dzwieku
    {
        data.assign (capacity + 1, 0.0f);
        write.store (0);
        read.store (0);
        dropped.store (0);
    }

    /** Z watku dzwieku. Nie czeka, nie przydziela. */
    void push (const float* x, int n) noexcept
    {
        const size_t size = data.size();
        if (size == 0) return;

        size_t w = write.load (std::memory_order_relaxed);
        const size_t r = read.load (std::memory_order_acquire);
        const size_t free = (r + size - w - 1) % size;

        const size_t take = std::min (free, (size_t) n);
        for (size_t i = 0; i < take; ++i) { data[w] = x[i]; w = (w + 1) % size; }

        write.store (w, std::memory_order_release);
        if (take < (size_t) n) dropped.fetch_add ((uint64_t) (n - (int) take), std::memory_order_relaxed);
    }

    /** Z watku Vosk. Zwraca, ile przeczytal. */
    size_t pop (float* out, size_t max) noexcept
    {
        const size_t size = data.size();
        if (size == 0) return 0;

        size_t r = read.load (std::memory_order_relaxed);
        const size_t w = write.load (std::memory_order_acquire);

        size_t n = 0;
        while (r != w && n < max) { out[n++] = data[r]; r = (r + 1) % size; }

        read.store (r, std::memory_order_release);
        return n;
    }

    uint64_t droppedSamples() const noexcept { return dropped.load(); }

private:
    std::vector<float> data;
    std::atomic<size_t> write { 0 }, read { 0 };
    std::atomic<uint64_t> dropped { 0 };
};

//==============================================================================
/** Dowolna czestotliwosc -> 16 kHz, kawalek po kawalku. */
class To16k
{
public:
    static constexpr double outRate = 16000.0;

    void prepare (double inRate)
    {
        rate = inRate;
        step = inRate / outRate;
        phase = 0.0;
        prev = 0.0f;

        // osiem biegunow Butterwortha: cztery sekcje z tymi Q
        static constexpr double q[4] = { 0.5098, 0.6013, 0.9000, 2.5629 };
        for (int i = 0; i < 4; ++i)
        {
            lp[(size_t) i].setCoeffs (mix::design::lowpass (std::min (6800.0, inRate * 0.45), inRate, q[i]));
            lp[(size_t) i].reset();
        }
    }

    /** Doklada probki wejscia, dopisuje do out probki 16 kHz (int16). */
    void process (const float* in, size_t n, std::vector<int16_t>& out)
    {
        for (size_t k = 0; k < n; ++k)
        {
            float x = in[k];
            for (auto& f : lp) x = f.process (x);

            // probki wyjscia lezace miedzy poprzednia a ta probka wejscia
            while (phase <= 1.0)
            {
                const float y = prev + (float) phase * (x - prev);
                const float s = std::fmax (-1.0f, std::fmin (1.0f, y)) * 32767.0f;
                out.push_back ((int16_t) std::lrint (s));
                phase += step;
            }

            phase -= 1.0;
            prev = x;
        }
    }

private:
    double rate = 48000.0, step = 3.0, phase = 0.0;
    float prev = 0.0f;
    std::array<mix::Biquad, 4> lp;
};

} // namespace tw
