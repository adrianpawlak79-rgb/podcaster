/*
    zs-mix — miks mowy bez DAW-a i bez wtyczek komercyjnych.

        zs-mix wejscie.wav wyjscie.wav [opcje]

    Wszystko, czego uzywa, jest nasze: brama, wyrownywacz, de-esser, barwa,
    dociskacz, pomiar glosnosci wedlug BS.1770 i limiter szczytu
    miedzyprobkowego. Zero zaleznosci — na Windowsie to jeden plik .exe.

    Program mowi, co zrobil i o ile. To nie jest kokieteria: kazda liczba w
    raporcie da sie sprawdzic w dowolnym innym mierniku, i wlasnie o to chodzi.
*/

#include "../Source/DSP/Mix/Decide.h"
#include "../Source/DSP/Mix/SpeechChain.h"
#include "../Source/DSP/Prosody.h"
#include "../Source/Wav.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <cstdarg>
#include <string>

//==============================================================================
/** Energia tonu w danej czestotliwosci, blok po bloku, z oknem Hanna.

    Okno jest tu nieodzowne. Bez niego przeciek widmowy rozmazuje kazdy ton na
    dziesiatki hercow, a my porownujemy 50 Hz z 44 i 56 Hz — czyli punkty
    lezace blizej siebie niz szerokosc rozmazania. Pierwsza wersja bez okna nie
    wykryla przydzwieku, ktory byl w pliku na pewno, bo "obok" mierzylo to samo
    co "w srodku". */
static void toneEnergyBlocks (const std::vector<float>& x, double hz, double sr,
                              std::vector<double>& outDb)
{
    const int block = 32768;                 // 1,5 Hz na przedzial przy 48 kHz
    const double w = 2.0 * M_PI * hz / sr;
    const double c = 2.0 * std::cos (w);

    outDb.clear();

    for (size_t start = 0; start + (size_t) block <= x.size(); start += (size_t) block)
    {
        double s1 = 0.0, s2 = 0.0;

        for (int i = 0; i < block; ++i)
        {
            const double win = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (block - 1));
            const double s0 = x[start + (size_t) i] * win + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const double mag = (s1 * s1 + s2 * s2 - c * s1 * s2) / ((double) block * block);
        outDb.push_back (mag > 1.0e-24 ? 10.0 * std::log10 (mag) : -200.0);
    }
}

static double blockEnergyDb (const std::vector<float>& x, size_t start, int block)
{
    double sum = 0.0;
    for (int i = 0; i < block && start + (size_t) i < x.size(); ++i)
        sum += (double) x[start + (size_t) i] * x[start + (size_t) i];

    return sum > 1.0e-20 ? 10.0 * std::log10 (sum / block) : -200.0;
}

/** Czy w nagraniu siedzi przydzwiek sieciowy i o jakiej czestotliwosci.

    Dwie rzeczy, bez ktorych to nie dziala:

    PATRZYMY W CISZE. Przydzwiek ma stale kilkadziesiat decybeli ponizej mowy,
    wiec w blokach z mowa ginie. Bierzemy pod uwage tylko najcichsza trzecia
    czesc materialu — tam, gdzie slychac sam pokoj.

    PATRZYMY NA WYSTAWANIE, NIE NA POZIOM. Sama energia na 50 Hz niczego nie
    dowodzi, bo glos tez tam cos ma. Dopiero to, ze 50 Hz wystaje ponad 44 i
    56 Hz, mowi, ze mamy do czynienia z waskim tonem z sieci. */
static double detectHum (const std::vector<float>& x, double sr)
{
    const int block = 32768;

    // ktore bloki sa ciche
    std::vector<std::pair<double, size_t>> energy;
    for (size_t start = 0; start + (size_t) block <= x.size(); start += (size_t) block)
        energy.push_back ({ blockEnergyDb (x, start, block), energy.size() });

    if (energy.size() < 3)
        return 0.0;

    auto sorted = energy;
    std::sort (sorted.begin(), sorted.end());
    const size_t quietCount = std::max ((size_t) 2, sorted.size() / 3);

    std::vector<size_t> quiet;
    for (size_t i = 0; i < quietCount; ++i)
        quiet.push_back (sorted[i].second);

    double bestHz = 0.0, bestMargin = 0.0;

    for (double hz : { 50.0, 60.0 })
    {
        std::vector<double> on, lo, hi;
        toneEnergyBlocks (x, hz, sr, on);
        toneEnergyBlocks (x, hz - 6.0, sr, lo);
        toneEnergyBlocks (x, hz + 6.0, sr, hi);

        std::vector<double> margins;

        for (size_t i : quiet)
            if (i < on.size() && i < lo.size() && i < hi.size())
                margins.push_back (on[i] - std::max (lo[i], hi[i]));

        if (margins.empty())
            continue;

        std::sort (margins.begin(), margins.end());
        const double median = margins[margins.size() / 2];

        if (median > bestMargin) { bestMargin = median; bestHz = hz; }
    }

    return bestMargin > 10.0 ? bestHz : 0.0;
}

