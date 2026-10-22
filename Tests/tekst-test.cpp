/*
    tekst-test.cpp — zapis nagrania wobec tekstu promptera (DSP/ScriptAlign.h).

    Slowa "uslyszane" sa podane recznie (jak z whisper.cpp: tekst i czasy),
    zeby sprawdzic samo dopasowanie: powrot do poczatku zdania, komentarz
    miedzy podejsciami, przejezyczenie, pominiety wers, slowo rozbite przez
    rozpoznawanie na dwa.
*/

#include "../Source/DSP/ScriptAlign.h"

#include <cstdio>
#include <sstream>
#include <string>

static int fails = 0;
static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

/** "ala ma kota" -> slowa co 0,5 s */
static std::vector<tw::HeardWord> said (const std::string& text)
{
    std::vector<tw::HeardWord> out;
    std::istringstream in (text);
    std::string w; double t = 0.0;
    while (in >> w) { out.push_back ({ w, t, t + 0.4 }); t += 0.5; }
    return out;
}

static std::string describe (const std::vector<tw::ScriptFinding>& f)
{
    static const char* names[] = { "powt", "poza", "przej", "pomin" };
    std::string s;
    for (const auto& x : f) s += std::string (names[(int) x.kind]) + "[" + x.heard + "|" + x.expected + "] ";
    return s;
}

static int count (const std::vector<tw::ScriptFinding>& f, tw::ScriptFinding::Kind k)
{
    int n = 0; for (const auto& x : f) n += x.kind == k; return n;
}

int main()
{
    using K = tw::ScriptFinding::Kind;
    std::printf ("\nZapis wobec tekstu\n==================\n\n");
    const std::string text = "Ala ma kota, a kot ma Alę.\nNikt nie wie, dokąd idzie pies.\n#uwaga dla czytajacego\nKoniec tej bajki.";

    {
        const auto f = tw::compareWithScript (said ("Ala ma kota a kot ma Alę nikt nie wie dokąd idzie pies koniec tej bajki"), text);
        check (f.empty(), "czyste czytanie: nic do zgloszenia (linia # pominieta)", describe (f));
    }
    {
        const auto f = tw::compareWithScript (said ("Ala ma kota a kot Ala ma kota a kot ma Alę nikt nie wie dokąd idzie pies koniec tej bajki"), text);
        check (f.size() == 1 && f[0].kind == K::Repeat && f[0].from == 0.0 && f[0].to > 2.0 && f[0].to < 2.5,
               "powrot do poczatku zdania: pierwsze podejscie to powtorzenie", describe (f));
    }
    {
        const auto f = tw::compareWithScript (said ("Ala ma kota a kot ma Alę nikt nie wie no dobra jeszcze raz nikt nie wie dokąd idzie pies koniec tej bajki"), text);
        check (count (f, K::OffScript) == 1 && count (f, K::Repeat) == 1,
               "komentarz miedzy podejsciami: poza tekstem + powtorzenie", describe (f));
        bool comment = false;
        for (const auto& x : f) if (x.kind == K::OffScript) comment = x.heard == "no dobra jeszcze raz";
        check (comment, "komentarz to dokladnie te slowa", describe (f));
    }
    {
        const auto f = tw::compareWithScript (said ("Ala ma kote a kot ma Alę nikt nie wie dokąd idzie pies koniec tej bajki"), text);
        check (f.size() == 1 && f[0].kind == K::Slip && f[0].heard == "kote" && f[0].expected == "kota,",
               "przejezyczenie: kote zamiast kota", describe (f));
    }
    {
        const auto f = tw::compareWithScript (said ("Ala ma kota a kot ma Alę koniec tej bajki"), text);
        check (f.size() == 1 && f[0].kind == K::Skipped && f[0].expected == "Nikt nie wie, dokąd idzie pies.",
               "pominiety wers (czytanie idzie dalej)", describe (f));
    }
    {
        const auto f = tw::compareWithScript (said ("Ala ma kota a kot ma Alę nikt nie wie do kąd idzie pies koniec tej bajki"), text);
        check (f.empty(), "slowo rozbite przez rozpoznawanie (do kad) to nie blad", describe (f));
    }
    {
        const auto f = tw::compareWithScript (said ("ALA MA KOTA, A KOT MA ALĘ! Nikt nie wie — dokąd idzie pies? Koniec tej bajki..."), text);
        check (f.empty(), "wielkie litery, interpunkcja i myslnik nie sa bledami", describe (f));
    }
    {
        const auto f = tw::compareWithScript ({}, text);
        check (f.size() == 1 && f[0].kind == K::Skipped, "pusty zapis: caly tekst pominiety", describe (f));
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
