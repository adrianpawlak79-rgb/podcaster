/*
    znaczki-test.cpp — ✓ / ! / ✗ przy wersach (Source/DSP/LineGrade.h).

    Zdarzenia lampki z czasem + zapis wersow -> znaczek. Najwazniejsze:
    przester na granicy dwoch wersow, pomylka tuz przed R (nie moze zabarwic
    powtorki) i to, ze po R znaczki od punktu cofniecia znikaja.
*/

#include "../Source/DSP/LineGrade.h"

#include <cstdio>
#include <string>
#include <vector>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static tw::LineMark mark (int line, double sec, tw::LineSource s = tw::LineSource::Words)
{
    tw::LineMark m; m.firstWord = line * 6; m.lastWord = line * 6 + 5; m.shown = line; m.sec = sec; m.source = s;
    return m;
}

static tw::Event ev (tw::Kind k, double t, tw::Severity s = tw::Severity::Redo)
{
    tw::Event e; e.kind = k; e.timeSec = t; e.severity = s; return e;
}

static const char* name (tw::LineGrade g)
{
    return g == tw::LineGrade::Good ? "ok" : g == tw::LineGrade::Fixable ? "!" : g == tw::LineGrade::Redo ? "X" : "-";
}

static tw::LineGrade at (const std::map<int, tw::LineGrade>& m, int line)
{
    const auto it = m.find (line * 6);
    return it == m.end() ? tw::LineGrade::None : it->second;
}

int main()
{
    std::printf ("\nZnaczki przy wersach\n====================\n\n");
    tw::LineGrader grader;

    // wersy zaczynaja sie (naprawde) o 1, 4, 7, 10 s; rozpoznawanie podaje +0,78 s
    const std::vector<tw::LineMark> marks { mark (0, 1.78), mark (1, 4.78), mark (2, 7.78), mark (3, 10.78) };

    // ---- 1. podstawy -----------------------------------------------------------------
    std::printf ("  1. podstawy\n");
    {
        const auto g = grader.grade (marks, {});
        check (at (g, 0) == tw::LineGrade::Good && at (g, 2) == tw::LineGrade::Good, "bez zdarzen: przeczytane wersy maja ok");
        check (at (g, 3) == tw::LineGrade::None, "wers czytany teraz: jeszcze bez znaczka", name (at (g, 3)));
        check (at (grader.grade (marks, {}, true), 3) == tw::LineGrade::Good, "po zatrzymaniu ostatni tez dostaje ok");

        const auto p = grader.grade (marks, { ev (tw::Kind::Plosive, 5.5) });
        check (at (p, 1) == tw::LineGrade::Fixable && at (p, 0) == tw::LineGrade::Good, "plozja: ! tylko w swoim wersie");

        const auto c = grader.grade (marks, { ev (tw::Kind::Clipping, 8.2, tw::Severity::Stop) });
        check (at (c, 2) == tw::LineGrade::Redo, "przester: X", name (at (c, 2)));

        const auto both = grader.grade (marks, { ev (tw::Kind::Plosive, 8.0), ev (tw::Kind::Clipping, 8.5, tw::Severity::Stop) });
        check (at (both, 2) == tw::LineGrade::Redo, "X wygrywa z !");

        const auto later = grader.grade (marks, { ev (tw::Kind::MouthClick, 5.0, tw::Severity::Later) });
        check (at (later, 1) == tw::LineGrade::Good, "to, czego lampka nie zglasza, nie psuje znaczka");

        const auto now = grader.grade (marks, { ev (tw::Kind::Clipping, 11.0, tw::Severity::Stop) });
        check (at (now, 3) == tw::LineGrade::Redo, "przester w czytanym wersie: X od razu (podpowiedz R)");
    }

    // ---- 2. granica dwoch wersow ----------------------------------------------------------
    std::printf ("\n  2. granica\n");
    {
        const auto g = grader.grade (marks, { ev (tw::Kind::Clipping, 6.9, tw::Severity::Stop) });
        check (at (g, 1) == tw::LineGrade::Redo && at (g, 2) == tw::LineGrade::Redo,
               "przester 0,1 s przed granica: X w obu", std::string (name (at (g, 1))) + " " + name (at (g, 2)));

        const auto p = grader.grade (marks, { ev (tw::Kind::Plosive, 6.9) });
        check (at (p, 1) == tw::LineGrade::Fixable && at (p, 2) == tw::LineGrade::Good,
               "plozja przy granicy: ! tylko w jednym", std::string (name (at (p, 1))) + " " + name (at (p, 2)));

        const auto far = grader.grade (marks, { ev (tw::Kind::Clipping, 6.0, tw::Severity::Stop) });
        check (at (far, 2) == tw::LineGrade::Good, "przester 1 s przed granica: nastepny wers czysty");
    }

    // ---- 3. R ------------------------------------------------------------------------
    std::printf ("\n  3. po R\n");
    {
        // wers 2: przester w 8,5 s, R w 8,7 s, czytanie od nowa; potem wers 3
        std::vector<tw::LineMark> m { mark (0, 1.78), mark (1, 4.78), mark (2, 7.78),
                                      mark (2, 8.7, tw::LineSource::Retake), mark (3, 13.78) };
        const std::vector<tw::Event> e { ev (tw::Kind::Clipping, 8.5, tw::Severity::Stop) };
        const auto g = grader.grade (m, e);
        check (at (g, 2) == tw::LineGrade::Good, "pomylka tuz przed R nie barwi powtorki", name (at (g, 2)));
        check (at (g, 1) == tw::LineGrade::Good, "poprzedni wers nietkniety");

        // R na wersie 1, gdy czytany byl juz wers 2: wers 2 traci znaczek
        std::vector<tw::LineMark> back { mark (0, 1.78), mark (1, 4.78), mark (2, 7.78),
                                         mark (1, 9.0, tw::LineSource::Retake) };
        const auto b = grader.grade (back, {});
        check (at (b, 2) == tw::LineGrade::None, "po cofnieciu wersy dalej traca znaczki", name (at (b, 2)));
        check (at (b, 1) == tw::LineGrade::None, "wers powtarzany teraz: bez znaczka", name (at (b, 1)));
        check (at (b, 0) == tw::LineGrade::Good, "wersy przed cofnieciem zostaja");
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
