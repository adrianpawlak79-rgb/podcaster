/*
    Breath.h — sciszanie oddechow (nie do zera).

    ---------------------------------------------------------------------------
    SKAD TO SIE WZIELO

    Adrian po odsluchu 12.10: "slychac oddechy". Jego lancuch na wtyczkach (v3)
    ma RX Breath Control, ale pomiar pokazal, ze oddechy wyszly tam wzgledem
    glosu o 2-9 dB GLOSNIEJ niz w surowym nagraniu: kompresja podnosi wszystko,
    co ciche. Sciszenie oddechu przed kompresja to jedyny sposob, zeby kompresor
    go potem nie wyciagal.

    ---------------------------------------------------------------------------
    CO JEST ODDECHEM (zmierzone na nagraniu Adriana, SM7B, Vosk jako granice slow)

    Kandydat: bezdzwieczny (brak tonu glosu), wyraznie nad tlem, 120-900 ms.
    Takie sa tez glosk "s", "sz", "ch", "f" w srodku slow — i tych ruszac nie
    wolno, bo to psuje slowa. Rozdziela je glosnosc i sasiedztwo:

        11 odcinkow miedzy slowami:  od -21,9 do -29,6 dB pod glosna mowa
        19 odcinkow w slowach:       od -13,0 do -21,2 dB (jeden -24,6 — odcinek
                                     z cisza po nim, pewnie oddech, ktory Vosk
                                     wliczyl do slowa)

    Wiec: co najmniej 22 dB pod glosna mowa i NIE miedzy dwiema samogloskami
    (glosk w srodku slowa ma dzwieczne z obu stron). Na tym nagraniu: 10 z 11
    przerw, zadna glosk w slowie.

    Sciszamy o 12 dB, nie do zera — oddech skasowany calkiem brzmi w czytanym
    tekscie nienaturalnie (Adrian ustawil RX na -18, nie -Inf). Brzegi lagodnie
    (15 ms), wewnatrz odcinka, zeby nie dotknac sasiednich glosek.

    Bez JUCE — test: Tests/oddechy-test.cpp.
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace mix
{

class BreathControl
{
public:
    struct Params
    {
        float  reductionDb   = -12.0f;
        double belowSpeechDb = 22.0;   ///< co najmniej tyle pod glosna mowa (95. percentyl)
        double aboveNoiseDb  = 10.0;   ///< i tyle nad tlem (10. percentyl)
        double minSec = 0.12, maxSec = 0.9;
        double fadeSec = 0.015;
        double voicedCorr = 0.45;      ///< ponizej: bezdzwieczne
        double neighbourCorr = 0.6;    ///< samogloska obok
    };

    Params params;
    int found = 0;                     ///< ile oddechow sciszyl ostatni render

    /** Odcinki oddechow (sekundy). */
    std::vector<std::pair<double, double>> find (const std::vector<float>& x, double sr) const
    {
        std::vector<std::pair<double, double>> out;
        const int hop = (int) std::lround (0.010 * sr), win = (int) std::lround (0.030 * sr);
        if (sr <= 0.0 || (int) x.size() < win * 10) return out;

        // glosnosc i dzwiecznosc co 10 ms; dzwiecznosc z autokorelacji na sygnale
        // przetrzebionym do ok. 8 kHz (ton glosu 70-400 Hz), zeby bylo szybko
        const int dec = std::max (1, (int) (sr / 8000.0));
        const double dsr = sr / dec;
        std::vector<float> d (x.size() / (size_t) dec);
        for (size_t i = 0; i < d.size(); ++i)
        {
            double s = 0.0;
            for (int k = 0; k < dec; ++k) s += x[i * (size_t) dec + (size_t) k];
            d[i] = (float) (s / dec);
        }

        const int frames = ((int) x.size() - win) / hop;
        std::vector<double> lev ((size_t) frames), voi ((size_t) frames);
        const int dw = win / dec, lagLo = (int) (dsr / 400.0), lagHi = (int) (dsr / 70.0);
        std::vector<double> seg ((size_t) dw);

        for (int f = 0; f < frames; ++f)
        {
            double e = 0.0;
            for (int k = 0; k < win; ++k) { const double v = x[(size_t) (f * hop + k)]; e += v * v; }
            lev[(size_t) f] = 10.0 * std::log10 (e / win + 1.0e-20);

            const size_t o = (size_t) (f * hop / dec);
            if (o + (size_t) dw > d.size()) { voi[(size_t) f] = 0.0; continue; }
            double mean = 0.0;
            for (int k = 0; k < dw; ++k) mean += d[o + (size_t) k];
            mean /= dw;
            double r0 = 0.0;
            for (int k = 0; k < dw; ++k) { seg[(size_t) k] = d[o + (size_t) k] - mean; r0 += seg[(size_t) k] * seg[(size_t) k]; }
            double best = 0.0;
            if (r0 > 1.0e-14)
                for (int lag = lagLo; lag <= lagHi && lag < dw; ++lag)
                {
                    double r = 0.0;
                    for (int k = 0; k + lag < dw; ++k) r += seg[(size_t) k] * seg[(size_t) (k + lag)];
                    best = std::max (best, r / r0);
                }
            voi[(size_t) f] = best;
        }

        auto sorted = lev; std::sort (sorted.begin(), sorted.end());
        const double noise  = sorted[sorted.size() / 10];
        const double speech = sorted[sorted.size() * 95 / 100];
        if (speech - noise < params.belowSpeechDb) return out;      // za malo miejsca miedzy tlem a mowa

        auto cand = [&] (int f) {
            return voi[(size_t) f] < params.voicedCorr && lev[(size_t) f] > noise + params.aboveNoiseDb
                && lev[(size_t) f] < speech - 10.0;
        };
        auto voicedNear = [&] (int a, int b) {
            for (int f = std::max (0, a); f < std::min (frames, b); ++f)
                if (voi[(size_t) f] > params.neighbourCorr && lev[(size_t) f] > noise + params.aboveNoiseDb) return true;
            return false;
        };

        for (int f = 0; f < frames;)
        {
            if (! cand (f)) { ++f; continue; }
            int g = f;
            while (g < frames && cand (g)) ++g;
            const double len = (g - f) * hop / sr;

            std::vector<double> l (lev.begin() + f, lev.begin() + g);
            std::nth_element (l.begin(), l.begin() + (long) (l.size() / 2), l.end());
            const double median = l[l.size() / 2];

            const bool between = voicedNear (f - 4, f) && voicedNear (g, g + 4);
            if (len >= params.minSec && len <= params.maxSec && median <= speech - params.belowSpeechDb && ! between)
                out.push_back ({ f * hop / sr, (g * hop + win) / sr });
            f = g;
        }
        return out;
    }

    void render (const std::vector<float>& in, std::vector<float>& out, double sr)
    {
        out = in;
        const auto segs = find (in, sr);
        found = (int) segs.size();
        const float g = std::pow (10.0f, params.reductionDb / 20.0f);
        const size_t fade = (size_t) std::lround (params.fadeSec * sr);

        for (const auto& s : segs)
        {
            const size_t a = (size_t) (s.first * sr), b = std::min (out.size(), (size_t) (s.second * sr));
            if (b <= a) continue;
            const size_t f = std::min (fade, (b - a) / 2);
            for (size_t i = a; i < b; ++i)
            {
                // lagodnie na brzegach, wewnatrz odcinka (sasiednie glosk nietkniete)
                float w = 1.0f;
                if (f > 0 && i < a + f)  w = 0.5f - 0.5f * std::cos (3.14159265f * (float) (i - a) / (float) f);
                if (f > 0 && i >= b - f) w = std::min (w, 0.5f - 0.5f * std::cos (3.14159265f * (float) (b - 1 - i) / (float) f));
                out[i] *= 1.0f + (g - 1.0f) * w;
            }
        }
    }
};

} // namespace mix
