/*
    PrompterText.h — tekst rozlozony na sylaby i miejsca do oddechu.

    ---------------------------------------------------------------------------
    PO CO LICZYC SYLABY W TEKSCIE

    Prompter ma sam podazac za glosem. Zeby to zrobic bez rozpoznawania mowy,
    trzeba miec dwie liczby tego samego rodzaju: ile sylab czlowiek juz
    powiedzial (to slychac) i ile sylab ma za soba kazde slowo w tekscie (to
    widac). Dopasowanie jednego do drugiego daje miejsce w tekscie.

    Sylaba jest tu wlasciwa jednostka, nie slowo i nie znak. Slowa maja bardzo
    rozna dlugosc ("w" i "niepodleglosciowego"), a znaki nie maja nic wspolnego
    z czasem mowienia. Sylaba trwa mniej wiecej tyle samo u tej samej osoby —
    i wlasnie ja slychac w sygnale jako szczyt energii samoglosek.

    ---------------------------------------------------------------------------
    ZASADA LICZENIA I JEJ WYJATEK

    W polszczyznie sylab jest tyle, ile samoglosek: a, a,, e, e,, i, o, o,, u, y.
    Jeden wyjatek robi roznice na kazdej stronie tekstu: "i" PRZED inna
    samoglaska nie jest osobna sylaba, tylko zmiekczeniem.

        ziemia      zie-mia         2, nie 3
        piasek      pia-sek         2, nie 3
        kwiat       kwiat           1, nie 2

    Natomiast "i" PO samoglosce jest osobna sylaba:

        moi         mo-i            2
        kraina      kra-i-na        3

    Zbitki "au" i "eu" czyta sie w rodzimych slowach jako dwie sylaby
    ("na-u-ka", "za-u-fa-nie") i tak je liczymy. W zapozyczeniach bywaja jedna
    ("au-to", "Eu-ro-pa") i tam sie mylimy o jedna sylabe — swiadomie, bo lista
    wyjatkow bylaby dluzsza niz pozytek. Blad jednej sylaby kasuje sie na
    najblizszej pauzie, bo tam prompter i tak dosuwa sie do znaku
    interpunkcyjnego.

    ---------------------------------------------------------------------------
    MIEJSCA DO ODDECHU

    Kropka, przecinek, mysln1k i koniec wiersza to miejsca, w ktorych czlowiek
    przestaje mowic. Prompter zapamietuje je osobno, bo to na nich kasuje sie
    nazbierany blad: gdy slychac dluzsza cisze, pozycja dosuwa sie do
    najblizszego takiego miejsca. Bez tego drobne pomylki w liczeniu sumuja sie
    przez cala strone i po dwoch minutach tekst jedzie o wers za daleko.
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace tw
{

//==============================================================================
struct Word
{
    size_t from = 0, to = 0;    ///< bajty w tekscie zrodlowym
    int    syllables = 0;
    int    beforeMe  = 0;       ///< ile sylab przed tym slowem
    int    line      = 0;

    /** Jak dluga pauza wypada PO tym slowie. 0 = zadna. */
    float  breathAfter = 0.0f;
};

//==============================================================================
class PrompterText
{
public:
    //==========================================================================
    void set (const std::string& source)
    {
        text = source;
        words.clear();
        total = 0;

        int line = 0;
        size_t i = 0;

        while (i < text.size())
        {
            // ---- przeskocz to, co nie jest slowem ---------------------------
            while (i < text.size() && ! isWordByte (text, i))
            {
                if (text[i] == '\n') ++line;
                ++i;
            }

            if (i >= text.size()) break;

            const size_t start = i;

            while (i < text.size() && isWordByte (text, i))
                i += byteLen (text, i);

            Word w;
            w.from      = start;
            w.to        = i;
            w.line      = line;
            w.syllables = countSyllables (text.substr (start, i - start));
            w.beforeMe  = total;

            total += w.syllables;

            // ---- co stoi zaraz za slowem ------------------------------------
            size_t j = i;
            float breath = 0.0f;

            while (j < text.size() && ! isWordByte (text, j))
            {
                const char c = text[j];

                if      (c == '.' || c == '!' || c == '?') breath = std::max (breath, 1.0f);
                else if (c == ':' || c == ';')             breath = std::max (breath, 0.8f);
                else if (c == ',')                         breath = std::max (breath, 0.5f);
                else if (c == '\n')                        breath = std::max (breath, 0.6f);

                // mysln1k jako osobne slowo (spacja-kreska-spacja)
                if ((unsigned char) c == 0xE2 && j + 2 < text.size())
                    breath = std::max (breath, 0.5f);

                ++j;
            }

            w.breathAfter = breath;
            words.push_back (w);
        }

        // akapit to mocniejsza pauza niz koniec wiersza
        for (size_t k = 0; k + 1 < words.size(); ++k)
            if (words[k + 1].line - words[k].line >= 2)
                words[k].breathAfter = std::max (words[k].breathAfter, 1.0f);

        if (! words.empty())
            words.back().breathAfter = std::max (words.back().breathAfter, 1.0f);
    }

