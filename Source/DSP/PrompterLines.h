/*
    PrompterLines.h — ktory wers podswietlic i od ktorego zaczac go jeszcze raz.

    Lamanie tekstu na wersy zalezy od pikseli, wiec robi je ekran. Ale to, co
    sie z wersami potem dzieje — ktory jest teraz, dokad cofa "powtorz wers" —
    nie zalezy od niczego poza numerami slow. Stoi tutaj, zeby test mogl to
    sprawdzic bez okna (Tests/powtorz-test.cpp).
*/

#pragma once

#include "PrompterText.h"
#include "ScriptNormalize.h"

#include <cstdlib>

#include <vector>

namespace tw
{

struct LineSpan
{
    int firstWord = 0, lastWord = 0;
};

/** Czy po slowie i konczy sie wers, ktory zaczal sie od slowa first.

    Jedna zasada dla ekranu (PrompterComponent::rebuildLines), testu
    (wersy-test) i narzedzi (zs-symulacja, zs-sledzenie): koniec wiersza
    zrodla, koniec tekstu, oddech (przecinek i mocniej) po co najmniej trzech
    slowach — albo brak miejsca w oknie (tooWide liczy ekran). */
inline bool lineEndsAfter (const PrompterText& t, int first, int i, bool tooWide = false)
{
    const auto& w = t.all()[(size_t) i];
    const bool last = i == t.wordCount() - 1;
    const bool newSourceLine = ! last && t.all()[(size_t) (i + 1)].line != w.line;
    const bool breathes = w.breathAfter >= 0.5f && i - first + 1 >= 3;
    return last || newSourceLine || tooWide || breathes;
}

/** Wersy bez okna — tak, jak w szerokim oknie (narzedzia, symulacja). */
inline std::vector<LineSpan> lineSpans (const PrompterText& t)
{
    std::vector<LineSpan> out;
    for (int i = 0, first = 0; i < t.wordCount(); ++i)
        if (lineEndsAfter (t, first, i)) { out.push_back ({ first, i }); first = i + 1; }
    return out;
}

/** Wersy, ktore zaczynaja sie podobnie jak inny wers w promieniu `near`
    wersow (to samo pierwsze slowo albo dwa z trzech pierwszych) — rozpoznawanie mowy
    myli je ("Patrz, do moich..." / "Patrz, jak widzac..."). Zwraca
    pierwsze slowa takich wersow (Punch::review). */
inline std::vector<int> similarlyStartingLines (const PrompterText& t, const std::vector<LineSpan>& lines, int near = 4)
{
    auto key = [&] (int w) { return spokenKey (t.wordText (w)); };
    std::vector<int> out;
    for (size_t i = 0; i < lines.size(); ++i)
        for (size_t j = 0; j < lines.size(); ++j)
        {
            if (i == j || std::abs ((int) i - (int) j) > near) continue;
            int same = 0;
            for (int k = 0; k < 3; ++k)
            {
                const int a = lines[i].firstWord + k, b = lines[j].firstWord + k;
                if (a > lines[i].lastWord || b > lines[j].lastWord) break;
                if (key (a) == key (b)) ++same;
            }
            // to samo PIERWSZE slowo wystarczy: "Spadly kwiaty..." / "Spadly liscie..."
            // rozpoznawanie pomylilo w probie na prawdziwym glosie (Krasicki)
            if (same >= 2 || key (lines[i].firstWord) == key (lines[j].firstWord)) { out.push_back (lines[i].firstWord); break; }
        }
    return out;
}

/** Wers, w ktorym lezy slowo. Poza tekstem: pierwszy albo ostatni. */
inline int lineOfWord (const std::vector<LineSpan>& lines, int word)
{
    if (lines.empty()) return 0;

    for (size_t i = 0; i < lines.size(); ++i)
        if (word >= lines[i].firstWord && word <= lines[i].lastWord)
            return (int) i;

    return word < lines.front().firstWord ? 0 : (int) lines.size() - 1;
}

/** Ktory wers podswietlic.

    Nie "ten, w ktorym stoi biezace slowo". Wers zmienia sie wczesniej — gdy
    usta doszly do ostatniej cwiartki wersu, bo oczy sa wtedy juz w nastepnym.
    Prog jest ulamkiem DLUGOSCI WERSU, a nie stala liczba slow: wers z dwoch
    slow i wers z osmiu potrzebuja innego wyprzedzenia. */
inline int lineToShow (const std::vector<LineSpan>& lines, const PrompterText& text, double syllablePos)
{
    if (lines.empty()) return 0;

    for (int i = 0; i < (int) lines.size(); ++i)
    {
        const auto& a = text.all()[(size_t) lines[(size_t) i].firstWord];
        const auto& b = text.all()[(size_t) lines[(size_t) i].lastWord];

        const double from = a.beforeMe;
        const double to   = b.beforeMe + b.syllables;
        const double lead = (to - from) * 0.25;

        if (syllablePos < to - lead) return i;
    }

    return (int) lines.size() - 1;
}

/** Od ktorego slowa zaczac, gdy czlowiek nacisnal "powtorz wers".

    Liczy sie wers, ktory jest w USTACH, nie ten podswietlony. Podswietlenie
    wyprzedza o cwiartke wersu (patrz lineToShow), a pomylka zdarza sie w
    tym, co sie wlasnie wymawia. Gdyby R cofal do podswietlonego wersu, pod
    koniec wersu cofalby... do przodu — na poczatek nastepnego. */
inline int repeatLineStart (const std::vector<LineSpan>& lines, int spokenWord)
{
    if (lines.empty()) return 0;
    return lines[(size_t) lineOfWord (lines, spokenWord)].firstWord;
}

/** Pozycja w slowach (ulamkowa, z WordTracker) -> pozycja w sylabach, czyli
    jednostka, w ktorej mysli reszta promptera. Wewnatrz slowa liniowo. */
inline double syllableAtWord (const PrompterText& text, double wordPos)
{
    if (text.wordCount() == 0) return 0.0;

    const int w = std::clamp ((int) wordPos, 0, text.wordCount() - 1);
    const double frac = std::clamp (wordPos - w, 0.0, 1.0);
    const auto& word = text.all()[(size_t) w];

    return word.beforeMe + frac * word.syllables;
}

} // namespace tw
