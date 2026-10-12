/*
    lista-ciec-test.cpp — lista cięć po nagraniu (Source/DSP/CutList.h).

    Pewne cięcia od razu do wycięcia, do wskazania czekają na człowieka;
    plik obok nagrania czyta się z powrotem bez strat; wycina się tylko to,
    co zatwierdzone.
*/

#include "../Source/DSP/CutList.h"

#include <cmath>
#include <cstdio>
#include <string>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

int main()
{
    std::printf ("\nLista ciec po nagraniu\n======================\n\n");
    using S = tw::CutProposal::State;

    std::vector<tw::Punch::Verdict> v (3);
    v[0].cut = { 2.0, 4.0, 10, 15 };  v[0].sure = true;
    v[1].cut = { 8.0, 11.0, 20, 25 }; v[1].sure = false; v[1].why = "poprzedni wers mógł jeszcze trwać w miejscu cięcia";
    v[2].cut = { 15.0, 17.0, 30, 35 }; v[2].cut.byHand = true; v[2].sure = true;

    auto list = tw::proposalsFrom (v);
    check (list.size() == 3, "trzy propozycje");
    check (list[0].state == S::Accepted && list[2].state == S::Accepted, "pewne: od razu do wycięcia");
    check (list[1].state == S::Pending && ! list[1].why.empty(), "niepewne: czeka na decyzję, z powodem");
    check (list[2].byHand, "zapamiętane: wskazane strzałkami");

    auto cuts = tw::acceptedCuts (list);
    check (cuts.size() == 2 && cuts[0].from == 2.0 && cuts[1].from == 15.0, "wycina tylko zatwierdzone");

    // człowiek wskazał miejsce na przebiegu i kliknął "Tnij tu"
    list[1].moveTo (11.1, 8.4);
    check (list[1].state == S::Accepted && list[1].from == 8.4 && list[1].to == 11.1, "Tnij tu: nowe granice, w kolejności");

    // "przywróć" przy pewnym
    list[0].state = S::Rejected;
    cuts = tw::acceptedCuts (list);
    check (cuts.size() == 2 && cuts[0].from == 8.4, "przywrócone nie jest wycinane");

    // zapis i odczyt
    const auto again = tw::parseCutList (tw::cutListToText (list));
    bool same = again.size() == list.size();
    for (size_t i = 0; same && i < list.size(); ++i)
        same = std::fabs (again[i].from - list[i].from) < 1e-3 && std::fabs (again[i].to - list[i].to) < 1e-3
            && again[i].state == list[i].state && again[i].sure == list[i].sure && again[i].byHand == list[i].byHand
            && again[i].lineFirst == list[i].lineFirst && again[i].why == list[i].why;
    check (same, "plik obok nagrania czyta się bez strat (z polskimi znakami)");

    // nachodzące zatwierdzone sklejają się
    std::vector<tw::CutProposal> ov (2);
    ov[0].from = 1; ov[0].to = 3; ov[0].state = S::Accepted;
    ov[1].from = 2.5; ov[1].to = 5; ov[1].state = S::Accepted;
    cuts = tw::acceptedCuts (ov);
    check (cuts.size() == 1 && cuts[0].to == 5, "nachodzące cięcia sklejone");

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
