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
#include "Punch.h"
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

    /** Wszystkie pauzy przed R, w ktorych mozna ciac (czas w nagraniu,
        rosnaco), i ktora z nich wybral program. Strzalki w czasie
        odliczania przesuwaja wybor (PrompterComponent). */
    std::vector<double> candidates;
    int chosen = -1;
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
    /** heard: slowa uslyszane przez rozpoznawanie mowy (PrompterComponent).

        Test Adriana na Windowsie (12.10, "Nic dwa razy"): 2 z 5 pre-rolli
        konczyly sie zle — raz w srodku slowa ("ucznia-"), bo cisza nie zostala
        znaleziona, a pre-roll i tak gral do zgadnietego miejsca; dwa razy w
        pauzie po myslniku W POPRZEDNIM wersie ("Jestes - | a wiec musisz
        minac"). Dlatego:
          - miejsce ciecia jest ZAWSZE w pauzie; bez pauzy nie ma pre-rollu;
          - gdy wiadomo, kiedy padlo ostatnie slowo poprzedniego wersu, bierzemy
            pierwsza pauze PO nim — pauzy w srodku poprzedniego wersu odpadaja. */
    double heardLagSec = 0.58;     ///< jak w Punch::heardLagSec

    PreRollPlan plan (const std::vector<LineMark>& marks,
                      const std::vector<float>& history, double sr, double historyStartSec,
                      const std::vector<HeardWord>* heard = nullptr) const
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
        auto cut = finder.find (lineSec);

        // pauzy przed R — do przesuwania strzalkami (czas w nagraniu)
        const double keyRel = r.sec - historyStartSec;
        std::vector<std::pair<double, double>> pausesBefore;      // [od, ciecie] wzgledem pamieci
        std::vector<double> resumeAfter;                           // gdzie po niej wraca mowa
        for (const auto& pz : finder.pauses())
        {
            if (pz.second > keyRel) break;
            const double c = std::max (pz.first, pz.second - cutParams.leadInSec);
            if (c >= minSec)
            {
                p.candidates.push_back (historyStartSec + c);
                pausesBefore.push_back ({ pz.first, c });
                resumeAfter.push_back (pz.second);
            }
        }

        // Ostatnie uslyszane slowo POPRZEDNIEGO wersu: ciecie w pierwszej pauzie,
        // po ktorej mowa wraca pozniej niz to slowo (czas slowa jest szacowany —
        // moze wypasc w srodku slowa albo juz w pauzie). Znacznik wersu tu nie
        // pomaga: to chwila, w ktorej prompter przelaczyl wers, a ten potrafi
        // przelaczyc o sekunde za wczesnie (drugi test Adriana: 4 z 6 pre-rolli
        // konczyly sie 0,8–1,1 s przed koncem poprzedniego wersu).
        double prevLast = -1.0e9;
        if (heard != nullptr)
            for (const auto& h : *heard)
                if (h.word < r.firstWord && h.sec <= r.sec)
                    prevLast = std::max (prevLast, h.sec - heardLagSec - historyStartSec);
        if (prevLast > -1.0e8)
        {
            // pierwsze uslyszane slowo powtarzanego wersu po poprzednim — pauza nie
            // moze byc pozniej, bo pre-roll zawieralby poczatek pomylki
            double curFirst = 1.0e9;
            for (const auto& h : *heard)
                if (h.word >= r.firstWord && h.word <= r.lastWord && h.sec <= r.sec
                    && h.sec - heardLagSec - historyStartSec > prevLast)
                    curFirst = std::min (curFirst, h.sec - heardLagSec - historyStartSec);

            bool found = false;
            for (size_t i = 0; i < pausesBefore.size(); ++i)
            {
                const double resumes = resumeAfter[i];
                if (resumes <= prevLast + 0.05) continue;
                if (pausesBefore[i].first > curFirst + 0.30) break;
                cut.sec = pausesBefore[i].second; cut.inPause = true; found = true;
                break;
            }
            if (! found) return onlyCandidates (p); // nie wiemy, gdzie konczy sie poprzedni wers
        }
        else if (heard != nullptr)
            return onlyCandidates (p);    // rozpoznawanie nie uslyszalo poprzedniego wersu: sam znacznik nie wystarczy

        if (! cut.inPause) return onlyCandidates (p);    // bez ciszy nie ma gdzie ciac — nie zgadujemy

        if (! render (p, history, sr, historyStartSec, historyStartSec + cut.sec)) return onlyCandidates (p);
        p.inPause = cut.inPause;

        double best = 1.0e9;
        for (size_t i = 0; i < p.candidates.size(); ++i)
            if (std::fabs (p.candidates[i] - p.toSec) < best) { best = std::fabs (p.candidates[i] - p.toSec); p.chosen = (int) i; }
        return p;
    }

    /** Bez propozycji (i bez dzwieku), ale z lista pauz przed R — czlowiek
        moze wskazac miejsce sam (PunchAim). */
    static PreRollPlan onlyCandidates (const PreRollPlan& p)
    {
        PreRollPlan q;
        q.candidates = p.candidates;
        return q;
    }

    /** Ten sam pre-roll, ale konczacy sie w podanym miejscu (czas w nagraniu)
        — po strzalce. Kandydaci i wybor zostaja w p. */
    bool render (PreRollPlan& p, const std::vector<float>& history, double sr, double historyStartSec, double cutSec) const
    {
        const double to   = cutSec - historyStartSec;
        const double from = std::max (0.0, to - lengthSec);
        if (to - from < minSec) return false;

        const size_t a = (size_t) std::lround (from * sr);
        const size_t b = std::min (history.size(), (size_t) std::lround (to * sr));
        if (b <= a) return false;

        p.audio.assign (history.begin() + (long) a, history.begin() + (long) b);
        p.fromSec = historyStartSec + from;
        p.toSec   = historyStartSec + to;

        const size_t fade = std::min (p.audio.size() / 2, (size_t) std::lround (fadeSec * sr));
        for (size_t k = 0; k < fade; ++k)
        {
            const float g = (float) (k + 1) / (float) (fade + 1);
            p.audio[k] *= g;
            p.audio[p.audio.size() - 1 - k] *= g;
        }
        return true;
    }
};