    //==========================================================================
    int  totalSyllables() const noexcept { return total; }
    int  wordCount()      const noexcept { return (int) words.size(); }
    const std::vector<Word>& all() const noexcept { return words; }
    const std::string& source() const noexcept { return text; }

    std::string wordText (int index) const
    {
        if (index < 0 || index >= (int) words.size()) return {};
        return text.substr (words[(size_t) index].from,
                            words[(size_t) index].to - words[(size_t) index].from);
    }

    /** Ktore slowo wypada po tylu sylabach. */
    int wordAtSyllable (double syllable) const
    {
        if (words.empty()) return 0;

        int lo = 0, hi = (int) words.size() - 1;

        while (lo < hi)
        {
            const int mid = (lo + hi + 1) / 2;
            if (words[(size_t) mid].beforeMe <= syllable) lo = mid; else hi = mid - 1;
        }

        return lo;
    }

    /** Najblizsze miejsce na oddech, nie dalej niz o tyle slow. */
    int nearestBreath (int fromWord, int maxDistance, float minBreath = 0.5f) const
    {
        if (words.empty()) return fromWord;

        for (int d = 0; d <= maxDistance; ++d)
        {
            const int a = fromWord - d;
            const int b = fromWord + d;

            if (a >= 0 && words[(size_t) a].breathAfter >= minBreath) return a;
            if (b < (int) words.size() && words[(size_t) b].breathAfter >= minBreath) return b;
        }

        return -1;
    }

    //==========================================================================
    /** Ile sylab ma to slowo. Publiczne, bo testy pytaja o nie wprost. */
    static int countSyllables (const std::string& word)
    {
        int n = 0;
        size_t i = 0;

        bool prevWasVowel = false;

        while (i < word.size())
        {
            const size_t len = byteLen (word, i);
            const int v = vowelKind (word, i);

            if (v != 0)
            {
                // "i" przed inna samogloska to zmiekczenie, nie sylaba —
                // ale tylko wtedy, gdy przed nim NIE bylo samogloski
                const bool softening = (v == 2)
                                    && ! prevWasVowel
                                    && nextIsVowel (word, i + len);

                if (! softening) ++n;
                prevWasVowel = true;
            }
            else
            {
                prevWasVowel = false;
            }

            i += len;
        }

        // slowo bez samogloski to zwykle skrot albo "w", "z" — jedna sylaba
        // przy czytaniu na glos i tak zajmuje
        return std::max (n, hasLetter (word) ? 1 : 0);
    }

private:
    //==========================================================================
    static size_t byteLen (const std::string& s, size_t i)
    {
        const unsigned char c = (unsigned char) s[i];
        if (c < 0x80) return 1;
        if ((c & 0xE0) == 0xC0) return std::min<size_t> (2, s.size() - i);
        if ((c & 0xF0) == 0xE0) return std::min<size_t> (3, s.size() - i);
        return 1;
    }

    /** 0 = nie samogloska, 1 = samogloska, 2 = "i". */
    static int vowelKind (const std::string& s, size_t i)
    {
        const unsigned char c = (unsigned char) s[i];

        if (c < 0x80)
        {
            switch (std::tolower (c))
            {
                case 'i': return 2;
                case 'a': case 'e': case 'o': case 'u': case 'y': return 1;
                default:  return 0;
            }
        }

        // dwubajtowe polskie znaki: a, (C4 85), e, (C4 99), o, (C3 B3)
        if (i + 1 < s.size())
        {
            const unsigned char d = (unsigned char) s[i + 1];

            if (c == 0xC4 && (d == 0x85 || d == 0x84)) return 1;   // a,
            if (c == 0xC4 && (d == 0x99 || d == 0x98)) return 1;   // e,
            if (c == 0xC3 && (d == 0xB3 || d == 0x93)) return 1;   // o'
        }

        return 0;
    }

    static bool nextIsVowel (const std::string& s, size_t i)
    {
        return i < s.size() && vowelKind (s, i) != 0;
    }

    static bool hasLetter (const std::string& s)
    {
        for (size_t i = 0; i < s.size(); ++i)
            if (std::isalpha ((unsigned char) s[i]) || (unsigned char) s[i] >= 0x80)
                return true;

        return false;
    }

    static bool isWordByte (const std::string& s, size_t i)
    {
        const unsigned char c = (unsigned char) s[i];

        if (c >= 0x80) return true;                    // polskie znaki
        if (std::isalnum (c)) return true;
        if (c == '\'') return true;

        // apostrof i lacznik w srodku slowa
        if (c == '-' && i > 0 && i + 1 < s.size()
            && std::isalnum ((unsigned char) s[i - 1])
            && std::isalnum ((unsigned char) s[i + 1]))
            return true;

        return false;
    }

    std::string text;
    std::vector<Word> words;
    int total = 0;
};

} // namespace tw
