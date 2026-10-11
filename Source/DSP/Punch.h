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

#include <algorithm>
#include <cmath>
#include <vector>

namespace tw
{

struct Removal { double from = 0.0, to = 0.0; };

class Punch
{
public:
    double resumeLookSec = 2.0;     ///< jak daleko po R szukac pauzy
    double crossfadeSec  = 0.020;

    /** Wyciecia z zapisu wersow i pauz w surowym nagraniu. */
    std::vector<Removal> plan (const std::vector<LineMark>& marks, const CutFinder& cuts) const
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
                    from = marks[j].source == LineSource::Retake ? resumeOf[j]
                                                                 : cuts.find (marks[j].sec).sec;
                    break;
                }
            if (from < 0.0) continue;            // R zanim wers sie zaczal — nie ma czego ciac

            from = std::min (from, r.sec);
            if (resume > from) out.push_back ({ from, resume });
        }

        std::sort (out.begin(), out.end(), [] (const Removal& a, const Removal& b) { return a.from < b.from; });
        std::vector<Removal> merged;
        for (const auto& x : out)
        {
            if (! merged.empty() && x.from <= merged.back().to) merged.back().to = std::max (merged.back().to, x.to);
            else merged.push_back (x);
        }
        return merged;
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
};

} // namespace tw
