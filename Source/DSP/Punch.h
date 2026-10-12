/*
    Punch.h — z jednego ciaglego nagrania i klawisza R robi czysty plik.

    ---------------------------------------------------------------------------
    PO CO

    Krok 3 nagrywania "punch and roll" (Tools/ZADANIA.md, zad. 4). Czlowiek
    czyta; gdy sie pomyli, wciska R i czyta wers jeszcze raz. Na koncu ma byc
    JEDEN plik, w ktorym pomylki nie ma.

    ---------------------------------------------------------------------------
    DLACZEGO NIE RUSZAMY ZAPISU W TRAKCIE

    Nagrywanie idzie jak dotad — w calosci, z pomylkami — bo zapis w watku
    dzwieku to najczulsze miejsce programu, a surowe nagranie to jedyna rzecz,
    ktorej nie da sie odtworzyc. Przy R zapamietujemy tylko, CO sie stalo
    (LineLog.h). Po zatrzymaniu wycinamy kawalki i zapisujemy czysty plik;
    surowe nagranie zostaje obok, w osobnym folderze.

    ---------------------------------------------------------------------------
    CO WYCINAMY

    Dla kazdego R (zrodlo Retake w LineLog):
      od   = poczatek tego wersu: pauza przed nim (CutPoint.h) — albo, gdy wers
             byl juz raz powtarzany, dokladnie miejsce poprzedniego wznowienia;
      do   = wznowienie: pauza tuz po R, 0,15 s przed tym, jak mowa rusza
             znowu (czlowiek po R zwykle bierze oddech).
    Nakladajace sie wyciecia sie sklejaja. Szwy maja przenikanie 20 ms; oba
    lezą w ciszy, wiec nie ma co slychac.
*/

#pragma once

#include "CutPoint.h"
#include "LineLog.h"
#include "TakeWatchCore.h"

#include <algorithm>
#include <string>
#include <cmath>
#include <vector>

namespace tw
{

struct Removal
{
    double from = 0.0, to = 0.0;
    int lineFirst = -1;     ///< slowa wersu, ktory jest powtarzany (-1 = nie wiadomo)
    int lineLast  = -1;
    bool mixedLines = false;   ///< sklejone wyciecia roznych wersow — nie wiadomo, co jest w srodku
};

/** Slowo tekstu uslyszane przez rozpoznawanie mowy: ktore i kiedy je oddalo
    (czas nagrania, z opoznieniem rozpoznawania). */
struct HeardWord { double sec = 0.0; int word = 0; };

/** Wyciecie, ktorego NIE zrobilismy — i dlaczego. Pomylka zostaje w nagraniu
    do recznego wyciecia na ekranie Montaz. */
struct SkippedCut { Removal cut; std::string why; };

class Punch
{
public:
    double resumeLookSec = 2.0;     ///< jak daleko po R szukac pauzy
    double crossfadeSec  = 0.020;

