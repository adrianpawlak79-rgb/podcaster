/*
    dziennik-test.cpp — dziennik nagrania z tekstem (DSP/TakeJournal.h).

    Audyt 14.10: po awarii wracal sam WAV; znaczniki R, poczatki wersow i
    uslyszane slowa ginely. Dziennik ma je przeniesc przez zapis i odczyt
    bez strat — takze gdy zapis urwal sie w polowie.
*/

#include "../Source/DSP/TakeJournal.h"

#include <cmath>
#include <cstdio>
#include <string>

static int fails = 0;
static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

int main()
{
    std::printf ("\nDziennik nagrania\n=================\n\n");

    tw::TakeJournal j;
    auto mark = [] (double sec, int a, int b, int shown, tw::LineSource src, const char* t) {
        tw::LineMark m; m.sec = sec; m.firstWord = a; m.lastWord = b; m.shown = shown; m.source = src; m.text = t; return m;
    };
    j.marks.push_back (mark (0.4, 0, 5, 0, tw::LineSource::Words, "Litwo! Ojczyzno moja!\tty jesteś"));
    j.marks.push_back (mark (3.2, 6, 11, 1, tw::LineSource::Syllables, "Ile cię trzeba cenić"));
    auto r = mark (7.9, 6, 11, 1, tw::LineSource::Retake, "Ile cię trzeba cenić");
    r.cutFrom = 6.85; r.cutByHand = true; r.cutTo = 7.75;
    j.marks.push_back (r);
    j.marks.push_back (mark (12.0, 12, 17, 2, tw::LineSource::Key, "ten tylko się dowie, kto cię stracił."));
    j.heard = { { 0.52, 0 }, { 0.91, 1 }, { 8.1, 6 } };
    j.script = "Litwo! Ojczyzno moja! ty jesteś jak zdrowie.\nIle cię trzeba cenić,\n\n#tekst z krzyżykiem\nten tylko się dowie, kto cię stracił.";

    const auto back = tw::TakeJournal::parse (j.toText());
    bool same = back.marks.size() == j.marks.size();
    for (size_t i = 0; same && i < j.marks.size(); ++i)
    {
        const auto& a = j.marks[i]; const auto& b = back.marks[i];
        same = std::fabs (a.sec - b.sec) < 1e-3 && a.firstWord == b.firstWord && a.lastWord == b.lastWord
            && a.shown == b.shown && a.source == b.source && std::fabs (a.cutFrom - b.cutFrom) < 1e-3
            && a.cutByHand == b.cutByHand && std::fabs (a.cutTo - b.cutTo) < 1e-3;
    }
    check (same, "znaczniki wersow i R z miejscami ciec wracaja bez strat", std::to_string (back.marks.size()));
    check (back.marks.size() == 4 && back.marks[0].text == "Litwo! Ojczyzno moja! ty jesteś", "tekst wersu (tabulator zamieniony na spacje)", back.marks.empty() ? "" : back.marks[0].text);
    check (back.heard.size() == 3 && back.heard[2].word == 6 && std::fabs (back.heard[2].sec - 8.1) < 1e-3, "uslyszane slowa wracaja");
    check (back.script == j.script, "tekst promptera wraca co do znaku (z #tekst w srodku)");
    check (back.anyRetake() && back.lastLine() && back.lastLine()->firstWord == 12, "jest R i wiadomo, na ktorym wersie skonczono");

    // urwany zapis: bez sekcji tekstu — znaczniki i slowa i tak sa
    std::string full = j.toText();
    const auto cut = tw::TakeJournal::parse (full.substr (0, full.find ("#tekst")));
    check (cut.marks.size() == 4 && cut.heard.size() == 3 && cut.script.empty(), "urwany dziennik: znaczniki i slowa zostaja");

    const auto half = tw::TakeJournal::parse (full.substr (0, full.size() / 3));
    check (half.marks.size() <= 4, "dziennik urwany w polowie wiersza nie wywraca odczytu", std::to_string (half.marks.size()));

    const auto empty = tw::TakeJournal::parse ("");
    check (empty.marks.empty() && empty.heard.empty() && ! empty.anyRetake() && empty.lastLine() == nullptr, "pusty dziennik: nic, bez wywrotki");

    const auto junk = tw::TakeJournal::parse ("#ciecie\t99\t1\t1\t1\n#slowo\tx\n#ciecie\tzle\n");
    check (junk.marks.empty() && junk.heard.size() <= 1, "smieci w dzienniku: ignorowane");

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
