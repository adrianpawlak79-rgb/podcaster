/*
    zs-montaz — z całej sesji robi odcinek.

        zs-montaz sesja.wav odcinek.wav [opcje]

    Wycina nieudane podejścia i zostawia te, które je zastąpiły. Program sam
    wie, które frazy były nieudane, bo słyszał je w trakcie nagrywania — nie
    zgaduje tego z zapisu po fakcie.

    ---------------------------------------------------------------------------
    BEZPIECZNIK

    Domyślnie wycinane jest TYLKO takie powtórzenie, którego wcześniejsze
    podejście zostało zgłoszone w trakcie nagrywania: plozywa, przester, zły
    start. Dwa zdania, które jedynie są do siebie podobne, zostają oba. Zawsze.

    To jest warunek, żeby komukolwiek powierzyć sesję. Program, który może
    skasować dobre podejście, bo "brzmiało podobnie", jest nie do użycia — i
    żadna liczba poprawnych decyzji tego nie odkupi.

    ---------------------------------------------------------------------------
    NIC NIE ZNIKA

    Plik źródłowy nie jest ruszany. Raport podaje czasy w ŹRÓDLE, więc każde
    cięcie da się znaleźć i odsłuchać. Gdy programowi nie podoba się szew,
    mówi o tym zamiast milczeć.
*/

#include "../Source/DSP/MontageRender.h"
#include "../Source/DSP/TakeWatchCore.h"
#include "../Source/Wav.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static std::string mmss (double s)
{
    char b[32];
    std::snprintf (b, sizeof (b), "%02d:%02d", (int) s / 60, (int) s % 60);
    return b;
}

static const char* kindName (tw::Kind k)
{
    switch (k)
    {
        case tw::Kind::DigitalSilence: return "cisza cyfrowa";
        case tw::Kind::Clipping:       return "przester";
        case tw::Kind::TooHot:         return "za gorąco";
        case tw::Kind::Plosive:        return "plozywa";
        case tw::Kind::MouthClick:     return "mlask";
        case tw::Kind::Breath:         return "oddech";
        case tw::Kind::Sibilance:      return "sybilant";
        case tw::Kind::OffAxis:        return "odsunięcie";
        case tw::Kind::LevelDrift:     return "poziom odjeżdża";
        case tw::Kind::NoiseRise:      return "szum rośnie";
        case tw::Kind::Hum:            return "przydźwięk";
        case tw::Kind::FalseStart:     return "zły start";
        case tw::Kind::LowLevel:       return "za cicho";
    }
    return "?";
}

//==============================================================================
static void usage()
{
    std::printf (
        "\nzs-montaz — z całej sesji robi odcinek\n\n"
        "  zs-montaz sesja.wav odcinek.wav [opcje]\n\n"
        "opcje:\n"
        "  --tylko-plan     pokaż, co by wyciął, i nic nie zapisuj\n"
        "  --tnij-wszystko  wycinaj też nieudane frazy BEZ powtórki\n"
        "                   (domyślnie zostają — lepiej mieć je i wyciąć ręcznie)\n"
        "  --podobienstwo <0..1>  jak podobne muszą być dwa podejścia, domyślnie 0,70\n"
        "  --raport <plik>  zapisz raport do pliku\n\n");
}