    /** Wyciecia z zapisu wersow i pauz w surowym nagraniu.

        Poczatek wersu musi lezec w PAUZIE. Gdy CutFinder pauzy nie znalazl
        (zgadl czas), nie tniemy wcale: ciecie "na oko" moze trafic w slowo. */
    std::vector<Removal> plan (const std::vector<LineMark>& marks, const CutFinder& cuts,
                               std::vector<SkippedCut>* skipped = nullptr) const
    {
        std::vector<Removal> out;
        std::vector<double> resumeOf (marks.size(), -1.0);

        for (size_t i = 0; i < marks.size(); ++i)
        {
            const auto& r = marks[i];
            if (r.source != LineSource::Retake) continue;

            const double resume = resumeAfter (r.sec, cuts);
            resumeOf[i] = resume;

            // poczatek wersu: ostatnie wejscie w ten wers przed R
            double from = -1.0;
            for (size_t j = i; j-- > 0;)
                if (marks[j].firstWord == r.firstWord)
                {
                    if (marks[j].source == LineSource::Retake) { from = resumeOf[j]; break; }   // ten sam wers
                    const auto c = cuts.find (marks[j].sec);
                    if (! c.inPause)
                    {
                        if (skipped != nullptr) skipped->push_back ({ { c.sec, resume }, "przed wersem nie ma ciszy, a ciąć w słowie nie wolno" });
                        from = -2.0;
                        break;
                    }
                    from = c.sec;
                    break;
                }
            if (from < 0.0) continue;            // R zanim wers sie zaczal (albo bez ciszy) — nie tniemy

            from = std::min (from, r.sec);
            if (resume > from) out.push_back ({ from, resume, r.firstWord, r.lastWord });
        }

        std::sort (out.begin(), out.end(), [] (const Removal& a, const Removal& b) { return a.from < b.from; });
        std::vector<Removal> merged;
        for (const auto& x : out)
        {
            if (! merged.empty() && x.from <= merged.back().to)
            {
                merged.back().to = std::max (merged.back().to, x.to);
                if (x.lineFirst != merged.back().lineFirst) merged.back().mixedLines = true;
            }
            else merged.push_back (x);
        }
        return merged;
    }

    //==========================================================================
    /** STRAZNIK: wyciecie tylko wtedy, gdy wszystko, co wycina, zostalo potem
        przeczytane jeszcze raz.

        To jest najwazniejsza zasada calego punch and roll: zostawiona pomylka
        to niewygoda (czlowiek wytnie ja recznie), wyciete dobre slowo to
        katastrofa. Pomiar (Docs/badania/SYMULACJA.md): R z rozpoznawaniem mowy
        trafia we wlasciwy wers w 90-96%; w pozostalych prompter cofa za
        daleko — i jesli czlowiek NIE przeczyta wtedy jeszcze raz poprzedniego
        wersu, wyciecie zabraloby dobry wers.

        Sprawdzamy to slowami, ktore rozpoznawanie naprawde uslyszalo:
          w wycinanym kawalku  — najwczesniejsze slowo tekstu W,
          po wznowieniu        — najwczesniejsze slowo tekstu P
                                 (do nastepnego wyciecia, najwyzej 20 s).
        Tniemy tylko, gdy P <= W: czytanie po wznowieniu zaczelo sie nie
        pozniej niz to, co wycinamy. Gdy ktorejs strony nie da sie ustalic
        (nic nie uslyszane) — nie tniemy.

        Slowa SPRZED wersu, od ktorego zaczyna sie wyciecie (numer mniejszy
        niz lineFirst), nie licza sie do W: padly przed poczatkiem wyciecia,
        tylko rozpoznawanie oddalo je pozniej. Pierwsza wersja odcinala je
        marginesem czasu 0,4 s — i razem z nimi pomylke na pierwszym slowie
        wersu, wiec straznik odrzucal prawie kazde dobre wyciecie (proba na
        prawdziwym glosie, Docs/badania/SYMULACJA.md). */
    double heardLagSec   = 0.58;   ///< mediana opoznienia rozpoznawania (pomiar na 12 nagraniach)

    /*  Dwie dalsze zasady — z proby na prawdziwym glosie (11 lektorow,
        Docs/badania/SYMULACJA.md). Rozpoznawanie potrafi pomylic podobne wersy
        ("Patrz, do moich..." / "Patrz, jak widzac...") i przypisac powtorke
        innemu wersowi; wtedy slow, ktore zginelyby, straznik po prostu nie
        widzi. Dlatego:
          - kazde slowo uslyszane w wycinanym kawalku musi nalezec do wersu,
            ktory jest powtarzany (lineFirst..lineLast) — slowo innego wersu
            znaczy, ze wycinamy cos wiecej niz pomylke;
          - mowy w wycinanym kawalku nie moze byc wiecej, niz tlumacza
            uslyszane slowa (1 s + 0,7 s na slowo): nierozpoznana mowa to
            moze byc dobre czytanie.  */
    double secondsPerHeardWord = 0.7;
    double unexplainedSec      = 1.0;