//==============================================================================
static void usage()
{
    std::printf (
        "\nzs-mix — miks mowy na wlasnych modulach\n\n"
        "  zs-mix wejscie.wav wyjscie.wav [opcje]\n\n"
        "opcje:\n"
        "\n  Domyslnie program SLUCHA nagrania i sam dobiera ustawienia, a potem\n"
        "  mowi, dlaczego tak. Kazda opcja ponizej nadpisuje jedna jego decyzje.\n\n"
        "  --recznie        nie dobieraj nic sam, uzyj ustawien domyslnych\n"
        "  --gdzie <x>      podcast | youtube | wlasne — dokad to jedzie\n"
        "  --styl <x>       delikatny | studyjny (domyslny) | mocny | pelny — brzmienie miksu\n"
        "  --lufs <x>       cel glosnosci (nadpisuje dobor)\n"
        "                   -14 dla YouTube'a, -19 dla radia w Europie\n"
        "  --sufit <x>      sufit szczytu miedzyprobkowego, domyslnie -1 dBTP\n"
        "  --bez-bramy      nie tlum tla miedzy zdaniami\n"
        "  --bez-deessera   nie tlum sybilantow\n"
        "  --bez-szumu      nie odszumiaj widmowo\n"
        "  --bez-trzaskow   nie usuwaj mlaskow\n"
        "  --bez-ciepla     bez nasycenia\n"
        "  --szum <dB>      sila odszumiania, domyslnie 14\n"
        "  --bity <16|24>   rozdzielczosc zapisu; 16 wlacza dither\n"
        "  --hum <50|60|0>  wymus czestotliwosc przydzwieku (0 = wylacz)\n"
        "  --sucho          bez barwy i bez kompresji: tylko filtr, brama i glosnosc\n"
        "  --raport <plik>  zapisz raport do pliku\n\n");
}

