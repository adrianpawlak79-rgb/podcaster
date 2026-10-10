/*
    WordTracker.h — gdzie w tekscie jest czlowiek, wedlug rozpoznanych slow.

    ---------------------------------------------------------------------------
    CO WIEMY Z POMIAROW (prawdziwy glos, Vosk small-pl 0.22)

    - Vosk ze slownikiem ograniczonym do slow skryptu i kompensacja opoznienia
      1-1,5 s: blad <= 1 slowo w 66% chwil, <= 2 slowa w 88%. Do podswietlania
      WERSU wystarcza, do pojedynczego slowa nie (trzeba >= 85% przy <= 1).
    - Metoda "klas glosek" dala 0% na prawdziwym glosie — odrzucona.

    Ten plik nie rozmawia z Vosk. Dostaje to, co Vosk zwraca (biezaca
    hipoteze jako liste slow), i odpowiada na jedno pytanie: ktore slowo
    tekstu jest teraz w ustach. Dzieki temu cala logika da sie sprawdzic
    testem bez modelu, bez DLL i bez mikrofonu (Tests/sledzenie-test.cpp).

    ---------------------------------------------------------------------------
    ZASADY (wprost z pomiarow)

    1. Nowe slowa hipotezy — te, ktorych nie bylo w poprzedniej — szukamy w
       tekscie w oknie od pozycji -3 do +8 slow. Trafienie to slowo rowne
       albo podobne w >= 0,8, ale podobienstwo tylko dla slow od 4 liter:
       krotkie ("i", "na", "to") sa za czeste, zeby zgadywac.
    2. Po 4 s bez trafienia okno przestaje wystarczac (czlowiek przeskoczyl
       akapit albo prompter sie zgubil): szukamy w calym tekscie, ale tylko
       DWOCH kolejnych slow naraz. Jedno slowo pasuje w dlugim tekscie w
       dziesieciu miejscach, dwa pod rzad prawie zawsze w jednym.
    3. Tempo = mediana z 6 ostatnich trafien, w granicach 0,8-5 slow/s.
       Mediana, bo pojedyncze trafienie za wczesnie albo za pozno (Vosk
       oddaje slowa paczkami) nie ma prawa szarpnac predkoscia.
    4. Pozycja = ostatnie trafienie + tempo x min(1,5 s, czas od trafienia).
       "Czas od trafienia" liczymy od chwili, w ktorej slowo PADLO, a nie od
       chwili, w ktorej Vosk je oddal: Vosk oddaje je ok. 1-1,5 s pozniej
       (latencySec). Liczone od oddania, pozycja w chwili trafienia stala
       3-4 slowa za czlowiekiem (Tests/sledzenie-test.cpp, wariant bez
       kompensacji). Dluzej niz 1,5 s jechac nie wolno — tyle czasu bez
       nowego trafienia i czlowiek rownie dobrze mogl przestac mowic.
*/

#pragma once

#include "PrompterText.h"
#include "ScriptNormalize.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <vector>

namespace tw
{

class WordTracker
{
public:
    struct Params
    {
        int    back           = 3;     ///< okno szukania: tyle slow w tyl
        int    ahead          = 8;     ///< i tyle w przod
        double minSimilarity  = 0.8;
        size_t fuzzyMinChars  = 4;     ///< krotszych slow nie zgadujemy

        double resyncAfterSec = 4.0;
        int    tempoHits      = 6;
        double minRate        = 0.8;   ///< slow na sekunde
        double maxRate        = 5.0;
        double startRate      = 2.5;   ///< zanim sa trafienia do mediany
        double maxCoastSec    = 1.5;
        double latencySec     = 1.2;   ///< o tyle slowo padlo wczesniej, niz Vosk je oddal
    };

    Params params;

    //==========================================================================
    void begin (const PrompterText* t)
    {
        keys.clear();

        if (t != nullptr)
            for (int i = 0; i < t->wordCount(); ++i)
                keys.push_back (spokenKey (t->wordText (i)));

        previous.clear();
        hits.clear();
        lastHit = -1;
        lastHitSec = 0.0;
        anchor = 0;
        coasting = false;
        rate = params.startRate;
        pendingWide.clear();
        totalHits = 0;
        resyncs = 0;
    }

    /** Slowa do slownika Vosk: kazde slowo skryptu raz, bez liczb. */
    std::vector<std::string> vocabulary() const
    {
        std::vector<std::string> v;

        for (const auto& k : keys)
            if (! k.empty() && ! hasDigits (k) && std::find (v.begin(), v.end(), k) == v.end())
                v.push_back (k);

        return v;
    }

    //==========================================================================
    /** Biezaca hipoteza Vosk (czesciowa albo koncowa), slowo po slowie.

        Vosk oddaje za kazdym razem CALA hipoteze wypowiedzi, nie przyrost, i
        potrafi poprawic jej koncowke. Nowe sa wiec slowa za wspolnym
        poczatkiem z poprzednia hipoteza. Poprawione slowo trafia drugi raz w
        to samo miejsce — to nie szkodzi, bo w tempo licza sie tylko trafienia
        dalej w tekscie. */
    void hypothesis (double nowSec, const std::vector<std::string>& words)
    {
        std::vector<std::string> k;
        k.reserve (words.size());
        for (const auto& w : words) k.push_back (spokenKey (w));

        size_t same = 0;
        while (same < k.size() && same < previous.size() && k[same] == previous[same]) ++same;

        for (size_t i = same; i < k.size(); ++i)
            take (nowSec, k[i]);

        previous = std::move (k);
    }

