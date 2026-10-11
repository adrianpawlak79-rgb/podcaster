/*
    zs-sledzenie.cpp — pomiar promptera na prawdziwym nagraniu.

    Druga polowa pomiaru; pierwsza to Tools/vosk-hipotezy.py. Liczy dokladnie
    tym samym WordTracker, ktory chodzi w programie — wiec liczba stad to
    liczba programu, a nie osobnej implementacji w Pythonie.

    Budowanie (bez JUCE):
        g++ -O2 -std=c++17 -ISource Tools/zs-sledzenie.cpp -o zs-sledzenie

    Uzycie:
        zs-sledzenie --slownik skrypt.txt  > slownik.txt
        python3 Tools/vosk-hipotezy.py MODEL nagranie-16k.wav slownik.txt wynik
        zs-sledzenie skrypt.txt wynik.hip wynik.czasy [opoznienie_s]

    Co mierzy: co 0,1 s porownuje slowo, ktore pokazuje prompter, ze slowem,
    ktore wedlug wynik.czasy wlasnie padlo. Podaje odsetek chwil z bledem
    <= 1 i <= 2 slowa oraz odsetek chwil z trafionym wersem (wers = wiersz
    pliku ze skryptem). Dla porownania to samo bez kompensacji opoznienia.
*/

#include "DSP/LineLog.h"
#include "DSP/CutPoint.h"
#include "DSP/WordTracker.h"
#include "DSP/PrompterLines.h"
#include "DSP/SyllableClock.h"
#include "ModelWords.h"
#include "Wav.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static std::string readAll (const char* path)
{
    std::ifstream f (path, std::ios::binary);
    if (! f) { std::fprintf (stderr, "nie moge otworzyc %s\n", path); std::exit (1); }
    std::stringstream b;
    b << f.rdbuf();
    return b.str();
}

struct Hyp   { double t; bool final; std::vector<std::string> words; };
struct Timed { double start, end; std::string key; };

static std::vector<std::string> split (const std::string& s)
{
    std::vector<std::string> out;
    std::istringstream in (s);
    for (std::string w; in >> w;) out.push_back (w);
    return out;
}

/** Wzorzec: ktore slowo skryptu kiedy padlo. Slowa z .czasy dopasowane do
    skryptu najdluzszym wspolnym podciagiem — slowa, ktorych nie da sie
    dopasowac (pomylka, wtracenie), po prostu nie sa wzorcem. */
static std::vector<double> alignTruth (const std::vector<std::string>& script, const std::vector<Timed>& heard)
{
    const size_t n = script.size(), m = heard.size();
    std::vector<std::vector<int>> L (n + 1, std::vector<int> (m + 1, 0));

    for (size_t i = n; i-- > 0;)
        for (size_t j = m; j-- > 0;)
            L[i][j] = script[i] == heard[j].key ? L[i + 1][j + 1] + 1 : std::max (L[i + 1][j], L[i][j + 1]);

    std::vector<double> at (n, -1.0);
    size_t i = 0, j = 0;

    while (i < n && j < m)
    {
        if (script[i] == heard[j].key) { at[i] = heard[j].start; ++i; ++j; }
        else if (L[i + 1][j] >= L[i][j + 1]) ++i;
        else ++j;
    }

    return at;
}

/** (slowo skryptu, zamiennik) — patrz ZS_GRFST w main(). */
static std::vector<std::pair<std::string, std::string>> gSubs;

struct Score { double w1, w2, line; int n; double medianDelay; };

/** Kiedy slychac mowe — tym samym licznikiem sylab, ktory chodzi w
    programie. Co 0,1 s: true = mowa, false = pauza. */
static std::vector<bool> speechActivity (const char* wavPath)
{
    wavio::Audio a;
    if (! wavio::read (wavPath, a).empty()) { std::fprintf (stderr, "nie moge czytac %s\n", wavPath); std::exit (1); }

    const auto x = a.toMono();
    tw::SyllableClock c;
    c.prepare (a.sampleRate);

    std::vector<bool> out;
    const size_t step = (size_t) (a.sampleRate / 10);
    for (size_t p = 0; p + step <= x.size(); p += step)
    {
        c.process (x.data() + p, (int) step);
        out.push_back (c.pauseLength() <= 0.0);
    }
    return out;
}

static const tw::CutFinder* gCuts = nullptr;   // ZS_WERSY + nagranie: punkt ciecia

