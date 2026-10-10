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

#include "DSP/WordTracker.h"
#include "DSP/PrompterLines.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
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

struct Score { double w1, w2, line; int n; };

static Score measure (const tw::PrompterText& text, const std::vector<Hyp>& hyps,
                      const std::vector<double>& truthAt, const std::vector<tw::LineSpan>& lines,
                      double latency)
{
    tw::WordTracker tr;
    tr.params.latencySec = latency;
    tr.begin (&text);

    double first = 1e9, last = -1;
    for (double s : truthAt) if (s >= 0) { first = std::min (first, s); last = std::max (last, s); }

    int n = 0, in1 = 0, in2 = 0, inLine = 0;
    size_t h = 0;

    for (double now = 0.0; now <= last; now += 0.1)
    {
        while (h < hyps.size() && hyps[h].t <= now)
        {
            tr.hypothesis (hyps[h].t, hyps[h].words);
            if (hyps[h].final) tr.endUtterance();
            ++h;
        }

        if (now < first) continue;

        int truth = -1;
        for (int i = 0; i < (int) truthAt.size(); ++i)
            if (truthAt[(size_t) i] >= 0 && truthAt[(size_t) i] <= now) truth = i;

        const int shown = tr.word (now);
        const int err = std::abs (shown - truth);

        ++n;
        if (err <= 1) ++in1;
        if (err <= 2) ++in2;
        if (tw::lineOfWord (lines, shown) == tw::lineOfWord (lines, truth)) ++inLine;
    }

    return { 100.0 * in1 / n, 100.0 * in2 / n, 100.0 * inLine / n, n };
}

int main (int argc, char** argv)
{
    if (argc == 3 && std::string (argv[1]) == "--slownik")
    {
        tw::PrompterText t;
        t.set (readAll (argv[2]));
        tw::WordTracker tr;
        tr.begin (&t);
        for (const auto& w : tr.vocabulary()) std::printf ("%s\n", w.c_str());
        return 0;
    }

    if (argc < 4)
    {
        std::fprintf (stderr, "zs-sledzenie --slownik skrypt.txt\n"
                              "zs-sledzenie skrypt.txt wynik.hip wynik.czasy [opoznienie_s]\n");
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

    std::printf ("  %-34s  <=1 slowo  <=2 slowa  wers\n", "");
    std::printf ("  %-34s  %7.1f%%  %8.1f%%  %5.1f%%\n", ("kompensacja " + std::to_string (latency).substr (0, 4) + " s").c_str(), a.w1, a.w2, a.line);
    std::printf ("  %-34s  %7.1f%%  %8.1f%%  %5.1f%%\n", "bez kompensacji", b.w1, b.w2, b.line);
    std::printf ("\n  (%d chwil co 0,1 s)\n\n", a.n);
    return 0;
}
