/*
    CutList.h — lista cięć po nagraniu: co program proponuje wyciąć, a co
    postanowił człowiek.

    Pomysł Adriana (12.10): automat tnie sam tylko to, czego jest pewien;
    gdy jest niepewny, pokazuje miejsce człowiekowi, który je wskazuje na
    nagraniu (jak punkt wejścia w studyjnym punch-in). Nagranie zostaje
    nietknięte, dopóki człowiek nie zatwierdzi listy — montaż niedestrukcyjny.

    Lista leży obok nagrania jako `sesja_<data>.ciecia.tsv` (zapisuje ją
    PrompterComponent::finishTake). Czyta ją ekran Odsłuch: pewne cięcia są
    od razu zaznaczone do wycięcia (z „posłuchaj szwu” i „przywróć”), do
    wskazania czekają na decyzję. Wycina dopiero zs::applyCutList (PunchFile.h).

    Bez JUCE — test: Tests/lista-ciec-test.cpp.
*/

#pragma once

#include "Punch.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace tw
{

struct CutProposal
{
    enum class State { Pending, Accepted, Rejected };

    double from = 0.0, to = 0.0;   ///< sekundy w nagraniu (surowym)
    int    lineFirst = -1;         ///< pierwsze słowo powtarzanego wersu (-1 = nie wiadomo)
    bool   sure   = false;         ///< automat pewny (Punch::review)
    bool   byHand = false;         ///< początek wskazany strzałkami w pre-rollu
    std::string why;               ///< dlaczego do wskazania (UTF-8); pusty przy pewnych
    State  state = State::Pending; ///< pewne: Accepted od razu; do wskazania: Pending

    /** Człowiek przesunął granicę na przebiegu ("Tnij tu"). */
    void moveTo (double newFrom, double newTo)
    {
        from = std::min (newFrom, newTo);
        to   = std::max (newFrom, newTo);
        state = State::Accepted;
    }
};

/** Z werdyktów Punch::review: pewne — do wycięcia, reszta — do decyzji. */
inline std::vector<CutProposal> proposalsFrom (const std::vector<Punch::Verdict>& verdicts)
{
    std::vector<CutProposal> out;
    for (const auto& v : verdicts)
    {
        CutProposal p;
        p.from = v.cut.from; p.to = v.cut.to; p.lineFirst = v.cut.lineFirst;
        p.sure = v.sure; p.byHand = v.cut.byHand;
        p.why = v.sure ? std::string() : v.why;
        p.state = v.sure ? CutProposal::State::Accepted : CutProposal::State::Pending;
        out.push_back (p);
    }
    return out;
}

/** Cięcia do wykonania: tylko zatwierdzone, rosnąco, nachodzące sklejone. */
inline std::vector<Removal> acceptedCuts (const std::vector<CutProposal>& list)
{
    std::vector<Removal> r;
    for (const auto& p : list)
        if (p.state == CutProposal::State::Accepted && p.to > p.from)
            r.push_back ({ p.from, p.to, p.lineFirst });
    std::sort (r.begin(), r.end(), [] (const Removal& a, const Removal& b) { return a.from < b.from; });
    std::vector<Removal> merged;
    for (const auto& x : r)
    {
        if (! merged.empty() && x.from <= merged.back().to) merged.back().to = std::max (merged.back().to, x.to);
        else merged.push_back (x);
    }
    return merged;
}

inline const char* stateName (CutProposal::State s)
{
    return s == CutProposal::State::Accepted ? "wytnij" : s == CutProposal::State::Rejected ? "zostaw" : "do_decyzji";
}

/** Plik obok nagrania. Kolumny oddzielone tabulatorem (da się otworzyć w arkuszu). */
inline std::string cutListToText (const std::vector<CutProposal>& list)
{
    std::string s = "# Zielone swiatlo - ciecia po nagraniu (CutList.h)\n"
                    "# od_s\tdo_s\tslowo_wersu\tpewne\tstrzalki\tstan\tdlaczego\n";
    char b[128];
    for (const auto& p : list)
    {
        std::snprintf (b, sizeof (b), "%.3f\t%.3f\t%d\t%d\t%d\t%s\t", p.from, p.to, p.lineFirst,
                       p.sure ? 1 : 0, p.byHand ? 1 : 0, stateName (p.state));
        std::string why = p.why;
        std::replace (why.begin(), why.end(), '\t', ' ');
        std::replace (why.begin(), why.end(), '\n', ' ');
        s += b + why + "\n";
    }
    return s;
}

inline std::vector<CutProposal> parseCutList (const std::string& text)
{
    std::vector<CutProposal> out;
    std::istringstream in (text);
    for (std::string line; std::getline (in, line);)
    {
        if (! line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        std::vector<std::string> c;
        size_t from = 0;
        for (int i = 0; i < 6; ++i)
        {
            const size_t tab = line.find ('\t', from);
            if (tab == std::string::npos) break;
            c.push_back (line.substr (from, tab - from));
            from = tab + 1;
        }
        if (c.size() < 6) continue;

        CutProposal p;
        p.from = std::atof (c[0].c_str());
        p.to   = std::atof (c[1].c_str());
        p.lineFirst = std::atoi (c[2].c_str());
        p.sure   = c[3] == "1";
        p.byHand = c[4] == "1";
        p.state  = c[5] == "wytnij" ? CutProposal::State::Accepted
                 : c[5] == "zostaw" ? CutProposal::State::Rejected : CutProposal::State::Pending;
        p.why = line.substr (from);
        out.push_back (p);
    }
    return out;
}

} // namespace tw
