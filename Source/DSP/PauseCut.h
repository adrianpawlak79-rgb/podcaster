/*
    PauseCut.h — gdzie ciac, gdy czlowiek na pauzie wybiera wers.

    Transport jak w DAW (PrompterComponent, Adrian 13.10): na pauzie klik w
    wers albo w fale stawia czerwona linie, od ktorej nagranie pojdzie od
    nowa. Ten plik liczy TYLKO to miejsce — bez JUCE, zeby ten sam kod
    sprawdzala symulacja (Tools/zs-symulacja.cpp, przebieg C).

    Kolejnosc:
      1. PreRoll::plan z chwilowym znacznikiem R na tym wersie (ta sama
         droga co pre-roll po R: pauza przed pierwszym uslyszanym slowem);
      2. bez pewnej propozycji: ostatnia cisza przed szacowanym poczatkiem
         wersu (pierwsze uslyszane slowo albo znacznik wersu);
      3. bez ciszy: sam szacunek.

    Test: Tests/preroll-test.cpp (sekcja 1f).
*/

#pragma once

#include "CutPoint.h"
#include "LineLog.h"
#include "PreRoll.h"
#include "Punch.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace tw
{

struct PauseCut
{
    /** Miejsca w ciszy, do ktorych przyciaga klik (0,15 s przed koncem pauzy). */
    static std::vector<double> silences (const std::vector<float>& hist, double rate, double histStart)
    {
        std::vector<double> out;
        if (hist.empty() || rate <= 0.0) return out;
        CutFinder f;
        f.analyse (hist.data(), hist.size(), rate);
        for (const auto& q : f.pauses())
            out.push_back (histStart + std::max (q.first, q.second - 0.15));
        return out;
    }

    /** Poczatek wersu [first, last] w nagraniu (szacunek): pierwsze uslyszane
        slowo tego wersu przed pauza, inaczej znacznik wersu, inaczej chwila pauzy. */
    static double lineStart (int first, int last, double atSec,
                             const std::vector<HeardWord>& heard, const std::vector<LineMark>& marks)
    {
        double best = 1.0e9;
        for (const auto& h : heard)
            if (h.word >= first && h.word <= last && h.sec <= atSec)
                best = std::min (best, h.sec - Punch().heardLagSec);
        if (best < 1.0e8) return best;
        for (const auto& m : marks)
            if (m.firstWord == first && m.source != LineSource::Retake) best = m.sec;
        return best < 1.0e8 ? best : atSec;
    }

    /** Klik w fale: cisza do 0,3 s od klikniecia, inaczej najcichsze miejsce
        obok (+-0,25 s); nie dalej niz chwila pauzy. Dawniej: najblizsza cisza
        w ogole — na glosie bez przerw linia skakala o kilka sekund. */
    static double nearest (double t, const std::vector<double>& cuts, double atSec,
                           const std::vector<float>& hist, double rate, double histStart)
    {
        double best = -1.0, dist = snapSec;
        for (auto c : cuts) if (std::abs (c - t) <= dist) { dist = std::abs (c - t); best = c; }
        if (best < 0.0) best = quietest (hist, rate, histStart, t - 0.25, t + 0.25, t);
        if (best < 0.0) best = t;
        return std::min (best, atSec);
    }

    static constexpr double snapSec = 0.3;

    /** Czerwona linia dla klikniecia w wers. retake = znacznik R na tym wersie
        w chwili pauzy (jak PrompterComponent::markFor). */
    static double forLine (const std::vector<LineMark>& marks, const LineMark& retake,
                           const std::vector<float>& hist, double rate, double histStart,
                           const std::vector<HeardWord>& heard, const std::vector<double>& cuts)
    {
        auto tmp = marks;
        tmp.push_back (retake);
        const auto plan = PreRoll().plan (tmp, hist, rate, histStart, &heard);
        if (! plan.audio.empty()) return plan.toSec;

        // Bez pewnej propozycji: cisza tuz przed szacowanym poczatkiem wersu,
        // a gdy jej nie ma — najcichsze miejsce obok. NIE cofamy sie do dalszej
        // ciszy: symulacja na lektorach Wolnych Lektur (przebieg C) pokazala, ze
        // miedzy wersami czesto nie ma ciszy, tylko 50 ms dolek, a "ostatnia
        // cisza" lezala wers albo dwa wczesniej — wypadaly dobre slowa.
        //
        // Szacunek z rozpoznawania jest spozniony o dlugosc pierwszego slowa
        // (slowo "slychac" po jego koncu), wiec okno siega do 1,2 s wstecz —
        // ale nie przed ostatnie uslyszane slowo poprzedniego wersu.
        const double est = lineStart (retake.firstWord, retake.lastWord, retake.sec, heard, marks);
        double from = est - nearSec;
        double prevEnd = -1.0e9;
        for (const auto& h : heard)
            if (h.word < retake.firstWord && h.sec - Punch().heardLagSec < est)
                prevEnd = std::max (prevEnd, h.sec - Punch().heardLagSec);
        if (prevEnd > -1.0e8) from = std::max (prevEnd - 0.3, est - farSec);
        // Slowo "slychac" po jego KONCU: szacunek z rozpoznawania lezy za
        // pierwszym slowem, a cisza tuz za nim bywa glebsza niz przerwa przed
        // wersem ("zjadl | kawal" — Wolne Lektury, wilk-pokutujacy). Wtedy
        // pierwsze slowo zostaloby dwa razy. Okno konczymy przed nim.
        const bool fromWords = prevEnd > -1.0e8 || std::any_of (heard.begin(), heard.end(), [&] (const HeardWord& h)
                                   { return h.word >= retake.firstWord && h.word <= retake.lastWord; });
        const double to = std::min (fromWords ? est - firstWordSec : est + 0.1, retake.sec);

        double cut = -1.0;
        for (auto c : cuts) if (c <= to + 0.05 && c >= from) cut = c;
        if (cut >= 0.0) return cut;
        cut = quietest (hist, rate, histStart, from, to);
        return cut >= 0.0 ? cut : est;
    }

    static constexpr double nearSec = 0.8;   ///< tak daleko przed szacunkiem szukamy...
    static constexpr double farSec  = 1.2;   ///< ...a gdy znamy koniec poprzedniego wersu, do niego
    static constexpr double firstWordSec = 0.3;   ///< najkrotsze pierwsze slowo + blad opoznienia

    /** Najcichsze 50 ms w [from, to] (srodek okna); przy prawie rownych (1 dB)
        najblizsze prefer (domyslnie koniec przedzialu — blizej szacunku). -1, gdy przedzial poza pamiecia. */
    static double quietest (const std::vector<float>& hist, double rate, double histStart, double from, double to,
                            double prefer = -1.0)
    {
        if (hist.empty() || rate <= 0.0) return -1.0;
        const double hEnd = histStart + (double) hist.size() / rate;
        from = std::max (from, histStart);
        to = std::min (to, hEnd);
        const size_t frame = (size_t) std::max (1.0, 0.010 * rate);
        if (to - from < 0.06) return -1.0;

        std::vector<double> e;                       // energia ramek 10 ms
        const size_t a = (size_t) ((from - histStart) * rate), b = (size_t) ((to - histStart) * rate);
        for (size_t i = a; i + frame <= b; i += frame)
        {
            double s = 0.0;
            for (size_t k = i; k < i + frame; ++k) s += (double) hist[k] * hist[k];
            e.push_back (s);
        }
        if (e.size() < 5) return -1.0;
        std::vector<double> db;
        double bestDb = 1.0e9;
        for (size_t i = 0; i + 5 <= e.size(); ++i)
        {
            double s = 0.0;
            for (size_t k = i; k < i + 5; ++k) s += e[k];
            db.push_back (10.0 * std::log10 (s + 1.0e-20));
            bestDb = std::min (bestDb, db.back());
        }
        // przy prawie tym samym poziomie (1 dB) — najblizej prefer
        if (prefer < 0.0) prefer = to;
        double best = from, dist = 1.0e9;
        for (size_t i = 0; i < db.size(); ++i)
        {
            const double c = from + ((double) i + 2.5) * (double) frame / rate;
            if (db[i] <= bestDb + 1.0 && std::abs (c - prefer) < dist) { dist = std::abs (c - prefer); best = c; }
        }
        return best;
    }
};

} // namespace tw
