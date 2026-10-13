/*
    EditedView.h — fala tak, jak bedzie wygladac PO cieciach (widok, nie plik).

    Adrian 13.10, test z R: po "Czytaj" chcial widziec "nadpisanie" — jak w
    DAW przy punch-in: nowe czytanie pojawia sie od miejsca ciecia, w miejscu
    pomylki. Plik jest ciety dopiero po zatrzymaniu (Punch.h); tu tylko
    pokazujemy ostatnie sekundy z pominieciem odcinkow, ktore wypadna.

    Czas widoku: koniec widoku = "teraz", a kazdy wczesniejszy moment jest
    przesuniety w prawo o dlugosc wycietych odcinkow miedzy nim a "teraz".

    Bez JUCE — test: Tests/preroll-test.cpp (sekcja 1e).
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace tw
{

struct EditedView
{
    using Spans = std::vector<std::pair<double, double>>;   ///< [od, do) w czasie nagrania

    static bool hidden (double t, const Spans& spans)
    {
        for (const auto& s : spans)
            if (t >= s.first && t < s.second) return true;
        return false;
    }

    /** Ile z (t, teraz] wypadnie. */
    static double removedAfter (double t, double now, const Spans& spans)
    {
        double sum = 0.0;
        for (const auto& s : spans)
        {
            const double a = std::max (s.first, t), b = std::min (s.second, now);
            if (b > a) sum += b - a;
        }
        return sum;
    }

    /** Czas w nagraniu -> czas w widoku (widok konczy sie na "teraz"). */
    static double toView (double t, double now, const Spans& spans) { return t + removedAfter (t, now, spans); }

    /** Ostatnie viewSec sekund bez wycietych odcinkow; brakujace na poczatku = cisza. */
    static std::vector<float> render (const std::vector<float>& hist, double histStart, double rate,
                                      double now, const Spans& spans, double viewSec)
    {
        const size_t want = (size_t) std::lround (viewSec * rate);
        std::vector<float> out;
        out.reserve (want);
        for (size_t k = hist.size(); k-- > 0 && out.size() < want;)
        {
            const double t = histStart + (double) k / rate;
            if (t > now || hidden (t, spans)) continue;
            out.push_back (hist[k]);
        }
        out.resize (want, 0.0f);
        std::reverse (out.begin(), out.end());
        return out;
    }
};

} // namespace tw