    std::vector<Removal> guard (const std::vector<Removal>& plan, const std::vector<HeardWord>& heard,
                                std::vector<SkippedCut>* skipped = nullptr, const CutFinder* speech = nullptr) const
    {
        std::vector<Removal> out;
        for (size_t i = 0; i < plan.size(); ++i)
        {
            const auto& c = plan[i];
            const double afterEnd = std::min (i + 1 < plan.size() ? plan[i + 1].from : 1.0e12, c.to + 20.0);

            int inside = -1, after = -1, insideCount = 0;
            bool foreign = false;
            for (const auto& h : heard)
            {
                const double t = h.sec - heardLagSec;      // kiedy slowo padlo
                if (t >= c.from && t < c.to)
                {
                    if (c.lineFirst >= 0 && h.word < c.lineFirst) continue;      // poprzedni wers, oddany pozniej
                    if (c.lineLast >= 0 && h.word > c.lineLast) foreign = true;
                    inside = inside < 0 ? h.word : std::min (inside, h.word);
                    ++insideCount;
                }
                else if (t >= c.to && t < afterEnd)
                    after = after < 0 ? h.word : std::min (after, h.word);
            }

            // ile mowy (nie-pauzy) jest w wycinanym kawalku
            double voiced = c.to - c.from;
            if (speech != nullptr)
                for (const auto& p : speech->pauses())
                    voiced -= std::max (0.0, std::min (p.second, c.to) - std::max (p.first, c.from));

            const char* why = nullptr;
            if (c.mixedLines)     why = "wycięcia różnych wersów nachodzą na siebie";
            else if (inside < 0)  why = "w wycinanym kawałku rozpoznawanie mowy niczego nie usłyszało";
            else if (foreign)     why = "w wycinanym kawałku są słowa innego wersu";
            else if (speech != nullptr && voiced > unexplainedSec + secondsPerHeardWord * insideCount)
                                  why = "w wycinanym kawałku jest mowa, której rozpoznawanie nie przypisało do tekstu";
            else if (after < 0)   why = "po wznowieniu rozpoznawanie mowy niczego nie usłyszało";
            else if (after > inside) why = "po wznowieniu nie przeczytano jeszcze raz wszystkiego, co byłoby wycięte";

            if (why == nullptr) out.push_back (c);
            else if (skipped != nullptr) skipped->push_back ({ c, why });
        }
        return out;
    }

    /** Gdzie wznowic po R: koniec pierwszej pauzy po R, 0,15 s przed mowa. */
    double resumeAfter (double keySec, const CutFinder& cuts) const
    {
        for (const auto& p : cuts.pauses())
        {
            if (p.second <= keySec) continue;
            if (p.first > keySec + resumeLookSec) break;
            return std::max (keySec, std::max (p.first, p.second - cuts.params.leadInSec));
        }
        return keySec;
    }

    /** Czysty dzwiek: kawalki poza wycieciami, szwy z przenikaniem. */
    std::vector<float> render (const std::vector<float>& in, double sr, const std::vector<Removal>& cuts) const
    {
        if (cuts.empty()) return in;

        const size_t fade = std::max<size_t> (1, (size_t) std::lround (crossfadeSec * sr));
        std::vector<float> out;
        out.reserve (in.size());

        size_t pos = 0;
        auto clampIdx = [&] (double s) { return std::min (in.size(), (size_t) std::max (0.0, std::round (s * sr))); };

        for (const auto& c : cuts)
        {
            const size_t a = clampIdx (c.from), b = clampIdx (c.to);
            if (a < pos || b <= a) continue;

            out.insert (out.end(), in.begin() + (long) pos, in.begin() + (long) a);

            // przenikanie: koncowka przed wycieciem (juz w out) z poczatkiem po nim
            const size_t n = std::min ({ fade, out.size(), in.size() - b });
            for (size_t k = 0; k < n; ++k)
            {
                const double t = (double) (k + 1) / (double) (n + 1);
                float& o = out[out.size() - n + k];
                o = (float) (o * std::cos (t * M_PI / 2.0) + in[b + k] * std::sin (t * M_PI / 2.0));
            }
            pos = b + n;
        }
        if (pos < in.size()) out.insert (out.end(), in.begin() + (long) pos, in.end());
        return out;
    }

