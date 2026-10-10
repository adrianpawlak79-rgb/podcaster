/*
    Tame.h — tlumienie tego, co wystaje ponad wlasna srednia.

    ---------------------------------------------------------------------------
    SKAD TEN POMYSL

    Czlowiek powiedzial: "mysle ze jestes w stanie zrobic lepszy de-esser
    wzorujac sie na moich wtyczkach". Wiec zajrzalem do jego lancucha. Nie ma
    tam zadnego Waves — jest FabFilter Pro-DS z zakresem 6 dB, a przed nim
    soothe2 na glebokosci 3,5.

    I to soothe2 jest odpowiedzia, nie de-esser. Bo de-esser — kazdy, takze
    dobry — odpowiada na pytanie "czy teraz jest za duzo gory?". A pytanie,
    ktore trzeba zadac, brzmi inaczej: "czy teraz cos wystaje ponad to, co ten
    glos robi zwykle?".

    ---------------------------------------------------------------------------
    NA CZYM POLEGA ROZNICA

    De-esser ma prog ustawiony z zewnatrz. Jesli ktos mowi jasno, de-esser tnie
    caly czas i glos robi sie matowy. Jesli ktos mowi ciemno, de-esser nie
    rusza sie wcale, a jego jedno ostre "s" przechodzi bez zmian.

    Tutaj prog jest wyliczony z samego nagrania i osobno dla kazdego pasma.
    Program pamieta, ile energii ma zwykle to konkretne pasmo w tym konkretnym
    glosie, i reaguje dopiero, gdy biezaca chwila przekroczy te pamiec o
    zadana liczbe decybelow. Sybilant przekracza swoje pasmo o kilkanascie dB i
    zostaje sciety. Jasna barwa glosu nie przekracza niczego, bo TO WLASNIE ONA
    ustawila srednia — i zostaje nietknieta.

    Dlatego to nie moze przytlumic nagrania. Nie ma jak: staly przechyl barwy
    wchodzi do sredniej i przestaje byc odchyleniem.

    ---------------------------------------------------------------------------
    DLACZEGO PASMA TERCJOWE, A NIE PRAZKI

    Kuszace jest patrzec na kazdy prazek osobno. To nie dziala: przy 2048
    punktach i probkowaniu 48 kHz prazki sa co 23 Hz, a harmoniczne glosu co
    ~120 Hz — wiec KAZDA harmoniczna "wystaje" o kilkanascie dB ponad sasiadow.
    Tlumiac to, co wystaje, wytlumiloby sie harmoniczne, czyli sam glos.

    Tercja usrednia po kilku harmonicznych naraz, wiec harmoniczne sa dla niej
    plaskim tlem. Wystaje dopiero to, co jest szersze niz harmoniczna: sybilant,
    rezonans pomieszczenia, ostry przydech. Czyli dokladnie to, co ma wystawac.
*/

#pragma once

