/*
    Restore.h — naprawianie tego, czego brama nie ruszy: szumu POD mowa,
    plozyw i mlaskow.

    To jest ta czesc, ktora odroznia "przyciszylem cisze" od "nagranie brzmi
    czysto". Brama dziala tylko miedzy zdaniami — w trakcie mowienia szum
    lodowki, wentylatora i ulicy leci razem z glosem i zadna dynamika go stamtad
    nie wyjmie. Trzeba wejsc w widmo.
*/

#pragma once

#include "Biquad.h"
#include "Dynamics.h"
#include "Fft.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mix
{

//==============================================================================
/** Odszumianie widmowe.

    Zasada: rozloz sygnal na prazki, oszacuj, ile w kazdym z nich jest szumu, i
    przytlum kazdy prazek proporcjonalnie do tego, ile w nim szumu wobec
    calosci. Skladajac z powrotem, dostajesz to samo nagranie bez tla.

    W praktyce caly problem polega na tym, zeby NIE zrobic przy tym gorzej.
    Naiwne odejmowanie widma daje "szum muzyczny": pojedyncze prazki przezywaja
    tlumienie i skacza z ramki na ramke, co slychac jako dzwoniace, wodne
    pobrzekiwanie. Jest to gorsze od szumu, ktory usuwalismy, bo szum jest
    stacjonarny i ucho go odfiltrowuje, a pobrzekiwanie zwraca na siebie uwage.

    Trzy rzeczy, ktore temu zapobiegaja, wszystkie tu sa:

      DNO TLUMIENIA        nie schodzimy ponizej zadanej granicy (domyslnie
                           -14 dB). Resztka szumu jest niesłyszalna pod mowa,
                           a trzyma prazki przy zyciu, wiec nie skacza.
      WYGLADZANIE W CZASIE  wzmocnienie prazka nie moze zmieniac sie z ramki na
                           ramke skokowo — ma wlasny atak i zwolnienie.
      WYGLADZANIE W PASMIE  sasiednie prazki usredniamy, bo prawdziwy dzwiek
                           jest szerszy niz jeden prazek, a artefakt nie.

    Profil jest srednia z ciaglych, cichych odcinkow nagrania, oddzielonych
    poziomem od glosu. Gdy nie ma wiarygodnego fragmentu tla, modul pomija
    odszumianie. Wygladzanie ogranicza artefakty, ale ich braku nie gwarantuje.
*/
class SpectralDenoise
{
public:
    struct Params
    {
        int    fftSize      = 2048;
        float  reductionDb  = -14.0f;  ///< dno tlumienia
        float  oversubtract = 1.6f;    ///< ile razy "wiecej niz zmierzone" odejmujemy
        float  noiseBias    = 5.0f;    ///< poprawka na zanizenie minimum
        double minWindowSec = 2.0;     ///< okno szukania minimum
        float  smoothUpMs   = 8.0f;
        float  smoothDownMs = 60.0f;
    };

    Params params;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        n  = params.fftSize;
        hop = n / 4;
        bins = n / 2 + 1;

        fft.prepare (n);
        hann (win, n);

        inBuf.assign ((size_t) n, 0.0f);
        outBuf.assign ((size_t) n, 0.0f);
        re.assign ((size_t) n, 0.0);
        im.assign ((size_t) n, 0.0);

        noise.assign ((size_t) bins, 1.0e-9);
        gainState.assign ((size_t) bins, 1.0);
        smoothed.assign ((size_t) bins, 0.0);



        const double frameSec = hop / sr;
        aUp   = std::exp (-frameSec / (0.001 * params.smoothUpMs));
        aDown = std::exp (-frameSec / (0.001 * params.smoothDownMs));

        fill = 0;
        std::fill (outBuf.begin(), outBuf.end(), 0.0f);
    }

    /** Przerabia caly przebieg. Wyjscie ma dlugosc wejscia; poczatek jest
        opozniony o jedno okno i tyle samo doklejone na koncu. */
    void render (const std::vector<float>& in, std::vector<float>& out)
    {
        out.assign (in.size(), 0.0f);
        std::fill(gainState.begin(),gainState.end(),1.0);
        std::fill(smoothed.begin(),smoothed.end(),0.0);
        used = learnQuietProfile(in);
        if (!used) { out = in; return; }

        std::vector<float> tail (in.size() + (size_t) (2 * n), 0.0f);
        std::copy (in.begin(), in.end(), tail.begin() + n);

        std::vector<float> acc (tail.size() + (size_t) n, 0.0f);

        for (size_t start = 0; start + (size_t) n <= tail.size(); start += (size_t) hop)
        {
            for (int i = 0; i < n; ++i)
            {
                re[(size_t) i] = tail[start + (size_t) i] * win[(size_t) i];
                im[(size_t) i] = 0.0;
            }

            fft.run (re, im, false);
            processSpectrum();
            fft.run (re, im, true);

            for (int i = 0; i < n; ++i)
                acc[start + (size_t) i] += (float) (re[(size_t) i] * win[(size_t) i] / 1.5);
        }

        // opoznienie calego przetwarzania to jedno okno
        for (size_t i = 0; i < out.size(); ++i)
            out[i] = acc[i + (size_t) n];
    }

    /** Ile decybeli szumu zdjeto — do raportu. */
    double measuredNoiseDb() const { return lastNoiseDb; }
    bool didProcess() const noexcept { return used; }

private:
    bool learnQuietProfile (const std::vector<float>& x)
    {
        std::vector<double> levels;
        for (size_t start = 0; start + (size_t) n <= x.size(); start += (size_t) hop)
        {
            double e = 0;
            for (int i = 0; i < n; ++i) e += double(x[start+i]) * x[start+i];
            levels.push_back (10.0 * std::log10 (e/n + 1.0e-30));
        }
        if (levels.size() < 8) return false;
        std::vector<double> nonzero;
        for (double v : levels) if (v > -100.0) nonzero.push_back (v);
        if (nonzero.size() < 8) return false;
        std::sort (nonzero.begin(), nonzero.end());
        const double quiet = nonzero[nonzero.size()/10];
        const double voice = nonzero[nonzero.size()*9/10];
        if (voice - quiet < 6.0) return false;
        // Demand a stable quiet run, not isolated low-energy speech frames.
        std::vector<bool> eligible(levels.size(),false);
        for(size_t a=0;a<levels.size();) {
            if(levels[a]<=-100.0 || levels[a]>quiet+2.0) { ++a;continue; }
            size_t b=a+1;
            while(b<levels.size() && levels[b]>-100.0 && levels[b]<=quiet+2.0) ++b;
            if((b-a)*hop/sr>=0.20) std::fill(eligible.begin()+a,eligible.begin()+b,true);
            a=b;
        }
        std::fill (noise.begin(), noise.end(), 0.0);
        int count = 0;
        for (size_t frame = 0; frame < levels.size(); ++frame)
        {
            if (!eligible[frame]) continue;
            const size_t start = frame * (size_t) hop;
            for (int i = 0; i < n; ++i) { re[i] = x[start+i]*win[i]; im[i] = 0; }
            fft.run (re, im, false);
            for (int k = 0; k < bins; ++k) noise[k] += re[k]*re[k]+im[k]*im[k];
            ++count;
        }
        if (count < 4) return false;
        double sum = 0;
        for (auto& v : noise) { v /= count; sum += v; }
        lastNoiseDb = 10.0 * std::log10 (sum / (double(n)*n) + 1.0e-30);
        return true;
    }

    void processSpectrum()
    {
        for (int k = 0; k < bins; ++k)
        {
            const double p = re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];
            smoothed[(size_t) k] = 0.7 * smoothed[(size_t) k] + 0.3 * p;
        }

        // ---- wzmocnienie prazka ----------------------------------------------
        const double floorGain = std::pow (10.0, params.reductionDb / 20.0);
        std::vector<double> want ((size_t) bins);

        for (int k = 0; k < bins; ++k)
        {
            const double p = smoothed[(size_t) k];
            const double nz = noise[(size_t) k] * params.oversubtract;

            double g = p > 1.0e-18 ? (p - nz) / p : 0.0;
            g = std::sqrt (std::max (0.0, g));          // z mocy na amplitude
            want[(size_t) k] = std::max ((double) floorGain, g);
        }

        // ---- wygladzenie w pasmie: trzy sasiednie prazki ---------------------
        // Prawdziwy dzwiek zajmuje kilka prazkow. Pojedynczy prazek wystajacy
        // ponad sasiadow jest artefaktem, a nie sygnalem — usrednienie go
        // sciaga i to wlasnie ono zabija szum muzyczny.
        std::vector<double> sm ((size_t) bins);

        for (int k = 0; k < bins; ++k)
        {
            const double a = want[(size_t) std::max (0, k - 1)];
            const double b = want[(size_t) k];
            const double c = want[(size_t) std::min (bins - 1, k + 1)];
            sm[(size_t) k] = (a + 2.0 * b + c) * 0.25;
        }

        // ---- wygladzenie w czasie --------------------------------------------
        for (int k = 0; k < bins; ++k)
        {
            const double target = sm[(size_t) k];
            const double a = target > gainState[(size_t) k] ? aUp : aDown;
            gainState[(size_t) k] = a * gainState[(size_t) k] + (1.0 - a) * target;

            re[(size_t) k] *= gainState[(size_t) k];
            im[(size_t) k] *= gainState[(size_t) k];

            // widmo jest symetryczne — druga polowa musi dostac to samo
            if (k > 0 && k < bins - 1)
            {
                re[(size_t) (n - k)] *= gainState[(size_t) k];
                im[(size_t) (n - k)] *= gainState[(size_t) k];
            }
        }
    }

    bool used = false;
    double sr = 48000.0, lastNoiseDb = -140.0;
    int n = 2048, hop = 512, bins = 1025, fill = 0;
    double aUp = 0.0, aDown = 0.0;

    Fft fft;
    std::vector<double> win, re, im, noise, gainState, smoothed;
    std::vector<float> inBuf, outBuf;
};