//==============================================================================
/** Wskazanie miejsca ciecia po R — zawsze, przed czytaniem.

    Pomysl Adriana (12.10, po tescie): zamiast zgadywac i pytac po nagraniu
    ("0 pewnych, 1 do wskazania" bylo niezrozumiale), program po R staje i
    pokazuje pod prompterem przebieg ostatnich sekund ze znacznikiem w
    proponowanej pauzie. Czlowiek przesuwa go strzalkami (← →), zmienia wers
    (↑ ↓), slucha (spacja) i zatwierdza (Enter albo R) — dopiero wtedy gra
    pre-roll i odliczanie. Kazde ciecie jest wiec wskazane przez czlowieka
    (cutByHand), a po nagraniu zostaje tylko odsluch szwow.

    Nagranie przez caly czas idzie dalej; czas wskazywania lezy w kawalku,
    ktory i tak wypadnie. */
struct PunchAim
{
    bool        active = false;
    PreRollPlan plan;            ///< pauzy przed R i wybrana; audio puste do zatwierdzenia
    double      keySec  = 0.0;   ///< chwila R (czas w nagraniu)
    double      viewSec = 10.0;  ///< ile sekund przed R widac na pasku

    void begin (PreRollPlan p, double key)
    {
        plan = std::move (p);
        plan.audio.clear();
        keySec = key;
        active = true;
    }

    void end() { active = false; plan = {}; }

    bool hasCut() const noexcept { return plan.chosen >= 0 && plan.chosen < (int) plan.candidates.size(); }

    /** Wybrane miejsce ciecia (czas w nagraniu); -1 = jeszcze nie wskazane. */
    double cutSec() const noexcept { return hasCut() ? plan.candidates[(size_t) plan.chosen] : -1.0; }

    /** O pauze wczesniej (-1) / pozniej (+1). Bez propozycji pierwsza
        strzalka wybiera ostatnia pauze przed R — najblizsza pomylki, wiec
        zabiera najmniej. Zwraca, czy wybor sie zmienil. */
    bool move (int by)
    {
        const int n = (int) plan.candidates.size();
        if (n == 0 || by == 0) return false;
        const int next = hasCut() ? std::clamp (plan.chosen + by, 0, n - 1) : n - 1;
        if (next == plan.chosen) return false;
        plan.chosen = next;
        return true;
    }

    /** Ta sama pauza w nowym planie (po zmianie wersu ↑ ↓): nowa propozycja,
        ale gdy jej brak — zostaje to, co czlowiek juz wskazal. */
    void replan (PreRollPlan p)
    {
        const double had = cutSec();
        p.audio.clear();
        if (p.chosen < 0 && had >= 0.0)
            for (size_t i = 0; i < p.candidates.size(); ++i)
                if (std::fabs (p.candidates[i] - had) < 1.0e-6) p.chosen = (int) i;
        plan = std::move (p);
    }

    double viewFrom() const noexcept { return keySec - viewSec; }
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
