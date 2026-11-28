/*
    Tutorial.h — maszyna stanow SAMOUCZKA (bez JUCE, test: Tests/samouczek-test.cpp).

    ---------------------------------------------------------------------------
    PO CO

    Przewodnik z dymkami (TourOverlay.h) pokazuje, co gdzie jest. Samouczek
    prowadzi przez CALY program na prawdziwym, krotkim nagraniu: krok
    przechodzi dalej dopiero wtedy, gdy czlowiek sam wykona czynnosc (kliknie
    Nagraj, zrobi pauze, wytnie fragment...). Tu jest tylko logika: ktory krok,
    kiedy jest wykonany, co po pominieciu, skad wznowic. Wyglad (dymek) i
    podpiecie do ekranow robi MainComponent.

    ---------------------------------------------------------------------------
    JAK

    MainComponent co klatke zegara buduje Observation: na jakim ekranie jest
    program i liczniki czynnosci (ile pauz, ile ciec...). Krok zapamietuje
    liczniki z chwili wejscia (baseline) i jest wykonany, gdy jego licznik
    urosl albo ekran sie zmienil. Liczniki zamiast "zdarzen" — nie trzeba
    niczego kolejkowac, a czynnosc zrobiona przed wejsciem w krok sie nie liczy.

    Gdy program jest juz na DALSZYM ekranie niz krok (np. ktos zakonczyl
    nagranie bez pauzy), samouczek przeskakuje do pierwszego kroku tego ekranu.
    Do tylu nie cofa sie nigdy.

    Tryb "tylko pokaz" (bez mikrofonu): kazdy krok ma "Dalej"; czynnosci dalej
    przesuwaja kroki, jesli da sie je zrobic.
*/

#pragma once

#include <algorithm>
#include <string>

namespace zs::tut
{

enum class Step : int
{
    Welcome,        ///< start: co nas czeka (Przygotowanie, tekst juz wczytany)
    Hardware,       ///< sprzet niesprawdzony: zacheta do "Sprawdz sprzet"
    Prepare,        ///< "Przejdz do nagrania"
    Record,         ///< "Kliknij Nagraj i czytaj"
    Pause,          ///< "Zrob pauze (spacja)"
    LineResume,     ///< "Kliknij wers i Nagraj od linii"
    Repeat,         ///< "Powtorz wers (R)" — opcjonalny
    EndRecording,   ///< "Zakoncz nagranie"
    Listen,         ///< montaz: "Kliknij fale, zeby posluchac"
    Cut,            ///< "Przeciagnij po fali i wytnij fragment"
    ToMix,          ///< "Dalej: miks"
    AbSwitch,       ///< miks: "Przelacz Przed/Po"
    Style,          ///< "Wybierz brzmienie"
    Save,           ///< "Zapisz miks"
    Done,           ///< Gotowe: "Tu jest Twoj plik"
    Finish,         ///< zakonczenie
    Count
};

/** Ekran programu w oczach samouczka (kolejnosc = kolejnosc w programie). */
enum class Screen : int { Other, Start, Wizard, Preparation, Recording, Montage, Mix, Done };

/** Stan programu w chwili zegara. Liczniki tylko rosna (od uruchomienia ekranu). */
struct Observation
{
    Screen screen = Screen::Other;
    bool hardwareChecked = false;
    int pauses = 0, lineResumes = 0, repeats = 0;          // prompter
    int recordings = 0;                                     // ile razy ruszylo nagrywanie
    int listens = 0, cuts = 0, abToggles = 0, styles = 0;   // ocena
};

inline int stepCount() { return (int) Step::Count; }

/** Stale nazwy do ustawien (nie zmieniac — siedza w plikach uzytkownikow). */
inline const char* keyOf (Step s)
{
    static const char* k[] = { "powitanie", "sprzet", "przygotowanie", "nagraj", "pauza", "od-linii",
                               "powtorz", "zakoncz", "posluchaj", "wytnij", "do-miksu", "przed-po",
                               "brzmienie", "zapisz", "gotowe", "koniec" };
    const int i = (int) s;
    return i >= 0 && i < stepCount() ? k[i] : "";
}

/** Krok z nazwy; false, gdy nazwy nie znamy (np. z nowszej wersji). */
inline bool stepFromKey (const std::string& key, Step& out)
{
    for (int i = 0; i < stepCount(); ++i)
        if (key == keyOf ((Step) i)) { out = (Step) i; return true; }
    return false;
}

/** Ekran, na ktorym krok sie dzieje. */
inline Screen screenOf (Step s)
{
    switch (s)
    {
        case Step::Welcome: case Step::Hardware: case Step::Prepare: return Screen::Preparation;
        case Step::Record: case Step::Pause: case Step::LineResume: case Step::Repeat: case Step::EndRecording: return Screen::Recording;
        case Step::Listen: case Step::Cut: case Step::ToMix: return Screen::Montage;
        case Step::AbSwitch: case Step::Style: case Step::Save: return Screen::Mix;
        case Step::Done: case Step::Finish: case Step::Count: return Screen::Done;
    }
    return Screen::Other;
}

/** Krok bez czynnosci do wykonania: zawsze z "Dalej". */
inline bool isInfoStep (Step s) { return s == Step::Welcome || s == Step::Done || s == Step::Finish; }

/** Krok, ktory mozna sobie darowac i mowimy to wprost ("opcjonalnie"). */
inline bool isOptional (Step s) { return s == Step::Repeat || s == Step::Hardware; }

/** Skad wznowic po zamknieciu programu. Nagranie w trakcie nie przetrwa
    zamkniecia, wiec kroki nagrania wracaja do "Przejdz do nagrania". Montaz
    wraca na swoje kroki, jesli plik proby jest na dysku; miks liczy sie od
    nowa, wiec jego kroki zaczynaja od "Dalej: miks". */
inline Step resumePoint (Step saved, bool haveRecording)
{
    switch (screenOf (saved))
    {
        case Screen::Preparation: return saved == Step::Hardware ? Step::Hardware : saved == Step::Welcome ? Step::Welcome : Step::Prepare;
        case Screen::Recording:   return Step::Prepare;
        case Screen::Montage:     return haveRecording ? saved : Step::Prepare;
        case Screen::Mix:         return haveRecording ? Step::ToMix : Step::Prepare;
        case Screen::Done:        return haveRecording ? (saved == Step::Finish ? Step::Finish : Step::Done) : Step::Finish;
        default: break;
    }
    return Step::Welcome;
}

//==============================================================================
class Tutorial
{
public:
    /** Zaczyna od poczatku albo od podanego kroku. showOnly = bez mikrofonu. */
    void begin (bool showOnlyMode, Step from = Step::Welcome)
    {
        on = true; finishedAll = false;
        showOnly = showOnlyMode;
        enter (from);
    }