//==============================================================================
/** Tlumik plozyw.

    "P" i "B" wystrzeliwuja z ust powietrze, ktore uderza w membrane i daje
    kilkudziesieciohercowy grzmot o poziomie czesto wyzszym niz cala reszta
    slowa. Staly filtr gornoprzepustowy tego nie zalatwia: musialby ciac tak
    wysoko, ze zabralby glosowi cala podstawe.

    Dlatego filtr jest DYNAMICZNY — wchodzi tylko wtedy, gdy dol nagle wystaje
    ponad srodek pasma, i tylko na czas trwania uderzenia. Wykrywanie idzie na
    STOSUNKU dolu do srodka, nie na samym poziomie dolu: niski glos ma duzo
    energii nisko przez caly czas i nie wolno go za to karac.
*/
class DePlosive
{
public:
    struct Params
    {
        double splitHz    = 150.0;
        float  thresholdDb = 6.0f;    ///< o ile dol musi wystawac ponad srodek
        float  maxCutDb   = -18.0f;
        double attackMs   = 3.0;
        double releaseMs  = 120.0;
    };

    Params params;

    void prepare (double sampleRate)
    {
        sr = sampleRate;

        // Filtr gornoprzepustowy czwartego rzedu — do NIEGO przenikamy, gdy
        // wykryjemy uderzenie.
        //
        // Pierwsza wersja robila to inaczej: wyciagala pasmo dolne jako
        // "x minus dolnoprzepustowe x" i tlumila je osobno, tak jak robi to
        // de-esser. Na gorze to dziala, bo tam pasma sa daleko od siebie. Na
        // dole NIE dziala: przy 70 Hz i podziale na 150 Hz filtr przesuwa faze
        // o kilkadziesiat stopni, wiec odejmowanie przestaje byc odejmowaniem.
        // Zmierzone: detektor melduje redukcje 17 dB, a grzmot na wyjsciu
        // jest o 1,4 dB GLOSNIEJSZY niz byl.
        hp1.setCoeffs (design::highpass (params.splitHz, sr, 0.7071));
        hp2.setCoeffs (design::highpass (params.splitHz, sr, 0.7071));

        low.setCoeffs (design::bandpass (80.0, sr, 0.9));
        mid.setCoeffs (design::bandpass (1200.0, sr, 0.8));

        env.prepare (sr);
        env.setTimes (params.attackMs, params.releaseMs);

        fast = (float) std::exp (-1.0 / (0.005 * sr));
        reset();
    }

