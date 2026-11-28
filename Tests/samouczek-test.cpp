/*
    samouczek-test.cpp — maszyna stanow samouczka (Source/Tutorial.h).

    Sprawdza bez okna i bez mikrofonu: przejscia po czynnosciach, to, ze
    czynnosc sprzed wejscia w krok sie nie liczy, przeskok, gdy program jest
    juz na dalszym ekranie, pomijanie, tryb "tylko pokaz", zapis i wznowienie.
*/

#include "../Source/Tutorial.h"

#include <cstdio>
#include <string>

using namespace zs::tut;

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static std::string name (Step s) { return keyOf (s); }

int main()
{
    std::printf ("samouczek-test\n");

    // ---- caly przebieg czynnosciami ------------------------------------------
    {
        Tutorial t;
        Observation o; o.screen = Screen::Preparation; o.hardwareChecked = false;
        t.begin (false);
        t.observe (o);
        check (t.step() == Step::Welcome, "start: powitanie", name (t.step()));
        check (t.hasNext(), "powitanie ma Dalej");
        t.skip();
        t.observe (o);
        check (t.step() == Step::Hardware, "sprzet niesprawdzony: krok sprzetu", name (t.step()));
        check (! t.hasNext(), "krok czynnosci bez Dalej (jest Pomin)");
        o.screen = Screen::Wizard; t.observe (o);
        check (t.step() == Step::Hardware, "w kreatorze krok czeka", name (t.step()));
        o.screen = Screen::Preparation; o.hardwareChecked = true; t.observe (o);
        check (t.step() == Step::Prepare, "po kreatorze: przygotowanie", name (t.step()));
        o.screen = Screen::Recording; t.observe (o);
        check (t.step() == Step::Record, "Przejdz do nagrania -> Nagraj", name (t.step()));
        o.recordings = 1; t.observe (o);
        check (t.step() == Step::Pause, "nagranie ruszylo -> pauza", name (t.step()));
        o.pauses = 1; t.observe (o);
        check (t.step() == Step::LineResume, "pauza -> nagraj od linii", name (t.step()));
        o.lineResumes = 1; t.observe (o);
        check (t.step() == Step::Repeat, "od linii -> powtorz wers", name (t.step()));
        o.repeats = 1; o.pauses = 2; t.observe (o);
        check (t.step() == Step::EndRecording, "R -> zakoncz (pauza z R nie przeskakuje dalej)", name (t.step()));
        o.screen = Screen::Montage; t.observe (o);
        check (t.step() == Step::Listen, "koniec nagrania -> posluchaj", name (t.step()));
        o.cuts = 1; t.observe (o);
        check (t.step() == Step::Listen, "ciecie przed sluchaniem nie zalicza sluchania", name (t.step()));
        o.listens = 1; t.observe (o);
        check (t.step() == Step::Cut, "klik w fale -> wytnij", name (t.step()));
        o.cuts = 2; t.observe (o);
        check (t.step() == Step::ToMix, "ciecie -> dalej: miks", name (t.step()));
        o.screen = Screen::Mix; t.observe (o);
        check (t.step() == Step::AbSwitch, "miks -> przed/po", name (t.step()));
        o.styles = 1; t.observe (o);
        check (t.step() == Step::AbSwitch, "brzmienie nie zalicza przed/po", name (t.step()));
        o.abToggles = 1; t.observe (o);
        check (t.step() == Step::Style, "przed/po -> brzmienie", name (t.step()));
        o.styles = 2; t.observe (o);
        check (t.step() == Step::Save, "brzmienie -> zapisz", name (t.step()));
        o.screen = Screen::Done; t.observe (o);
        check (t.step() == Step::Done, "zapis -> gotowe", name (t.step()));
        t.skip();
        check (t.step() == Step::Finish && t.active(), "gotowe -> zakonczenie", name (t.step()));
        t.skip();
        check (! t.active() && t.completed(), "ostatnie Dalej konczy i oznacza ukonczenie");
    }

    // ---- sprzet sprawdzony: krok sprzetu znika sam ---------------------------
    {
        Tutorial t; Observation o; o.screen = Screen::Preparation; o.hardwareChecked = true;
        t.begin (false); t.observe (o); t.skip(); t.observe (o);
        check (t.step() == Step::Prepare, "sprzet sprawdzony: od razu przygotowanie", name (t.step()));
    }

    // ---- przeskok na dalszy ekran, nigdy do tylu ------------------------------
    {
        Tutorial t; Observation o; o.screen = Screen::Recording;
        t.begin (false, Step::Pause); t.observe (o);
        o.screen = Screen::Montage; t.observe (o);
        check (t.step() == Step::Listen, "koniec nagrania w kroku pauzy -> posluchaj", name (t.step()));
        o.screen = Screen::Recording; t.observe (o);
        check (t.step() == Step::Listen, "powrot na nagranie nie cofa samouczka", name (t.step()));
        o.screen = Screen::Done; t.observe (o);
        check (t.step() == Step::Done, "zapis z montazu -> gotowe", name (t.step()));
    }

    // ---- czynnosc sprzed wejscia w krok sie nie liczy -------------------------
    {
        Tutorial t; Observation o; o.screen = Screen::Recording; o.pauses = 5; o.recordings = 1;
        t.begin (false, Step::Pause); t.observe (o);
        check (t.step() == Step::Pause, "stare pauzy nie zaliczaja kroku", name (t.step()));
    }

    // ---- pomijanie ------------------------------------------------------------
    {
        Tutorial t; Observation o; o.screen = Screen::Recording;
        t.begin (false, Step::Repeat); t.observe (o);
        check (isOptional (Step::Repeat), "powtorz wers jest opcjonalny");
        t.skip(); t.observe (o);
        check (t.step() == Step::EndRecording, "Pomin ten krok -> nastepny", name (t.step()));
        t.stop();
        check (! t.active() && ! t.completed(), "Zakoncz samouczek: przerwany, nie ukonczony");
        check (t.save() == "zakoncz", "krok zostaje do wznowienia", t.save());
    }

    // ---- tryb tylko pokaz -----------------------------------------------------
    {
        Tutorial t; Observation o; o.screen = Screen::Preparation;
        t.begin (true); t.observe (o); t.skip(); t.observe (o);
        check (t.step() == Step::Prepare, "bez mikrofonu krok sprzetu pominiety", name (t.step()));
        check (t.hasNext(), "bez mikrofonu kazdy krok ma Dalej");
        t.skip(); t.observe (o);
        check (t.step() == Step::Record, "Dalej przesuwa bez czynnosci", name (t.step()));
        check (t.save() == "nagraj;pokaz", "zapis pamieta tryb", t.save());
    }

    // ---- zapis i odczyt -------------------------------------------------------
    {
        Step s = Step::Welcome; bool show = true;
        check (! Tutorial::load ("", false, s, show), "pusty zapis: nic do wznowienia");
        check (! Tutorial::load ("cos-nowego", false, s, show), "nieznany krok: nic do wznowienia");
        check (Tutorial::load ("wytnij", true, s, show) && s == Step::Cut && ! show, "montaz z plikiem: ten sam krok", name (s));
        check (Tutorial::load ("wytnij", false, s, show) && s == Step::Prepare, "montaz bez pliku: od przygotowania", name (s));
        check (Tutorial::load ("pauza", true, s, show) && s == Step::Prepare, "nagranie nie przetrwa zamkniecia: przygotowanie", name (s));
        check (Tutorial::load ("brzmienie;pokaz", true, s, show) && s == Step::ToMix && show, "miks: od Dalej: miks, tryb pokaz", name (s));
        check (Tutorial::load ("sprzet", false, s, show) && s == Step::Hardware, "sprzet: ten sam krok", name (s));
        check (Tutorial::load ("gotowe", true, s, show) && s == Step::Done, "gotowe: gotowe", name (s));
        check (Tutorial::load ("koniec", false, s, show) && s == Step::Finish, "koniec bez pliku: zakonczenie", name (s));

        // kazda nazwa w obie strony
        bool all = true;
        for (int i = 0; i < stepCount(); ++i) { Step x; all = all && stepFromKey (keyOf ((Step) i), x) && x == (Step) i; }
        check (all, "nazwy krokow w obie strony");

        Tutorial t; Observation o; o.screen = Screen::Montage;
        t.begin (false, Step::Cut); t.observe (o);
        Step r; bool sh;
        check (Tutorial::load (t.save(), true, r, sh) && r == Step::Cut, "zapis -> odczyt -> ten sam krok", t.save());
    }

    std::printf (fails == 0 ? "\nWSZYSTKO OK\n" : "\nBLEDY: %d\n", fails);
    return fails == 0 ? 0 : 1;
}
