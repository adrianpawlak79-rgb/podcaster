/*
    zs-odniesienie — wskazujesz nagranie, ktore ma brzmiec tak, jak chcesz.

        zs-odniesienie wiersz.wav --jak ulubione.wav wynik.wav

    Program zdejmuje z ulubionego nagrania profil — glosnosc, oddech, ile w nim
    ciszy, jaka barwa — i celuje w niego zamiast w norme podcastowa.

    ---------------------------------------------------------------------------
    DLACZEGO TO ISTNIEJE

    Norma -16 LUFS jest dobra dla mowy sluchanej w samochodzie. Nie jest dobra
    dla wszystkiego, a program, ktory zna jedna liczbe i trzyma sie jej, robi
    kazdemu to samo. Czlowiek nie potrafi powiedziec "chce -19,5 LUFS przy
    rozrzucie 24 LU". Potrafi powiedziec "chce brzmiec jak to". To jest to samo
    zdanie, tylko w jezyku, w ktorym ludzie mowia.

    ---------------------------------------------------------------------------
    CZEGO TO NIE ZROBI

    Nie zrobi z glosu piosenki. Jesli wskazesz gotowy miks z perkusja i
    gitarami, program to zauwazy — bo w takim nagraniu prawie nie ma ciszy — i
    powie o tym wprost, zamiast po cichu podglosnic Twoj glos do poziomu, przy
    ktorym bedzie meczyl.
*/

#include "../Source/DSP/Mix/Reference.h"
#include "../Source/DSP/Mix/SpeechChain.h"
#include "../Source/Wav.h"

#include <cstdio>
#include <string>

static void usage()
{
    std::printf (
        "\nzs-odniesienie — celuj w nagranie, nie w norme\n\n"
        "  zs-odniesienie wejscie.wav --jak odniesienie.wav [wyjscie.wav]\n\n"
        "opcje:\n"
        "  --jak <plik>     nagranie, ktore ma byc wzorem\n"
        "  --tylko-pomiar   pokaz roznice i nic nie zapisuj\n"
        "  --w-miksie       material ma podklad (domyslnie: sam glos)\n\n");
}

int main (int argc, char** argv)
{
    if (argc < 2) { usage(); return 1; }

    std::string inPath = argv[1], refPath, outPath;
    bool measureOnly = false, voiceAlone = true;

    for (int i = 2; i < argc; ++i)
    {
        const std::string a = argv[i];

        if      (a == "--jak" && i + 1 < argc) refPath = argv[++i];
        else if (a == "--tylko-pomiar")        measureOnly = true;
        else if (a == "--w-miksie")            voiceAlone = false;
        else if (a[0] != '-')                  outPath = a;
        else { std::printf ("nieznana opcja: %s\n", a.c_str()); usage(); return 1; }
    }

    if (refPath.empty()) { usage(); return 1; }
    if (outPath.empty()) measureOnly = true;

    // ---- wczytanie -----------------------------------------------------------
    wavio::Audio in, ref;

    if (auto e = wavio::read (inPath, in); ! e.empty())
    { std::printf ("BLAD (wejscie): %s\n", e.c_str()); return 2; }

    if (auto e = wavio::read (refPath, ref); ! e.empty())
    { std::printf ("BLAD (odniesienie): %s\n", e.c_str()); return 2; }

    const auto src = in.toMono();
    const auto rfm = ref.toMono();
    const double sr = (double) in.sampleRate;

    // ---- profile -------------------------------------------------------------
    const auto pSrc = mix::Reference::measure (src, sr);
    const auto pRef = mix::Reference::measure (rfm, (double) ref.sampleRate);

    auto show = [] (const char* title, const mix::Profile& p)
    {
        std::printf ("\n  %s\n", title);
        std::printf ("    glosnosc      %6.1f LUFS\n", p.lufs);
        std::printf ("    szczyt        %6.1f dBTP   (zapas %.1f dB)\n", p.truePeakDb, p.plr);
        std::printf ("    oddech        %6.1f LU\n", p.spreadLu);
        std::printf ("    u szczytu     %6.0f %% czasu\n", p.nearPct);
        std::printf ("    cisza         %6.0f %% czasu\n", p.quietPct);
    };

    std::printf ("\n%s\n", inPath.c_str());
    show ("to, co masz", pSrc);
    show ("to, na co celujesz", pRef);

    // ---- co wolno wziac ------------------------------------------------------
    const auto plan = mix::Reference::plan (pSrc, pRef, voiceAlone);

    std::printf ("\n  co z tego biore\n");

    for (const auto& n : plan.notes)
    {
        // zawijanie na 72 znakach, zeby dalo sie to przeczytac w konsoli
        std::string line = "    ";
        size_t col = 4;

        for (size_t i = 0; i <= n.size(); ++i)
        {
            if (i == n.size() || n[i] == ' ')
            {
                if (col > 72) { std::printf ("%s\n", line.c_str()); line = "    "; col = 4; }
                if (i == n.size()) break;
                line += ' '; ++col;
            }
            else { line += n[i]; ++col; }
        }

        std::printf ("%s\n\n", line.c_str());
    }

    const double target = plan.targetLufs;
    std::printf ("  cel glosnosci: %.1f LUFS\n", target);

    // ---- gdzie konczy sie glosnosc bez strat ---------------------------------
    const double free = mix::Reference::loudestUntouched (pSrc);
    const double squash = mix::Reference::squashAtTarget (pSrc, target);

    std::printf ("\n  najglosniej bez ruszania najglosniejszego momentu: %.1f LUFS\n", free);

    if (squash > 0.3)
        std::printf ("  przy celu %.1f LUFS limiter musi go sciagnac o %.1f dB\n",
                     target, squash);
    else
        std::printf ("  cel miesci sie ponizej tej granicy — krzyk zostaje caly\n");

    if (measureOnly)
    {
        std::printf ("\n  (tylko pomiar — nic nie zapisano)\n\n");
        return 0;
    }

    // ---- render --------------------------------------------------------------
    mix::SpeechChain chain;
    chain.params.targetLufs = target;

    // Gdy odniesienie oddycha szeroko, kompresja ma sie cofnac. To nie jest
    // gust — to jest jedyny sposob, zeby oddech w ogole mogl sie zgodzic.
    if (! plan.matchDynamics || pRef.spreadLu > 22.0)
    {
        chain.params.useRider        = false;
        chain.params.levelerThreshDb = -18.0f;
        chain.params.levelerRatio    = 1.5f;
        chain.params.peakThreshDb    = -8.0f;
        chain.params.peakRatio       = 2.5f;
    }

    chain.prepare (sr);

    std::vector<float> out;
    const auto rep = chain.render (src, out);

    wavio::Audio res;
    res.sampleRate = in.sampleRate;
    res.channels.push_back (out);

    if (auto e = wavio::write24 (outPath, res); ! e.empty())
    { std::printf ("BLAD (zapis): %s\n", e.c_str()); return 3; }

    const auto pOut = mix::Reference::measure (out, sr);

    show ("to, co wyszlo", pOut);

    std::printf ("\n  roznice wobec odniesienia:\n");

    for (const auto& line : mix::Reference::compare (pOut, pRef))
        std::printf ("    %s\n", line.c_str());

    std::printf ("\n  zapisane: %s\n\n", outPath.c_str());
    (void) rep;
    return 0;
}
