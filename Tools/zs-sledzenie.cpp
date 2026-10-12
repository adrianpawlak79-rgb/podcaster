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
#include "DSP/Punch.h"
#include "DSP/WordTracker.h"
#include "DSP/PrompterLines.h"
#include "DSP/SyllableClock.h"
#include "ModelWords.h"
#include "Wav.h"

#include <algorithm>
#include <array>
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

    // ZS_R=1: dokad cofnie R wcisniete w tej chwili (PrompterComponent::repeatLine)
    // — na wersach promptera, nie wierszach pliku. Liczone tylko bez
    // kompensacji opoznienia, czyli tak, jak dziala program.
    const bool probeR = std::getenv ("ZS_R") != nullptr && latency == 0.0;
    const auto pspans = tw::lineSpans (text);
    int rAll = 0, rOk = 0, rBack = 0, rAhead = 0, rStartAll = 0, rStartOk = 0, rStartBack = 0;

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

        if (probeR && truth >= 0 && shown >= 0)
        {
            const int want = tw::lineOfWord (pspans, truth);
            const int got  = tw::lineOfWord (pspans, tw::repeatLineStart (pspans, shown));
            const bool atStart = now - truthAt[(size_t) pspans[(size_t) want].firstWord] < 1.0
                              && truthAt[(size_t) pspans[(size_t) want].firstWord] >= 0.0;
            ++rAll; if (got == want) ++rOk; else if (got < want) ++rBack; else ++rAhead;
            if (atStart) { ++rStartAll; if (got == want) ++rStartOk; else if (got < want) ++rStartBack; }
        }

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

    if (probeR && rAll > 0)
        std::printf ("R: chwil %d — wlasciwy wers %.1f%%, za daleko wstecz %.1f%%, do przodu %.1f%%; "
                     "w 1. sekundzie wersu (%d chwil): wlasciwy %.1f%%, wstecz %.1f%%\n",
                     rAll, 100.0 * rOk / rAll, 100.0 * rBack / rAll, 100.0 * rAhead / rAll,
                     rStartAll, rStartAll ? 100.0 * rStartOk / rStartAll : 0.0,
                     rStartAll ? 100.0 * rStartBack / rStartAll : 0.0);

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

    // R w pauzie po pomylce (Tools/zs-r-pauzy.py): r.tsv = "czas_R  wiersz_zrodla  gdzie".
    // Hipotezy ida do WordTracker jak w programie (bez kompensacji opoznienia),
    // w chwili R liczymy to, co PrompterComponent::repeatLine: poczatek wersu
    // promptera, w ktorym stoi biezace slowo.
    if (argc == 5 && std::string (argv[1]) == "--r")
    {
        tw::PrompterText t;
        t.set (readAll (argv[2]));
        const auto spans = tw::lineSpans (t);

        std::vector<Hyp> hs;
        {
            std::istringstream in (readAll (argv[3]));
            for (std::string line; std::getline (in, line);)
            {
                std::istringstream ls (line);
                Hyp h; std::string kind;
                if (! (ls >> h.t >> kind)) continue;
                std::string rest; std::getline (ls, rest);
                h.final = kind == "F";
                h.words = split (rest);
                hs.push_back (h);
            }
        }
        struct Probe { double t; int line; std::string where; };
        std::vector<Probe> probes;
        {
            std::istringstream in (readAll (argv[4]));
            for (Probe p; in >> p.t >> p.line >> p.where;) probes.push_back (p);
        }

        // Kazda proba osobno, od poczatku nagrania: po prawdziwym R czlowiek czyta
        // wers jeszcze raz, a tu nagranie idzie dalej — wspolny tracker po
        // jumpTo mylilby sie na kolejnych probach z powodu samego pomiaru.
        std::map<std::string, std::array<int, 3>> res;     // gdzie -> {dobrze, wstecz, naprzod}
        for (const auto& p : probes)
        {
            tw::WordTracker tr;
            tr.begin (&t);
            for (size_t h = 0; h < hs.size() && hs[h].t <= p.t; ++h)
            {
                tr.hypothesis (hs[h].t, hs[h].words);
                if (hs[h].final) tr.endUtterance();
            }
            const int shown = std::max (0, tr.word (p.t));
            const int to = tw::repeatLineStart (spans, shown);
            const int gotLine = t.all()[(size_t) to].line;      // wiersz zrodla, do ktorego cofa R
            auto& r = res[p.where];
            if (gotLine == p.line) ++r[0]; else if (gotLine < p.line) ++r[1]; else ++r[2];
        }
        for (const auto& r : res)
        {
            const int n = r.second[0] + r.second[1] + r.second[2];
            std::printf ("%s: %d/%d dobrze, %d wstecz, %d naprzod;  ", r.first.c_str(), r.second[0], n, r.second[1], r.second[2]);
        }
        std::printf ("\n");
        return 0;
    }

    // Pelna proba wyciecia na prawdziwym glosie (Tools/zs-r-pauzy.py --powtorka).
    // To samo, co program: hipotezy -> WordTracker (pozycja, uslyszane slowa),
    // LineLog (wersy co 0,1 s, R jako Retake + jumpTo), po zatrzymaniu CutFinder,
    // Punch::plan i Punch::guard. r.tsv: czas_R wiersz wolno_od wolno_do pomylka_od powtorka_od.
    if (argc == 6 && std::string (argv[1]) == "--r-ciecie")
    {
        tw::PrompterText t;
        t.set (readAll (argv[2]));
        const auto spans = tw::lineSpans (t);

        std::vector<Hyp> hs;
        {
            std::istringstream in (readAll (argv[3]));
            for (std::string line; std::getline (in, line);)
            {
                std::istringstream ls (line);
                Hyp h; std::string kind;
                if (! (ls >> h.t >> kind)) continue;
                std::string rest; std::getline (ls, rest);
                h.final = kind == "F";
                h.words = split (rest);
                hs.push_back (h);
            }
        }
        struct Probe { double t; int line; double okFrom, okTo, mistakeFrom, rereadFrom; };
        std::vector<Probe> probes;
        {
            std::istringstream in (readAll (argv[4]));
            for (Probe p; in >> p.t >> p.line >> p.okFrom >> p.okTo >> p.mistakeFrom >> p.rereadFrom;) probes.push_back (p);
        }
        wavio::Audio a;
        if (! wavio::read (argv[5], a).empty()) { std::fprintf (stderr, "nie moge czytac %s\n", argv[5]); return 1; }
        const auto x = a.toMono();
        tw::CutFinder cuts;
        cuts.analyse (x.data(), x.size(), a.sampleRate);
        const double endSec = (double) x.size() / a.sampleRate;

        tw::WordTracker tr;
        tr.begin (&t);
        tw::LineLog log;
        log.begin();
        std::vector<tw::HeardWord> heard;
        int lastHeard = -1;
        size_t h = 0, next = 0;
        auto markFor = [&] (int line, double sec, tw::LineSource src)
        {
            tw::LineMark m; m.sec = sec; m.shown = line; m.source = src;
            m.firstWord = spans[(size_t) line].firstWord; m.lastWord = spans[(size_t) line].lastWord;
            return m;
        };

        for (double now = 0.0; now <= endSec; now += 0.1)
        {
            while (h < hs.size() && hs[h].t <= now)
            {
                const int before = tr.hitCount();
                tr.hypothesis (hs[h].t, hs[h].words);
                if (hs[h].final) tr.endUtterance();
                if (tr.hitCount() > before && tr.lastHitWord() >= 0)       // PrompterComponent::noteHeard
                {
                    const int w = tr.lastHitWord();
                    if (lastHeard >= 0 && w > lastHeard && w - lastHeard <= 3)
                        for (int k = lastHeard + 1; k < w; ++k) heard.push_back ({ now, k });
                    heard.push_back ({ now, w });
                    lastHeard = w;
                }
                ++h;
            }
            const int shown = std::max (0, tr.word (now));
            if (tr.hitCount() > 0)
                log.observe (markFor (tw::lineToShow (spans, t, tw::syllableAtWord (t, shown)), now, tw::LineSource::Words));

            if (next < probes.size() && now >= probes[next].t)
            {
                const int to = tw::repeatLineStart (spans, shown);      // PrompterComponent::repeatLine
                tr.jumpTo (to, now);
                lastHeard = -1;
                log.restart (markFor (tw::lineOfWord (spans, to), now, tw::LineSource::Retake), tw::LineSource::Retake);
                ++next;
            }
        }

        tw::Punch punch;
        const auto planned = punch.plan (log.marks(), cuts);
        const auto guarded = punch.guard (planned, heard, nullptr, &cuts);
        const std::vector<tw::Removal>* sureRef = nullptr;   // ZS_NIEBEZP wypisuje niebezpieczne PEWNE

        // ocena kazdego R: ciecie, ktore pokrywa jego chwile
        auto judge = [&] (const std::vector<tw::Removal>& rm, int& good, int& partial, int& none, int& danger)
        {
            good = partial = none = danger = 0;
            for (const auto& p : probes)
            {
                const tw::Removal* c = nullptr;
                for (const auto& r : rm) if (r.from <= p.t && r.to >= p.t - 1.0) c = &r;
                if (c == nullptr) { ++none; continue; }
                if (c->from < p.okFrom - 0.05 || c->to > p.okTo + 0.05)
                {
                    ++danger;
                    if (std::getenv ("ZS_NIEBEZP") && &rm == sureRef)
                        std::printf ("  NIEBEZP przy R %.2f: ciecie %.2f-%.2f, wolno %.2f-%.2f (pomylka od %.2f, powtorka od %.2f)\n",
                                     p.t, c->from, c->to, p.okFrom, p.okTo, p.mistakeFrom, p.rereadFrom);
                    continue;
                }
                if (c->from <= p.mistakeFrom + 0.05 && c->to >= p.rereadFrom - 0.05) ++good; else ++partial;
            }
            // ciecie, ktore nie pokrywa zadnego R, tez jest niebezpieczne
            for (const auto& r : rm)
            {
                bool covers = false;
                for (const auto& p : probes) if (r.from <= p.t && r.to >= p.t - 1.0) covers = true;
                if (! covers)
                {
                    ++danger;
                    if (std::getenv ("ZS_NIEBEZP") && &rm == sureRef)
                        std::printf ("  NIEBEZP ciecie bez R: %.2f-%.2f\n", r.from, r.to);
                }
            }
        };
        if (std::getenv ("ZS_DEBUG"))
        {
            for (const auto& p : probes)
            {
                std::printf ("R %.2f wiersz %d  wolno %.2f-%.2f  pomylka od %.2f  powtorka od %.2f\n", p.t, p.line, p.okFrom, p.okTo, p.mistakeFrom, p.rereadFrom);
                for (const auto& r : planned) if (r.from <= p.t && r.to >= p.t - 1.0) std::printf ("   plan %.2f-%.2f\n", r.from, r.to);
                std::printf ("   slyszane:");
                for (const auto& hw : heard) if (hw.sec > p.okFrom - 1.0 && hw.sec < p.rereadFrom + 3.0) std::printf (" %d@%.2f", hw.word, hw.sec);
                std::printf ("\n");
            }
            for (const auto& m : log.marks()) std::printf ("mark %.2f wers %d %s\n", m.sec, m.shown, tw::sourceName (m.source));
        }
        int g0, p0, n0, d0, g1, p1, n1, d1;
        judge (planned, g0, p0, n0, d0);
        judge (guarded, g1, p1, n1, d1);
        // PEWNE / DO WSKAZANIA (Punch::review): oceniamy tylko pewne
        const auto verdicts = punch.review (planned, heard, &cuts, tw::similarlyStartingLines (t, spans));
        std::vector<tw::Removal> sure;
        int unsure = 0;
        std::map<std::string, int> whyCount;
        for (const auto& v : verdicts) { if (v.sure) sure.push_back (v.cut); else { ++unsure; ++whyCount[v.why]; } }
        int g2, p2, n2, d2;
        sureRef = &sure;
        judge (sure, g2, p2, n2, d2);
        std::printf ("R %zu: bez straznika dobrze %d, czesciowo %d, bez ciecia %d, NIEBEZPIECZNE %d; "
                     "ze straznikiem dobrze %d, czesciowo %d, bez ciecia %d, NIEBEZPIECZNE %d; "
                     "PEWNE dobrze %d, czesciowo %d, NIEBEZPIECZNE %d, do wskazania %d\n",
                     probes.size(), g0, p0, n0, d0, g1, p1, n1, d1, g2, p2, d2, unsure);
        if (std::getenv ("ZS_DLACZEGO"))
            for (const auto& w : whyCount) std::printf ("   do wskazania: %s — %d\n", w.first.c_str(), w.second);
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
