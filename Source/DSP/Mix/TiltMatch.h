/*
    TiltMatch.h — cofanie barwy, ktorej nikt nie zamawial.

    ---------------------------------------------------------------------------
    PO CO TO JEST

    Kompresja rozjasnia mowe. Nie dlatego, ze ktos tak chcial — tylko dlatego,
    ze najglosniejsze w mowie sa samogloski (dol i srodek), a spolgloski i
    oddechy siedza cicho i wysoko. Kompresor sciaga samogloski, spolgloski
    zostawia, i po wszystkim glos ma o kilka dB wiecej gory niz mial.

    Na prawdziwym wierszu zmierzylem to tak:

        kompresja                 +5 dB powyzej 4 kHz, +3 dB ponizej 250 Hz
        jazda suwakiem            drugie +3 dB gory
        --------------------------------------------------------------
        razem                     +9 dB gory

    Potem wchodzil de-esser i scinal cala gore o 11 dB, zeby to opanowac.
    Wynik: nagranie ciemniejsze niz oryginal, bez powietrza, i z sybilantami,
    ktore i tak wracaly, gdy tylko de-esser sie odpuscil. Czlowiek nazwal to
    "przytlumione, nieprzyjemne ale glosne" — i mial racje co do wszystkich
    trzech slow naraz.

    ---------------------------------------------------------------------------
    CO ROBIMY ZAMIAST TEGO

    Mierzymy widmo PRZED dynamika i PO niej, i cofamy roznice. Nie "poprawiamy
    barwe" — przywracamy te, ktora byla, plus to, co EQ mialo zrobic celowo.

    To jest wazna roznica. Program nie ma tu zadnego gustu. Ma tylko zasade:
    czego nie zamowiliscie, tego nie dostajecie. Jesli ktos ustawil +1 dB
    powietrza, dostaje +1 dB powietrza — a nie +9 dB, bo tak wyszlo.

    ---------------------------------------------------------------------------
    DLACZEGO PASMA, A NIE FFT

    Dopasowanie widma bin po binie potrafi wpisac w filtr rezonanse pokoju i
    formanty jednej samogloski, ktora akurat byla czesta. Szesc szerokich pasm
    nie ma jak tego zrobic — zlapie przechyl, nie szczegol. Przechyl jest tym,
    co sie zepsulo.

    Ograniczenie +-6 dB jest bezpiecznikiem: gdyby pomiar wyszedl absurdalny
    (inne zrodlo, cisza, urwany plik), filtr i tak nie zrobi krzywdy.
*/

#pragma once