    /** Szew: czy tlo po obu stronach ma ten sam poziom.

        Ciecie lezy w ciszy, wiec trzasku nie bedzie — ale gdy miedzy pomylka
        a powtorka wlaczyla sie lodowka albo czlowiek przysunal sie do
        mikrofonu, cisza po jednej stronie szwu jest glosniejsza i to slychac
        jako "latke". Mierzymy 150 ms ciszy tuz przed i tuz po wycieciu
        (ta sama miara co w MontageRender: roznica ponad 1,5 dB jest
        slyszalna). Naprawic sie tego nie da — da sie powiedziec. */
    struct Seam { double at = 0.0; double levelDiffDb = 0.0; bool audible = false; };

    std::vector<Seam> seams (const std::vector<float>& in, double sr, const std::vector<Removal>& cuts) const
    {
        std::vector<Seam> out;
        auto rmsDb = [&] (double from, double to)
        {
            const size_t a = (size_t) std::max (0.0, from * sr);
            const size_t b = std::min (in.size(), (size_t) std::max (0.0, to * sr));
            double s = 0.0;
            for (size_t i = a; i < b; ++i) s += (double) in[i] * in[i];
            return b > a ? 10.0 * std::log10 (s / (double) (b - a) + 1.0e-12) : -120.0;
        };

        for (const auto& c : cuts)
        {
            Seam s;
            s.at = remap (c.from, cuts);
            // Po stronie wznowienia mierzymy 150 ms PRZED nim, nie po: wznowienie
            // lezy 0,15 s przed mowa (CutFinder::leadInSec), wiec okno "po"
            // konczylo sie dokladnie na pierwszej glosce i lapalo jej poczatek.
            // Symulacja sesji (Docs/badania/SYMULACJA.md) dala przez to
            // "slyszalny szew" przy idealnie stalym tle. Tuz przed wznowieniem
            // jest ta sama cisza pokoju, ktora po nim zagra — bez mowy.
            s.levelDiffDb = rmsDb (c.to - 0.15, c.to) - rmsDb (c.from - 0.15, c.from);
            s.audible = std::fabs (s.levelDiffDb) > 1.5;
            out.push_back (s);
        }
        return out;
    }

    /** Czas z surowego nagrania na czas w czystym pliku. Czas wewnatrz
        wyciecia trafia na szew. */
    double remap (double sec, const std::vector<Removal>& cuts) const
    {
        double removed = 0.0;
        for (const auto& c : cuts)
        {
            if (sec >= c.to) removed += (c.to - c.from) + crossfadeSec;
            else if (sec > c.from) return c.from - removed;
            else break;
        }
        return std::max (0.0, sec - removed);
    }

    /** Zdarzenia lampki na czas czystego pliku. Te z wycietych kawalkow
        znikaja — ocena po nagraniu nie moze kazac powtarzac pomylki, ktorej
        w pliku juz nie ma. */
    std::vector<Event> remapEvents (const std::vector<Event>& events, const std::vector<Removal>& cuts) const
    {
        std::vector<Event> out;
        for (auto e : events)
        {
            const bool gone = std::any_of (cuts.begin(), cuts.end(),
                                           [&] (const Removal& c) { return e.timeSec >= c.from && e.timeSec < c.to; });
            if (gone) continue;
            e.timeSec  = remap (e.timeSec, cuts);
            e.redoFrom = remap (e.redoFrom, cuts);
            out.push_back (e);
        }
        return out;
    }
};

} // namespace tw