    /** Vosk zamknal wypowiedz (wynik koncowy). Nastepna hipoteza zaczyna sie
        od zera, wiec wszystkie jej slowa beda nowe. */
    void endUtterance() { previous.clear(); }

    /** Reczna poprawka albo "powtorz wers": stad liczymy dalej. */
    void jumpTo (int word, double nowSec)
    {
        if (keys.empty()) return;

        anchor = std::clamp (word, 0, (int) keys.size() - 1);
        lastHit = anchor - 1;          // nastepne trafienie spodziewane na "word"
        lastHitSec = nowSec;
        coasting = false;              // stoimy, az uslyszymy to slowo
        hits.clear();
        pendingWide.clear();
    }

    //==========================================================================
    /** Pozycja w slowach (ulamkowa). Przed pierwszym trafieniem 0. */
    double position (double nowSec) const
    {
        if (keys.empty()) return 0.0;
        if (! coasting) return anchor;

        const double coast = std::clamp (nowSec - (lastHitSec - params.latencySec), 0.0, params.maxCoastSec);
        return std::min ((double) keys.size() - 1.0, anchor + rate * coast);
    }

    int word (double nowSec) const { return (int) std::floor (position (nowSec) + 1.0e-9); }

    /** Czy Vosk niedawno cos trafil. Gdy nie — prompter wraca do licznika sylab. */
    bool hasLock (double nowSec) const
    {
        return totalHits > 0 && nowSec - lastHitSec <= params.resyncAfterSec;
    }

    bool   started()     const noexcept { return totalHits > 0; }
    double wordsPerSec() const noexcept { return rate; }
    int    hitCount()    const noexcept { return totalHits; }
    int    resyncCount() const noexcept { return resyncs; }
    int    lastHitWord() const noexcept { return lastHit; }

private:
    //==========================================================================
    bool matches (const std::string& heard, int textWord) const
    {
        const auto& k = keys[(size_t) textWord];
        if (k.empty() || heard.empty()) return false;
        if (k == heard) return true;

        return charCount (k) >= params.fuzzyMinChars && charCount (heard) >= params.fuzzyMinChars
            && similarity (k, heard) >= params.minSimilarity;
    }

    void take (double nowSec, const std::string& heard)
    {
        if (heard.empty() || hasDigits (heard)) return;     // "[unk]" daje pusty klucz

        const bool wide = totalHits > 0 && nowSec - lastHitSec > params.resyncAfterSec;

        if (wide)
        {
            if (! pendingWide.empty())
            {
                const int at = findPair (pendingWide, heard);
                if (at >= 0) { ++resyncs; hit (nowSec, at); pendingWide.clear(); return; }
            }

            pendingWide = heard;
            return;
        }

        pendingWide.clear();

        const int here = std::max (lastHit, 0);
        const int from = std::max (0, here - params.back);
        const int to   = std::min ((int) keys.size() - 1, here + params.ahead);

        // Najpierw najblizsze slowo DALEJ niz ostatnie trafienie — czytajacy
        // idzie do przodu. W tyl dopiero, gdy z przodu nic nie pasuje
        // (powtorzone slowo, poprawka).
        for (int i = lastHit + 1; i <= to; ++i)
            if (matches (heard, i)) { hit (nowSec, i); return; }

        for (int i = std::min (lastHit, to); i >= from; --i)
            if (matches (heard, i)) { hit (nowSec, i); return; }
    }

    /** Dwa kolejne slowa w calym tekscie; z kilku miejsc — najblizsze obecnej
        pozycji. Zwraca indeks DRUGIEGO slowa albo -1. */
    int findPair (const std::string& first, const std::string& second) const
    {
        int best = -1, bestDist = 1 << 30;

        for (int i = 0; i + 1 < (int) keys.size(); ++i)
            if (matches (first, i) && matches (second, i + 1))
            {
                const int d = std::abs (i + 1 - lastHit);
                if (d < bestDist) { best = i + 1; bestDist = d; }
            }

        return best;
    }

    void hit (double nowSec, int at)
    {
        hits.push_back ({ nowSec, at });
        while ((int) hits.size() > params.tempoHits) hits.pop_front();

        lastHit = at;
        anchor = at;
        coasting = true;
        lastHitSec = nowSec;
        ++totalHits;

        // tempo: mediana predkosci miedzy kolejnymi trafieniami do przodu
        std::vector<double> r;
        for (size_t i = 1; i < hits.size(); ++i)
        {
            const double dt = hits[i].sec - hits[i - 1].sec;
            const int    dw = hits[i].word - hits[i - 1].word;
            if (dt > 0.05 && dw > 0) r.push_back (dw / dt);
        }

        if (r.size() >= 2)
        {
            std::nth_element (r.begin(), r.begin() + (long) (r.size() / 2), r.end());
            double m = r[r.size() / 2];

            if (r.size() % 2 == 0)
            {
                const double lower = *std::max_element (r.begin(), r.begin() + (long) (r.size() / 2));
                m = 0.5 * (m + lower);
            }

            rate = std::clamp (m, params.minRate, params.maxRate);
        }
    }

    struct Hit { double sec; int word; };

    std::vector<std::string> keys, previous;
    std::deque<Hit> hits;
    std::string pendingWide;
    int    lastHit = -1;
    int    anchor = 0;
    bool   coasting = false;
    double lastHitSec = 0.0;
    double rate = 2.5;
    int    totalHits = 0, resyncs = 0;
};

} // namespace tw
