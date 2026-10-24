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

    - paleta: ciemne tlo, tekst, kolory znaczen i jeden akcent (#39CF7B),
    - trzy rodzaje przyciskow, w tym okragly Nagraj,
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
/* Palety, ARGB. Znaczenia, nie nazwy kolorow — patrz komentarz w Look.h.

   Adrian 13.10: "wszystko zielone jest jednostajne i nudne; zielen tylko dla
   zielonego swiatla". Zielen (#39CF7B) znaczy wiec jedno: DOBRZE — lampka,
   miernik, znaczek przy wersie, "Czytaj", werdykt. Reszta programu (przyciski,
   kroki, zaznaczenia) ma akcent palety. Palety A i B zaprojektowal ChatGPT
   (Docs/projekt/), Klasyczna to dotychczasowa — zostaje do porownania. */
struct Palette
{
    const char* name;
    uint32_t bg, bar, panel, raised, edge;
    uint32_t textNow, textNext, textPast, textDim;
    uint32_t accent, onAccent, select;
    uint32_t green, yellow, red, lostBlue, lostText;
};

enum class PaletteId { Copper, Console, Classic, Plugin };

inline const Palette& paletteFor (PaletteId id)
{
    // A — Studio Copper: ciepla, "studyjna", pasuje do ekranu startowego ze zdjeciem
    static const Palette copper { "Studio Copper",
        0xff080b0e, 0xff0e1318, 0xff141a21, 0xff1e262f, 0xff2b3540,
        0xfff4f1ea, 0xffc6cbd1, 0xff808b97, 0xff9aa6b2,
        0xffc47a44, 0xff1a0e06, 0xff8fa8ff,
        0xff39cf7b, 0xfff4c84a, 0xffef4e57, 0xff6789ef, 0xffa8c0ff };
    // B — Night Console: chlodna, kazdy kolor znaczy jedno
    static const Palette console { "Night Console",
        0xff07090f, 0xff0c111b, 0xff121a28, 0xff1b2537, 0xff2a3549,
        0xffeef3fa, 0xffc3ccda, 0xff7f8ca2, 0xff9ba8bc,
        0xff4f7dff, 0xff050a1a, 0xffb08cff,
        0xff39cf7b, 0xffffd166, 0xffff4d5d, 0xff6789ef, 0xffa8c0ff };
    // dotychczasowa, cala zielona
    static const Palette classic { "Klasyczna",
        0xff141719, 0xff1b1f21, 0xff22272a, 0xff30373a, 0xff3b4547,
        0xfff1f4f2, 0xffb8c2bd, 0xff829089, 0xff929e97,
        0xff39cf7b, 0xff08190f, 0xff39cf7b,
        0xff39cf7b, 0xffe8bd4d, 0xffe36a63, 0xff6789ef, 0xffa8c0ff };
    // D — Wtyczka (24.10, wybor Adriana z kierunkow A-D, wzor: wtyczki RS /
    // Soundshot): granat jak w panelach wtyczek, niebieski = sterowanie,
    // zielen = dobrze, pomarancz = uwaga i wzorzec, czerwien = nagrywanie.
    static const Palette plugin { "Wtyczka",
        0xff0a1220, 0xff101b2d, 0xff0d1829, 0xff12203a, 0xff2c4366,
        0xffe3ecf8, 0xffc3d1e4, 0xff6b7a92, 0xff9fb2cc,
        0xff4ea8ff, 0xff06172e, 0xff7cc0ff,
        0xff39cf7b, 0xfff0a33a, 0xffff6b6b, 0xff6789ef, 0xffa8c0ff };
    return id == PaletteId::Console ? console : id == PaletteId::Classic ? classic
         : id == PaletteId::Plugin ? plugin : copper;
}

inline PaletteId& currentPaletteId() { static PaletteId id = PaletteId::Plugin; return id; }   // Adrian 24.10: wtyczka (D)
inline void setPalette (PaletteId id) { currentPaletteId() = id; }
inline const Palette& pal() { return paletteFor (currentPaletteId()); }

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
/** Glowny i drugorzedny prostokat oraz okragly Nagraj z prototypu.
    Kolory i rysowanie przyciskow ustawia wylacznie Look.h. */
enum class ButtonKind { Primary, Secondary, Record };
constexpr int buttonKindCount = 3;

struct ButtonPaint { uint32_t fill, text, outline; };

inline ButtonPaint paintFor (ButtonKind k)
{
    const auto& c = pal();
    if (k == ButtonKind::Primary || k == ButtonKind::Record) return { c.accent, c.onAccent, c.accent };
    return { c.raised, c.textNow, c.edge };
}

//==============================================================================
/** Pasek krokow: kreator (szesc krokow) i ekran glowny (piec etapow z prototypu). */
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
namespace frame
{
    // Kroki w lewej kolumnie (projekt ChatGPT, 13.10) — pasek kroków u góry zniknął,
    // tekst promptera ma więcej wysokości.
    // 24.10 (kierunek D): kroki to zakladki w naglowku, lewej kolumny nie ma;
    // tresc i prawy panel odsuniete od krawedzi o pad, miedzy nimi gap.
    constexpr int header = 64, steps = 0, footer = 42;
    constexpr int margin = 16, sidePadding = 22, pad = 12, gap = 10;
    inline int navWidth (int) { return 0; }
    inline int sideWidth (int width) { return std::clamp (width / 4, 260, 316); }
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

    /** Przeciagniecie myszka po przebiegu w montazu: zakres do wyciecia.
        Ruch krotszy niz minDragPx to klikniecie (przestaw kursor), nie
        zaznaczenie — inaczej drgniecie reki zaznaczaloby pol sekundy.
        Wynik w sekundach, uporzadkowany i w granicach nagrania; from < 0
        znaczy "to bylo klikniecie". */
    constexpr int minDragPx = 5;

    struct Range { double from = -1.0, to = -1.0; bool isDrag() const { return from >= 0.0 && to > from; } };

    /** Powiekszenie fali w montazu (audyt 14.10: fala godzinnego nagrania
        jest za gesta, zeby trafic w granice slowa). Najwiecej: 4 s na cala
        szerokosc — przy 720 px to ok. 5 ms na piksel, czyli widac pauze
        miedzy slowami. Krok kolka/klawisza: dwa razy. */
    constexpr double minViewSec = 4.0;
    constexpr double zoomStep   = 2.0;

    inline Range dragRange (float x0, float x1, int left, int width, double seconds)
    {
        if (width <= 0 || seconds <= 0.0 || std::abs (x1 - x0) < (float) minDragPx) return {};

        auto at = [&] (float x) { return std::clamp ((double) (x - left) / width, 0.0, 1.0) * seconds; };
        Range r { at (std::min (x0, x1)), at (std::max (x0, x1)) };
        return r.to > r.from ? r : Range{};
    }

    /** Ile zwykle trwa ocena i miks, w sekundach, zaokraglone do 5 s.
        Zmierzone 11 pazdziernika: nagranie 197 s — sluchanie 3,1 s + miks
        36,1 s, czyli 0,2 dlugosci nagrania (serwer testowy; komputer
        Adriana moze byc szybszy). Lepiej obiecac troche za duzo niz za malo. */
    constexpr double mixSecondsPerSecond = 0.1;   ///< 19.10: 0,067 po przyspieszeniu (TruePeak, Limiter), z zapasem

    inline int mixEstimateSec (double recordingSec)
    {
        const double est = std::max (5.0, recordingSec * mixSecondsPerSecond);
        return (int) (std::ceil (est / 5.0) * 5.0);
    }

    /** Przesluchanie nagrania po wczytaniu albo cieciu (listenOffline):
        zmierzone 57 s na godzine (Linux, 14.10). Krotkie nagrania — bez
        obietnic ponizej 5 s. */
    constexpr double listenSecondsPerSecond = 0.02;

    inline int listenEstimateSec (double recordingSec)
    {
        const double est = std::max (5.0, recordingSec * listenSecondsPerSecond);
        return (int) (std::ceil (est / 5.0) * 5.0);
    }
}

//==============================================================================
/** Klawisze ekranu nagrywania.

    Jedna reka, bez patrzenia, bez Ctrl. Esc zatrzymuje i od razu ocenia — to
    jest jedyna rzecz, ktora po nagraniu chce sie zrobic. R cofa prompter na
    poczatek wersu, ktory sie wlasnie pomylilo: czytasz go jeszcze raz, a
    nagranie leci dalej, bo dogrywka jednego wersu nie jest powodem, zeby
    zaczynac caly plik od nowa. */
enum class KeyAction { None, StopAndReview, RepeatLine, LeavePrompter, StartStop };

inline KeyAction keyAction (bool isEscape, char32_t ch, bool recording, bool hasScript)
{
    if (isEscape) return recording ? KeyAction::StopAndReview : KeyAction::LeavePrompter;

    // Spacja: start i stop, jak w kazdym DAW (Adrian 13.10).
    if (ch == U' ') return KeyAction::StartStop;

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
        return hasScript ? "Spacja — pauza / nagraj   ·   R — powtórz wers   ·   Esc — zakończ"
                         : "Spacja albo Esc — zakończ i oceń";

    return hasScript ? "Spacja — nagrywaj   ·   strzałki — popraw miejsce   ·   Home — na początek   ·   "
                       "+ / − — wielkość tekstu   ·   L — lustro   ·   Esc — wyjdź"
                     : "Spacja albo Nagraj — zacznij nagrywać";
}

/** Zamkniecie programu (zasady 20.10: ochrona przed przypadkowa utrata).
    Nagrywanie: najpierw zakonczyc jak Esc (zapis, R do montazu), potem
    zamknac. Niezapisany montaz w pamieci: zapytac. Reszta lezy na dysku. */
namespace closing
{
    enum class Ask { None, Recording, Unsaved };

    inline Ask ask (bool recording, bool unsavedEdits)
    {
        return recording ? Ask::Recording : unsavedEdits ? Ask::Unsaved : Ask::None;
    }
}

/** Najdluzsza sesja (audyt 14.10: "ogranicz i przetestuj dlugosc sesji").

    60 min to tyle, ile sprawdzil zs-dlugo (13.10): nagranie, R, ocena i miks
    godziny w ok. 2,6 GB pamieci. Dluzej nikt nie sprawdzal, a miks trzyma
    w pamieci kilka kopii calosci — wiec program mowi to wprost: od 55. minuty
    licznik ostrzega, w 60. nagranie zatrzymuje sie samo i idzie do oceny
    (jak przy odlaczonej karcie: wszystko zapisane). Rozdzial dluzszy niz
    godzina = dwie sesje. */
namespace session
{
    constexpr double limitSec = 60.0 * 60.0;
    constexpr double warnSec  = 55.0 * 60.0;

    enum class Phase { Ok, Warn, Stop };

    inline Phase phase (double recordedSec)
    {
        if (recordedSec >= limitSec) return Phase::Stop;
        if (recordedSec >= warnSec)  return Phase::Warn;
        return Phase::Ok;
    }

    /** Ile pelnych minut zostalo (do napisu "Zostały 3 min"), co najmniej 1. */
    inline int minutesLeft (double recordedSec)
    {
        return std::max (1, (int) std::ceil ((limitSec - recordedSec) / 60.0));
    }
}

} // namespace ui
} // namespace zs
