/*
    VoiceProfile.h — wzorzec brzmienia: barwa miksu, ktory sie podobal.

    ---------------------------------------------------------------------------
    PO CO

    Glosnosc odcinkow jest juz rowna — kazdy miks celuje w te sama liczbe
    LUFS. Barwa nie: inna odleglosc od mikrofonu (efekt zblizeniowy: +3-6 dB
    dolu), inny pokoj, inny dzien glosu. Ten sam lancuch na dwoch takich
    nagraniach daje dwa rozne brzmienia — i seria odcinkow brzmi jak z dwoch
    studiow.

    Wzorzec to srednie widmo glosnych fragmentow miksu, ktory czlowiek
    zatwierdzil ("Zapamietaj to brzmienie"). Przy nastepnym miksie lancuch
    dociaga barwe do wzorca tym samym narzedziem, ktore cofa barwe po
    kompresji (TiltMatch): trzy szerokie ksztalty, najwyzej +-3 dB, bez
    rezonansow i formantow jednej samogloski.

    Zapisujemy ksztalt, nie glosnosc: krzywa ma odjeta srednia. Glosnosc
    robi osobny stopien.

    ---------------------------------------------------------------------------
    CZEGO TO NIE ROBI

    Nie zamienia jednego glosu w drugi. Wzorzec z cudzego nagrania zadziala,
    ale tylko w granicach +-3 dB szerokimi pasmami — przechyl, nie barwa
    czyjegos gardla. I nie rusza Delikatnego: ten z zalozenia zostawia glos
    taki, jak go nagrano.
*/

#pragma once

