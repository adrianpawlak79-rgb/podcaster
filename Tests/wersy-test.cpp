/*
    wersy-test.cpp — jak tekst dzieli sie na wersy promptera.

    ---------------------------------------------------------------------------
    SKAD SIE WZIAL TEN TEST

    Ze zrzutu ekranu. Czlowiek przyslal zdjecie dzialajacego promptera i widac
    na nim bylo cos, czego nie zglosil i czego zaden z moich testow nie mogl
    zlapac, bo zaden nie patrzyl na TEKST, tylko na pozycje w nim:

        w pliku:   Mikrofon stoi blisko ust, wiec slychac kazdy oddech.
        na ekranie: Mikrofon stoi blisko ust wiec slychac kazdy oddech

    Zniknely przecinki i kropki. Wers skladalem ze SLOW, a slowa wyciaga sie z
    tekstu po literach — interpunkcja zostawala w zrodle. Dla kogos, kto czyta
    na glos, to jest zabranie polowy partytury: znaki przestankowe sa jedyna
    informacja o tym, gdzie zrobic pauze.

    Drugi blad wynikal z pierwszego: bez interpunkcji nie bylo czego szukac przy
    lamaniu, wiec wersy konczyly sie tam, gdzie konczylo sie miejsce w oknie.

    Ten test pilnuje obu rzeczy naraz.
*/

#include "../Source/DSP/PrompterLines.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got)
{
    std::printf ("  %-5s %-44s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

/** To samo lamanie co na ekranie, tylko z limitem w znakach zamiast w pikselach. */
static std::vector<std::string> breakLines (const tw::PrompterText& t, size_t maxChars)
{
    std::vector<std::string> out;
    const auto& src = t.source();

    int first = 0;

    for (int i = 0; i < t.wordCount(); ++i)
    {
        const auto& w = t.all()[(size_t) i];

        const bool tooWide = (w.to - t.all()[(size_t) first].from) > maxChars;

        if (tw::lineEndsAfter (t, first, i, tooWide))  // ta sama zasada co na ekranie
        {
            std::string line = src.substr (t.all()[(size_t) first].from,
                                           w.to - t.all()[(size_t) first].from);

            size_t tail = w.to;
            while (tail < src.size() && std::strchr (",.!?:;", src[tail]))
                line += src[tail++];

            out.push_back (line);
            first = i + 1;
        }
    }

    return out;
}

static std::string squash (const std::string& s)
{
    std::string out;
    bool space = false;

    for (char c : s)
    {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r') { space = ! out.empty(); continue; }
        if (space) { out += ' '; space = false; }
        out += c;
    }

    return out;
}

int main()
{
    const std::string src =
        "Dzisiaj jest dobry dzień na nagrywanie.\n\n"
        "Mikrofon stoi blisko ust, więc słychać każdy oddech. Czytam spokojnie,\n"
        "nie spiesząc się, i robię pauzę tam, gdzie stoi przecinek.\n\n"
        "Prompter sam podąża za moim głosem. Nie muszę niczego naciskać.\n";

    tw::PrompterText t;
    t.set (src);

    const auto lines = breakLines (t, 46);

    std::printf ("\nWersy promptera\n===============\n\n");

    for (const auto& l : lines) std::printf ("    %s\n", l.c_str());
    std::printf ("\n");

    // ---- 1. nic nie ginie po drodze -------------------------------------------
    {
        std::string joined;
        for (const auto& l : lines) { if (! joined.empty()) joined += ' '; joined += l; }

        check (squash (joined) == squash (src),
               "caly tekst, ze znakami przestankowymi",
               squash (joined) == squash (src) ? "zgadza sie co do znaku" : "ROZNI SIE");
    }

    // ---- 2. wersy koncza sie tam, gdzie bierze sie oddech ---------------------
    {
        int atBreath = 0;

        for (const auto& l : lines)
            if (! l.empty() && std::strchr (",.!?:;", l.back())) ++atBreath;

        const double pct = 100.0 * atBreath / (double) lines.size();

        check (pct >= 80.0, "wers konczy sie na znaku przestankowym",
               std::to_string (atBreath) + " z " + std::to_string (lines.size()));
    }

    // ---- 3. wersy sa krotkie ---------------------------------------------------
    {
        size_t longest = 0;
        for (const auto& l : lines) longest = std::max (longest, l.size());

        check (longest <= 52, "zaden wers nie jest za dlugi",
               std::to_string (longest) + " znakow");
        check (lines.size() >= 8, "tekst rozbity na wiele krotkich wersow",
               std::to_string (lines.size()) + " wersow");
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
