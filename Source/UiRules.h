/*
    UiRules.h — zasady wygladu, ktore da sie sprawdzic bez okna.

    ---------------------------------------------------------------------------
    PO CO OSOBNY PLIK BEZ JUCE

    Wyglad programu do tej pory sprawdzal czlowiek: patrzyl na zrzut ekranu i
    mowil, ze cos nie gra. Tak wyszly na jaw zgubione przecinki w prompterze i
    trzy odcienie zieleni, ktore mialy byc jednym. Testy w Tests/ kompiluje
    sie jednym g++ bez JUCE, wiec wszystko, co ma byc pilnowane przy
    budowaniu, musi stac tutaj — liczby, a nie okienka.

    Look.h, StepBar.h i ekrany biora stad wartosci; testy (Tests/wyglad-test.cpp)
    sprawdzaja te same wartosci. Jedno zrodlo, dwoch czytelnikow.

    ---------------------------------------------------------------------------
    CO TU JEST

    - paleta: ciemne tlo, tekst, kolory znaczen i jeden akcent (#D5A879),
    - dwa rodzaje przyciskow — i ani jednego wiecej,
    - stany paska krokow,
    - wysokosc przebiegu na ekranie oceny w zaleznosci od okna,
    - klawisze ekranu nagrywania i pasek podpowiedzi.
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace zs
{
namespace ui
{

//==============================================================================
/* Paleta, ARGB. Znaczenia, nie nazwy kolorow — patrz komentarz w Look.h. */
namespace colour
{
    constexpr uint32_t bg        = 0xff0d1117;
    constexpr uint32_t panel     = 0xff151b24;
    constexpr uint32_t raised    = 0xff1c2430;   ///< przycisk drugorzedny, pole
    constexpr uint32_t edge      = 0xff2a3442;

    constexpr uint32_t textNow   = 0xffeceff4;
    constexpr uint32_t textNext  = 0xff9aa6b6;
    constexpr uint32_t textPast  = 0xff6f7d8f;
    constexpr uint32_t textDim   = 0xff8997a8;

    /** Akcent: to, co wolno nacisnac jako nastepne. Cieply, zeby nie mylil sie
        z zielenia lampki — zielen znaczy "nagranie jest dobre" i nic wiecej. */
    constexpr uint32_t accent    = 0xffd5a879;
    constexpr uint32_t onAccent  = 0xff14100a;   ///< tekst na akcencie

    constexpr uint32_t green     = 0xff71d9ac;
    constexpr uint32_t yellow    = 0xffffd600;
    constexpr uint32_t red       = 0xffff3d00;
    constexpr uint32_t clip      = 0xffff1744;
    constexpr uint32_t lostBlue  = 0xff3d5afe;
    constexpr uint32_t lostText  = 0xff82b1ff;
}

/** Kontrast wedlug WCAG 2.x (1..21). Tekst czytany w biegu potrzebuje >= 4,5. */
inline double contrast (uint32_t a, uint32_t b)
{
    auto lin = [] (uint32_t c, int shift)
    {
        const double v = ((c >> shift) & 0xff) / 255.0;
        return v <= 0.03928 ? v / 12.92 : std::pow ((v + 0.055) / 1.055, 2.4);
    };

    auto lum = [&] (uint32_t c)
    {
        return 0.2126 * lin (c, 16) + 0.7152 * lin (c, 8) + 0.0722 * lin (c, 0);
    };

    const double la = lum (a), lb = lum (b);
    return (std::max (la, lb) + 0.05) / (std::min (la, lb) + 0.05);
}

//==============================================================================
/** Dokladnie dwa rodzaje przyciskow.

    Glowny: wypelniony akcentem, jeden na ekran — to, co robi sie teraz
    ("Nagrywaj", "Zatrzymaj i ocen", "Dalej", "Zapisz miks"). Drugorzedny:
    ciemny z obwodka — cala reszta.

    W 0.4.0 ekrany kolorowaly przyciski same: zielony "Nagrywaj", czerwony
    "Zatrzymaj", szare reszta, a przelacznik przed/po mial do tego zielony
    albo zolty napis. Zielen i czerwien znacza u nas stan nagrania (lampka),
    wiec przycisk w tych kolorach mowil cos, czego nie mial na mysli. Kolor przycisku ustawia teraz wylacznie Look.h —
    test pilnuje, zeby zaden ekran nie robil tego na wlasna reke. */
enum class ButtonKind { Primary, Secondary };
constexpr int buttonKindCount = 2;

