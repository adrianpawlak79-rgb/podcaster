/*
    TakeJournal.h — dziennik nagrania z tekstem, na wypadek awarii.

    Audyt 14.10 (krytyczne): "Surowy WAV nie jest pelnym mechanizmem
    odzyskiwania sesji. Trzeba odzyskac tekst, pozycje, powtorki." Po awarii
    programu WAV wracal (WavRepair.h), ale znaczniki R ("pomylilem sie,
    powtarzam"), poczatki wersow i to, co uslyszalo rozpoznawanie mowy, zyly
    tylko w pamieci promptera. Odzyskane nagranie mialo wiec w srodku wszystkie
    pomylki i zadnej propozycji ciecia.

    Prompter zapisuje ten dziennik obok nagrania co kilka sekund w trakcie
    nagrywania (<nagranie>.dziennik.txt) i usuwa go po zwyklym zatrzymaniu.
    Przy starcie po awarii z dziennika powstaja propozycje ciec po R (ekran
    Montaz) i wiadomo, na ktorym wersie czytanie sie urwalo.

    Format: tekstowy, czytelny. Wiersze znacznikow jak w LineLog::toText()
    (wiec ten sam parser), "#slowo<TAB>czas<TAB>numer" dla uslyszanych slow,
    a po wierszu "#tekst" — caly tekst promptera do konca pliku.

    Test: Tests/dziennik-test.cpp.
*/

#pragma once

#include "LineLog.h"
#include "Punch.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace tw
{

struct TakeJournal
{
    std::vector<LineMark>  marks;
    std::vector<HeardWord> heard;
    std::string            script;   ///< tekst promptera (UTF-8)

    std::string toText() const
    {
        LineLog log;
        log.begin();
        for (const auto& m : marks) log.restart (m, m.source);

        std::string s = "# Zielone swiatlo - dziennik nagrania (odzyskiwanie po awarii)\n";
        s += log.toText();

        // miejsca ciec po R (pre-roll, strzalki, wznowienie) — LineLog::toText ich nie zapisuje
        char b[96];
        for (size_t i = 0; i < marks.size(); ++i)
        {
            const auto& m = marks[i];
            if (m.cutFrom < 0.0 && m.cutTo < 0.0 && ! m.cutByHand) continue;
            std::snprintf (b, sizeof (b), "#ciecie\t%d\t%.3f\t%d\t%.3f\n", (int) i, m.cutFrom, m.cutByHand ? 1 : 0, m.cutTo);
            s += b;
        }
        for (const auto& h : heard)
        {
            std::snprintf (b, sizeof (b), "#slowo\t%.3f\t%d\n", h.sec, h.word);
            s += b;
        }
        s += "#tekst\n";
        s += script;
        return s;
    }

    static TakeJournal parse (const std::string& text)
    {
        TakeJournal j;
        const std::string tag = "#tekst\n";
        size_t cut = text.find ("\n" + tag);
        std::string head;
        if (cut != std::string::npos) { head = text.substr (0, cut + 1); j.script = text.substr (cut + 1 + tag.size()); }
        else if (text.compare (0, tag.size(), tag) == 0) j.script = text.substr (tag.size());
        else head = text;

        j.marks = LineLog::parse (head);

        size_t from = 0;
        while (from < head.size())
        {
            size_t end = head.find ('\n', from);
            if (end == std::string::npos) end = head.size();
            const std::string line = head.substr (from, end - from);
            from = end + 1;
            if (line.compare (0, 8, "#ciecie\t") == 0)
            {
                int i = -1, hand = 0; double a = -1.0, z = -1.0;
                if (std::sscanf (line.c_str() + 8, "%d\t%lf\t%d\t%lf", &i, &a, &hand, &z) == 4
                    && i >= 0 && i < (int) j.marks.size())
                {
                    j.marks[(size_t) i].cutFrom = a; j.marks[(size_t) i].cutByHand = hand != 0; j.marks[(size_t) i].cutTo = z;
                }
                continue;
            }
            if (line.compare (0, 7, "#slowo\t") != 0) continue;
            const size_t tab = line.find ('\t', 7);
            if (tab == std::string::npos) continue;
            HeardWord h;
            h.sec  = std::atof (line.substr (7, tab - 7).c_str());
            h.word = std::atoi (line.substr (tab + 1).c_str());
            j.heard.push_back (h);
        }
        return j;
    }

    /** Ostatni wers, od ktorego zaczeto czytac (do zdania "czytales: ..."). */
    const LineMark* lastLine() const { return marks.empty() ? nullptr : &marks.back(); }

    bool anyRetake() const
    {
        for (const auto& m : marks) if (m.source == LineSource::Retake) return true;
        return false;
    }
};

} // namespace tw