int main (int argc, char** argv)
{
    if (argc < 3) { usage(); return 1; }

    const std::string inPath = argv[1];
    const std::string outPath = argv[2];
    std::string reportPath;

    mix::SpeechChain chain;
    bool autoDecide = true;
    mix::Decide::Wish wish;
    double forcedLufs = 1.0;      // 1.0 = nie podano
    std::vector<std::string> off;
    double forcedHum = -1.0;
    bool dry = false;

    for (int i = 3; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&] () -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };

        if      (a == "--recznie")      autoDecide = false;
        else if (a == "--gdzie")
        {
            const std::string w = next();
            if      (w == "podcast") wish.where = mix::Decide::Destination::Podcast;
            else if (w == "youtube") wish.where = mix::Decide::Destination::YouTube;
            else if (w == "wlasne")  wish.where = mix::Decide::Destination::Wlasne;
            else { std::printf ("nie znam celu: %s\n", w.c_str()); return 1; }
        }
        else if (a == "--styl")
        {
            const std::string w = next();
            if      (w == "delikatny") wish.style = mix::Decide::Style::Delikatny;
            else if (w == "studyjny")  wish.style = mix::Decide::Style::Studyjny;
            else if (w == "mocny")     wish.style = mix::Decide::Style::Mocny;
            else if (w == "pelny")     wish.studio = false;
            else { std::printf ("nie znam stylu: %s (delikatny, studyjny, mocny, pelny)\n", w.c_str()); return 1; }
        }
        else if (a == "--lufs")         { forcedLufs = std::atof (next().c_str()); }
        else if (a == "--sufit")        chain.params.ceilingDbTp = (float) std::atof (next().c_str());
        else if (a == "--bez-bramy")    off.push_back ("useGate");
        else if (a == "--bez-deessera") off.push_back ("useDeEsser");
        else if (a == "--bez-szumu")    off.push_back ("useDenoise");
        else if (a == "--bez-trzaskow") off.push_back ("useDeClick");
        else if (a == "--bez-ciepla")   off.push_back ("useSaturation");
        else if (a == "--szum")         chain.params.denoiseDb = -(float) std::fabs (std::atof (next().c_str()));
        else if (a == "--bity")         chain.params.outputBits = std::atoi (next().c_str());
        else if (a == "--hum")          forcedHum = std::atof (next().c_str());
        else if (a == "--raport")       reportPath = next();
        else if (a == "--sucho")        dry = true;
        else { std::printf ("nieznana opcja: %s\n", a.c_str()); usage(); return 1; }
    }

    if (dry)
    {
        chain.params.mudDb = 0.0f;
        chain.params.presenceDb = 0.0f;
        chain.params.airDb = 0.0f;
        chain.params.levelerRatio = 1.0f;
        chain.params.peakRatio = 1.0f;
        chain.params.useDeEsser = false;
        chain.params.useSaturation = false;
        chain.params.useRider = false;
    }

    // ---- wczytanie -----------------------------------------------------------
    wavio::Audio in;
    const auto err = wavio::read (inPath, in);

    if (! err.empty()) { std::printf ("BLAD: %s\n", err.c_str()); return 2; }

    const auto mono = in.toMono();
    const double sr = (double) in.sampleRate;
    const double seconds = mono.size() / sr;

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
    say ("  %.1f s, %d kanal(y), %d Hz\n", seconds, in.numChannels(), in.sampleRate);

    // ---- dobor ustawien ------------------------------------------------------
    //
    // Idzie PRZED wszystkim innym, bo opcje z linii polecen maja nadpisywac
    // decyzje programu, a nie odwrotnie.
    if (autoDecide)
    {
        wish.ceilingDbTp = chain.params.ceilingDbTp;

        const auto d = mix::Decide::listen (mono, sr, wish);

        const auto keepBits = chain.params.outputBits;
        chain.params = d.params;
        chain.params.outputBits = keepBits;

        say ("\n  co slysze i co z tego wynika\n");

        for (const auto& why : d.because)
        {
            std::string lineBuf = "    ";
            size_t col = 4;

            for (size_t i = 0; i <= why.size(); ++i)
            {
                if (i == why.size() || why[i] == ' ')
                {
                    if (col > 74) { say ("%s\n", lineBuf.c_str()); lineBuf = "    "; col = 4; }
                    if (i == why.size()) break;
                    lineBuf += ' '; ++col;
                }
                else { lineBuf += why[i]; ++col; }
            }

            say ("%s\n\n", lineBuf.c_str());
        }
    }

    for (const auto& o : off)
    {
        if      (o == "useGate")       chain.params.useGate = false;
        else if (o == "useDeEsser")    { chain.params.useDeEsser = false; chain.params.useTame = false; }
        else if (o == "useDenoise")    chain.params.useDenoise = false;
        else if (o == "useDeClick")    chain.params.useDeClick = false;
        else if (o == "useSaturation") chain.params.useSaturation = false;

        say ("    (wylaczone recznie: %s)\n", o.c_str());
    }

    if (forcedLufs < 0.0)
    {
        chain.params.targetLufs = forcedLufs;
        say ("    (cel glosnosci nadpisany recznie: %.1f LUFS)\n\n", forcedLufs);
    }

    // ---- przydzwiek ----------------------------------------------------------
    double hum = forcedHum >= 0.0 ? forcedHum : detectHum (mono, sr);

    chain.params.deHum = hum > 0.0;
    if (hum > 0.0) chain.params.humHz = hum;

    if (hum > 0.0) say ("  przydzwiek %.0f Hz — wycinam go i dwie harmoniczne\n", hum);
    else           say ("  przydzwieku nie widac\n");

    // ---- miks ----------------------------------------------------------------
    chain.setSampleRate (sr);
    chain.prepare (sr);

    std::vector<float> mixed;
    const auto rep = chain.render (mono, mixed);

    say ("  tlo pokoju %.1f dBFS, mowa %.1f dBFS — roznica %.0f dB\n",
         rep.noiseFloorDb, rep.speechDb, rep.speechDb - rep.noiseFloorDb);

    if (chain.params.useGate)
        say ("  prog bramy ustawiony na %.0f dBFS\n", rep.gateThreshDb);

    say ("\n  co bylo          %6.1f LUFS   szczyt %5.1f dBTP\n", rep.lufsBefore, rep.peakBefore);
    say ("  co jest          %6.1f LUFS   szczyt %5.1f dBTP\n", rep.lufsAfter, rep.peakAfter);
    say ("  wzmocnienie      %+6.1f dB\n", rep.gainAppliedDb);

    if (rep.clicksRepaired > 0)
        say ("  naprawione trzaski: %d\n", rep.clicksRepaired);

    if (rep.denoiseDb < 0.0)
        say ("  odszumianie widmowe wlaczone (dno %.0f dB)\n", rep.denoiseDb);
    else if (chain.params.useDenoise)
        say ("  odszumianie niepotrzebne — tlo dosc nisko\n");

    say ("\n  najwieksza praca na etapie:\n");
    if (chain.params.usePlosive)  say ("    plozywy        %6.1f dB\n", rep.plosiveMaxGrDb);
    if (chain.params.useGate)     say ("    brama          %6.1f dB\n", rep.gateMaxGrDb);
    if (chain.params.useRider)    say ("    jazda suwakiem %6.1f dB\n", rep.riderMaxDb);
    say ("    wyrownywacz    %6.1f dB\n", rep.levelMaxGrDb);
    if (chain.params.useDeEsser)  say ("    de-esser       %6.1f dB\n", rep.essMaxGrDb);
    say ("    dociskacz      %6.1f dB\n", rep.peakMaxGrDb);

    // ---- ocena ---------------------------------------------------------------
    say ("\n");

    if (rep.lufsBefore < -35.0)
        say ("  UWAGA: material byl bardzo cichy (%.1f LUFS). Podniesienie go do celu\n"
             "  podnioslo tez szum tla. Nastepnym razem celuj w -12 dBFS na szczytach.\n",
             rep.lufsBefore);

    if (rep.peakBefore > -0.2)
        say ("  UWAGA: w oryginale byl przester (%.1f dBTP). Scietych probek nie da sie\n"
             "  odzyskac — to slychac jako trzaski i zadna wtyczka tego nie cofnie.\n",
             rep.peakBefore);

    if (rep.speechDb - rep.noiseFloorDb < 30.0)
        say ("  UWAGA: tlo pokoju jest glosne — tylko %.0f dB ponizej mowy. Brama je\n"
             "  przyciszy miedzy zdaniami, ale POD mowa zostanie. Wylacz wentylator,\n"
             "  zamknij okno, odsun komputer.\n", rep.speechDb - rep.noiseFloorDb);

    if (rep.levelMaxGrDb < -12.0)
        say ("  Wyrownywacz musial sciagac do %.0f dB — to znaczy, ze roznice glosnosci\n"
             "  miedzy zdaniami byly duze. Staly dystans od mikrofonu bardzo tu pomaga.\n",
             rep.levelMaxGrDb);

    // ---- jak to bylo powiedziane ---------------------------------------------
    // Poziom i szum sa sprawa sprzetu i da sie je ustawic raz. To ponizej jest
    // sprawa czlowieka i wraca przy kazdym nagraniu.
    {
        tw::Prosody pro;
        const auto pr = pro.analyse (mono, sr);

        if (pr.syllables > 20)
        {
            say ("\n  tempo %.0f slow na minute", pr.wordsPerMin);

            if (pr.wordsPerMin > 170.0)      say ("  — szybko, warto zwolnic\n");
            else if (pr.wordsPerMin < 100.0) say ("  — wolno, ale to bywa zamierzone\n");
            else                             say ("  — w dobrym zakresie\n");

            say ("  melodia: rozrzut %.1f poltonu%s\n", pr.f0RangeSemis,
                 pr.monotone ? " — brzmi jak czytanie z kartki" : "");

            if (pr.longPauses > 0)
                say ("  przerw dluzszych niz 2,5 s: %d (najdluzsza %.1f s)\n",
                     pr.longPauses, pr.longestPauseSec);

            if (! pr.fillerAtSec.empty())
            {
                say ("  podejrzenia o przeciagniete \"yyy\": %d —",
                     (int) pr.fillerAtSec.size());

                for (size_t i = 0; i < pr.fillerAtSec.size() && i < 6; ++i)
                    say (" %d:%02d", (int) pr.fillerAtSec[i] / 60,
                         (int) pr.fillerAtSec[i] % 60);

                say ("\n");
            }
        }
    }

    // ---- zapis ---------------------------------------------------------------
    wavio::Audio result;
    result.sampleRate = in.sampleRate;
    result.channels.push_back (mixed);

    const auto werr = chain.params.outputBits <= 16 ? wavio::write16 (outPath, result)
                                                    : wavio::write24 (outPath, result);

    if (! werr.empty()) { std::printf ("BLAD: %s\n", werr.c_str()); return 3; }

    say ("\n  zapisane: %s\n\n", outPath.c_str());

    if (! reportPath.empty())
    {
        FILE* f = std::fopen (reportPath.c_str(), "w");
        if (f) { std::fputs (out.c_str(), f); std::fclose (f); }
    }

    return 0;
}
