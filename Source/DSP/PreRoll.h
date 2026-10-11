/*
    PreRoll.h — po R: koncowka tego, co zostanie w nagraniu, w sluchawkach,
    i odliczanie do czytania.

    ---------------------------------------------------------------------------
    PO CO

    Krok 5 nagrywania "punch and roll" (Tools/ZADANIA.md, zad. 4). Po R
    czlowiek czyta wers jeszcze raz — ale bez pre-rollu nie wie, Z JAKIM
    tonem i tempem skonczyl poprzedni wers, ktory w czystym pliku bedzie tuz
    przed nim. Programy studyjne robia to tak samo: przed wejsciem puszczaja
    1–3 s tego, co zostaje, a potem "teraz".

    ---------------------------------------------------------------------------
    CO GRA

    Dokladnie te 2 s, ktore w czystym pliku (Punch.h) beda przed szwem:
    dzwiek tuz przed pauza przed poczatkiem wersu. Gdy wers byl juz
    powtarzany, wyciecia sie skleja (Punch::plan), wiec liczy sie PIERWSZE
    wejscie w wers, nie ostatnia powtorka — przed nia jest material, ktory i
    tak wypadnie.

    Nagranie przez caly czas idzie dalej (Recorder.h: "nagrywanie sie nie
    zatrzymuje"); pre-roll i odliczanie leza w kawalku, ktory po zatrzymaniu
    zostanie wyciety.

    ---------------------------------------------------------------------------
    KIEDY NIE GRA

    - poczatek wersu wypadl dalej niz pamiec (HistoryRing) — samo odliczanie;
    - kreator zmierzyl, ze sluchawki uciekaja do mikrofonu (albo to glosniki):
      pre-roll wszedlby do nagrania i zaklocil szukanie pauzy po R — samo
      odliczanie.
*/

#pragma once

#include "CutPoint.h"
#include "LineLog.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace tw
{

//==============================================================================
/** Ostatnie sekundy nagrania (mono, kanal analizy).

    Pisze watek dzwieku (push — bez czekania i przydzialu), czyta watek
    interfejsu (copyLast). Pisarz nadpisuje najstarsze probki, wiec czytamy
    o zapas krotszy niz pojemnosc: kopiowanie trwa mikrosekundy, a zapas to
    sekundy. */
class HistoryRing
{
public:
    void allocate (size_t capacity)           // NIE z watku dzwieku
    {
        data.assign (std::max<size_t> (1, capacity), 0.0f);
        count.store (0);
    }

    /** Z watku dzwieku: nowe nagranie zaczyna od zera. */
    void reset() noexcept { count.store (0, std::memory_order_release); }

    /** Z watku dzwieku. */
    void push (const float* x, int n) noexcept
    {
        const size_t size = data.size();
        if (size <= 1 || x == nullptr || n <= 0) return;

        uint64_t c = count.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) data[(size_t) ((c + (uint64_t) i) % size)] = x[i];
        count.store (c + (uint64_t) n, std::memory_order_release);
    }

    /** Ile probek przyszlo od reset() — to jest czas w nagraniu. */
    uint64_t written() const noexcept { return count.load (std::memory_order_acquire); }

    /** Ostatnie najwyzej maxSamples probek (nie wiecej niz pojemnosc minus
        zapas). endSample: numer probki tuz za ostatnia skopiowana. */
    std::vector<float> copyLast (size_t maxSamples, size_t safety, uint64_t& endSample) const
    {
        const size_t size = data.size();
        endSample = written();
        if (size <= 1) return {};

        const size_t room = size > safety ? size - safety : 0;
        const size_t n = (size_t) std::min<uint64_t> (endSample, std::min (maxSamples, room));

        std::vector<float> out (n);
        const uint64_t from = endSample - n;
        for (size_t i = 0; i < n; ++i) out[i] = data[(size_t) ((from + i) % size)];
        return out;
    }

private:
    std::vector<float> data;
    std::atomic<uint64_t> count { 0 };
};

//==============================================================================
struct PreRollPlan
{
    std::vector<float> audio;     ///< puste = samo odliczanie
    double fromSec = 0.0, toSec = 0.0;   ///< czas w nagraniu
    bool   inPause = false;       ///< szew znaleziony w pauzie, nie zgadniety
};

class PreRoll
{
public:
    double lengthSec = 2.0;
    double fadeSec   = 0.030;
    double minSec    = 0.5;      ///< krocej nie ma sensu grac
    CutFinder::Params cutParams;

    /** marks: zapis wersow; ostatni to wlasnie wcisniete R.
        history: ostatnie sekundy nagrania, zaczynajace sie w historyStartSec. */
    PreRollPlan plan (const std::vector<LineMark>& marks,
                      const std::vector<float>& history, double sr, double historyStartSec) const
    {
        PreRollPlan p;
        if (marks.empty() || history.empty() || sr <= 0.0) return p;

        const auto& r = marks.back();
        if (r.source != LineSource::Retake) return p;

        // pierwsze wejscie w ten wers w obecnym lancuchu powtorek
        int first = -1;
        for (size_t j = marks.size() - 1; j-- > 0;)
        {
            if (marks[j].firstWord != r.firstWord) continue;
            first = (int) j;
            if (marks[j].source != LineSource::Retake) break;
        }
        if (first < 0) return p;                      // R przed wersem — nie ma czego cofac

        double lineSec = marks[(size_t) first].sec - historyStartSec;
        if (marks[(size_t) first].source == LineSource::Retake)
            return p;                                 // wers zaczal sie od R, bez wczesniejszego wejscia

        // poczatek wersu musi byc w pamieci razem z oknem szukania pauzy
        if (lineSec - cutParams.lookBackSec < 0.0) return p;

        CutFinder finder;
        finder.params = cutParams;
        finder.analyse (history.data(), history.size(), sr);
        const auto cut = finder.find (lineSec);

        const double to   = cut.sec;
        const double from = std::max (0.0, to - lengthSec);
        if (to - from < minSec) return p;

        const size_t a = (size_t) std::lround (from * sr);
        const size_t b = std::min (history.size(), (size_t) std::lround (to * sr));
        if (b <= a) return p;

        p.audio.assign (history.begin() + (long) a, history.begin() + (long) b);
        p.fromSec = historyStartSec + from;
        p.toSec   = historyStartSec + to;
        p.inPause = cut.inPause;

        const size_t fade = std::min (p.audio.size() / 2, (size_t) std::lround (fadeSec * sr));
        for (size_t k = 0; k < fade; ++k)
        {
            const float g = (float) (k + 1) / (float) (fade + 1);
            p.audio[k] *= g;
            p.audio[p.audio.size() - 1 - k] *= g;
        }
        return p;
    }
};

//==============================================================================
/** Odliczanie na ekranie promptera. Trwa tyle, co pre-roll (najmniej
    minSec, gdy pre-rollu nie ma), potem "Czytaj" przez chwile. */
struct Countdown
{
    double minSec   = 1.5;
    double goSec    = 0.7;     ///< jak dlugo wisi "Czytaj"

    double totalFor (double preRollSec) const { return std::max (minSec, preRollSec); }

    /** Napis po elapsed sekundach od R; pusty = nic nie pokazujemy. */
    std::string label (double elapsed, double total) const
    {
        if (elapsed < 0.0) return {};
        if (elapsed < total)
        {
            const int n = std::min (3, std::max (1, (int) std::ceil (total - elapsed)));
            return std::to_string (n);
        }
        if (elapsed < total + goSec) return "Czytaj";
        return {};
    }
};

} // namespace tw
