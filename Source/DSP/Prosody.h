/*
    Prosody.h — tempo, melodia i pauzy. Czyli to, o czym mowi lampka, gdy nie ma
    juz nic do zglaszania w sprawie poziomow.

    Po co to jest. Poziom, przester i szum sa sprawa sprzetu — da sie je ustawic
    raz i zapomniec. Natomiast to, ze poczatkujacy pod wplywem stresu mowi
    poltora raza szybciej, niz mu sie wydaje, albo czyta z kartki jednym tonem,
    jest sprawa CZLOWIEKA i wraca przy kazdym nagraniu. Zaden kompresor tego nie
    naprawi, a sluchacz odchodzi wlasnie od tego, nie od szumu.

    NIE UZYWA ROZPOZNAWANIA MOWY. To jest tu rzecz kluczowa, bo naturalny odruch
    kaze do liczenia slow zamienic mowe na tekst — a to znaczy model, pol
    gigabajta, czekanie i niepewnosc przy polszczyznie. Tymczasem tempa nie
    trzeba liczyc ze slow: sylaba to szczyt glosnosci w pasmie samoglosek,
    poprzedzony i zakonczony dolkiem. Wystarczy je policzyc. Metoda jest znana z
    fonetyki (jadra sylabiczne wedlug de Jonga i Wempego) i dziala na kazdym
    jezyku, bo opiera sie na budowie sylaby, a nie na slowniku.

    Melodia tak samo: rozrzut czestotliwosci podstawowej w poltonach mowi o
    monotonii wszystko, co trzeba, i nie wymaga rozumienia ani jednego slowa.
*/

#pragma once

