/*
    start-test.cpp — czym rozni sie poczatek czytania od halasu.

    ---------------------------------------------------------------------------
    SKAD SIE WZIAL TEN TEST

    Czlowiek: "startuje dziwnie, moze na kazdy dzwiek. Musze wtedy cofac
    strzalkami, ale jak juz ustawie poczatek dobrze to idzie ladnie".

    I tak bylo. Trzask krzesla, chrzakniecie, stuk statywu, kliknieciecie myszy
    tuz przy mikrofonie — kazde z nich daje w pasmie samoglosek szczyt energii
    nie do odroznienia od sylaby. Licznik je liczyl, prompter ruszal, a czlowiek
    zaczynal czytac od tekstu przesunietego o kilka slow.

    Roznica nie siedzi w pojedynczym dzwieku. Siedzi w RYTMIE: mowa to seria
    sylab w ludzkim odstepie, halas jest pojedynczy. Ten test sprawdza wlasnie
    to rozroznienie, bo jest jedyna rzecza, ktora tu dziala.
*/

#include "../Source/DSP/Follower.h"

#include <cstdio>
#include <vector>
#include <string>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got)
{
    std::printf ("  %-5s %-46s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

/** Podaje prompterowi sylaby o zadanych czasach i zwraca pozycje na koncu.

    Cisze liczy tak jak prawdziwy licznik: czas od ostatniej sylaby, zerowany
    gdy ktos mowi. Bez tego prompter jechalby dalej przez cala przerwe i test
    mierzylby jazde na biegu jalowym zamiast tego, co bada. */
static double feed (tw::Follower& f, const std::vector<double>& ticks, double untilSec)
{
    size_t next = 0;
    double lastTick = -10.0;

    for (double t = 0.0; t < untilSec; t += 1.0 / 30.0)
    {
        int n = 0;
        while (next < ticks.size() && ticks[next] <= t) { ++n; ++next; lastTick = t; }

        const double silence = t - lastTick;
        f.advance (t, n, silence > 0.35 ? silence : 0.0);
    }

    return f.syllablePosition();
}

int main()
{
    tw::PrompterText text;
    text.set ("Dzisiaj jest dobry dzien na nagrywanie. "
              "Mikrofon stoi blisko ust, wiec slychac kazdy oddech. "
              "Czytam spokojnie i nie spiesze sie.");

    char b[96];

    std::printf ("\nPoczatek czytania a halas\n=========================\n\n");

    // ---- 1. jeden trzask -------------------------------------------------------
    {
        tw::Follower f;
        f.begin (&text);

        const double pos = feed (f, { 0.4 }, 3.0);

        std::snprintf (b, sizeof (b), "pozycja %.2f sylaby", pos);
        check (pos == 0.0 && ! f.hasStarted(), "pojedynczy trzask nie rusza tekstu", b);
    }

    // ---- 2. dwa stuki w odstepie sekundy ---------------------------------------
    {
        tw::Follower f;
        f.begin (&text);

        const double pos = feed (f, { 0.3, 1.4, 2.6 }, 4.0);

        std::snprintf (b, sizeof (b), "pozycja %.2f sylaby", pos);
        check (pos == 0.0 && ! f.hasStarted(),
               "stuki za rzadkie, zeby byc mowa", b);
    }

    // ---- 3. oddech i chrzakniecie ----------------------------------------------
    {
        tw::Follower f;
        f.begin (&text);

        const double pos = feed (f, { 0.2, 0.9 }, 3.0);

        std::snprintf (b, sizeof (b), "pozycja %.2f sylaby", pos);
        check (pos == 0.0 && ! f.hasStarted(), "dwa dzwieki to jeszcze nie mowa", b);
    }

    // ---- 4. prawdziwa mowa ------------------------------------------------------
    {
        tw::Follower f;
        f.begin (&text);

        // "Dzi-siaj jest do-bry dzien" — sylaby co okolo 0,22 s
        std::vector<double> t;
        for (int i = 0; i < 10; ++i) t.push_back (0.5 + i * 0.22);

        const double pos = feed (f, t, 0.5 + 9 * 0.22 + 0.1);

        std::snprintf (b, sizeof (b), "pozycja %.1f sylaby", pos);
        check (f.hasStarted(), "mowa rusza prompter", b);
        check (pos > 5.0, "i tekst naprawde jedzie", b);
    }

    // ---- 5. halas, a POTEM mowa -------------------------------------------------
    //
    // To jest przypadek z zycia: stuk mikrofonu, chwila ciszy, i dopiero czytanie.
    // Pozycja po starcie ma odpowiadac SYLABOM MOWY, a nie liczyc stuku.
    {
        tw::Follower f;
        f.begin (&text);

        std::vector<double> t { 0.3, 1.2 };                 // dwa halasy
        for (int i = 0; i < 8; ++i) t.push_back (3.0 + i * 0.22);

        // zatrzymujemy pomiar tuz po ostatniej sylabie mowy
        const double pos = feed (f, t, 3.0 + 7 * 0.22 + 0.1);

        std::snprintf (b, sizeof (b), "pozycja %.1f przy 8 sylabach mowy", pos);
        check (f.hasStarted(), "mowa po halasie i tak rusza", b);
        check (pos < 10.0, "halas nie doliczyl sie do pozycji", b);
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
