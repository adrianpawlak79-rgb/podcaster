/*
    LineGrade.h — znaczek przy kazdym przeczytanym wersie: ✓, ! albo ✗.

    ---------------------------------------------------------------------------
    PO CO

    Pomysl Adriana (Tools/ZADANIA.md, zad. 4): przy wersach juz przeczytanych
    maly znak, zeby od razu bylo wiadomo, czy jechac dalej, czy wcisnac R.

      ✓  w czasie tego wersu lampka nic nie zglosila;
      !  cos bylo, ale miks to poprawi (plozja, mlask, odsuniecie…);
      ✗  rzecz nie do naprawienia (przester, brak sygnalu) — R.

    Podzial "naprawialne / nie" jest ten sam co w ocenie po nagraniu
    (TakeReview.h, fixableInMix), wiec znaczek w prompterze i werdykt na
    ekranie Odsluch mowia to samo.

    "Dobrze" znaczy TECHNICZNIE dobrze, nie "ladnie przeczytane".

    ---------------------------------------------------------------------------
    KTORY WERS DOSTAJE ZDARZENIE

    Wers trwa od swojego poczatku do poczatku nastepnego zapisu w LineLog.
    Poczatek z rozpoznawania mowy jest SPOZNIONY (mediana +0,78 s, pomiar w
    CutPoint.h), wiec przesuwamy go o tyle wstecz; R i strzalki sa dokladne.

    Zdarzenie blizej niz edgeSec od granicy dwoch wersow: przy ✗ dostaja je
    OBA wersy. Lepiej raz za duzo powtorzyc wers niz zostawic przester w
    nagraniu, bo program nie byl pewien, gdzie wypadl. Przy ! nie — to nic
    nie kosztuje, a dwa wykrzykniki za jedna plozje to szum.

    Po R wersy od punktu cofniecia traca znaczki (finalLines: to, co bylo po
    nich, przestaje sie liczyc) i dostaja je od nowa przy nastepnym czytaniu.
*/

#pragma once

#include "LineLog.h"
#include "TakeReview.h"

#include <algorithm>
#include <map>
#include <vector>

namespace tw
{

enum class LineGrade { None = 0, Good, Fixable, Redo };

class LineGrader
{
public:
    double recogniserLagSec = 0.78;   ///< spoznienie poczatku z rozpoznawania (CutPoint.h)
    double edgeSec          = 0.30;

    /** firstWord wersu -> znaczek. Wers jeszcze czytany (ostatni zapis)
        dostaje znaczek tylko, gdy cos w nim JUZ bylo: ✓ dopiero po nim —
        chyba ze nagranie sie skonczylo (finished). */
    std::map<int, LineGrade> grade (const std::vector<LineMark>& all,
                                    const std::vector<Event>& events,
                                    bool finished = false) const
    {
        std::map<int, LineGrade> out;
        if (all.empty()) return out;

        LineLog log;
        log.begin();
        for (const auto& m : all) log.restart (m, m.source);
        const auto kept = log.finalLines();

        // kazdy wers, ktory zostaje: przedzial od jego zapisu do nastepnego zapisu
        for (const auto& k : kept)
        {
            size_t i = 0;
            for (size_t j = all.size(); j-- > 0;)
                if (all[j].firstWord == k.firstWord && all[j].sec == k.sec && all[j].source == k.source) { i = j; break; }

            const double from = startOf (all[i]);
            const bool   last = i + 1 >= all.size();
            const double to   = last ? 1.0e12 : startOf (all[i + 1]);

            LineGrade g = last && ! finished ? LineGrade::None : LineGrade::Good;

            // Wstecz za poczatek po R nie siegamy: tuz przed R jest zwykle
            // wlasnie ta pomylka, ktora R wycina.
            const bool exactStart = all[i].source == LineSource::Retake || all[i].source == LineSource::Key;

            for (const auto& e : events)
            {
                if (e.severity < Severity::Redo) continue;          // lampka tego nie zglasza
                const bool fixable = zs::fixableInMix (e.kind);
                const double edge = fixable ? 0.0 : edgeSec;
                if (e.timeSec < from - (exactStart ? 0.0 : edge) || e.timeSec >= to + edge) continue;

                const auto now = fixable ? LineGrade::Fixable : LineGrade::Redo;
                if (now > g) g = now;
            }

            if (g != LineGrade::None) out[k.firstWord] = g;
        }
        return out;
    }

private:
    double startOf (const LineMark& m) const
    {
        const bool late = m.source == LineSource::Words || m.source == LineSource::Syllables;
        return late ? std::max (0.0, m.sec - recogniserLagSec) : m.sec;
    }
};

} // namespace tw
