/*
    PrompterLines.h — ktory wers podswietlic i od ktorego zaczac go jeszcze raz.

    Lamanie tekstu na wersy zalezy od pikseli, wiec robi je ekran. Ale to, co
    sie z wersami potem dzieje — ktory jest teraz, dokad cofa "powtorz wers" —
    nie zalezy od niczego poza numerami slow. Stoi tutaj, zeby test mogl to
    sprawdzic bez okna (Tests/powtorz-test.cpp).
*/

#pragma once

#include "PrompterText.h"

#include <vector>

namespace tw
{

struct LineSpan
{
    int firstWord = 0, lastWord = 0;
};

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

} // namespace tw
