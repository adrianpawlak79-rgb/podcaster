/*
    powtorz-test.cpp — prompter po dluzszej ciszy i po "powtorz wers".

    ---------------------------------------------------------------------------
    SKAD SIE WZIAL TEN TEST

    Dwie rzeczy z wersji 0.5, ktore przepadly razem z kodem i sa tu odtwarzane.

    1. Licznik raz uzbrojony zostawal uzbrojony do konca nagrania. Po akapicie
       czlowiek siegal po wode albo przekladal kartke, a kazdy stuk w tej
       ciszy przesuwal tekst. Teraz po 1,6 s ciszy prompter wraca do "czekam
       na mowe" — ale w tym samym miejscu, nie od zera.

    2. "Powtorz wers" (R) cofa na poczatek wersu, ktory jest w USTACH, i czeka,
       az czlowiek zacznie go czytac.

    Sylaby podajemy recznie, w znanych chwilach — tak jak w start-test.cpp.
*/

#include "../Source/DSP/Follower.h"
#include "../Source/DSP/PrompterLines.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got)
{
    std::printf ("  %-5s %-50s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

/** Karmi prompter od chwili `from` do `until` (30 razy na sekunde, jak ekran).
    Cisza liczona tak jak w prawdziwym liczniku: od ostatniej sylaby, zgloszona
    dopiero po 0,35 s. */
struct Feeder
{
    tw::Follower& f;
    double lastTick = -10.0;
    double t = 0.0;

    void run (const std::vector<double>& ticks, double until)
    {
        size_t next = 0;
        while (next < ticks.size() && ticks[next] < t) ++next;

        for (; t < until; t += 1.0 / 30.0)
        {
            int n = 0;
            while (next < ticks.size() && ticks[next] <= t) { ++n; ++next; lastTick = t; }

            const double silence = t - lastTick;
            f.advance (t, n, silence > 0.35 ? silence : 0.0);
        }
    }
};

static std::vector<double> speech (double from, int syllables, double every = 0.22)
{
    std::vector<double> t;
    for (int i = 0; i < syllables; ++i) t.push_back (from + i * every);
    return t;
}

/** Wersy = wiersze zrodla. Na ekranie lamie je szerokosc, tu wystarczy to. */
static std::vector<tw::LineSpan> sourceLines (const tw::PrompterText& t)
{
    std::vector<tw::LineSpan> out;
    int first = 0;

    for (int i = 0; i < t.wordCount(); ++i)
    {
        const bool last = i == t.wordCount() - 1;
        if (last || t.all()[(size_t) (i + 1)].line != t.all()[(size_t) i].line)
        {
            out.push_back ({ first, i });
            first = i + 1;
        }
    }

    return out;
}

int main()
{
    tw::PrompterText text;
    text.set ("Dzisiaj jest dobry dzien na nagrywanie\n"
              "Mikrofon stoi blisko ust wiec slychac kazdy oddech\n"
              "Czytam spokojnie i nie spiesze sie wcale\n"
              "A potem robie przerwe zeby sie napic wody");

    const auto lines = sourceLines (text);
    char b[128];

    std::printf ("\nPrompter: dluga cisza i powtorz wers\n====================================\n\n");

    // ---- 1. po 1,6 s ciszy stuk nie rusza tekstu --------------------------------
    std::printf ("  1. dluga cisza ze stukiem w srodku\n");

    double heldAt = 0.0, afterKnock = 0.0, resumed = 0.0;
    {
        tw::Follower f;
        f.begin (&text);
        Feeder feed { f };

        feed.run (speech (0.5, 12), 0.5 + 11 * 0.22 + 0.05);
        const double spoken = f.syllablePosition();

        // cisza 2 s — pauza na kropce (0,55 s) i ponowne uzbrojenie (1,6 s)
        const double quietFrom = feed.lastTick;
        feed.run ({}, quietFrom + 2.0);
        heldAt = f.syllablePosition();

        std::snprintf (b, sizeof (b), "po %.1f s ciszy: stoi=%s", 2.0, f.hasStarted() ? "nie" : "tak");
        check (! f.hasStarted() && f.waitingToResume(), "po 1,6 s ciszy czeka na mowe", b);

        std::snprintf (b, sizeof (b), "przed cisza %.1f, po ciszy %.1f sylaby", spoken, heldAt);
        check (heldAt > 5.0, "miejsce w tekscie zostaje (nie wraca do zera)", b);

        // stuk szklanki i znow cisza
        const double knock = quietFrom + 2.3;
        feed.run ({ knock }, knock + 1.5);
        afterKnock = f.syllablePosition();

        std::snprintf (b, sizeof (b), "%.2f -> %.2f sylaby", heldAt, afterKnock);
        check (std::fabs (afterKnock - heldAt) < 1.0e-9, "pojedynczy stuk nie przesuwa tekstu", b);

        // czytanie od nowa
        const double back = knock + 1.6;
        feed.run (speech (back, 8), back + 7 * 0.22 + 0.05);
        resumed = f.syllablePosition();

        std::snprintf (b, sizeof (b), "%.1f -> %.1f po 8 sylabach", afterKnock, resumed);
        check (f.hasStarted(), "mowa po ciszy znow rusza prompter", b);
        check (resumed > afterKnock + 3.0 && resumed < afterKnock + 12.0,
               "i jedzie dalej od miejsca, w ktorym stal", b);
    }

    // ---- 2. to samo bez ponownego uzbrajania: tak bylo w 0.4.0 ------------------
    {
        tw::Follower f;
        f.params.rearmAfterSec = 1.0e9;
        f.begin (&text);
        Feeder feed { f };

        feed.run (speech (0.5, 12), 0.5 + 11 * 0.22 + 0.05);
        const double quietFrom = feed.lastTick;
        feed.run ({}, quietFrom + 2.0);
        const double held = f.syllablePosition();

        // patrzymy tuz po stuku, zanim pauza na kropce zdazy dosunac pozycje
        const double knock = quietFrom + 2.3;
        feed.run ({ knock }, knock + 0.45);

        std::snprintf (b, sizeof (b), "stary sposob: stuk przesunal o %.2f sylaby",
                       f.syllablePosition() - held);
        check (f.syllablePosition() > held + 0.5, "(kontrola) bez tego stuk przesuwal tekst", b);
    }

    // ---- 3. krotka pauza nie rozbraja -------------------------------------------
    {
        tw::Follower f;
        f.begin (&text);
        Feeder feed { f };

        feed.run (speech (0.5, 12), 0.5 + 11 * 0.22 + 0.05);
        feed.run ({}, feed.lastTick + 1.2);

        check (f.hasStarted(), "pauza 1,2 s (oddech, kropka) nie rozbraja", "");
    }

    // ---- 4. wersy -----------------------------------------------------------------
    std::printf ("\n  2. powtorz wers\n");
    {
        std::snprintf (b, sizeof (b), "%d wersy", (int) lines.size());
        check (lines.size() == 4, "tekst ma cztery wersy", b);

        const int w2 = lines[1].firstWord;     // "Mikrofon"
        const int w2last = lines[1].lastWord;  // "oddech"

        check (tw::lineOfWord (lines, w2 + 2) == 1, "slowo w srodku wersu 2 lezy w wersie 2", "");
        check (tw::repeatLineStart (lines, w2 + 3) == w2, "R w srodku wersu cofa na jego poczatek", "");

        // ostatnie slowo wersu: podswietlony jest juz nastepny wers,
        // ale R ma cofnac do poczatku TEGO, ktory sie wymawia
        const auto& last = text.all()[(size_t) w2last];
        const int shown = tw::lineToShow (lines, text, last.beforeMe + 0.5);

        std::snprintf (b, sizeof (b), "podswietlony wers %d, R -> slowo %d (poczatek wersu 2 = %d)",
                       shown + 1, tw::repeatLineStart (lines, w2last), w2);
        check (shown == 2 && tw::repeatLineStart (lines, w2last) == w2,
               "pod koniec wersu R nie skacze do nastepnego", b);
    }

    // ---- 5. R w trakcie czytania ---------------------------------------------------
    {
        tw::Follower f;
        f.begin (&text);
        Feeder feed { f };

        const int line2 = lines[1].firstWord;
        const double line2Syll = text.all()[(size_t) line2].beforeMe;

        // czytamy az do srodka wersu 2
        int n = 0;
        while (f.syllablePosition() < line2Syll + 5.0 && n < 80)
        {
            feed.run ({ feed.t + 0.01 }, feed.t + 0.22);
            ++n;
        }

        const int spoken = f.currentWord();
        f.repeatFrom (tw::repeatLineStart (lines, spoken));

        std::snprintf (b, sizeof (b), "slowo %d -> %d, pozycja %.1f (poczatek wersu %.0f)",
                       spoken, f.currentWord(), f.syllablePosition(), line2Syll);
        check (spoken > line2 && f.currentWord() == line2
                   && std::fabs (f.syllablePosition() - line2Syll) < 1.0e-9,
               "R cofa do poczatku biezacego wersu", b);

        check (! f.hasStarted(), "po R tekst stoi, az czlowiek zacznie czytac", "");

        // oddech i ten sam wers jeszcze raz
        const double at = feed.t + 0.8;
        feed.run ({}, at);
        const double still = f.syllablePosition();
        feed.run (speech (at, 6), at + 5 * 0.22 + 0.05);

        std::snprintf (b, sizeof (b), "oddech: %.1f, po 6 sylabach: %.1f", still, f.syllablePosition());
        check (std::fabs (still - line2Syll) < 1.0e-9 && f.hasStarted()
                   && f.syllablePosition() > line2Syll + 2.0
                   && f.syllablePosition() < line2Syll + 10.0,
               "powtorka jedzie od poczatku wersu", b);
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
