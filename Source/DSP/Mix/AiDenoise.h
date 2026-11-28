/*
    AiDenoise.h — "Usuwanie tla (AI)": RNNoise jako pierwszy krok toru miksu.

    WYJATEK od zasady "cale DSP od zera": RNNoise (xiph, BSD-3, licencja w
    ThirdParty/rnnoise/COPYING i w LICENCJE.txt) — decyzja wlasciciela
    (badania AI 1.1: telewizor i rozmowa za sciana, z ktorymi nasze widmowe
    odszumianie i ekspander nie daja rady, bo tlo ma ksztalt mowy).

    Siec dziala na 48 kHz, ramkach po 480 probek i skali int16 (x32768).
    Inna czestotliwosc: przejscie na 48 kHz i z powrotem (mix::Resampler).
    RNNoise opoznia o dwie ramki (960 probek, 20 ms; zmierzone) — wynik jest przesuniety z
    powrotem, wiec probka i wyjdzie w miejscu i (Tests/ai-tlo-test.cpp).

    dryDb: ile oryginalu domieszac (np. -30 dB). Ponizej -90 = bez domieszki.

    Kod sieci (C) jest linkowany tylko w programie i testach z -DZS_RNNOISE=1
    (CMakeLists.txt, Tests/rnnoise-lib.sh). Bez niego available() = false
    i denoise() nic nie robi — reszta testow kompiluje sie jak dotad.
*/
#pragma once
#include "Colour.h"
#include <algorithm>
#include <cmath>
#include <vector>

#if ZS_RNNOISE
extern "C" {
    struct DenoiseState;
    struct RNNModel;
    int  rnnoise_get_frame_size (void);
    DenoiseState* rnnoise_create (RNNModel* model);
    void rnnoise_destroy (DenoiseState* st);
    float rnnoise_process_frame (DenoiseState* st, float* out, const float* in);
    RNNModel* rnnoise_model_from_buffer (const void* ptr, int len);
    const void* zs_rnnoise_model_data (void);
    int zs_rnnoise_model_size (void);
}
#endif

namespace mix
{
namespace ai
{

inline bool available() noexcept
{
#if ZS_RNNOISE
    return true;
#else
    return false;
#endif
}

/** RNNoise na 48 kHz, bez przesuniecia. Zwraca false, gdy sieci nie ma. */
inline bool denoise48 (std::vector<float>& x)
{
#if ZS_RNNOISE
    // model jeden na program (wagi tylko do odczytu, wspolne dla watkow)
    static RNNModel* model = rnnoise_model_from_buffer (zs_rnnoise_model_data(), zs_rnnoise_model_size());
    if (model == nullptr) return false;
    DenoiseState* st = rnnoise_create (model);
    if (st == nullptr) return false;

    const size_t F = 480, lag = 960, n = x.size();   // zmierzone: 2 ramki (rnnoise_demo + 1 ramka)
    std::vector<float> out (n, 0.0f);
    float in[480], o[480];
    for (size_t a = 0; a < n + lag; a += F)
    {
        for (size_t k = 0; k < F; ++k)
        {
            const size_t i = a + k;
            in[k] = i < n ? x[i] * 32768.0f : 0.0f;
        }
        rnnoise_process_frame (st, o, in);
        for (size_t k = 0; k < F; ++k)
        {
            const size_t i = a + k;
            if (i >= lag && i - lag < n) out[i - lag] = o[k] * (1.0f / 32768.0f);
        }
    }
    rnnoise_destroy (st);
    x.swap (out);
    return true;
#else
    (void) x;
    return false;
#endif
}

/** Calosc: resampling w razie potrzeby, siec, domieszka oryginalu. */
inline bool denoise (std::vector<float>& x, double sr, float dryDb = -100.0f)
{
    if (! available() || x.empty() || sr < 8000.0) return false;
    const std::vector<float> orig = x;
    const int rate = (int) std::lround (sr);
    if (rate == 48000)
    {
        if (! denoise48 (x)) return false;
    }
    else
    {
        Resampler up, down;
        up.prepare (rate, 48000);
        down.prepare (48000, rate);
        std::vector<float> h, back;
        up.render (x, h);
        if (! denoise48 (h)) return false;
        down.render (h, back);
        back.resize (orig.size(), 0.0f);
        x.swap (back);
    }
    if (dryDb > -90.0f)
    {
        const float g = std::pow (10.0f, dryDb / 20.0f);
        for (size_t i = 0; i < x.size(); ++i) x[i] = (1.0f - g) * x[i] + g * orig[i];
    }
    return true;
}

} // namespace ai
} // namespace mix
