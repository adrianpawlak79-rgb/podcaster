/*
    ScriptNormalize.h — tekst skryptu w postaci, ktora da sie porownac z mowa.

    ---------------------------------------------------------------------------
    SKAD SIE TO WZIELO

    Przy pomiarach Vosk na prawdziwym glosie czesc slow nie trafiala nigdy,
    choc byly wymawiane wyraznie. Przyczyna nie lezala w rozpoznawaniu, tylko
    w tekscie: w skrypcie stalo cyrylickie "е" (U+0435) zamiast lacinskiego
    "e". Na ekranie wygladaja identycznie, dla komputera to dwa rozne znaki —
    wiec "nagrywanie" z cyrylicka "е" nie jest slowem "nagrywanie" i nie ma go
    w slowniku modelu. Tekst wklejany z PDF-ow, stron i komunikatorow niesie
    takie rzeczy czesciej, niz sie wydaje.

    To samo psuje licznik sylab: cyrylickie "е" nie jest dla niego samogloska,
    wiec slowo traci sylabe.

    ---------------------------------------------------------------------------
    DWA POZIOMY

    foldLookalikes() — zamienia znaki udajace lacinke na lacinke i sprzata
    niewidoczne smieci (twarda spacja, miekki lacznik, znaki zerowej
    szerokosci). Tekst nadal wyglada tak samo i tak samo sie go wyswietla.
    PrompterText::set() przepuszcza przez to kazdy skrypt.

    spokenKey() — klucz slowa do porownan z rozpoznana mowa: male litery,
    bez cudzyslowow, myslnikow i kropek. "„Raz," i "raz" to ten sam klucz.
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace tw
{

//==============================================================================
namespace utf8
{
    /** Jeden znak od pozycji i; i przesuwa sie za niego. Bledny bajt -> U+FFFD. */
    inline char32_t next (const std::string& s, size_t& i)
    {
        const unsigned char c = (unsigned char) s[i];

        auto cont = [&] (size_t k) { return i + k < s.size() && (((unsigned char) s[i + k]) & 0xC0) == 0x80; };

        if (c < 0x80) { ++i; return c; }

        if ((c & 0xE0) == 0xC0 && cont (1))
        {
            const char32_t cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
            i += 2; return cp;
        }

        if ((c & 0xF0) == 0xE0 && cont (1) && cont (2))
        {
            const char32_t cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
            i += 3; return cp;
        }

        if ((c & 0xF8) == 0xF0 && cont (1) && cont (2) && cont (3))
        {
            const char32_t cp = ((c & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12)
                              | ((s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F);
            i += 4; return cp;
        }

        ++i;
        return 0xFFFD;
    }

    inline void put (std::string& out, char32_t cp)
    {
        if (cp < 0x80) out += (char) cp;
        else if (cp < 0x800) { out += (char) (0xC0 | (cp >> 6)); out += (char) (0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000)
        {
            out += (char) (0xE0 | (cp >> 12));
            out += (char) (0x80 | ((cp >> 6) & 0x3F));
            out += (char) (0x80 | (cp & 0x3F));
        }
        else
        {
            out += (char) (0xF0 | (cp >> 18));
            out += (char) (0x80 | ((cp >> 12) & 0x3F));
            out += (char) (0x80 | ((cp >> 6) & 0x3F));
            out += (char) (0x80 | (cp & 0x3F));
        }
    }

    inline std::vector<char32_t> decode (const std::string& s)
    {
        std::vector<char32_t> out;
        for (size_t i = 0; i < s.size();) out.push_back (next (s, i));
        return out;
    }
}

//==============================================================================
/** Znak udajacy lacinke -> lacinka. 0 = usunac. Inaczej bez zmian. */
inline char32_t foldLookalike (char32_t c)
{
    switch (c)
    {
        // cyrylica, ktora na ekranie wyglada jak lacinka
        case 0x0430: return U'a';  case 0x0410: return U'A';
        case 0x0435: return U'e';  case 0x0415: return U'E';
        case 0x043E: return U'o';  case 0x041E: return U'O';
        case 0x0440: return U'p';  case 0x0420: return U'P';
        case 0x0441: return U'c';  case 0x0421: return U'C';
        case 0x0443: return U'y';  case 0x0423: return U'Y';
        case 0x0445: return U'x';  case 0x0425: return U'X';
        case 0x0456: return U'i';  case 0x0406: return U'I';
        case 0x0458: return U'j';  case 0x0408: return U'J';
        case 0x0455: return U's';  case 0x0405: return U'S';
        case 0x0412: return U'B';  case 0x041A: return U'K';
        case 0x041C: return U'M';  case 0x041D: return U'H';
        case 0x0422: return U'T';  case 0x043A: return U'k';

        // greka, z tego samego powodu
        case 0x03BF: return U'o';  case 0x039F: return U'O';
        case 0x0391: return U'A';  case 0x0395: return U'E';

        // niewidoczne i "prawie spacje"
        case 0x00A0: case 0x2007: case 0x202F: case 0x2009: case 0x200A: return U' ';
        case 0x00AD: case 0x200B: case 0x200C: case 0x200D: case 0x2060: case 0xFEFF: return 0;

        // apostrof drukarski -> zwykly
        case 0x2019: case 0x02BC: return U'\'';

        default: return c;
    }
}

/** Tekst po zamianie znakow-udawaczy. Wyglada tak samo, liczy sie poprawnie. */
inline std::string foldLookalikes (const std::string& s)
{
    std::string out;
    out.reserve (s.size());

    for (size_t i = 0; i < s.size();)
    {
        const char32_t c = foldLookalike (utf8::next (s, i));
        if (c != 0) utf8::put (out, c);
    }

    return out;
}

//==============================================================================
inline bool isPolishLetter (char32_t c)
{
    if ((c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z')) return true;

    switch (c)
    {
        case U'ą': case U'ć': case U'ę': case U'ł': case U'ń': case U'ó': case U'ś': case U'ź': case U'ż':
        case U'Ą': case U'Ć': case U'Ę': case U'Ł': case U'Ń': case U'Ó': case U'Ś': case U'Ź': case U'Ż':
            return true;
        default:
            return false;
    }
}

inline char32_t lowerPolish (char32_t c)
{
    if (c >= U'A' && c <= U'Z') return c + 32;

    switch (c)
    {
        case U'Ą': return U'ą'; case U'Ć': return U'ć'; case U'Ę': return U'ę';
        case U'Ł': return U'ł'; case U'Ń': return U'ń'; case U'Ó': return U'ó';
        case U'Ś': return U'ś'; case U'Ź': return U'ź'; case U'Ż': return U'ż';
        default:   return c;
    }
}

/** Klucz slowa do porownan z mowa: male polskie litery i cyfry, nic wiecej.
    Pusty, gdy w slowie nie ma ani litery, ani cyfry (np. samotny myslnik). */
inline std::string spokenKey (const std::string& word)
{
    std::string out;

    for (size_t i = 0; i < word.size();)
    {
        const char32_t c = foldLookalike (utf8::next (word, i));

        if (isPolishLetter (c))           utf8::put (out, lowerPolish (c));
        else if (c >= U'0' && c <= U'9')  out += (char) c;
    }

    return out;
}

/** Czy klucz zawiera cyfry — takich slow model nie wymowi tak, jak stoja
    ("2026"), wiec nie trafiaja do slownika i nie sa szukane w mowie. */
inline bool hasDigits (const std::string& key)
{
    for (char c : key) if (c >= '0' && c <= '9') return true;
    return false;
}

/** Podobienstwo dwoch kluczy, 0..1: 1 - odleglosc_edycyjna / dluzsze_slowo,
    liczone na znakach, nie bajtach ("ł" to jeden znak). */
inline double similarity (const std::string& a, const std::string& b)
{
    const auto x = utf8::decode (a), y = utf8::decode (b);
    if (x.empty() && y.empty()) return 1.0;

    std::vector<size_t> row (y.size() + 1);
    for (size_t j = 0; j <= y.size(); ++j) row[j] = j;

    for (size_t i = 1; i <= x.size(); ++i)
    {
        size_t diag = row[0];
        row[0] = i;

        for (size_t j = 1; j <= y.size(); ++j)
        {
            const size_t up = row[j];
            row[j] = std::min ({ row[j] + 1, row[j - 1] + 1, diag + (x[i - 1] == y[j - 1] ? 0u : 1u) });
            diag = up;
        }
    }

    const size_t longer = std::max (x.size(), y.size());
    return 1.0 - (double) row[y.size()] / (double) longer;
}

inline size_t charCount (const std::string& s)
{
    size_t n = 0;
    for (size_t i = 0; i < s.size();) { utf8::next (s, i); ++n; }
    return n;
}

} // namespace tw