    bool active() const     { return on; }
    bool completed() const  { return finishedAll; }
    bool isShowOnly() const { return showOnly; }
    Step step() const       { return current; }

    /** Numer kroku dla czlowieka: 1..total (bez kroku sprzetu, gdy go pominiemy, liczymy wszystkie). */
    int number() const { return (int) current + 1; }
    int total() const  { return stepCount(); }

    /** Czy dymek ma przycisk "Dalej" (krok informacyjny albo tryb tylko pokaz). */
    bool hasNext() const { return on && (showOnly || isInfoStep (current)); }

    /** Co klatke zegara. Zwraca true, gdy krok sie zmienil. */
    bool observe (const Observation& o)
    {
        if (! on) return false;
        if (! haveBase) { base = o; haveBase = true; }
        const Step before = current;

        for (int guard = 0; guard < stepCount() && on; ++guard)
        {
            // program jest juz dalej niz krok: pierwszy krok tamtego ekranu
            const Screen want = screenOf (current);
            if (rank (o.screen) > rank (want) && rank (o.screen) >= rank (Screen::Recording))
            {
                const Step first = firstStepOf (o.screen);
                if ((int) first > (int) current) { enterWith (first, o); continue; }
            }
            if (current == Step::Hardware && (o.hardwareChecked || showOnly)) { enterWith (next (current), o); continue; }
            if (! done (o)) break;
            enterWith (next (current), o);
        }
        return current != before;
    }

    /** "Dalej" albo "Pomin ten krok". Na ostatnim kroku konczy (completed). */
    void skip()
    {
        if (! on) return;
        if (current == Step::Finish) { on = false; finishedAll = true; return; }
        enter (next (current));
    }

    /** "Zakoncz samouczek": przerwany, nie ukonczony (krok zostaje do wznowienia). */
    void stop() { on = false; }

    //---- zapis ----------------------------------------------------------------
    /** Tekst do ustawien, np. "nagraj" albo "nagraj;pokaz". */
    std::string save() const
    {
        std::string s = keyOf (current);
        if (showOnly) s += ";pokaz";
        return s;
    }

    /** Odczyt zapisu: krok (przez resumePoint) i tryb. false = nic do wznowienia. */
    static bool load (const std::string& text, bool haveRecording, Step& step, bool& showOnlyMode)
    {
        if (text.empty()) return false;
        const auto semi = text.find (';');
        const std::string key = text.substr (0, semi);
        showOnlyMode = semi != std::string::npos && text.substr (semi + 1) == "pokaz";
        Step s;
        if (! stepFromKey (key, s)) return false;
        step = resumePoint (s, haveRecording);
        return true;
    }

private:
    bool on = false, finishedAll = false, showOnly = false, haveBase = false;
    Step current = Step::Welcome;
    Observation base;

    static int rank (Screen s) { return (int) s; }

    static Step firstStepOf (Screen s)
    {
        switch (s)
        {
            case Screen::Recording: return Step::Record;
            case Screen::Montage:   return Step::Listen;
            case Screen::Mix:       return Step::AbSwitch;
            case Screen::Done:      return Step::Done;
            default: break;
        }
        return Step::Welcome;
    }

    static Step next (Step s) { return (Step) std::min ((int) Step::Finish, (int) s + 1); }

    void enter (Step s) { current = s; haveBase = false; }
    void enterWith (Step s, const Observation& o) { current = s; base = o; haveBase = true; }

    /** Czy czynnosc kroku jest wykonana (od wejscia w krok). */
    bool done (const Observation& o) const
    {
        switch (current)
        {
            case Step::Welcome:      return false;   // "Zaczynamy"
            case Step::Hardware:     return o.hardwareChecked;
            case Step::Prepare:      return o.screen == Screen::Recording && o.screen != base.screen;
            case Step::Record:       return o.recordings > base.recordings;
            case Step::Pause:        return o.pauses > base.pauses;
            case Step::LineResume:   return o.lineResumes > base.lineResumes;
            case Step::Repeat:       return o.repeats > base.repeats;
            case Step::EndRecording: return rank (o.screen) >= rank (Screen::Montage);
            case Step::Listen:       return o.listens > base.listens;
            case Step::Cut:          return o.cuts > base.cuts;
            case Step::ToMix:        return rank (o.screen) >= rank (Screen::Mix);
            case Step::AbSwitch:     return o.abToggles > base.abToggles;
            case Step::Style:        return o.styles > base.styles;
            case Step::Save:         return o.screen == Screen::Done;
            case Step::Done:         return false;
            case Step::Finish:       return false;
            case Step::Count:        return false;
        }
        return false;
    }
};

} // namespace zs::tut