static Score measure (const tw::PrompterText& text, const std::vector<Hyp>& hyps,
                      const std::vector<double>& truthAt, const std::vector<tw::LineSpan>& lines,
                      double latency, const std::vector<bool>* activity = nullptr)
{
    tw::WordTracker tr;
    tr.params.latencySec = latency;
    if (const char* c = std::getenv ("ZS_JAZDA")) tr.params.maxCoastSec = std::atof (c);   // do badan
    tr.begin (&text);
    for (auto& sb : gSubs) tr.addAlias (sb.second, sb.first);

    double first = 1e9, last = -1;
    for (double s : truthAt) if (s >= 0) { first = std::min (first, s); last = std::max (last, s); }

    int n = 0, in1 = 0, in2 = 0, inLine = 0;
    size_t h = 0;

    // Czasy wersow tak, jak zapisze je program (LineLog.h) — ZS_WERSY=1.
    const bool logLines = std::getenv ("ZS_WERSY") != nullptr && latency == 0.0;
    tw::LineLog lineLog;
    lineLog.begin();
    std::vector<double> delays;     // oddanie przez Vosk - poczatek slowa we wzorcu

    for (double now = 0.0; now <= last; now += 0.1)
    {
        while (h < hyps.size() && hyps[h].t <= now)
        {
            const int before = tr.hitCount();
            tr.hypothesis (hyps[h].t, hyps[h].words);
            const int w = tr.lastHitWord();
            if (tr.hitCount() > before && w >= 0 && truthAt[(size_t) w] >= 0)
                delays.push_back (hyps[h].t - truthAt[(size_t) w]);
            if (hyps[h].final) tr.endUtterance();
            ++h;
        }

        if (activity != nullptr)
        {
            const size_t k = (size_t) std::lround (now * 10.0);
            tr.speaking (now, k < activity->size() && (*activity)[k]);
        }

        if (now < first) continue;

        int truth = -1;
        for (int i = 0; i < (int) truthAt.size(); ++i)
            if (truthAt[(size_t) i] >= 0 && truthAt[(size_t) i] <= now) truth = i;

        const int shown = tr.word (now);
        const int err = std::abs (shown - truth);

        if (logLines && shown >= 0)
        {
            tw::LineMark m;
            m.sec = now; m.shown = tw::lineOfWord (lines, shown);
            m.firstWord = lines[(size_t) m.shown].firstWord; m.lastWord = lines[(size_t) m.shown].lastWord;
            m.source = tr.hasLock (now) ? tw::LineSource::Words : tw::LineSource::Syllables;
            lineLog.observe (m);
        }

        if (std::getenv ("ZS_SLAD") != nullptr && latency > 0.0)
            std::printf ("slad %6.1f  wzorzec %3d  pokazane %3d  trafienie %3d  tempo %.2f\n",
                         now, truth, shown, tr.lastHitWord(), tr.wordsPerSec());

        ++n;
        if (err <= 1) ++in1;
        if (err <= 2) ++in2;
        if (tw::lineOfWord (lines, shown) == tw::lineOfWord (lines, truth)) ++inLine;
    }

    if (std::getenv ("ZS_SLAD") != nullptr)
        std::printf ("slad-koniec trafien %d, ostatnie w slowie %d\n", tr.hitCount(), tr.lastHitWord());

    if (logLines)
    {
        // Spoznienie zapisanego poczatku wersu wzgledem prawdziwego poczatku
        // jego pierwszego slowa (wzorzec z wynik.czasy).
        std::vector<double> late, cutErr;
        int cutOk = 0;
        const auto fin = lineLog.finalLines();
        int noTruth = 0;
        for (const auto& l : lines) if (truthAt[(size_t) l.firstWord] < 0) ++noTruth;
        std::printf ("wersy: w tekscie %zu, zapisane %zu (wszystkich wpisow %zu), bez wzorca czasu %d\n",
                     lines.size(), fin.size(), lineLog.marks().size(), noTruth);
        for (const auto& m : fin)
        {
            const double t = truthAt[(size_t) m.firstWord];
            if (t < 0) { std::printf ("wers slowo %3d  zapisany %6.1f  (bez wzorca)\n", m.firstWord, m.sec); continue; }
            late.push_back (m.sec - t);
            std::printf ("wers slowo %3d  zapisany %6.1f  prawdziwy %6.1f  roznica %+5.2f s",
                         m.firstWord, m.sec, t, m.sec - t);
            if (gCuts != nullptr)
            {
                const auto c = gCuts->find (m.sec);
                const bool ok = c.sec <= t && c.sec >= t - 1.0;
                cutErr.push_back (c.sec - t);
                if (ok) ++cutOk;
                std::printf ("   ciecie %6.2f (%+5.2f s od poczatku, %s)%s", c.sec, c.sec - t,
                             c.inPause ? "w pauzie" : "BEZ PAUZY", ok ? "" : "  <-- zle");
            }
            std::printf ("\n");
        }
        std::sort (late.begin(), late.end());
        if (! late.empty())
            std::printf ("\n  wersow %zu (z %zu w tekscie): spoznienie poczatku wersu: mediana %+.2f s, "
                         "10%% %+.2f s, 90%% %+.2f s, najwiecej %+.2f s\n\n",
                         late.size(), lines.size(), late[late.size() / 2], late[late.size() / 10],
                         late[late.size() * 9 / 10], late.back());
        if (gCuts != nullptr && ! cutErr.empty())
            std::printf ("  punkt ciecia przed slowem (do 1 s wczesniej): %d z %zu\n\n", cutOk, cutErr.size());
    }

    std::sort (delays.begin(), delays.end());
    const double med = delays.empty() ? 0.0 : delays[delays.size() / 2];
    return { 100.0 * in1 / n, 100.0 * in2 / n, 100.0 * inLine / n, n, med };
}

