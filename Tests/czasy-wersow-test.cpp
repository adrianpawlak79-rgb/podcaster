/*
    czasy-wersow-test.cpp — czy program wie, kiedy zaczal sie ktory wers.

    Pierwszy krok punch and roll (Tools/ZADANIA.md, zad. 4). Sprawdzamy
    zasady z Source/DSP/LineLog.h na wymyslonym przebiegu czytania:
    zwykle czytanie, drgniecie sledzenia, powtorka klawiszem R, cofniecie
    o kilka wersow, zapis do pliku i odczyt.
*/

#include "../Source/DSP/LineLog.h"

#include <cstdio>
#include <string>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-56s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

// wers n: slowa od 4n do 4n+3, na ekranie numer n
static tw::LineMark at (int n, double sec, tw::LineSource s = tw::LineSource::Words)
{
    tw::LineMark m;
    m.sec = sec; m.firstWord = 4 * n; m.lastWord = 4 * n + 3; m.shown = n; m.source = s;
    m.text = "Wers " + std::to_string (n) + " — źdźbło";
    return m;
}

static std::string list (const std::vector<tw::LineMark>& v)
{
    std::string s;
    for (const auto& m : v) s += std::to_string (m.shown) + "@" + std::to_string ((int) (m.sec * 10)) + " ";
    return s;
}

int main()
{
    std::printf ("\nCzasy wersow\n============\n\n");

    // ---- 1. zwykle czytanie -----------------------------------------------------
    {
        tw::LineLog log; log.begin();
        for (int n = 0; n < 4; ++n)
            for (int k = 0; k < 5; ++k) log.observe (at (n, n * 3.0 + k * 0.1));   // 5 klatek na wers

        check (log.marks().size() == 4, "kazdy wers zapisany raz, mimo wielu klatek",
               std::to_string (log.marks().size()));
        check (log.marks()[2].sec == 6.0, "czas wersu = pierwsza klatka, w ktorej sie pokazal");
    }

    // ---- 2. drgniecie sledzenia ---------------------------------------------------
    {
        tw::LineLog log; log.begin();
        log.observe (at (0, 0.0)); log.observe (at (1, 3.0));
        log.observe (at (0, 3.4));                 // sledzenie cofnelo sie o jeden — ignorujemy
        log.observe (at (1, 3.6));
        log.observe (at (2, 6.0));
        check (log.marks().size() == 3, "cofniecie o jeden wers (drgniecie) nie jest zapisywane",
               list (log.marks()));
    }

    // ---- 3. R: ten sam wers jeszcze raz ---------------------------------------------
    {
        tw::LineLog log; log.begin();
        log.observe (at (0, 0.0)); log.observe (at (1, 3.0)); log.observe (at (2, 6.0));
        log.restart (at (2, 7.5));                 // pomylka w wersie 2 — R
        log.observe (at (3, 10.0));

        const auto fin = log.finalLines();
        check (log.marks().size() == 5 && log.marks()[3].source == tw::LineSource::Key,
               "R zapisuje wers jeszcze raz, ze zrodlem 'klawisz'", list (log.marks()));
        check (fin.size() == 4 && fin[2].sec == 7.5,
               "zostaje ostatnie przeczytanie wersu 2 (7,5 s, nie 6,0 s)", list (fin));
    }

    // ---- 4. cofniecie o kilka wersow ---------------------------------------------------
    //
    // Sledzenie samo cofnelo sie o dwa wersy: to poprawka pozycji, nie powtorka.
    // Zapisujemy ja (to tez wiedza), ale wersy zostaja z pierwszego pojawienia.
    // Pomiar na nagraniu Adriana: traktowanie takich cofniec jak powtorki
    // kasowalo dobrze przeczytane wersy.
    {
        tw::LineLog log; log.begin();
        for (int n = 0; n < 6; ++n) log.observe (at (n, n * 3.0));
        log.observe (at (3, 18.0));                // sledzenie cofnelo sie o 2 wersy
        log.observe (at (4, 21.0));

        const auto fin = log.finalLines();
        check (log.marks().size() == 8, "cofniecie sledzenia o 2+ wersy jest zapisane", list (log.marks()));
        check (fin.size() == 6 && fin[3].sec == 9.0 && fin[5].sec == 15.0,
               "ale wersy zostaja z pierwszego pojawienia (bez klawisza)", list (fin));
    }
    {
        tw::LineLog log; log.begin();
        for (int n = 0; n < 6; ++n) log.observe (at (n, n * 3.0));
        log.restart (at (3, 18.0));                // czlowiek: strzalka w gore — od wersu 3 od nowa
        log.observe (at (4, 21.0));

        const auto fin = log.finalLines();
        check (fin.size() == 5, "po klawiszu: wers 5 z pierwszego czytania wypada", list (fin));
        check (fin.size() == 5 && fin[3].sec == 18.0 && fin[4].sec == 21.0 && fin[2].sec == 6.0,
               "wersy 3 i 4 z drugiego czytania, wczesniejsze z pierwszego", list (fin));
    }

    // ---- 5. zmiana szerokosci okna w trakcie ---------------------------------------------
    {
        tw::LineLog log; log.begin();
        log.observe (at (0, 0.0)); log.observe (at (1, 3.0));
        auto same = at (1, 3.2); same.shown = 3;    // ten sam wers, inny numer na ekranie
        log.observe (same);
        check (log.marks().size() == 2, "inny numer na ekranie, ten sam wers: bez wpisu");
    }

    // ---- 6. plik ------------------------------------------------------------------------
    {
        tw::LineLog log; log.begin();
        log.observe (at (0, 0.0)); log.observe (at (1, 3.25, tw::LineSource::Syllables));
        auto tab = at (2, 6.5); tab.text = "tekst\tz tabulatorem"; log.restart (tab);

        const auto back = tw::LineLog::parse (log.toText());
        check (back.size() == 3, "zapis i odczyt: tyle samo wpisow", std::to_string (back.size()));
        check (back.size() == 3 && back[1].sec == 3.25 && back[1].source == tw::LineSource::Syllables
                   && back[2].source == tw::LineSource::Key && back[0].firstWord == 0 && back[2].lastWord == 11,
               "czas, zrodlo i slowa przetrwaly");
        check (back.size() == 3 && back[0].text == "Wers 0 — źdźbło", "polskie litery przetrwaly", back[0].text);
        check (back.size() == 3 && back[2].text == "tekst z tabulatorem", "tabulator w tekscie nie psuje kolumn",
               back.size() == 3 ? back[2].text : "");
        check (tw::LineLog::parse ("# tylko naglowek\n\nsmieci bez tabulatorow\n").empty(),
               "uszkodzony plik: nic, a nie smieci");
    }

    // ---- punkt ciecia ostatniego R (pre-roll, strzalki)
    {
        tw::LineLog lg; lg.begin();
        tw::LineMark a; a.firstWord = 0; a.sec = 1.0; lg.observe (a);
        lg.setLastCut (0.5, true);
        check (lg.marks().back().cutFrom < 0.0, "punkt ciecia tylko przy R");
        tw::LineMark r = a; r.sec = 3.0; lg.restart (r, tw::LineSource::Retake);
        lg.setLastCut (0.8, true);
        check (lg.marks().back().cutFrom == 0.8 && lg.marks().back().cutByHand, "R pamieta punkt ciecia i to, ze wskazal go czlowiek");
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
