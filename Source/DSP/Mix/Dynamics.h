/*
    Dynamics.h — brama, kompresor, de-esser. Trzy rzeczy, ktore robia z surowego
    nagrania mowy material nadajacy sie do sluchania.

    Wszystko liczy sie w dziedzinie decybeli, na REDUKCJI wzmocnienia, nie na
    sygnale. Roznica jest praktyczna: wygladzanie liniowego wzmocnienia daje
    czasy ataku zalezne od poziomu wejscia — ten sam kompresor przy glosnej
    mowie dziala szybciej niz przy cichej i nie da sie tego uczciwie opisac
    liczba w milisekundach.

    Zadny z tych modulow nie ma makijazu domyslnie. Wyrownanie glosnosci robi
    dopiero koniec lancucha, jedna liczba, wobec zmierzonego LUFS — bo tylko
    tam wiadomo, ile naprawde brakuje.
*/

#pragma once

#include "Biquad.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mix
{

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  noexcept { return g > 1.0e-9f ? 20.0f * std::log10 (g) : -180.0f; }

//==============================================================================
/** Detektor obwiedni w decybelach. Osobne stale czasowe w gore i w dol. */
class EnvelopeDb
{
public:
    /** Stan poczatkowy to ZERO decybeli, czyli "nic nie robie".

        Zaczynalem od -180 dB, bo tak wyglada "cisza" dla detektora poziomu.
        To byl blad kategorii: ta obwiednia nie sledzi POZIOMU, tylko REDUKCJE
        wzmocnienia. Przy starcie od -180 pierwsza probka pliku dostawala
        +178 dB wzmocnienia, zanim filtr zdazyl dojsc do zera — w tescie tego
        nie bylo widac, bo testy mierza po ustabilizowaniu, ale pierwszy
        prawdziwy plik puszczony przez lancuch wyszedl z redukcja -178 dB na
        bramie i -127 dB korekty glosnosci. */
    void prepare (double sr) noexcept { sampleRate = sr; y = 0.0f; }
    void reset() noexcept             { y = 0.0f; }

    void setTimes (double attackMs, double releaseMs) noexcept
    {
        aUp   = coeff (attackMs);
        aDown = coeff (releaseMs);
    }

    inline float process (float db) noexcept
    {
        const float a = db > y ? aUp : aDown;
        y = a * y + (1.0f - a) * db;
        return y;
    }

    float value() const noexcept { return y; }

private:
    float coeff (double ms) const noexcept
    {
        if (ms <= 0.0) return 0.0f;
        return (float) std::exp (-1.0 / (0.001 * ms * sampleRate));
    }

    double sampleRate = 48000.0;
    float y = -180.0f, aUp = 0.0f, aDown = 0.0f;
};

//==============================================================================
/** Kompresor w przod, z miekkim kolanem.

    Do mowy uzywamy go dwa razy i w zupelnie innych rolach:

      wyrownywacz  prog nisko, stopien 2:1, atak 30 ms, zwolnienie 300 ms —
                   pilnuje, zeby zdanie mialo taka sama glosnosc jak poprzednie
      dociskacz    prog wysoko, stopien 4:1, atak 5 ms, zwolnienie 80 ms —
                   lapie pojedyncze szczyty, ktorych wyrownywacz nie widzi

    Jeden kompresor probujacy robic obie rzeczy naraz albo oddycha slyszalnie,
    albo przepuszcza szczyty. Dwa lagodne w szeregu brzmia lepiej niz jeden
    ostry — to jest cala tajemnica lancuchow spikerskich.
*/
class Compressor
{
public:
    struct Params
    {
        float thresholdDb = -20.0f;
        float ratio       = 3.0f;
        float kneeDb      = 6.0f;
        double attackMs   = 10.0;
        double releaseMs  = 150.0;
        float makeupDb    = 0.0f;
        bool  rmsDetector = false;   ///< true = usrednianie 10 ms zamiast szczytu
    };

    Params params;

    void prepare (double sr)
    {
        sampleRate = sr;
        env.prepare (sr);
        env.setTimes (params.attackMs, params.releaseMs);
        rmsCoeff = (float) std::exp (-1.0 / (0.010 * sr));
        reset();
    }

    void updateTimes() { env.setTimes (params.attackMs, params.releaseMs); }

    void reset()
    {
        env.reset();
        rms = 0.0f;
        lastGr = 0.0f;
    }

    inline float process (float x) noexcept
    {
        float detect;

        if (params.rmsDetector)
        {
            rms = rmsCoeff * rms + (1.0f - rmsCoeff) * x * x;
            detect = std::sqrt (rms);
        }
        else
        {
            detect = std::fabs (x);
        }

        const float inDb = gainToDb (detect);

        // ---- statyczna krzywa z miekkim kolanem -----------------------------
        const float over = inDb - params.thresholdDb;
        const float half = params.kneeDb * 0.5f;
        float outOver;

        if (params.kneeDb > 0.0f && over > -half && over < half)
        {
            // kwadratowa interpolacja w obrebie kolana
            const float t = over + half;
            outOver = over + (1.0f / params.ratio - 1.0f) * t * t / (2.0f * params.kneeDb);
        }
        else if (over <= -half)
        {
            outOver = over;
        }
        else
        {
            outOver = over / params.ratio;
        }

        const float wantGrDb = std::min (0.0f, outOver - over);

        // wygladzanie na REDUKCJI: atak, gdy redukcja rosnie
        const float smoothed = -env.process (-wantGrDb);
        lastGr = smoothed;

        return x * dbToGain (smoothed + params.makeupDb);
    }

    /** Biezaca redukcja wzmocnienia w dB (liczba ujemna). Do wskaznika. */
    float gainReductionDb() const noexcept { return lastGr; }

private:
    double sampleRate = 48000.0;
    EnvelopeDb env;
    float rms = 0.0f, rmsCoeff = 0.0f, lastGr = 0.0f;
};

//==============================================================================
/** Brama z ekspansja w dol, wyprzedzeniem i histereza.

    Trzy rzeczy, ktorych brakuje bramom "z podrecznika", a bez ktorych mowa
    brzmi jak wycinana nozyczkami:

      wyprzedzenie   brama otwiera sie ZANIM przyjdzie glosna probka. Bez tego
                     kazde "p" i "t" traci pierwsze milisekundy — a to wlasnie
                     one niosa zrozumialosc spolglosek.
      przytrzymanie  po ostatniej glosnej probce brama trzyma sie otwarta jeszcze
                     chwile, wiec ogon slowa i oddech miedzy wyrazami nie
                     zostaja uciete w polowie.
      histereza      prog zamkniecia lezy nizej niz otwarcia, wiec brama nie
                     mruga przy poziomie krecacym sie wokol progu.

    Ekspansja, nie ciecie: ponizej progu tlumimy STOPNIOWO, do zadanego dna.
    Cisza absolutna miedzy zdaniami brzmi nienaturalnie — lepiej zostawic tlo
    pokoju o 12–18 dB ciszej niz je wyciac.
*/
class Gate
{
public:
    struct Params
    {
        float thresholdDb   = -45.0f;
        float hysteresisDb  = 4.0f;
        float ratio         = 3.0f;    ///< stopien ekspansji w dol
        float floorDb       = -18.0f;  ///< maksymalne stlumienie
        double attackMs     = 2.0;
        double holdMs       = 180.0;
        double releaseMs    = 250.0;
        double lookaheadMs  = 5.0;
    };

    Params params;

    void prepare (double sr)
    {
        sampleRate = sr;
        env.prepare (sr);
        env.setTimes (params.attackMs, params.releaseMs);

        delayLen = std::max (1, (int) std::llround (sr * params.lookaheadMs * 0.001));
        delay.assign ((size_t) delayLen, 0.0f);

        detectFast = (float) std::exp (-1.0 / (0.001 * sr));   // 1 ms
        holdSamples = (int) std::llround (sr * params.holdMs * 0.001);

        reset();
    }

    void reset()
    {
        std::fill (delay.begin(), delay.end(), 0.0f);
        pos = 0;
        env.reset();
        peakHold = 0.0f;
        openFor = 0;
        isOpen = false;
        lastGr = 0.0f;
    }

    inline float process (float x) noexcept
    {
        // ---- detekcja na sygnale NIEOPOZNIONYM ------------------------------
        const float a = std::fabs (x);
        peakHold = a > peakHold ? a : detectFast * peakHold;

        const float db = gainToDb (peakHold);
        const float openAt  = params.thresholdDb;
        const float closeAt = params.thresholdDb - params.hysteresisDb;

        if (db > openAt)        { isOpen = true;  openFor = holdSamples; }
        else if (db < closeAt)  { if (openFor > 0) --openFor; else isOpen = false; }
        else if (openFor > 0)   { --openFor; }

        // ---- ile tlumimy -----------------------------------------------------
        float wantDb = 0.0f;

        if (! isOpen)
        {
            // Zaciecie do zera jest tu konieczne, nie kosmetyczne. Gdy sygnal
            // siedzi MIEDZY progiem zamkniecia a otwarcia (histereza), roznica
            // "closeAt - db" wychodzi UJEMNA, a wzor bez zaciecia zamienial ja
            // wtedy na dodatnie wzmocnienie: brama podbijala cisze zamiast ja
            // tlumic. Na prawdziwym pliku objawilo sie to tak, ze brama
            // meldowala redukcje -0,0 dB i nie robila nic.
            const float under = std::max (0.0f, closeAt - db);
            wantDb = std::max (params.floorDb, -under * (params.ratio - 1.0f));
        }

        // Wygladzanie idzie na wartosci TLUMIENIA (liczba ujemna), nie na jej
        // odwrotnosci jak w kompresorze. Roznica jest zasadnicza: otwarcie
        // bramy to wzrost tej liczby ku zeru, wiec lapie sie na ATAK — 2 ms.
        // Przy odwrotnym znaku otwarcie szlo po czasie zwolnienia i brama
        // otwierala sie przez cwierc sekundy. W tescie widac to bylo jako
        // 17 dB urwane z poczatku frazy: pierwsze slowo po ciszy wchodzilo
        // przez zamknieta jeszcze brame.
        const float smoothed = env.process (wantDb);
        lastGr = smoothed;

        // ---- sygnal opozniony o wyprzedzenie ---------------------------------
        const float out = delay[(size_t) pos];
        delay[(size_t) pos] = x;
        pos = (pos + 1) % delayLen;

        return out * dbToGain (smoothed);
    }

    float gainReductionDb() const noexcept { return lastGr; }
    int   latencySamples()  const noexcept { return delayLen; }

private:
    double sampleRate = 48000.0;
    EnvelopeDb env;
    std::vector<float> delay;
    int delayLen = 1, pos = 0, holdSamples = 0, openFor = 0;
    float detectFast = 0.0f, peakHold = 0.0f, lastGr = 0.0f;
    bool isOpen = false;
};

//==============================================================================
/** De-esser pasmowy.

    Dziala na roznicy: pasmo gorne to x minus dolnoprzepustowe x, wiec suma
    wraca dokladnie do oryginalu, gdy nie tlumimy. To wazniejsze, niz wyglada —
    de-essery zbudowane na dwoch osobnych filtrach zmieniaja barwe glosu nawet
    wtedy, gdy nic nie robia.

    Detekcja porownuje energie sybilantow z energia calego sygnalu. Sam poziom
    w pasmie 5–9 kHz nie wystarcza: glosne "a" tez ma tam energie. Dopiero
    STOSUNEK mowi, ze to jest "s", a nie samogloska — i dzieki temu de-esser nie
    tlumi calej gory przy glosnym mowieniu.
*/
class DeEsser
{
public:
    /*
        Dlaczego polka, a nie podzial na dwa pasma.

        Pierwsza wersja dzielila sygnal tak: dol = filtr dolnoprzepustowy
        czwartego rzedu przy 5 kHz, gora = sygnal MINUS dol, i tlumila sama
        gore. Wyglada niewinnie. Nie jest.

        Odejmowanie nie jest odwrotnoscia filtrowania, bo filtr przesuwa faze.
        Przy czestotliwosci podzialu filtr czwartego rzedu obraca faze o 180
        stopni, wiec "x minus dol" to w praktyce "x PLUS dol" — a po scisniesciu
        gory i dodaniu jej z powrotem do dolu obie czesci sie czesciowo kasuja.
        Zmierzone na szumie, przy zadanym tlumieniu 6 dB:

            2,0 kHz   -1,7 dB
            2,5 kHz   -3,0 dB
            3,2 kHz   -5,4 dB
            4,0 kHz  -10,0 dB      <- zamiast zera
            5,0 kHz  -11,4 dB      <- zamiast szesciu
            8,0 kHz   -6,2 dB

        Czyli ustawienie "tlumij najwyzej 6 dB" robilo dziure 11 dB w okolicy
        piatego kiloherca i zabieralo trzy decybele obecnosci przy 2,5 kHz.
        Czlowiek slyszal to za kazdym razem i za kazdym razem mowil to samo:
        "przytlumione", "zamulone", "gorsze". Ja trzy razy zmienialem liczbe
        zamiast sprawdzic, co ta liczba robi.

        Polka nie ma podzialu, wiec nie ma czego kasowac. Jeden filtr, faza
        minimalna, wspolczynniki przeliczane co szesnascie probek — a ze
        wzmocnienie i tak jest wygladzone obwiednia, miedzy przeliczeniami
        zmienia sie o ulamek decybela.
    */
    struct Params
    {
        double shelfHz     = 6000.0;  ///< od ktorej czestotliwosci polka opada
        double shelfSlope  = 0.7;     ///< lagodne zbocze; 1.0 byloby strome
        float  thresholdDb = -12.0f;  ///< prog na STOSUNKU sybilanty/calosc
        float  ratio       = 4.0f;
        float  maxCutDb    = -9.0f;
        double attackMs    = 1.0;
        double releaseMs   = 60.0;
    };

    Params params;

    void prepare (double sr)
    {
        sampleRate = sr;

        sib.setCoeffs  (design::bandpass (7000.0, sr, 1.2));
        body.setCoeffs (design::bandpass (1000.0, sr, 0.8));

        env.prepare (sr);
        env.setTimes (params.attackMs, params.releaseMs);

        fast = (float) std::exp (-1.0 / (0.003 * sr));
        reset();
    }

    void reset()
    {
        sib.reset(); body.reset(); shelf.reset();
        env.reset();
        sibRms = bodyRms = 0.0f;
        lastGr = 0.0f;
        appliedDb = 1.0f;            // wymusza przeliczenie przy pierwszej probce
        counter = 0;
        current = target = design::highShelf (params.shelfHz, sampleRate, 0.0, params.shelfSlope);
        step = { 0, 0, 0, 0, 0 };
        rampLeft = 0;
        shelf.setCoeffs (current);
    }

    inline float process (float x) noexcept
    {
        const float s = sib.process (x);
        const float b = body.process (x);

        sibRms  = fast * sibRms  + (1.0f - fast) * s * s;
        bodyRms = fast * bodyRms + (1.0f - fast) * b * b;

        // stosunek sybilantow do korpusu, w dB
        const float ratioDb = 10.0f * std::log10 ((sibRms + 1.0e-12f) / (bodyRms + 1.0e-12f));

        const float over = ratioDb - params.thresholdDb;
        float wantDb = 0.0f;

        if (sibRms + bodyRms > 1.0e-10f && over > 0.0f)
            wantDb = std::max (params.maxCutDb, -over * (1.0f - 1.0f / params.ratio));

        const float smoothed = -env.process (-wantDb);
        lastGr = smoothed;

        // Projekt filtra co 16 probek, ale wspolczynniki zmieniaja sie plynnie.
        // Przy 48 kHz jest to 1/3 ms; obwiednia sama nie usuwa schodkow.
        if (--counter <= 0)
        {
            counter = 16;

            if (std::fabs (smoothed - appliedDb) > 0.01f)
                setShelf (smoothed);
        }

        if (rampLeft > 0)
        {
            current.b0 += step.b0; current.b1 += step.b1; current.b2 += step.b2;
            current.a1 += step.a1; current.a2 += step.a2;
            if (--rampLeft == 0) current = target;
            shelf.setCoeffs (current);
        }
        return shelf.process (x);
    }

    float gainReductionDb() const noexcept { return lastGr; }

private:
    void setShelf (float db) noexcept
    {
        appliedDb = db;
        target = design::highShelf (params.shelfHz, sampleRate, db, params.shelfSlope);
        step = { (target.b0-current.b0)/16, (target.b1-current.b1)/16,
                 (target.b2-current.b2)/16, (target.a1-current.a1)/16, (target.a2-current.a2)/16 };
        rampLeft = 16;
    }

    double sampleRate = 48000.0;
    Biquad sib, body, shelf;
    EnvelopeDb env;
    float fast = 0.0f, sibRms = 0.0f, bodyRms = 0.0f, lastGr = 0.0f, appliedDb = 0.0f;
    int   counter = 0, rampLeft = 0;
    Coeffs current, target, step;
};

} // namespace mix
