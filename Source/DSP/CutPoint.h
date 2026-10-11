/*
    CutPoint.h — gdzie ciac, gdy czlowiek wraca do poczatku wersu.

    ---------------------------------------------------------------------------
    PO CO

    Krok 2 nagrywania "punch and roll" (Tools/ZADANIA.md, zad. 4). Krok 1
    (LineLog.h) zapisuje, kiedy prompter przeszedl do wersu. Ten czas jest
    SPOZNIONY: pomiar na nagraniu Adriana dal zwykle +0,5…+1,0 s (mediana
    +0,78 s — opoznienie rozpoznawania mowy). Ciecie w tym miejscu przecina
    pierwsze slowo wersu.

    Wiec: szukamy PAUZY tuz przed zapisanym czasem i tniemy w niej, krotko
    przed tym, jak mowa rusza. Pauza, a nie "czas minus 0,78 s", bo glos nie
    ma stalego opoznienia, a szew w ciszy jest niesłyszalny, szew w slowie —
    zawsze.

    ---------------------------------------------------------------------------
    CO JEST PAUZA

    Glosnosc w ramkach 10 ms. Prog: 8 dB nad tlem pokoju (10. percentyl
    ramek), ale nie blizej niz 30 dB pod glosna mowa (95. percentyl) — zeby w
    bardzo cichym pokoju oddech nie byl "mowa", a w glosnym pokoju tlo nie
    bylo "pauza" wszedzie. Pauza trwa co najmniej 120 ms; przerwy w pauzie
    krotsze niz 30 ms (klik, trzask) jej nie przerywaja.

    ---------------------------------------------------------------------------
    KTORA PAUZA

    Spośród pauz, ktore koncza sie w oknie [czas − 1,5 s, czas + 0,25 s]
    (zapas 0,25 s na sylaby, ktore potrafia wyprzedzic), wygrywa NAJDLUZSZA,
    z lekka kara za odleglosc: wynik = dlugosc − 0,3 × odleglosc.

    Pierwsza wersja brala pauze najblizsza zapisanemu czasowi. Na nagraniu
    Adriana dzialalo (12 z 15), ale test z przecinkiem w polowie wersu
    pokazal blad: przy spoznieniu 0,78 s pauza po przecinku jest blizej niz
    pauza przed wersem — i ciecie wypadalo w srodku wersu. Przerwa miedzy
    wersami jest zwykle dluzsza niz przecinek, wiec dlugosc rozstrzyga.

    Gdy okno nie ma pauzy — czas minus mediana spoznienia, z flaga "niepewne".
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace tw
{

struct CutPoint
{
    double sec       = 0.0;    ///< gdzie ciac (czas w nagraniu)
    bool   inPause   = false;  ///< false = nie znalazlem pauzy, czas zgadniety
    double pauseFrom = 0.0, pauseTo = 0.0;
};

class CutFinder
{
public:
    struct Params
    {
        double frameSec      = 0.010;
        double minPauseSec   = 0.120;
        double bridgeSec     = 0.030;   ///< krotsza przerwa nie dzieli pauzy
        double aboveFloorDb  = 8.0;
        double belowSpeechDb = 30.0;
        double lookBackSec   = 1.5;
        double lookAheadSec  = 0.25;
        double leadInSec     = 0.15;    ///< tyle ciszy zostaje przed slowem
        double fallbackLagSec = 0.78;   ///< mediana spoznienia z pomiaru
        double distancePenalty = 0.3;   ///< s dlugosci pauzy za 1 s odleglosci
    };

    Params params;

    /** Raz na nagranie (albo na kazdy segment). Mono. */
    void analyse (const float* x, size_t n, double sr)
    {
        frames.clear();
        found.clear();
        if (sr <= 0.0 || x == nullptr) return;

        const size_t len = std::max<size_t> (1, (size_t) std::lround (sr * params.frameSec));
        for (size_t p = 0; p + len <= n; p += len)
        {
            double s = 0.0;
            for (size_t i = 0; i < len; ++i) s += (double) x[p + i] * x[p + i];
            frames.push_back (10.0 * std::log10 (s / (double) len + 1.0e-12));
        }
        if (frames.empty()) return;

        auto sorted = frames;
        std::sort (sorted.begin(), sorted.end());
        floorDb  = std::max (-100.0, sorted[sorted.size() / 10]);
        speechDb = sorted[sorted.size() * 95 / 100];
        threshold = std::max (floorDb + params.aboveFloorDb, speechDb - params.belowSpeechDb);

        // ramki ciche → pauzy, z mostkowaniem krotkich przerw
        const int bridge  = (int) std::lround (params.bridgeSec / params.frameSec);
        const int minLen  = (int) std::lround (params.minPauseSec / params.frameSec);
        int start = -1, lastQuiet = -1;

        auto close = [&] (int endExclusive)
        {
            if (start >= 0 && endExclusive - start >= minLen)
                found.push_back ({ start * params.frameSec, endExclusive * params.frameSec });
            start = -1;
        };

        for (int i = 0; i < (int) frames.size(); ++i)
        {
            if (frames[(size_t) i] < threshold)
            {
                if (start < 0) start = i;
                lastQuiet = i;
            }
            else if (start >= 0 && i - lastQuiet > bridge)
            {
                close (lastQuiet + 1);
            }
        }
        if (start >= 0) close (lastQuiet + 1);
    }

    /** Gdzie ciac dla wersu zapisanego w chwili loggedSec. */
    CutPoint find (double loggedSec) const
    {
        CutPoint best;
        double bestScore = -1.0e9;

        for (const auto& p : found)
        {
            if (p.second > loggedSec + params.lookAheadSec) continue;
            if (p.second < loggedSec - params.lookBackSec) continue;

            const double dist  = std::fabs (loggedSec - p.second);
            const double score = (p.second - p.first) - params.distancePenalty * dist;
            if (score > bestScore)
            {
                bestScore = score;
                best.inPause = true;
                best.pauseFrom = p.first;
                best.pauseTo = p.second;
            }
        }

        if (best.inPause)
            best.sec = std::max (best.pauseFrom, best.pauseTo - params.leadInSec);
        else
            best.sec = std::max (0.0, loggedSec - params.fallbackLagSec);

        return best;
    }

    const std::vector<std::pair<double, double>>& pauses() const noexcept { return found; }
    double thresholdDb() const noexcept { return threshold; }

private:
    std::vector<double> frames;
    std::vector<std::pair<double, double>> found;
    double floorDb = -100.0, speechDb = -20.0, threshold = -60.0;
};

} // namespace tw