#include "Mix/Biquad.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace tw
{

struct ProsodyReport
{
    double seconds        = 0.0;
    double speakingSec    = 0.0;   ///< bez pauz
    int    syllables      = 0;
    double syllablesPerMin = 0.0;
    double wordsPerMin    = 0.0;   ///< przeliczone przez srednia dlugosc slowa

    double medianF0Hz     = 0.0;
    double f0RangeSemis   = 0.0;   ///< rozrzut melodii, w poltonach
    bool   monotone       = false;

    int    pauses         = 0;     ///< przerwy dluzsze niz 0,4 s
    double longestPauseSec = 0.0;
    int    longPauses     = 0;     ///< przerwy dluzsze niz 2,5 s

    std::vector<double> fillerAtSec;   ///< podejrzenia o przeciagniete "yyy"
};

//==============================================================================
class Prosody
{
public:
    struct Params
    {
        /** Ile sylab przypada na slowo. Dla polszczyzny mowionej okolo 2,3;
            dla angielskiego 1,4. Bez tego nie da sie podac liczby slow na
            minute, a wlasnie ta liczba cos ludziom mowi. */
        double syllablesPerWord = 2.3;

        double pauseSec      = 0.40;   ///< od tylu liczymy przerwe
        double longPauseSec  = 2.50;   ///< tyle to juz za dlugo
        /* Te dwie liczby sa NASTROJONE, nie wybrane. Punktem odniesienia byl
           tekst o znanej liczbie sylab (107) przeczytany przez syntezator w
           trzech tempach. Przy odstepie 0,09 s detektor liczyl 131 / 121 / 116,
           czyli mylil sie o 8–22% i zawsze w gore — dzielil dlugie samogloski
           na dwie. Przy 0,12 s wychodzi 123 / 112 / 105, czyli od -2% do +15%.

           Wiekszy odstep (0,15 s) poprawia wolna mowe, ale przy szybkiej gubi
           16% sylab — a to gorszy blad, bo program powiedzialby "zwolnij"
           komus, kto juz zwolnil.

           UWAGA: to jest kalibracja na mowie SYNTETYCZNEJ. Prawdziwy glos ma
           inne przejscia miedzy sylabami i te liczby trzeba bedzie sprawdzic
           na nagraniu czlowieka. */
        float  dipDb         = 4.0f;   ///< o tyle musi opasc miedzy sylabami
        double minSyllableSec = 0.12;  ///< szybciej nikt nie mowi
        float  monotoneSemis = 2.0f;   ///< ponizej tego rozrzutu to czytanie z kartki

        double fillerMinSec  = 0.40;   ///< przeciagniete stęknięcie trwa dluzej
        float  fillerJitterSemis = 0.6f;  ///< i ma bardzo stala wysokosc
    };

    Params params;

    ProsodyReport analyse (const std::vector<float>& x, double sr)
    {
        ProsodyReport r;
        r.seconds = x.size() / sr;

        if (x.size() < (size_t) (sr * 0.5))
            return r;

        frameHop = (int) std::llround (sr * 0.010);      // 10 ms
        frameLen = (int) std::llround (sr * 0.040);      // 40 ms

        measureFrames (x, sr);
        findVoicing();
        countSyllables (r, sr);
        describeMelody (r);
        findPauses (r, sr);
        findFillers (r, sr);

        return r;
    }

private:
    //--------------------------------------------------------------------------
    void measureFrames (const std::vector<float>& x, double sr)
    {
        // pasmo samoglosek — tam siedzi energia, ktora tworzy sylabe
        mix::Biquad band;
        band.setCoeffs (mix::design::bandpass (650.0, sr, 0.55));

        std::vector<float> filtered (x.size());
        for (size_t i = 0; i < x.size(); ++i) filtered[i] = band.process (x[i]);

        intensity.clear();
        f0.clear();
        clarity.clear();

        for (size_t start = 0; start + (size_t) frameLen <= x.size();
             start += (size_t) frameHop)
        {
            double sum = 0.0;

            for (int i = 0; i < frameLen; ++i)
                sum += (double) filtered[start + (size_t) i] * filtered[start + (size_t) i];

            intensity.push_back (sum > 1.0e-20 ? 10.0 * std::log10 (sum / frameLen) : -140.0);

            float conf = 0.0f;
            f0.push_back (estimateF0 (x.data() + start, frameLen, sr, conf));
            clarity.push_back (conf);
        }
    }

    /** Wysokosc z autokorelacji. Prosto, ale na mowie dziala: szukamy
        opoznienia, przy ktorym przebieg najbardziej przypomina sam siebie. */
    static float estimateF0 (const float* x, int n, double sr, float& confidence)
    {
        const int minLag = (int) (sr / 400.0);
        const int maxLag = (int) (sr / 70.0);

        if (maxLag >= n) { confidence = 0.0f; return 0.0f; }

        double energy = 0.0;
        for (int i = 0; i < n; ++i) energy += (double) x[i] * x[i];

        if (energy < 1.0e-12) { confidence = 0.0f; return 0.0f; }

        double best = 0.0;
        int bestLag = 0;

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double sum = 0.0, e2 = 0.0;

            for (int i = 0; i + lag < n; ++i)
            {
                sum += (double) x[i] * x[i + lag];
                e2  += (double) x[i + lag] * x[i + lag];
            }

            const double norm = std::sqrt (energy * e2);
            const double c = norm > 1.0e-12 ? sum / norm : 0.0;

            if (c > best) { best = c; bestLag = lag; }
        }

        confidence = (float) best;
        return bestLag > 0 ? (float) (sr / bestLag) : 0.0f;
    }

    void findVoicing()
    {
        voiced.assign (intensity.size(), false);

        for (size_t i = 0; i < intensity.size(); ++i)
            voiced[i] = clarity[i] > 0.35f && f0[i] > 60.0f;
    }

    //--------------------------------------------------------------------------
    /** Jadra sylabiczne: szczyty glosnosci rozdzielone dolkiem.

        Sam szczyt nie wystarcza — glosnosc w mowie faluje bez przerwy i przy
        liczeniu wszystkich maksimow wychodzi kilkakrotnie za duzo. Warunkiem
        jest DOLEK miedzy dwoma szczytami: dopiero opadniecie o kilka decybeli
        znaczy, ze skonczyla sie jedna sylaba i zaczela nastepna. */
    void countSyllables (ProsodyReport& r, double sr)
    {
        if (intensity.empty()) return;

        // prog: liczymy tylko to, co jest wyraznie ponad tlem
        std::vector<double> sorted = intensity;
        std::sort (sorted.begin(), sorted.end());

        const double floorDb = sorted[sorted.size() / 10];
        const double peakDb  = sorted[sorted.size() * 95 / 100];
        const double thresh  = std::max (floorDb + 8.0, peakDb - 25.0);

        peaks.clear();

        int lastPeak = -1000;
        const int minGap = std::max (1, (int) (params.minSyllableSec * sr / frameHop));

        for (size_t i = 1; i + 1 < intensity.size(); ++i)
        {
            if (intensity[i] < thresh) continue;
            if (! voiced[i]) continue;
            if (intensity[i] < intensity[i - 1] || intensity[i] < intensity[i + 1]) continue;

            if (! peaks.empty())
            {
                // dolek miedzy tym a poprzednim szczytem
                double dip = intensity[i];

                for (size_t k = (size_t) lastPeak; k < i; ++k)
                    dip = std::min (dip, intensity[k]);

                if (intensity[i] - dip < params.dipDb) continue;
                if ((int) i - lastPeak < minGap) continue;
            }

            peaks.push_back ((int) i);
            lastPeak = (int) i;
        }

        r.syllables = (int) peaks.size();

        // Tempo liczymy wzgledem czasu MOWIENIA, nie calego nagrania — inaczej
        // dluga pauza na poczatku robi z czlowieka mrukliwego gadule.
        //
        // Ale "mowienie" to NIE to samo co "dzwiecznosc". Pierwsza wersja
        // liczyla tylko ramki dzwieczne, czyli samogloski i spolgloski
        // dzwieczne — a "sz", "s", "t", "k", "p" wypadaly z mianownika, choc
        // czlowiek je wtedy wymawia. W polszczyznie to kilkanascie procent
        // czasu, wiec zmierzone tempo wychodzilo o tyle za wysokie i program
        // mowilby "zwolnij" komus, kto mowi normalnie.
        //
        // Mowa to wszystko, co jest wyraznie ponad tlem, dzwieczne czy nie.
        int speakingFrames = 0;
        for (size_t i = 0; i < intensity.size(); ++i)
            if (intensity[i] > thresh) ++speakingFrames;

        r.speakingSec = speakingFrames * frameHop / sr;

        if (r.speakingSec > 0.5)
        {
            r.syllablesPerMin = r.syllables / r.speakingSec * 60.0;
            r.wordsPerMin = r.syllablesPerMin / params.syllablesPerWord;
        }
    }

    //--------------------------------------------------------------------------
    void describeMelody (ProsodyReport& r)
    {
        std::vector<double> semis;

        for (size_t i = 0; i < f0.size(); ++i)
            if (voiced[i] && f0[i] > 60.0f && f0[i] < 400.0f)
                semis.push_back (12.0 * std::log2 (f0[i] / 100.0));

        if (semis.size() < 10) return;

        std::vector<double> sorted = semis;
        std::sort (sorted.begin(), sorted.end());

        r.medianF0Hz = 100.0 * std::pow (2.0, sorted[sorted.size() / 2] / 12.0);

        // Rozrzut liczony z percentyli, nie z odchylenia standardowego: jeden
        // blad detektora wysokosci na oktawe potrafi rozdac odchylenie, a
        // percentyle sa na to odporne.
        const double lo = sorted[sorted.size() * 10 / 100];
        const double hi = sorted[sorted.size() * 90 / 100];

        r.f0RangeSemis = hi - lo;
        r.monotone = r.f0RangeSemis < params.monotoneSemis;
    }

    //--------------------------------------------------------------------------
    void findPauses (ProsodyReport& r, double sr)
    {
        const double frameSec = frameHop / sr;
        int run = 0;

        for (size_t i = 0; i <= voiced.size(); ++i)
        {
            const bool speaking = i < voiced.size() && voiced[i];

            if (! speaking) { ++run; continue; }

            const double len = run * frameSec;

            if (len >= params.pauseSec)
            {
                ++r.pauses;
                r.longestPauseSec = std::max (r.longestPauseSec, len);
                if (len >= params.longPauseSec) ++r.longPauses;
            }

            run = 0;
        }
    }

    //--------------------------------------------------------------------------
    /** Przeciagniete "yyy" i "eee".

        Podpis jest charakterystyczny i nie wymaga rozumienia slowa: dzwiek
        dzwieczny, trwajacy ponad 400 ms bez przerwy, o wysokosci STALEJ z
        dokladnoscia do ulamka poltonu. W normalnej mowie tak nie ma — nawet
        przeciagnieta samogloska w slowie ma opadajaca lub rosnaca melodie,
        bo niesie akcent. Placki bez melodii to wypelniacz.

        To jest wykrywanie podejrzen, nie wyrok. Dlatego raport podaje CZASY,
        a nie liczbe do pokazania uzytkownikowi jako ocena. */
    void findFillers (ProsodyReport& r, double sr)
    {
        const double frameSec = frameHop / sr;
        const int minFrames = (int) (params.fillerMinSec / frameSec);

        size_t i = 0;

        while (i < voiced.size())
        {
            if (! voiced[i]) { ++i; continue; }

            size_t j = i;
            while (j < voiced.size() && voiced[j]) ++j;

            if ((int) (j - i) >= minFrames)
            {
                // rozrzut wysokosci wewnatrz tego odcinka
                std::vector<double> semis;

                for (size_t k = i; k < j; ++k)
                    if (f0[k] > 60.0f && f0[k] < 400.0f)
                        semis.push_back (12.0 * std::log2 (f0[k] / 100.0));

                if (semis.size() >= (size_t) minFrames)
                {
                    std::sort (semis.begin(), semis.end());
                    const double spread = semis[semis.size() * 9 / 10] - semis[semis.size() / 10];

                    if (spread < params.fillerJitterSemis)
                        r.fillerAtSec.push_back (i * frameSec);
                }
            }

            i = j;
        }
    }

    int frameHop = 480, frameLen = 1920;
    std::vector<double> intensity;
    std::vector<float>  f0, clarity;
    std::vector<bool>   voiced;
    std::vector<int>    peaks;
};

} // namespace tw
