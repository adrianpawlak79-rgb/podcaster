/*
    wyglad-test.cpp — wspolny wyglad, sprawdzany bez okna.

    ---------------------------------------------------------------------------
    SKAD SIE WZIAL TEN TEST

    Wersja 0.5 miala wspolny wyglad (Look/StepBar/Fonts) i przepadla. Wyglad
    to zwykle rzecz "na oko", ale wiekszosc tego, co sie w nim psuje, da sie
    policzyc:

    - kontrast: czy tekst da sie przeczytac z dwoch metrow,
    - rodzaje przyciskow: czy zaden ekran nie dorobil sobie trzeciego,
    - kroj: czy przyciety Inter ma wszystkie polskie litery (brak "ł" nie
      wywala programu — JUCE po cichu wstawia inny kroj w srodek slowa),
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
/* Minimalny czytnik tablicy cmap (format 4) — tylko po to, zeby zapytac
   kroj, czy ma dany znak. */
static uint32_t be32 (const std::string& d, size_t o) { return ((uint8_t) d[o] << 24) | ((uint8_t) d[o + 1] << 16) | ((uint8_t) d[o + 2] << 8) | (uint8_t) d[o + 3]; }
static uint16_t be16 (const std::string& d, size_t o) { return (uint16_t) (((uint8_t) d[o] << 8) | (uint8_t) d[o + 1]); }

static bool hasGlyph (const std::string& ttf, uint32_t cp)
{
    if (ttf.size() < 12) return false;

    const int tables = be16 (ttf, 4);
    size_t cmap = 0;

    for (int i = 0; i < tables; ++i)
        if (ttf.compare (12 + 16 * i, 4, "cmap") == 0) cmap = be32 (ttf, 12 + 16 * i + 8);

    if (cmap == 0) return false;

    const int subs = be16 (ttf, cmap + 2);

    for (int i = 0; i < subs; ++i)
    {
        const size_t sub = cmap + be32 (ttf, cmap + 4 + 8 * i + 4);
        if (be16 (ttf, sub) != 4) continue;

        const int segs = be16 (ttf, sub + 6) / 2;
        const size_t ends = sub + 14, starts = ends + 2 * segs + 2;
        const size_t deltas = starts + 2 * segs, ranges = deltas + 2 * segs;

        for (int s = 0; s < segs; ++s)
        {
            const uint16_t end = be16 (ttf, ends + 2 * s), start = be16 (ttf, starts + 2 * s);
            if (cp < start || cp > end) continue;

            const uint16_t ro = be16 (ttf, ranges + 2 * s);
            if (ro == 0) return (uint16_t) (cp + be16 (ttf, deltas + 2 * s)) != 0;

            const size_t at = ranges + 2 * s + ro + 2 * (cp - start);
            return be16 (ttf, at) != 0;
        }
    }

    return false;
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
    check ((colour::accent & 0xffffff) == 0xD5A879, "akcent to #D5A879", b);

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

    std::snprintf (b, sizeof (b), "kontrast akcentu z zielenia %.2f", contrast (colour::accent, colour::green));
    check ((colour::accent & 0xffffff) != (colour::green & 0xffffff),
           "akcent to nie zielen lampki", b);

    // ---- 2. dwa rodzaje przyciskow --------------------------------------------------
    std::printf ("\n  2. przyciski\n");

    check (buttonKindCount == 2, "sa dokladnie dwa rodzaje przyciskow", std::to_string (buttonKindCount));

    const auto pri = paintFor (ButtonKind::Primary), sec = paintFor (ButtonKind::Secondary);
    check (pri.fill == colour::accent && sec.fill != pri.fill, "glowny ma akcent, drugorzedny nie", "");

    // Zaden ekran nie koloruje przyciskow sam — robi to tylko Look.h.
    const char* sources[] = { "MainComponent.h", "PrompterComponent.h", "ReviewComponent.h",
                              "WizardComponent.h", "StepBar.h", "Main.cpp" };
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

    // ---- 3. kroj --------------------------------------------------------------------
    std::printf ("\n  3. kroj Inter\n");

    const char32_t polish[] = { U'ą', U'ć', U'ę', U'ł', U'ń', U'ó', U'ś', U'ź', U'ż',
                                U'Ą', U'Ć', U'Ę', U'Ł', U'Ń', U'Ó', U'Ś', U'Ź', U'Ż',
                                U'„', U'”', U'—', U'–', U'…', U'·', U'←', U'→', U'↑', U'↓', U'−', U'×' };

    for (auto* name : { "Inter-Regular.ttf", "Inter-SemiBold.ttf" })
    {
        const auto ttf = readFile (std::string ("../Resources/Fonts/") + name);
        int missing = 0;
        for (auto cp : polish) if (! hasGlyph (ttf, (uint32_t) cp)) ++missing;
        for (uint32_t cp = 0x20; cp < 0x7f; ++cp) if (! hasGlyph (ttf, cp)) ++missing;

        std::snprintf (b, sizeof (b), "%zu KB, brakuje %d znakow", ttf.size() / 1024, missing);
        check (! ttf.empty() && missing == 0, (std::string (name) + ": polskie litery i strzalki").c_str(), b);
    }

    check (readFile ("../Resources/Fonts/OFL.txt").find ("SIL OPEN FONT LICENSE") != std::string::npos,
           "licencja kroju lezy obok", "");

    const auto cmake = readFile ("../CMakeLists.txt");
    check (cmake.find ("Inter-Regular.ttf") != std::string::npos
               && cmake.find ("Inter-SemiBold.ttf") != std::string::npos,
           "kroje sa wszyte w .exe (BinaryData)", "");

    // ---- 4. pasek krokow ------------------------------------------------------------
    std::printf ("\n  4. pasek krokow\n");

    check (stepState (0, 2) == StepState::Done && stepState (2, 2) == StepState::Current
               && stepState (3, 2) == StepState::Todo,
           "przed biezacym zrobione, za nim do zrobienia", "");
    check (stepState (5, 6, true) == StepState::Done, "po kreatorze wszystkie zrobione", "");

    bool sums = true;
    for (int total : { 640, 641, 980, 1857 })
        for (int n : { 4, 6 })
        {
            const auto w = stepWidths (total, n, 8);
            int sum = 8 * (n - 1);
            for (int x : w) sum += x;
            if (sum != total || w.front() > w.back()) sums = false;
        }
    check (sums, "kroki wypelniaja pasek co do piksela", "");

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