#include "Biquad.h"
#include "Fft.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mix
{

//==============================================================================
class TiltMatch
{
public:
    static constexpr int kBands = 4;

    /** Trzy szerokie ksztalty, nie szesc waskich.

        Pierwsza wersja miala szesc dzwonow co pol oktawy i trafiala w zadana
        krzywa co do setnej dB — w swoich szesciu punktach. Miedzy nimi falowala
        o cztery decybele, bo zeby zgasic gore pasma, dzwon musi podbic dol
        sasiedniego. Zmierzona krzywa bledu jest gladka: szeroki garb 100-500 Hz,
        lekkie podbicie 1,6-3,2 kHz, lekki ubytek 5-6 kHz. Gladkiego bledu nie
        prostuje sie ostrymi narzedziami.

        Te trzy ksztalty nie opisza kazdego bledu co do decybela. Maja opisac
        ten, ktory robi kompresja — i nie dolozyc wlasnego.
    */
    static constexpr double centres[kBands] = { 300.0, 500.0, 1400.0, 5000.0 };

    struct Spectrum
    {
        std::vector<double> bin;
        double              hzPerBin = 0.0;
        bool                valid = false;
    };

    struct Curve
    {
        float gainDb[kBands] {};
        bool  active = false;

        float maxAbsDb() const noexcept
        {
            float m = 0.0f;
            for (int i = 0; i < kBands; ++i) m = std::max (m, std::fabs (gainDb[i]));
            return m;
        }
    };

    //==========================================================================
    /** Srednie widmo glosnych ramek.

        Zbior ramek ustala sie RAZ i narzuca obu stronom. Gdyby kazda strona
        wybierala sobie sama najglosniejsze 40%, po kompresji wpadlyby tam inne
        miejsca nagrania — wiecej oddechow i spolglosek — i porownywaloby sie
        widma z roznego materialu.
    */
    static Spectrum analyse (const std::vector<float>& x, double sr,
                             std::vector<char>* mask = nullptr)
    {
        constexpr size_t N = 2048, H = 1024;

        Spectrum sp;
        sp.hzPerBin = sr / (double) N;

        if (x.size() < N * 4) return sp;

        std::vector<double> win;
        hann (win, (int) N);

        Fft fft;
        fft.prepare ((int) N);

        std::vector<std::vector<double>> power;
        std::vector<double> energy;
        std::vector<double> re (N), im (N);

        for (size_t p = 0; p + N <= x.size(); p += H)
        {
            for (size_t i = 0; i < N; ++i) { re[i] = x[p + i] * win[i]; im[i] = 0.0; }

            fft.run (re, im, false);

            std::vector<double> m (N / 2 + 1);
            double e = 0.0;

            for (size_t i = 0; i <= N / 2; ++i)
            {
                m[i] = re[i] * re[i] + im[i] * im[i];
                e += m[i];
            }

            power.push_back (std::move (m));
            energy.push_back (e);
        }

        if (power.size() < 8) return sp;

        std::vector<char> own (power.size(), 0);

        if (mask != nullptr && mask->size() == power.size())
        {
            own = *mask;
        }
        else
        {
            auto sorted = energy;
            std::sort (sorted.begin(), sorted.end());
            const double thresh = sorted[sorted.size() * 60 / 100];

            for (size_t k = 0; k < power.size(); ++k)
                own[k] = energy[k] > thresh ? 1 : 0;

            if (mask != nullptr) *mask = own;
        }

        std::vector<double> avg (N / 2 + 1, 0.0);
        int used = 0;

        for (size_t k = 0; k < power.size(); ++k)
            if (own[k])
            {
                for (size_t i = 0; i <= N / 2; ++i) avg[i] += power[k][i];
                ++used;
            }

        if (used == 0) return sp;

        for (auto& v : avg) v /= (double) used;

        sp.bin = std::move (avg);
        sp.valid = true;
        return sp;
    }

    //==========================================================================
    /** Siatka pomiarowa: szesnascie punktow na oktawe miedzy 70 Hz a 16 kHz. */
    struct Grid
    {
        std::vector<double> hz, errDb, weight;
    };

    /** Krzywa bledu: o ile TRZEBA zmienic "got", zeby wygladalo jak "ref".

        Wagi to energia sygnalu odniesienia — pasmo, w ktorym prawie nic nie ma,
        nie moze rzadzic korekta. Staly skladnik jest odejmowany, bo glosnoscia
        zajmuje sie osobny stopien PO tej korekcie; korekta ma poprawic ksztalt,
        a nie dolozyc limiterowi pracy.
    */
    /** @param topHz  gdzie konczy sie zakres korekty.

        Gdy de-esser pracuje, korekta nie ma prawa siegac tam, gdzie on tnie.
        Inaczej wychodzi to, co wyszlo za pierwszym razem: de-esser sciaga
        sybilanty, korekta widzi "brakuje gory" i oddaje ja z powrotem statycznie
        — czyli dokladnie tam, gdzie mialo jej nie byc. Dwa stopnie robia wtedy
        prace, ktora sie znosi, i zostaja tylko ich skutki uboczne.
    */
    static Grid compare (const Spectrum& ref, const Spectrum& got,
                         double sr, double topHz)
    {
        Grid g;

        if (! ref.valid || ! got.valid) return g;

        const double lo = 70.0, hi = std::min (std::min (16000.0, topHz), sr * 0.45);
        const double step = std::pow (2.0, 1.0 / 16.0);

        // Okno usredniania: TERCJA, mimo ze krok jest co 1/16 oktawy.
        //
        // Przy kroku co 1/16 oktawy i prazku co 23 Hz pasmo wokol 350 Hz ma
        // szerokosc 15 Hz — czyli zero albo jeden prazek. Krzywa bledu liczona
        // z jednego prazka skacze o siedem decybeli w zaleznosci od tego, czy
        // trafila w harmoniczna glosu, czy miedzy nie. Pierwsza wersja robila
        // dokladnie to i dopasowanie probowalo potem isc za tym grzebieniem.
        //
        // Okna zachodza na siebie — punkty nie sa niezalezne i nie musza byc.
        // Szukamy ksztaltu, a ten jest gladki.
        const double halfWidth = std::pow (2.0, 1.0 / 6.0);

        for (double f = lo; f < hi; f *= step)
        {
            double a = f / halfWidth;
            double b = f * halfWidth;

            // na samym dole tercja to wciaz za malo prazkow
            if ((b - a) < 4.0 * ref.hzPerBin)
            {
                const double mid = (a + b) * 0.5;
                a = mid - 2.0 * ref.hzPerBin;
                b = mid + 2.0 * ref.hzPerBin;
            }

            double sr1 = 0.0, sg1 = 0.0;
            int n = 0;

            for (size_t i = 0; i < ref.bin.size(); ++i)
            {
                const double fr = (double) i * ref.hzPerBin;
                if (fr < a || fr >= b) continue;
                sr1 += ref.bin[i]; sg1 += got.bin[i]; ++n;
            }

            if (n < 3) continue;

            g.hz.push_back (f);
            g.errDb.push_back (10.0 * std::log10 ((sr1 + 1e-30) / (sg1 + 1e-30)));
            g.weight.push_back (10.0 * std::log10 (sr1 / (double) n + 1e-30));
        }

        // ---- wagi: rowne na oktawe, ale tylko tam, gdzie cos jest ------------
        //
        // Wazenie surowa moca bylo bledem. Glos ma ponizej 250 Hz o trzydziesci
        // decybeli wiecej energii niz powyzej 8 kHz, wiec dopasowanie widzialo
        // wylacznie dol i reszte krzywej ignorowalo — wynik byl odwrotny do
        // potrzebnego w gorze pasma. Ucho tak nie slyszy: pasmo jest istotne,
        // gdy w ogole cos w nim jest, a nie proporcjonalnie do mocy.
        //
        // Wiec: rowna waga na kazdy punkt siatki (czyli rowna na oktawe),
        // wygaszana dopiero tam, gdzie poziom spada ponad 40 dB pod szczyt —
        // bo tam juz nie ma czego poprawiac, jest szum pomiaru.
        double peak = -300.0;
        for (double w : g.weight) peak = std::max (peak, w);

        for (auto& w : g.weight)
        {
            const double under = peak - w;              // dB pod szczytem
            w = under <= 30.0 ? 1.0
              : under >= 45.0 ? 0.0
                              : (45.0 - under) / 15.0;
        }

        // zdejmij skladowa stala
        double ws = 0.0, es = 0.0;
        for (size_t i = 0; i < g.hz.size(); ++i) { ws += g.weight[i]; es += g.weight[i] * g.errDb[i]; }
        if (ws > 0.0) { const double m = es / ws; for (auto& e : g.errDb) e -= m; }

        return g;
    }

    //==========================================================================
    /** Dopasowanie trzech wzmocnien do calej krzywej bledu, metoda najmniejszych
        kwadratow wazonych energia.

        Nie "ustaw kazdy filtr na wartosc w jego srodku" — to bylo zrodlo
        falowania. Tutaj liczy sie, jak bank wyglada na CALEJ siatce, i szuka
        takich trzech liczb, zeby suma kwadratow odchylek byla najmniejsza. Tam,
        gdzie trzy ksztalty nie potrafia opisac bledu, korekta po prostu go nie
        rusza — zamiast wpisywac w filtr cos, czego nikt nie mierzyl.
    */
    Curve fit (const Grid& grid, double sr, float maxDb, bool protectTreble = false)
    {
        Curve c;
        sampleRate = sr;

        if (grid.hz.size() < 8) return c;

        if (grid.weight.size() != grid.hz.size() || grid.errDb.size() != grid.hz.size()) return c;
        double g[kBands] {};
        auto error = [&] (const double* gains)
        {
            setGains (gains);
            double e = 0;
            for (size_t i = 0; i < grid.hz.size(); ++i)
            {
                const double r = grid.errDb[i] - responseDb (grid.hz[i]);
                e += grid.weight[i] * r * r;
            }
            // Fitting only below 4 kHz must not permit a boost above it.
            if (protectTreble)
                for (double f = 4000; f < sr * .45; f *= 1.08)
                    if (responseDb (f) > 0.05) return 1.0e100;
            return std::isfinite (e) ? e : 1.0e100;
        };

        for (int iter = 0; iter < 40; ++iter)
        {
            setGains (g);

            // residuum i pochodne (ksztalt kazdego filtru na +1 dB)
            std::vector<double> res (grid.hz.size());
            std::vector<std::vector<double>> J (kBands, std::vector<double> (grid.hz.size()));

            for (size_t i = 0; i < grid.hz.size(); ++i)
                res[i] = grid.errDb[i] - responseDb (grid.hz[i]);

            for (int k = 0; k < kBands; ++k)
            {
                double probe[kBands];
                for (int j = 0; j < kBands; ++j) probe[j] = g[j];
                probe[k] += 1.0;
                setGains (probe);

                for (size_t i = 0; i < grid.hz.size(); ++i)
                    J[k][i] = responseDb (grid.hz[i]);

                setGains (g);

                for (size_t i = 0; i < grid.hz.size(); ++i)
                    J[k][i] -= responseDb (grid.hz[i]);
            }

            // normalne rownania 3x3
            double A[kBands][kBands] {}, rhs[kBands] {};

            for (size_t i = 0; i < grid.hz.size(); ++i)
            {
                const double w = grid.weight[i];
                for (int k = 0; k < kBands; ++k)
                {
                    rhs[k] += w * J[k][i] * res[i];
                    for (int m = 0; m < kBands; ++m) A[k][m] += w * J[k][i] * J[m][i];
                }
            }

            double scale = 0;
            for (int k = 0; k < kBands; ++k) scale = std::max (scale, A[k][k]);
            for (int k = 0; k < kBands; ++k) A[k][k] += std::max (1.0e-6, scale * 1.0e-4);

            double d[kBands];
            if (! solve3 (A, rhs, d)) break;

            const double before = error (g);
            double candidate[kBands] {}, move = 0;
            bool accepted = false;
            for (double step = 1.0; step >= 1.0/128.0; step *= 0.5)
            {
                move = 0;
                for (int k = 0; k < kBands; ++k)
                {
                    candidate[k] = std::clamp (g[k] + d[k] * step, -(double)maxDb, (double)maxDb);
                    move = std::max (move, std::fabs (candidate[k] - g[k]));
                }
                if (error (candidate) < before) { accepted = true; break; }
            }
            if (! accepted) break;
            std::copy (candidate, candidate + kBands, g);
            if (move < 0.005) break;
        }

        setGains (g);
        for (int k = 0; k < kBands; ++k) c.gainDb[k] = (float) g[k];
        c.active = c.maxAbsDb() > 0.25f;
        return c;
    }

    //==========================================================================
    void prepare (const Curve& curve, double sr)
    {
        sampleRate = sr;
        active = curve.active;

        double g[kBands];
        for (int k = 0; k < kBands; ++k) g[k] = curve.gainDb[k];

        setGains (g);
        for (auto& f : filt) f.reset();
    }

    void reset() { for (auto& f : filt) f.reset(); }

    inline float process (float x) noexcept
    {
        if (! active) return x;

        double y = x;
        for (int b = 0; b < kBands; ++b) y = filt[b].process ((float) y);
        return (float) y;
    }

    /** Laczna odpowiedz banku, w dB. */
    double responseDb (double hz) const noexcept
    {
        double m = 1.0;
        for (int b = 0; b < kBands; ++b) m *= filt[b].magnitudeAt (hz, sampleRate);
        return 20.0 * std::log10 (std::max (1.0e-9, m));
    }

    bool isActive() const noexcept { return active; }

private:
    void setGains (const double g[kBands]) noexcept
    {
        // polki i dzwon celowo szerokie: maja isc za ksztaltem bledu, a nie
        // wycinac w nim dziure
        filt[0].setCoeffs (design::lowShelf  (centres[0], sampleRate, g[0], 0.70));
        filt[1].setCoeffs (design::peaking   (centres[1], sampleRate, 1.4, g[1]));
        filt[2].setCoeffs (design::peaking   (centres[2], sampleRate, 0.9, g[2]));
        filt[3].setCoeffs (design::highShelf (centres[3], sampleRate, g[3], 0.62));
    }

    static bool solve3 (double A[kBands][kBands], const double b[kBands], double x[kBands])
    {
        double m[kBands][kBands + 1];
        for (int i = 0; i < kBands; ++i)
        {
            for (int j = 0; j < kBands; ++j) m[i][j] = A[i][j];
            m[i][kBands] = b[i];
        }

        for (int i = 0; i < kBands; ++i)
        {
            int piv = i;
            for (int r = i + 1; r < kBands; ++r)
                if (std::fabs (m[r][i]) > std::fabs (m[piv][i])) piv = r;

            if (std::fabs (m[piv][i]) < 1.0e-12) return false;
            if (piv != i) for (int j = 0; j <= kBands; ++j) std::swap (m[i][j], m[piv][j]);

            for (int r = 0; r < kBands; ++r)
            {
                if (r == i) continue;
                const double f = m[r][i] / m[i][i];
                for (int j = i; j <= kBands; ++j) m[r][j] -= f * m[i][j];
            }
        }

        for (int i = 0; i < kBands; ++i) x[i] = m[i][kBands] / m[i][i];
        return true;
    }

    double sampleRate = 48000.0;
    Biquad filt[kBands];
    bool   active = false;
};

} // namespace mix