int main (int argc, char** argv)
{
    if (argc < 2) { usage(); return 1; }

    const std::string inPath = argv[1];
    std::string outPath = argc > 2 && argv[2][0] != '-' ? argv[2] : std::string();
    std::string reportPath;

    tw::MontageOptions opt;
    bool planOnly = false;

    for (int i = outPath.empty() ? 2 : 3; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&] () -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };

        if      (a == "--tylko-plan")    planOnly = true;
        else if (a == "--tnij-wszystko") opt.cutUnreplacedBad = true;
        else if (a == "--podobienstwo")  opt.retakeSimilarity = (float) std::atof (next().c_str());
        else if (a == "--raport")        reportPath = next();
        else { std::printf ("nieznana opcja: %s\n", a.c_str()); usage(); return 1; }
    }

    if (outPath.empty()) planOnly = true;

    // ---- wczytanie -----------------------------------------------------------
    wavio::Audio in;
    const auto err = wavio::read (inPath, in);

    if (! err.empty()) { std::printf ("BLAD: %s\n", err.c_str()); return 2; }

    const auto mono = in.toMono();
    const double sr = (double) in.sampleRate;

    std::string out;
    char line[512];

    auto say = [&] (const char* fmt, ...)
    {
        va_list ap;
        va_start (ap, fmt);
        std::vsnprintf (line, sizeof (line), fmt, ap);
        va_end (ap);
        out += line;
        std::fputs (line, stdout);
    };

    say ("\n%s\n", inPath.c_str());
    say ("  %.1f s, %d Hz\n", mono.size() / sr, in.sampleRate);

    // ---- przesluchanie sesji -------------------------------------------------
    tw::TakeWatch watch;
    watch.prepare (sr, 512);
    watch.reset();

    std::vector<tw::Event> events;

    for (size_t pos = 0; pos < mono.size(); pos += 512)
    {
        const int n = (int) std::min ((size_t) 512, mono.size() - pos);
        const float* ch[1] = { mono.data() + pos };
        watch.process (ch, 1, n, (double) pos / sr);

        tw::Event e;
        while (watch.nextEvent (e)) events.push_back (e);
    }

    say ("  fraz: %d, zdarzeń: %d\n", watch.numPhrases(), (int) events.size());

    if (watch.numPhrases() < 2)
    {
        say ("\n  Za mało fraz, żeby był z czego montować.\n\n");
        return 0;
    }

    // ---- plan ----------------------------------------------------------------
    const auto plan = tw::planMontage (watch, opt);

    int toCut = 0;
    for (const auto& it : plan.items)
        if (it.action != tw::Action::Keep) ++toCut;

    say ("\n  do wycięcia: %d\n", toCut);

    if (toCut == 0)
    {
        say ("\n  Nic do wycięcia — żadne nieudane podejście nie doczekało się powtórki.\n");

        if (! opt.cutUnreplacedBad)
            say ("  (nieudane frazy bez powtórki zostają; --tnij-wszystko to zmienia)\n");
    }

    for (const auto& it : plan.items)
    {
        if (it.action == tw::Action::Keep) continue;

        say ("    %s–%s  %-12s %s\n",
             mmss (it.startSec).c_str(), mmss (it.endSec).c_str(),
             it.action == tw::Action::CutRetake ? "powtórzone"
           : it.action == tw::Action::CutBad    ? "nieudane" : "przerwa",
             it.reason.c_str());
    }

    // ---- co program zgłosił w trakcie ----------------------------------------
    if (! events.empty())
    {
        say ("\n  zgłoszone w trakcie nagrywania:\n");

        int shown = 0;

        for (const auto& e : events)
        {
            if ((int) e.severity < 1) continue;
            if (++shown > 12) { say ("    ...i %d więcej\n", (int) events.size() - shown); break; }

            say ("    %s  %s\n", mmss (e.timeSec).c_str(), kindName (e.kind));
        }
    }

    if (planOnly)
    {
        say ("\n  (tylko plan — nic nie zapisano)\n\n");
        return 0;
    }

    // ---- wykonanie -----------------------------------------------------------
    tw::MontageRender render;
    std::vector<float> edited;
    const auto rep = render.render (mono, sr, plan, edited);

    say ("\n  %.1f s  ->  %.1f s   (cięć: %d", rep.originalSec, rep.renderedSec, rep.cutsMade);

    if (rep.cutsRefused > 0) say (", odmów: %d", rep.cutsRefused);
    say (")\n");

    for (const auto& r : rep.refusals)
        say ("    %s\n", r.c_str());

    // ---- szwy ----------------------------------------------------------------
    int audible = 0;
    for (const auto& s : rep.seams) if (s.audible) ++audible;

    if (! rep.seams.empty())
    {
        say ("\n  szwy: %d, podejrzanych: %d\n", (int) rep.seams.size(), audible);

        for (const auto& s : rep.seams)
            if (s.audible)
                say ("    %s  %s (poziom %+.1f dB, barwa %+.1f dB)\n",
                     mmss (s.atSec).c_str(), s.note.c_str(), s.levelStepDb, s.tiltStepDb);
    }

    if (audible == 0 && ! rep.seams.empty())
        say ("  Żadnego szwu nie powinno być słychać.\n");

    // ---- zapis ---------------------------------------------------------------
    wavio::Audio result;
    result.sampleRate = in.sampleRate;
    result.channels.push_back (edited);

    const auto werr = wavio::write24 (outPath, result);

    if (! werr.empty()) { std::printf ("BLAD: %s\n", werr.c_str()); return 3; }

    say ("\n  zapisane: %s\n", outPath.c_str());
    say ("  Oryginał nietknięty — czasy w raporcie są z niego, więc każde\n"
         "  cięcie da się odsłuchać i cofnąć.\n\n");

    if (! reportPath.empty())
    {
        FILE* f = std::fopen (reportPath.c_str(), "w");
        if (f) { std::fputs (out.c_str(), f); std::fclose (f); }
    }

    return 0;
}