    void reset()
    {
        hp1.reset(); hp2.reset(); low.reset(); mid.reset();
        env.reset();
        lowRms = midRms = 0.0f;
        lastGr = 0.0f;
    }

    inline float process (float x) noexcept
    {
        const float l = low.process (x);
        const float m = mid.process (x);

        lowRms = fast * lowRms + (1.0f - fast) * l * l;
        midRms = fast * midRms + (1.0f - fast) * m * m;

        const float ratioDb = 10.0f * std::log10 ((lowRms + 1.0e-12f) / (midRms + 1.0e-12f));
        const float over = ratioDb - params.thresholdDb;

        float wantDb = 0.0f;
        if (over > 0.0f)
            wantDb = std::max (params.maxCutDb, -over);

        const float smoothedDb = -env.process (-wantDb);
        lastGr = smoothedDb;

        // Filtr chodzi ZAWSZE, zeby jego stan byl ciagly — inaczej wlaczenie go
        // w polowie uderzenia samo daje trzask. Zmienia sie tylko proporcja
        // przenikania: 0 to czyste wejscie, 1 to samo pasmo powyzej 150 Hz.
        const float filtered = hp2.process (hp1.process (x));
        const float mixAmount = params.maxCutDb < 0.0f
                              ? std::min (1.0f, std::max (0.0f, smoothedDb / params.maxCutDb))
                              : 0.0f;

        return x * (1.0f - mixAmount) + filtered * mixAmount;
    }

