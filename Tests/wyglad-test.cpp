/*
    wyglad-test.cpp — wspolny wyglad, sprawdzany bez okna.

    ---------------------------------------------------------------------------
    SKAD SIE WZIAL TEN TEST

    Wersja 0.5 miala wspolny wyglad (Look/StepBar/Fonts) i przepadla. Wyglad
    to zwykle rzecz "na oko", ale wiekszosc tego, co sie w nim psuje, da sie
    policzyc:

    - kontrast: czy tekst da sie przeczytac z dwoch metrow,
    - rodzaje przyciskow: glowny, drugorzedny i okragly Nagraj,
    - kroj: Segoe UI z systemu, bez podmiany na Inter,
    - przebieg na ekranie oceny: czy rosnie z oknem i czy nie wypycha
      przyciskow za krawedz,
    - klawisze ekranu nagrywania i pasek podpowiedzi.

    Uruchamiac z katalogu Tests/ (czyta zrodla i kroje z ../).
*/

#include "../Source/UiRules.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got)
{
    std::printf ("  %-5s %-52s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static std::string readFile (const std::string& path)
{
    std::ifstream f (path, std::ios::binary);
    std::stringstream b;
    b << f.rdbuf();
    return b.str();
}

//==============================================================================
int main()
{
    using namespace zs::ui;
    char b[160];

    std::printf ("\nWspolny wyglad\n==============\n\n");

    // ---- 1. paleta ---------------------------------------------------------------
    std::printf ("  1. paleta i kontrast\n");

    // Palety A (miedz), B (granat) i klasyczna — kazda musi byc czytelna, a
    // w nowych zielen znaczy tylko "dobrze" (Adrian 13.10).
    for (auto id : { PaletteId::Copper, PaletteId::Console, PaletteId::Classic })
    {
        const auto& c = paletteFor (id);
        std::printf ("    %s\n", c.name);

        struct Pair { const char* what; uint32_t fg, bg; double need; };
        const Pair pairs[] =
        {
            { "tekst glowny na tle",            c.textNow,  c.bg,     7.0 },
            { "tekst drugorzedny na tle",       c.textNext, c.bg,     4.5 },
            { "tekst drugorzedny na panelu",    c.textNext, c.panel,  4.5 },
            { "napis na przycisku glownym",     c.onAccent, c.accent, 4.5 },
            { "napis na przycisku drugorzednym",c.textNow,  c.raised, 4.5 },
            { "przeczytane wersy na tle",       c.textPast, c.bg,     3.0 },
            { "podpowiedzi (textDim) na panelu",c.textDim,  c.panel,  4.5 },
        };
        for (const auto& p : pairs)
        {
            const double k = contrast (p.fg, p.bg);
            std::snprintf (b, sizeof (b), "%.1f : 1 (potrzeba %.1f)", k, p.need);
            check (k >= p.need, p.what, b);
        }
        check ((c.green & 0xffffff) == 0x39CF7B, "zielen lampki #39CF7B", "");
        if (id != PaletteId::Classic)
            check (c.accent != c.green && c.accent != c.yellow && c.accent != c.red,
                   "akcent nie jest zadnym z kolorow lampki", "");
    }
    check (currentPaletteId() == PaletteId::Console, "domyslna paleta: B (granat) — wybor Adriana", "");

    // ---- 2. trzy rodzaje przyciskow --------------------------------------------------
    std::printf ("\n  2. przyciski\n");

    check (buttonKindCount == 3, "sa trzy rodzaje, w tym okragly Nagraj", std::to_string (buttonKindCount));

    const auto pri = paintFor (ButtonKind::Primary), sec = paintFor (ButtonKind::Secondary);
    check (pri.fill == pal().accent && sec.fill != pri.fill, "glowny ma akcent, drugorzedny nie", "");

    const auto rec = paintFor (ButtonKind::Record);
    check (rec.fill == pri.fill && contrast (rec.text, rec.fill) >= 4.5,
           "okragly Nagraj ma czytelny napis na akcencie", "");

    // Zaden ekran nie koloruje przyciskow sam — robi to tylko Look.h.
    const char* sources[] = { "MainComponent.h", "PrompterComponent.h", "ReviewComponent.h",
                              "WizardComponent.h", "StepBar.h", "ScreenFrame.h", "Main.cpp" };
    const char* forbidden[] = { "TextButton::buttonColourId", "TextButton::buttonOnColourId",
                                "TextButton::textColourOffId", "TextButton::textColourOnId" };
    int found = 0;
    std::string where;

    for (auto* f : sources)
    {
        const auto src = readFile (std::string ("../Source/") + f);
        if (src.empty()) { where += std::string (" brak:") + f; ++found; continue; }

        for (auto* x : forbidden)
            for (size_t at = src.find (x); at != std::string::npos; at = src.find (x, at + 1))
            {
                ++found;
                where += std::string (" ") + f;
            }
    }

    check (found == 0, "kolor przycisku ustawia tylko Look.h", found == 0 ? "0 miejsc" : where);

    // ---- 3. kroj systemowy ----------------------------------------------------------
    std::printf ("\n  3. kroj systemowy\n");
    const auto look = readFile ("../Source/Look.h");
    check (look.find ("Segoe UI Variable") != std::string::npos
               && look.find ("Segoe UI") != std::string::npos,
           "Look wybiera Segoe UI z prototypu", "kroj systemowy Windows");
    check (readFile ("../Source/Fonts.h").find ("BinaryData") == std::string::npos
               && readFile ("../CMakeLists.txt").find ("Inter-Regular.ttf") == std::string::npos,
           "Inter nie zastepuje kroju systemowego", "");

    // ---- 4. pasek krokow ------------------------------------------------------------
    std::printf ("\n  4. pasek krokow\n");

    check (stepState (0, 2) == StepState::Done && stepState (2, 2) == StepState::Current
               && stepState (3, 2) == StepState::Todo,
           "przed biezacym zrobione, za nim do zrobienia", "");
    check (stepState (5, 6, true) == StepState::Done, "po kreatorze wszystkie zrobione", "");

    bool sums = true;
    for (int total : { 640, 641, 980, 1857 })
        for (int n : { 5, 6 })
        {
            const auto w = stepWidths (total, n, 8);
            int sum = 8 * (n - 1);
            for (int x : w) sum += x;
            if (sum != total || w.front() > w.back()) sums = false;
        }
    check (sums, "kroki wypelniaja pasek co do piksela", "");

    bool layoutFits = true;
    for (int width : { 960, 1280, 1440, 2400 })
        for (int height : { 640, 800, 900, 1600 })
        {
            const int mainWidth = width - frame::sideWidth (width) - 2 * frame::margin;
            const int cardHeight = height - frame::header - frame::steps - frame::footer - 48 - 98 - 36;
            if (mainWidth < 600 || cardHeight < 200) layoutFits = false;
        }
    check (layoutFits, "tresc i panel mieszcza sie od 960 x 640", "");

    // ---- 5. przebieg na ekranie oceny ------------------------------------------------
    std::printf ("\n  5. przebieg na ekranie oceny\n");

    bool grows = true, fits = true;
    int prev = 0;
    for (int h = 640; h <= 1600; ++h)
    {
        const int w = review::waveHeight (h);
        if (w < prev) grows = false;
        if (review::fixedRest + w > h) fits = false;
        prev = w;
    }

    std::snprintf (b, sizeof (b), "640 px: %d, 900: %d, 1200: %d, 1600: %d (bylo 140/190)",
                   review::waveHeight (640), review::waveHeight (900),
                   review::waveHeight (1200), review::waveHeight (1600));
    check (grows, "rosnie razem z oknem (nigdy nie maleje)", b);
    check (fits, "przyciski i podsumowanie mieszcza sie od 640 do 1600 px", "");
    check (review::waveHeight (640) >= 140 && review::waveHeight (1200) >= 300,
           "male okno nie gorsze niz bylo, duze wyraznie wieksze", "");

    // ---- 5b. montaz myszka i czas miksu ----------------------------------------------
    std::printf ("\n  5b. zaznaczenie myszka i czas miksu\n");

    check (! review::dragRange (100.0f, 103.0f, 0, 1000, 60.0).isDrag(),
           "drgniecie reki (3 px) to klikniecie, nie zaznaczenie", "");
    {
        const auto r = review::dragRange (600.0f, 200.0f, 100, 1000, 60.0);
        std::snprintf (b, sizeof (b), "%.1f-%.1f s", r.from, r.to);
        check (r.isDrag() && std::fabs (r.from - 6.0) < 1e-9 && std::fabs (r.to - 30.0) < 1e-9,
               "przeciagniecie w lewo daje ten sam zakres, w sekundach", b);
    }
    {
        const auto r = review::dragRange (-50.0f, 2000.0f, 0, 1000, 60.0);
        check (r.from == 0.0 && r.to == 60.0, "zaznaczenie poza przebieg konczy sie na krawedzi", "");
    }
    std::snprintf (b, sizeof (b), "4:26 -> %d s, 0:20 -> %d s, 0:01 -> %d s",
                   review::mixEstimateSec (266.0), review::mixEstimateSec (20.0), review::mixEstimateSec (1.0));
    check (review::mixEstimateSec (266.0) == 55 && review::mixEstimateSec (20.0) == 5
               && review::mixEstimateSec (1.0) == 5,
           "szacunek miksu: 0,2 dlugosci, w gore do 5 s", b);

    // ---- 6. klawisze ekranu nagrywania ------------------------------------------------
    std::printf ("\n  6. klawisze i pasek podpowiedzi\n");

    check (keyAction (true, 0, true, true) == KeyAction::StopAndReview, "Esc w trakcie nagrania: zatrzymaj i ocen", "");
    check (keyAction (true, 0, false, true) == KeyAction::LeavePrompter, "Esc przed nagraniem: wyjscie z promptera", "");
    check (keyAction (false, U'r', true, true) == KeyAction::RepeatLine
               && keyAction (false, U'R', true, true) == KeyAction::RepeatLine,
           "R w trakcie nagrania z tekstem: powtorz wers", "");
    check (keyAction (false, U'r', true, false) == KeyAction::None
               && keyAction (false, U'r', false, true) == KeyAction::None,
           "R bez tekstu albo bez nagrania nic nie robi", "");
    check (keyAction (false, U'l', true, true) == KeyAction::None, "inne klawisze zostaja prompterowi", "");

    check (keyAction (false, U' ', true, true) == KeyAction::StartStop
               && keyAction (false, U' ', false, false) == KeyAction::StartStop,
           "spacja: start i stop (jak w DAW)", "");

    const auto recHints = hintLine (true, true);
    check (recHints.find ("Spacja") != std::string::npos && recHints.find ("R \xe2\x80\x94 powt") != std::string::npos,
           "pasek w trakcie nagrania mowi o spacji i R", recHints);
    check (hintLine (false, true).find ("Spacja") != std::string::npos && hintLine (false, false).find ("Spacja") != std::string::npos,
           "przed nagraniem tez podpowiada spacje", hintLine (false, false));
    check (hintLine (true, false).find ("R \xe2\x80\x94") == std::string::npos,
           "bez tekstu nie podpowiada R", hintLine (true, false));

    // Limit sesji (audyt 14.10): tyle, ile sprawdzil zs-dlugo.
    {
        namespace S = zs::ui::session;
        check (S::phase (0) == S::Phase::Ok && S::phase (54 * 60.0) == S::Phase::Ok, "do 55. minuty bez ostrzezen", "");
        check (S::phase (55 * 60.0) == S::Phase::Warn && S::phase (3599.0) == S::Phase::Warn, "od 55. minuty ostrzezenie", "");
        check (S::phase (3600.0) == S::Phase::Stop, "w 60. minucie stop", "");
        check (S::minutesLeft (55 * 60.0) == 5 && S::minutesLeft (3599.5) == 1, "zostalo: pelne minuty, co najmniej 1",
               std::to_string (S::minutesLeft (55 * 60.0)));
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