struct ButtonPaint { uint32_t fill, text, outline; };

inline ButtonPaint paintFor (ButtonKind k)
{
    if (k == ButtonKind::Primary) return { colour::accent, colour::onAccent, colour::accent };
    return { colour::raised, colour::textNow, colour::edge };
}

//==============================================================================
/** Pasek krokow: kreator (szesc krokow) i ekran glowny (przygotuj, nagraj,
    odsluchaj, zapisz). */
enum class StepState { Done, Current, Todo };

inline StepState stepState (int index, int current, bool allDone = false)
{
    if (allDone || index < current) return StepState::Done;
    return index == current ? StepState::Current : StepState::Todo;
}

/** Ile miejsca dostaje kazdy krok. Rowno — dlugosc napisu nie moze
    sugerowac, ze ktorys krok jest wazniejszy. Reszta z dzielenia idzie do
    ostatniego, zeby pasek konczyl sie rowno z krawedzia. */
inline std::vector<int> stepWidths (int total, int count, int gap)
{
    std::vector<int> w;
    if (count <= 0) return w;

    const int usable = std::max (0, total - gap * (count - 1));
    const int each = usable / count;

    w.assign ((size_t) count, each);
    w.back() += usable - each * count;
    return w;
}

//==============================================================================
/** Ekran oceny: uklad w pionie.

    Przebieg mial dwie stale wysokosci: 140 albo 190 pikseli. Na monitorze
    1440 px wysokim przebieg zajmowal jedna osma okna, a pod nim lezalo pol
    ekranu pustego tla — a to wlasnie na przebiegu sie klika, zeby posluchac
    miejsca. Teraz rosnie z oknem, ale nie zjada miejsca na przyciski i
    podsumowanie. */
namespace review
{
    constexpr int marginY  = 22;
    constexpr int verdict  = 66;
    constexpr int strip    = 46;    ///< os z kawalkami pod przebiegiem
    constexpr int buttons  = 38;
    constexpr int gapA     = 14;
    constexpr int tools    = 36;
    constexpr int gapB     = 14;
    constexpr int summaryMin = 200; ///< trzy karty i dwa zdania

    constexpr int waveMin  = 120;
    constexpr int waveMax  = 420;

    /** Wszystko poza przebiegiem, czyli to, co MUSI sie zmiescic. */
    constexpr int fixedRest = 2 * marginY + verdict + strip + buttons + gapA + tools + gapB + summaryMin;

    inline int waveHeight (int windowHeight)
    {
        const int proportional = (int) std::lround (windowHeight * 0.28);
        const int room = windowHeight - fixedRest;
        return std::clamp (std::min (proportional, room), waveMin, waveMax);
    }
}

//==============================================================================
/** Klawisze ekranu nagrywania.

    Jedna reka, bez patrzenia, bez Ctrl. Esc zatrzymuje i od razu ocenia — to
    jest jedyna rzecz, ktora po nagraniu chce sie zrobic. R cofa prompter na
    poczatek wersu, ktory sie wlasnie pomylilo: czytasz go jeszcze raz, a
    nagranie leci dalej, bo dogrywka jednego wersu nie jest powodem, zeby
    zaczynac caly plik od nowa. */
enum class KeyAction { None, StopAndReview, RepeatLine, LeavePrompter };

inline KeyAction keyAction (bool isEscape, char32_t ch, bool recording, bool hasScript)
{
    if (isEscape) return recording ? KeyAction::StopAndReview : KeyAction::LeavePrompter;

    if ((ch == U'r' || ch == U'R') && recording && hasScript)
        return KeyAction::RepeatLine;

    return KeyAction::None;
}

/** Pasek podpowiedzi pod przyciskami nagrywania (UTF-8). W trakcie
    nagrywania tylko to, co da sie zrobic jedna reka; przed nagraniem pelny
    spis, bo wtedy jest czas go przeczytac. */
inline std::string hintLine (bool recording, bool hasScript)
{
    if (recording)
        return hasScript ? "Esc — zatrzymaj i oceń   ·   R — powtórz wers   ·   ↑ ↓ — popraw miejsce"
                         : "Esc — zatrzymaj i oceń";

    return hasScript ? "strzałki — popraw miejsce   ·   Home — na początek   ·   "
                       "+ / − — wielkość tekstu   ·   L — lustro   ·   Esc — wyjdź"
                     : "Naciśnij Nagrywaj, kiedy zechcesz zacząć";
}

} // namespace ui
} // namespace zs
