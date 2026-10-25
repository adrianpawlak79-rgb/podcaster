/*
    BackingEdit.h — ciecie i wyciszanie nagrania zrobionego do podkladu (25.10).

    Muzyka jest nieruszalna (Adrian 24.10): gra od poczatku nagrania i nikt
    jej nie tnie. Podczas nagrania wznowienie po pauzie startuje tam, gdzie
    glos wypadnie po cieciach (EditedView::editedTime). Zeby to sie zgadzalo
    w pliku, po zatrzymaniu trzeba wyciac DOKLADNIE te odcinki, ktore liczyl
    rozbieg — bez szukania innej ciszy (Punch::plan) i bez zjadania
    dodatkowych probek na przenikanie (Punch::render bierze 20 ms wiecej na
    kazdym ciecie, CutEdit::cutRange 10 ms). Po kilku R glos uciekalby
    wtedy z bitu o kilkadziesiat milisekund.

    Dlatego:
    - exactCuts: suma zbiorow odcinkow (moga na siebie zachodzic),
    - renderExact: wycina dokladnie te probki; probka zaraz po wycinku trafia
      w pliku na probke editedTime. Przenikanie lezy ZA szwem: koncowka
      wycinka (cisza przed wersem, w niej stoi ciecie) wygasa, a nowe
      czytanie narasta — bez przesuwania czegokolwiek;
    - muteRange: montaz z podkladem nie usuwa, tylko wycisza (dlugosc ta sama).

    Bez JUCE — test: Tests/podklad-montaz-test.cpp.
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace tw
{

struct BackingEdit
{
    using Spans = std::vector<std::pair<double, double>>;   ///< [od, do) w sekundach nagrania

    static constexpr double fadeSec = 0.010;

    /** Odcinki do wyciecia: posortowane, zachodzace i stykajace sie sklejone,
        puste pominiete. */
    static Spans exactCuts (Spans spans)
    {
        std::sort (spans.begin(), spans.end());
        Spans out;
        for (const auto& s : spans)
        {
            if (! (s.second > s.first) || ! std::isfinite (s.first) || ! std::isfinite (s.second)) continue;
            const auto a = std::max (0.0, s.first);
            if (! out.empty() && a <= out.back().second) out.back().second = std::max (out.back().second, s.second);
            else out.push_back ({ a, s.second });
        }
        return out;
    }

    static size_t index (double sec, double sr, size_t n)
    {
        return std::min (n, (size_t) std::max (0.0, (double) std::llround (sec * sr)));
    }

    /** Wycina dokladnie podane odcinki (po exactCuts). Wynik ma
        n - suma(dlugosci) probek; in[b] (pierwsza probka po wycinku) laduje
        w out na indeksie a - (wczesniej wyciete). */
    static std::vector<float> renderExact (const std::vector<float>& in, double sr, const Spans& spansIn)
    {
        const auto spans = exactCuts (spansIn);
        if (spans.empty() || sr <= 0.0) return in;
        const size_t fade = std::max<size_t> (1, (size_t) std::lround (fadeSec * sr));
        std::vector<float> out;
        out.reserve (in.size());
        size_t pos = 0;
        for (const auto& s : spans)
        {
            const size_t a = std::max (pos, index (s.first, sr, in.size()));
            const size_t b = index (s.second, sr, in.size());
            if (b <= a) continue;
            out.insert (out.end(), in.begin() + (long) pos, in.begin() + (long) a);
            // za szwem: koncowka wycinka (in[a..]) wygasa, nowe (in[b..]) narasta
            const size_t m = std::min ({ fade, b - a, in.size() - b });
            for (size_t k = 0; k < m; ++k)
            {
                const double t = (double) (k + 1) / (double) (m + 1);
                out.push_back ((float) (in[a + k] * std::cos (t * M_PI / 2.0) + in[b + k] * std::sin (t * M_PI / 2.0)));
            }
            pos = b + m;
        }
        if (pos < in.size()) out.insert (out.end(), in.begin() + (long) pos, in.end());
        return out;
    }

    /** Wycisza [from, to) z krotkim wygaszeniem i narastaniem na brzegach,
        dlugosc bez zmian. Zwraca false, gdy odcinek jest pusty. */
    static bool muteRange (std::vector<float>& audio, double sr, double from, double to)
    {
        if (sr <= 0.0 || ! std::isfinite (from) || ! std::isfinite (to)) return false;
        const size_t a = index (from, sr, audio.size()), b = index (to, sr, audio.size());
        if (b <= a) return false;
        const size_t fade = std::min ((size_t) std::lround (fadeSec * sr), (b - a) / 2);
        for (size_t i = a; i < b; ++i)
        {
            float g = 0.0f;
            if (i - a < fade)      g = 1.0f - (float) (i - a + 1) / (float) (fade + 1);
            else if (b - i <= fade) g = 1.0f - (float) (b - i) / (float) (fade + 1);
            audio[i] *= g;
        }
        return true;
    }
};

} // namespace tw