#include "Fft.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mix
{

//==============================================================================
class Tame
{
public:
    struct Params
    {
        int    fftSize     = 2048;
        double lowHz       = 1200.0;  ///< ponizej nie ruszamy: tam mieszka glos
        double highHz      = 16000.0;
        int    perOctave   = 3;       ///< tercje
        float  thresholdDb = 6.0f;    ///< o ile pasmo musi przekroczyc wlasna srednia
        float  ratio       = 3.0f;
        float  maxCutDb    = -10.0f;
        double memorySec   = 2.5;     ///< jak dlugo pamieta "co robi ten glos"
        float  attackMs    = 4.0f;
        float  releaseMs   = 90.0f;
    };

    Params params;

    /** Do raportu: najglebsze tlumienie i ile pasm w ogole ruszylo. */
    float  maxCutUsedDb = 0.0f;
    double activeRatio  = 0.0;

    //==========================================================================
    void prepare (double sampleRate)
    {
        sr  = sampleRate;
        n   = params.fftSize;
        hop = n / 4;
        bins = n / 2 + 1;

        fft.prepare (n);
        hann (win, n);

        re.assign ((size_t) n, 0.0);
        im.assign ((size_t) n, 0.0);

        buildBands();

        level.assign (band.size(), 0.0);
        mem.assign   (band.size(), 0.0);
        cut.assign   (band.size(), 0.0);

        const double frameSec = hop / sr;
        aMem   = std::exp (-frameSec / params.memorySec);
        aUp    = std::exp (-frameSec / (0.001 * params.attackMs));
        aDown  = std::exp (-frameSec / (0.001 * params.releaseMs));

        warm = 0;
        loudSeen = -300.0;
        maxCutUsedDb = 0.0f;
        activeRatio = 0.0;
    }

    //==========================================================================
    void render (const std::vector<float>& in, std::vector<float>& out)
    {
        out.assign (in.size(), 0.0f);

        std::vector<float> tail (in.size() + (size_t) (2 * n), 0.0f);
        std::copy (in.begin(), in.end(), tail.begin() + n);

        std::vector<float> acc (tail.size() + (size_t) n, 0.0f);

        long long frames = 0, active = 0;

        for (size_t start = 0; start + (size_t) n <= tail.size(); start += (size_t) hop)
        {
            for (int i = 0; i < n; ++i)
            {
                re[(size_t) i] = tail[start + (size_t) i] * win[(size_t) i];
                im[(size_t) i] = 0.0;
            }

            fft.run (re, im, false);

            if (processSpectrum()) ++active;
            ++frames;

            fft.run (re, im, true);

            for (int i = 0; i < n; ++i)
                acc[start + (size_t) i] += (float) (re[(size_t) i] * win[(size_t) i] / 1.5);
        }

        for (size_t i = 0; i < out.size(); ++i)
            out[i] = acc[i + (size_t) n];

        activeRatio = frames > 0 ? (double) active / (double) frames : 0.0;
    }

private:
    struct Band { int lo, hi; };

    //==========================================================================
    void buildBands()
    {
        band.clear();

        const double step = std::pow (2.0, 1.0 / (double) params.perOctave);
        const double hzPerBin = sr / (double) n;
        const double top = std::min (params.highHz, sr * 0.47);

        for (double f = params.lowHz; f < top; f *= step)
        {
            const double a = f / std::sqrt (step);
            const double b = f * std::sqrt (step);

            Band w { (int) std::ceil (a / hzPerBin), (int) std::floor (b / hzPerBin) };

            w.lo = std::max (1, w.lo);
            w.hi = std::min (bins - 1, w.hi);

            if (w.hi - w.lo >= 1) band.push_back (w);
        }
    }

    //==========================================================================
    /** @return true, jesli w tej ramce cokolwiek zostalo scisniete */
    bool processSpectrum()
    {
        bool touched = false;

        // ---- ile ma teraz kazde pasmo ----------------------------------------
        for (size_t b = 0; b < band.size(); ++b)
        {
            double p = 0.0;
            for (int k = band[b].lo; k <= band[b].hi; ++k)
                p += re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];

            level[b] = p / (double) (band[b].hi - band[b].lo + 1);
        }

        // ---- cala ramka za cicho: to cisza, nie material ---------------------
        double total = 0.0;
        for (double v : level) total += v;

        double broad = 0.0;
        for (int k = 0; k < bins; ++k)
            broad += (k == 0 || k == bins - 1 ? 1.0 : 2.0)
                   * (re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k]);
        const double frameDb = 10.0 * std::log10 (broad / (double (n) * n) + 1.0e-30);

        loudSeen = std::max (loudSeen, frameDb);

        if (frameDb < -90.0 || frameDb < loudSeen - 35.0)
        {
            // Cisza nie ma prawa wchodzic do pamieci — inaczej srednia zjezdza
            // w dol, wszystko zaczyna "wystawac" i program tlumi mowe za to, ze
            // jest glosniejsza od ciszy.
            releaseAll();
            applyGains();
            return false;
        }

        if (warm < warmFrames) ++warm;

        // ---- porownanie z wlasna pamiecia ------------------------------------
        int cutBands = 0;

        for (size_t b = 0; b < band.size(); ++b)
        {
            const double nowDb = 10.0 * std::log10 (level[b] / (broad + 1.0e-30) + 1.0e-30);

            if (warm == 1) mem[b] = nowDb;

            const double refDb = mem[b];

            // pamiec nadaza za glosem, ale wolno — inaczej sybilant zdazylby
            // podniesc wlasny prog, zanim zostanie sciety
            mem[b] = aMem * mem[b] + (1.0 - aMem) * nowDb;

            double want = 0.0;

            if (warm >= warmFrames && nowDb > -70.0)
            {
                const double over = nowDb - refDb - (double) params.thresholdDb;

                if (over > 0.0)
                {
                    want = -over * (1.0 - 1.0 / (double) params.ratio);
                    want = std::max (want, (double) params.maxCutDb);
                }
            }

            // obwiednia na tlumieniu, w decybelach
            const double a = want < cut[b] ? aUp : aDown;
            cut[b] = a * cut[b] + (1.0 - a) * want;

            if (cut[b] < -0.2) { ++cutBands; touched = true; }

            maxCutUsedDb = std::min (maxCutUsedDb, (float) cut[b]);
        }

        (void) cutBands;
        applyGains();
        return touched;
    }

    void releaseAll()
    {
        for (auto& c : cut) c = aDown * c;
    }

    //==========================================================================
    /** Przejscie miedzy pasmami musi byc gladkie.

        Gdyby kazde pasmo dostalo swoje wzmocnienie i koniec, na granicach
        powstalyby schodki — a schodek w widmie slychac jako gwizd przy kazdej
        zmianie tlumienia. Dlatego wzmocnienie kazdego prazka jest interpolowane
        miedzy srodkami sasiednich pasm.
    */
    void applyGains()
    {
        if (band.empty()) return;

        for (int k = 0; k < bins; ++k)
        {
            double g = 0.0;

            const int first = (band.front().lo + band.front().hi) / 2;
            const int last  = (band.back().lo  + band.back().hi)  / 2;

            if (k <= first)      g = k < band.front().lo ? 0.0 : cut[0];
            else if (k >= last)  g = cut[band.size() - 1];
            else
            {
                for (size_t b = 0; b + 1 < band.size(); ++b)
                {
                    const int c0 = (band[b].lo + band[b].hi) / 2;
                    const int c1 = (band[b + 1].lo + band[b + 1].hi) / 2;

                    if (k >= c0 && k < c1)
                    {
                        const double t = (double) (k - c0) / (double) std::max (1, c1 - c0);
                        g = cut[b] * (1.0 - t) + cut[b + 1] * t;
                        break;
                    }
                }
            }

            if (g >= -0.001) continue;

            const double lin = std::pow (10.0, g / 20.0);

            re[(size_t) k] *= lin;
            im[(size_t) k] *= lin;

            if (k > 0 && k < n - k)
            {
                re[(size_t) (n - k)] *= lin;
                im[(size_t) (n - k)] *= lin;
            }
        }
    }

    //==========================================================================
    static constexpr int warmFrames = 40;   ///< zanim pamiec cos znaczy

    double sr = 48000.0;
    int    n = 2048, hop = 512, bins = 1025;

    Fft fft;
    std::vector<double> win, re, im;
    std::vector<Band>   band;
    std::vector<double> level, mem, cut;

    double aMem = 0.0, aUp = 0.0, aDown = 0.0;
    double loudSeen = -300.0;
    int    warm = 0;
};

} // namespace mix