#include "TiltMatch.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace mix
{

struct VoiceProfile
{
    std::vector<double> hz, db;    ///< siatka 1/16 oktawy 70 Hz - 16 kHz, dB ze srednia 0
    std::string label;             ///< skad wzorzec (np. "sesja_2026-10-22", data) — dla czlowieka

    bool valid() const noexcept { return hz.size() >= 16 && hz.size() == db.size(); }

    //==========================================================================
    /** Wzorzec z gotowego miksu. Wystarcza pierwsze 10 minut: barwa glosu nie
        zmienia sie w trakcie czytania, a godzina to niepotrzebne sekundy FFT. */
    static VoiceProfile measure (const std::vector<float>& x, double sr)
    {
        const size_t maxLen = (size_t) (sr * 600.0);
        const std::vector<float> part (x.begin(), x.begin() + (long) std::min (maxLen, x.size()));
        return fromSpectrum (TiltMatch::analyse (part, sr));
    }

    /** Krzywa na siatce 1/16 oktawy, kazdy punkt to srednia z tercji wokol. */
    static VoiceProfile fromSpectrum (const TiltMatch::Spectrum& sp)
    {
        VoiceProfile p;
        if (! sp.valid) return p;
        const double top = std::min (16000.0, sp.hzPerBin * (double) sp.bin.size() * 0.9);
        const double step = std::pow (2.0, 1.0 / 16.0), half = std::pow (2.0, 1.0 / 6.0);
        for (double f = 70.0; f < top; f *= step)
        {
            double a = f / half, b = f * half;
            if ((b - a) < 4.0 * sp.hzPerBin) { const double m = (a + b) * 0.5; a = m - 2.0 * sp.hzPerBin; b = m + 2.0 * sp.hzPerBin; }
            double s = 0.0; int n = 0;
            for (size_t i = 0; i < sp.bin.size(); ++i)
            {
                const double fr = (double) i * sp.hzPerBin;
                if (fr >= a && fr < b) { s += sp.bin[i]; ++n; }
            }
            if (n < 3) continue;
            p.hz.push_back (f);
            p.db.push_back (10.0 * std::log10 (s / n + 1.0e-30));
        }
        if (! p.valid()) { p.hz.clear(); p.db.clear(); return p; }
        double mean = 0.0;
        for (double v : p.db) mean += v;
        mean /= (double) p.db.size();
        for (auto& v : p.db) v -= mean;
        return p;
    }

    /** Poziom krzywej w dowolnej czestotliwosci (liniowo w skali log f). */
    double at (double f) const
    {
        if (f <= hz.front()) return db.front();
        if (f >= hz.back()) return db.back();
        const size_t k = (size_t) (std::upper_bound (hz.begin(), hz.end(), f) - hz.begin());
        const double t = std::log (f / hz[k - 1]) / std::log (hz[k] / hz[k - 1]);
        return db[k - 1] + t * (db[k] - db[k - 1]);
    }

    //==========================================================================
    /** Krzywa bledu dla TiltMatch::fit: o ile trzeba zmienic "got", zeby
        brzmialo jak wzorzec. Obie strony mierzone TYM SAMYM sposobem (tercje
        na siatce 1/16 oktawy) — porownanie wygladzonego wzorca z niewygladzonym
        widmem dawalo +-2 dB zmarszczek na glosie o wyraznych harmonicznych.
        Wagi jak w TiltMatch::compare: rowne, wygaszane 30-45 dB pod szczytem. */
    TiltMatch::Grid against (const TiltMatch::Spectrum& gotSp, double topHz) const
    {
        TiltMatch::Grid g;
        const auto got = fromSpectrum (gotSp);
        if (! valid() || ! got.valid()) return g;

        double peak = -300.0;
        std::vector<double> level;
        for (size_t i = 0; i < got.hz.size(); ++i)
        {
            if (got.hz[i] > topHz || got.hz[i] < hz.front() || got.hz[i] > hz.back()) continue;
            g.hz.push_back (got.hz[i]);
            g.errDb.push_back (at (got.hz[i]) - got.db[i]);
            level.push_back (got.db[i]);
            peak = std::max (peak, got.db[i]);
        }
        for (double l : level)
        {
            const double under = peak - l;
            g.weight.push_back (under <= 30.0 ? 1.0 : under >= 45.0 ? 0.0 : (45.0 - under) / 15.0);
        }
        double ws = 0.0, es = 0.0;
        for (size_t i = 0; i < g.hz.size(); ++i) { ws += g.weight[i]; es += g.weight[i] * g.errDb[i]; }
        if (ws > 0.0) { const double m = es / ws; for (auto& e : g.errDb) e -= m; }
        return g;
    }

    /** Roznica barwy wobec wzorca: wazone RMS krzywej bledu (dB). */
    double distance (const std::vector<float>& x, double sr) const
    {
        const auto g = against (TiltMatch::analyse (x, sr), 16000.0);
        double ws = 0.0, e2 = 0.0;
        for (size_t i = 0; i < g.hz.size(); ++i) { ws += g.weight[i]; e2 += g.weight[i] * g.errDb[i] * g.errDb[i]; }
        return ws > 0.0 ? std::sqrt (e2 / ws) : 0.0;
    }

    //==========================================================================
    // Zapis w ustawieniach: jedna linia "etykieta|hz:db;hz:db;..."

    std::string toText() const
    {
        std::string out;
        for (char c : label) out += (c == '|' || c == '\n') ? ' ' : c;
        out += '|';
        char b[48];
        for (size_t i = 0; i < hz.size(); ++i)
        {
            std::snprintf (b, sizeof (b), "%s%.1f:%.2f", i ? ";" : "", hz[i], db[i]);
            out += b;
        }
        return out;
    }

    static VoiceProfile fromText (const std::string& text)
    {
        VoiceProfile p;
        const size_t bar = text.find ('|');
        if (bar == std::string::npos) return p;
        p.label = text.substr (0, bar);
        std::istringstream in (text.substr (bar + 1));
        std::string item;
        while (std::getline (in, item, ';'))
        {
            const size_t colon = item.find (':');
            if (colon == std::string::npos) return {};
            const double f = std::atof (item.substr (0, colon).c_str()), d = std::atof (item.substr (colon + 1).c_str());
            if (! (f > 0.0) || ! std::isfinite (d) || (! p.hz.empty() && f <= p.hz.back())) return {};
            p.hz.push_back (f); p.db.push_back (d);
        }
        if (! p.valid()) return {};
        return p;
    }
};

} // namespace mix