    float gainReductionDb() const noexcept { return lastGr; }

private:
    double sr = 48000.0;
    Biquad hp1, hp2, low, mid;
    EnvelopeDb env;
    float fast = 0.0f, lowRms = 0.0f, midRms = 0.0f, lastGr = 0.0f;
};

//==============================================================================
/** Usuwanie mlaskow i trzaskow.

    Mlask to pojedynczy, bardzo krotki impuls — kilka, kilkanascie probek. W
    widmie jest wszedzie, wiec zadnym filtrem sie go nie wytnie bez szkody dla
    glosu. Jedyne, co dziala, to znalezc go w czasie i ZASTAPIC tym, co
    powinno tam byc.

    Wykrywanie: predyktor liniowy drugiego rzedu przewiduje kazda probke z
    dwoch poprzednich. Mowa jest gladka, wiec blad przewidywania jest maly.
    Trzask lamie te gladkosc i blad skacze — kilkanascie razy ponad swoja
    biezaca srednia. Naprawa: interpolacja szescienna przez dziure.

    Wazne ograniczenie: to dziala na TRZASKI, nie na "brzydkie sykniecie".
    Program nie ma prawa poprawiac tego, jak ktos mowi — tylko to, co zepsul
    sprzet albo slina.
*/
class DeClick
{
public:
    struct Params
    {
        float sensitivity = 25.0f;   ///< ile razy ponad mediane to juz trzask
        int   maxLength   = 48;      ///< dluzsze zdarzenia to nie sa trzaski
        float isolation   = 0.1f;    ///< blad przed zdarzeniem najwyzej taki ulamek jego szczytu
    };

    Params params;
    int repaired = 0;