int main (int argc, char** argv)
{
    // Zamienniki slow, ktorych model nie zna (ModelWords.h): ZS_GRFST=sciezka/graph/Gr.fst
    auto prepareSubs = [&] (tw::WordTracker& tr)
    {
        if (const char* gr = std::getenv ("ZS_GRFST"))
        {
            if (gSubs.empty())
            {
                gSubs = tw::substitutes (tr.vocabulary(), tw::readModelWords (gr));
                for (auto& sb : gSubs) std::fprintf (stderr, "  zamiennik: %s -> %s\n", sb.first.c_str(), sb.second.c_str());
            }
            for (auto& sb : gSubs) tr.addAlias (sb.second, sb.first);
        }
    };

    if (argc == 3 && std::string (argv[1]) == "--slownik")
    {
        tw::PrompterText t;
        t.set (readAll (argv[2]));
        tw::WordTracker tr;
        tr.begin (&t);
        prepareSubs (tr);
        for (const auto& w : tr.vocabulary()) std::printf ("%s\n", w.c_str());
        return 0;
    }

    // Prawda dla symulacji na prawdziwym glosie (Tools/zs-prawdziwe.py):
    // kiedy zaczyna i konczy sie kazdy wiersz tekstu, z czasow slow Vosk.
    // Format prawda.tsv jak w zs-scenariusz.py (tylko wiersze "mowa").
    if (argc == 4 && std::string (argv[1]) == "--prawda")
    {
        tw::PrompterText t;
        t.set (readAll (argv[2]));
        std::vector<std::string> k;
        for (int i = 0; i < t.wordCount(); ++i) k.push_back (tw::spokenKey (t.wordText (i)));

        std::vector<Timed> h;
        std::istringstream in (readAll (argv[3]));
        for (Timed w; in >> w.start >> w.end >> w.key;) h.push_back ({ w.start, w.end, tw::spokenKey (w.key) });

        const auto at = alignTruth (k, h);
        std::map<double, double> endOf;                       // start slowa -> koniec
        for (const auto& w : h) endOf[w.start] = w.end;

        std::printf ("# Zielone swiatlo - prawda z czasow slow Vosk (zs-sledzenie --prawda)\n"
                     "# typ\tod_s\tdo_s\twers\topis\tdodatkowo\n");
        int found = 0, words = 0;
        for (int i = 0; i < t.wordCount();)
        {
            const int line = t.all()[(size_t) i].line;
            double from = -1.0, to = -1.0;
            int j = i;
            for (; j < t.wordCount() && t.all()[(size_t) j].line == line; ++j)
            {
                ++words;
                if (at[(size_t) j] < 0.0) continue;
                ++found;
                if (from < 0.0) from = at[(size_t) j];
                to = endOf[at[(size_t) j]];
            }
            if (from >= 0.0) std::printf ("mowa\t%.3f\t%.3f\t%d\tdobrze\t\n", from, to, line);
            i = j;
        }
        std::fprintf (stderr, "dopasowane slowa: %d z %d\n", found, words);
        return 0;
    }

    if (argc < 4)
    {
        std::fprintf (stderr, "zs-sledzenie --slownik skrypt.txt\n"
                              "zs-sledzenie skrypt.txt wynik.hip wynik.czasy [opoznienie_s] [nagranie-16k.wav]\n");
        return 2;
    }

    tw::PrompterText text;
    text.set (readAll (argv[1]));

    std::vector<std::string> keys;
    for (int i = 0; i < text.wordCount(); ++i) keys.push_back (tw::spokenKey (text.wordText (i)));

    // wers = wiersz pliku
    std::vector<tw::LineSpan> lines;
    for (int i = 0, first = 0; i < text.wordCount(); ++i)
        if (i == text.wordCount() - 1 || text.all()[(size_t) i + 1].line != text.all()[(size_t) i].line)
        { lines.push_back ({ first, i }); first = i + 1; }

    std::vector<Hyp> hyps;
    {
        std::istringstream in (readAll (argv[2]));
        for (std::string line; std::getline (in, line);)
        {
            std::istringstream ls (line);
            Hyp h; std::string kind;
            if (! (ls >> h.t >> kind)) continue;
            std::string rest; std::getline (ls, rest);
            h.final = kind == "F";
            h.words = split (rest);
            hyps.push_back (h);
        }
    }

    std::vector<Timed> heard;
    {
        std::istringstream in (readAll (argv[3]));
        for (Timed w; in >> w.start >> w.end >> w.key;) heard.push_back ({ w.start, w.end, tw::spokenKey (w.key) });
    }

    {
        tw::WordTracker probe;          // tylko po slownik dla zamiennikow
        probe.begin (&text);
        prepareSubs (probe);
    }

    const auto truthAt = alignTruth (keys, heard);
    int aligned = 0;
    for (double s : truthAt) if (s >= 0) ++aligned;

    const double latency = argc > 4 ? std::atof (argv[4]) : 1.2;

    std::printf ("\nPomiar promptera na nagraniu\n============================\n\n");
    std::printf ("  skrypt: %d slow, %d wersow; wzorzec czasow dopasowany do %d slow (%.0f%%)\n",
                 text.wordCount(), (int) lines.size(), aligned, 100.0 * aligned / std::max (1, text.wordCount()));
    std::printf ("  hipotez: %zu\n\n", hyps.size());

    const auto a = measure (text, hyps, truthAt, lines, latency);
    const auto b = measure (text, hyps, truthAt, lines, 0.0);

    std::vector<bool> activity;
    if (argc > 5) activity = speechActivity (argv[5]);
    static tw::CutFinder cuts;
    if (argc > 5)
    {
        wavio::Audio wa;
        if (wavio::read (argv[5], wa).empty())
        {
            const auto mono = wa.toMono();
            cuts.analyse (mono.data(), mono.size(), wa.sampleRate);
            gCuts = &cuts;
        }
    }

    std::printf ("  %-34s  <=1 slowo  <=2 slowa  wers\n", "");
    std::printf ("  %-34s  %7.1f%%  %8.1f%%  %5.1f%%\n", ("kompensacja " + std::to_string (latency).substr (0, 4) + " s").c_str(), a.w1, a.w2, a.line);
    std::printf ("  %-34s  %7.1f%%  %8.1f%%  %5.1f%%\n", "bez kompensacji", b.w1, b.w2, b.line);

    if (! activity.empty())
    {
        const auto c = measure (text, hyps, truthAt, lines, latency, &activity);
        const auto d = measure (text, hyps, truthAt, lines, 0.0, &activity);
        std::printf ("  %-34s  %7.1f%%  %8.1f%%  %5.1f%%\n", ("jazda tylko w mowie, komp. " + std::to_string (latency).substr (0, 4)).c_str(), c.w1, c.w2, c.line);
        std::printf ("  %-34s  %7.1f%%  %8.1f%%  %5.1f%%\n", "jazda tylko w mowie, bez komp.", d.w1, d.w2, d.line);
    }
    std::printf ("\n  (%d chwil co 0,1 s; mediana opoznienia trafienia wzgledem poczatku slowa %.2f s)\n\n",
                 a.n, b.medianDelay);
    return 0;
}
