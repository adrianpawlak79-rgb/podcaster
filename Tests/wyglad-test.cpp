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

    std::snprintf (b, sizeof (b), "#%06X", colour::accent & 0xffffff);
    check ((colour::accent & 0xffffff) == 0x39CF7B, "akcent to #39CF7B", b);

    struct Pair { const char* what; uint32_t fg, bg; double need; };
    const Pair pairs[] =
    {
        { "tekst glowny na tle",            colour::textNow,  colour::bg,     7.0 },
        { "tekst drugorzedny na tle",       colour::textNext, colour::bg,     4.5 },
        { "tekst drugorzedny na panelu",    colour::textNext, colour::panel,  4.5 },
        { "napis na przycisku glownym",     colour::onAccent, colour::accent, 4.5 },
        { "napis na przycisku drugorzednym",colour::textNow,  colour::raised, 4.5 },
        { "przeczytane wersy na tle",       colour::textPast, colour::bg,     3.0 },
    };

    for (const auto& p : pairs)
    {
        const double c = contrast (p.fg, p.bg);
        std::snprintf (b, sizeof (b), "%.1f : 1 (potrzeba %.1f)", c, p.need);
        check (c >= p.need, p.what, b);
    }

    check (colour::accent == colour::green, "akcent i lampka maja zielen prototypu", "#39CF7B");
    check (colour::bg == 0xff141719 && colour::bar == 0xff1b1f21
               && colour::panel == 0xff22272a && colour::raised == 0xff30373a
               && colour::textNow == 0xfff1f4f2 && colour::yellow == 0xffe8bd4d
               && colour::red == 0xffe36a63, "paleta z Docs/projekt/README.md", "");

    // ---- 2. trzy rodzaje przyciskow --------------------------------------------------
    std::printf ("\n  2. przyciski\n");

    check (buttonKindCount == 3, "sa trzy rodzaje, w tym okragly Nagraj", std::to_string (buttonKindCount));

    const auto pri = paintFor (ButtonKind::Primary), sec = paintFor (ButtonKind::Secondary);
    check (pri.fill == colour::accent && sec.fill != pri.fill, "glowny ma akcent, drugorzedny nie", "");

    const auto rec = paintFor (ButtonKind::Record);
    check (rec.fill == pri.fill && contrast (rec.text, rec.fill) >= 4.5,
           "okragly Nagraj ma czytelny napis na zieleni", "");

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

    const auto recHints = hintLine (true, true);
    check (recHints.find ("Esc") != std::string::npos && recHints.find ("R \xe2\x80\x94 powt") != std::string::npos,
           "pasek w trakcie nagrania mowi o Esc i R", recHints);
    check (hintLine (true, false).find ("R \xe2\x80\x94") == std::string::npos,
           "bez tekstu nie podpowiada R", hintLine (true, false));

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