    void render (const std::vector<float>& in, std::vector<float>& out)
    {
        out = in;
        repaired = 0;

        const size_t n = in.size();
        if (n < 512) return;

        // Trzecia roznica zamiast drugiej: mowa jest gladka do drugiej pochodnej,
        // wiec trzecia zostawia z niej jeszcze mniej, a impuls przechodzi caly.
        std::vector<float> err (n, 0.0f);

        for (size_t i = 3; i < n; ++i)
            err[i] = in[i] - 3.0f * in[i - 1] + 3.0f * in[i - 2] - in[i - 3];

        // ---- miara lokalna: MEDIANA, nie srednia -----------------------------
        // Pierwsza wersja porownywala blad z biezaca SREDNIA bledu. Sredniej
        // nie da sie tu uzyc: podnosza ja same trzaski i kazdy atak
        // spolgloski, wiec prog ucieka w gore tam, gdzie ma byc czuly, i
        // spada tam, gdzie ma milczec. Na czystej mowie z espeaka dalo to
        // 3367 "trzaskow" w dziesiec sekund — czyli detektor wykrywal mowe.
        //
        // Mediana nie zauwaza pojedynczych wyskokow: zeby ja ruszyc, polowa
        // probek w oknie musialaby byc trzaskiem.
        const size_t blockLen = (size_t) 4096;
        std::vector<double> scale;

        for (size_t start = 0; start < n; start += blockLen)
        {
            const size_t end = std::min (n, start + blockLen);
            std::vector<float> tmp (err.begin() + (long) start, err.begin() + (long) end);

            for (auto& v : tmp) v = std::fabs (v);
            std::sort (tmp.begin(), tmp.end());

            scale.push_back (tmp.empty() ? 0.0 : (double) tmp[tmp.size() / 2]);
        }

        auto localScale = [&] (size_t i) -> double
        {
            const size_t b = std::min (scale.size() - 1, i / blockLen);
            return std::max (1.0e-9, scale[b]);
        };

        for (size_t i = 3; i + 3 < n; ++i)
        {
            const double thresh = localScale (i) * params.sensitivity;

            if (std::fabs (err[i]) < thresh)
                continue;

            // gdzie sie konczy
            size_t j = i;
            while (j + 1 < n && j - i < (size_t) params.maxLength
                   && std::fabs (err[j]) > thresh * 0.3)
                ++j;

            const size_t width = j - i;

            // ---- czy to na pewno trzask, a nie atak spolgloski ---------------
            // Trzask jest KROTKI i OSAMOTNIONY. Atak "t" albo "k" tez daje duzy
            // blad, ale trwa dluzej i jest poprzedzony narastaniem. Wymagamy
            // wiec, zeby tuz przed zdarzeniem bylo spokojnie.
            if (width < 1 || width >= (size_t) params.maxLength)
                continue;

            //
            // "Spokojnie" mierzymy WZGLEDEM SILY ZDARZENIA, nie wzgledem progu.
            // Pierwsza wersja porownywala z polowa progu — a prog w mowie lezy
            // tysiac razy ponizej prawdziwego trzasku (pomiar: prog 0,003, blad
            // trzasku 2,8). Zwykla mowa przed trzaskiem przekraczala wiec pol
            // progu i polowa trzaskow uchodzila (restore-test: 6 z 12).
            // Atak spolgloski narasta — przed szczytem jest blad tego samego
            // rzedu; trzask spada z nieba — przed nim jest o rzad wielkosci mniej.
            double before = 0.0, peak = 0.0;
            const size_t look = std::min (i, (size_t) 128);

            for (size_t k = i - look; k < i; ++k)
                before = std::max (before, (double) std::fabs (err[k]));
            for (size_t k = i; k <= j; ++k)
                peak = std::max (peak, (double) std::fabs (err[k]));

            if (before > peak * params.isolation)
                continue;

            // ---- to samo, ale na SYGNALE: czy zdarzenie jest w srodku czegos
            // ---- ciaglego, czy na jego poczatku -----------------------------
            //
            // Powyzszy warunek "przed zdarzeniem bylo spokojnie" opisuje trzask
            // rownie dobrze jak POCZATEK glosnego dzwieku. Na prawdziwym
            // nagraniu wyszlo to natychmiast: w spiewanym refrenie detektor
            // zrobil dziewiec "napraw" w ciagu jednej dziesiatej sekundy,
            // zmieniajac probki o 0,37 — czyli o -8,5 dBFS. To nie bylo
            // usuwanie trzasku, tylko wycinanie dziury w ataku nuty.
            //
            // Roznica jest prosta i da sie ja zmierzyc: trzask siedzi WEWNATRZ
            // materialu, wiec poziom przed nim i po nim jest podobny. Atak ma
            // po sobie glosny ciag dalszy, ktorego przed nim nie bylo.
            auto levelAround = [&] (size_t from, size_t to) -> double
            {
                double sum = 0.0;
                size_t n = 0;

                for (size_t k = from; k < to && k < in.size(); ++k) { sum += (double) in[k] * in[k]; ++n; }

                return n ? std::sqrt (sum / n) : 0.0;
            };

            const size_t guard = (size_t) 96;     // 2 ms przy 48 kHz
            const size_t span  = (size_t) 960;    // 20 ms

            const double lvBefore = i > guard + span ? levelAround (i - guard - span, i - guard) : 0.0;
            const double lvAfter  = levelAround (j + guard, j + guard + span);

            if (lvAfter > lvBefore * 4.0)          // +12 dB = to jest atak, nie trzask
                continue;

            if (i < 2 || j + 2 >= n)
                continue;

            interpolate (out, i - 1, j + 1);
            ++repaired;
            i = j + 2;
        }
    }

private:
    /** Szescienna interpolacja Hermite'a miedzy probkami po obu stronach dziury. */
    static void interpolate (std::vector<float>& x, size_t from, size_t to)
    {
        if (from < 1 || to + 1 >= x.size() || to <= from) return;

        const float p0 = x[from - 1], p1 = x[from];
        const float p2 = x[to],       p3 = x[to + 1];
        const float m0 = (p2 - p0) * 0.5f;
        const float m1 = (p3 - p1) * 0.5f;
        const float len = (float) (to - from);

        for (size_t k = from + 1; k < to; ++k)
        {
            const float t = (float) (k - from) / len;
            const float t2 = t * t, t3 = t2 * t;

            x[k] = (2 * t3 - 3 * t2 + 1) * p1
                 + (t3 - 2 * t2 + t)     * m0
                 + (-2 * t3 + 3 * t2)    * p2
                 + (t3 - t2)             * m1;
        }
    }
};

} // namespace mix
