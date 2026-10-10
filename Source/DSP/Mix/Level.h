/*
    Level.h — jazda glosnoscia i duckowanie podkladu.

    Dwie rzeczy, ktorych kompresor nie umie, choc wydaje sie, ze powinien.
*/

#pragma once

#include "Biquad.h"
#include "Dynamics.h"
#include "Loudness.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mix
{

//==============================================================================
/** Jazda glosnoscia — to, co robil czlowiek przy suwaku, zanim wymyslono
    kompresory.

    Roznica wobec kompresji jest zasadnicza i warto ja zapisac, bo latwo uznac
    te dwie rzeczy za wymienne:

      KOMPRESOR dziala w milisekundach i wyrownuje RÓŻNICE WEWNĄTRZ zdania —
      spolgloske wobec samogloski. Zeby wyrownac cale zdania wobec siebie,
      musialby miec zwolnienie liczone w sekundach, a wtedy slychac, jak
      oddycha. I podnosi wszystko, co ciche, wiec razem z koncowka zdania
      podnosi tez szum.

      JAZDA GLOSNOSCIA dziala w sekundach i wyrownuje ZDANIA WOBEC SIEBIE, nie
      ruszajac niczego w srodku. Dynamika mowy zostaje nietknieta, barwa sie nie
      zmienia, a szum nie rosnie, bo w ciszy nie ma czego jechac — i wlasnie
      dlatego suwak trzyma sie tam, gdzie byl, dopoki ktos nie mowi.

    Ograniczenie predkosci jest tu najwazniejszym parametrem. Powyzej mniej
    wiecej 6 dB na sekunde przestaje to byc niesłyszalne i zamienia sie w
    powolny kompresor, czyli w to, czego chcielismy uniknac.
*/
class GainRider
{
public:
    struct Params
    {
        float  targetDb    = -20.0f;  ///< do jakiego poziomu skutecznego jechac
        float  rangeDb     = 12.0f;   ///< maksymalne odchylenie w gore i w dol
        float  maxSlopeDbS = 4.0f;    ///< ile decybelow na sekunde
        float  gateDb      = -45.0f;  ///< ponizej tego nie ruszamy suwaka
        double windowMs    = 400.0;   ///< okno pomiaru poziomu
    };

    Params params;

    void prepare (double sampleRate)
    {
        sr = sampleRate;

        // ---- czym mierzyc poziom ---------------------------------------------
        //
        // Bylo tu pasmo 700 Hz o dobroci 0,5, z uzasadnieniem "mierz tam, gdzie
        // jest mowa". Filtr K z normy BS.1770 odpowiada na to samo pytanie
        // lepiej, bo jest odpowiedzia na pytanie, jak wazyc pasma, zeby liczba
        // zgadzala sie z tym, co slychac — i jest prawie plaski od 100 Hz do
        // 10 kHz, wiec nie rysuje wlasnej krzywej na sygnale.
        //
        // UCZCIWIE: zamiana nie naprawila tego, przez co ja zrobilem. Podejrzewalem
        // detektor o to, ze wprowadza korektor, ktorego nikt nie ustawil (+4,1 dB
        // przy 443 Hz, -3,0 dB przy 704 Hz na wierszu). Po zamianie jest +4,3 i
        // -3,8 — czyli tak samo. Przyczyna lezy gdzie indziej i jest wbudowana w
        // sam pomysl jazdy suwakiem: ciche frazy tego glosu maja INNA BARWE niz
        // glosne (nizsze F1, mniej okolic 700 Hz). Wyrownujac ich poziom,
        // wyrownuje sie tez ich udzial w sredniej barwie — i nagranie robi sie
        // ciemniejsze, bo ciche frazy przestaja byc ciche.
        //
        // Z tego wynika wniosek dla ustawien, nie dla kodu: przy materiale, w
        // ktorym dynamika jest trescia, suwak ma miec maly zakres albo nie
        // pracowac wcale. Filtr K zostaje, bo jest poprawny sam w sobie.
        kShelf.setCoeffs (kWeightShelf (sr));
        kHp.setCoeffs    (kWeightHighpass (sr));

        rmsCoeff = (float) std::exp (-1.0 / (0.001 * params.windowMs * sr));
        stepPerSample = params.maxSlopeDbS / (float) sr;

        reset();
    }

    void reset()
    {
        kShelf.reset(); kHp.reset();
        rms = 0.0f;
        gainDb = 0.0f;
    }

    inline float process (float x) noexcept
    {
        const float b = kHp.process (kShelf.process (x));
        rms = rmsCoeff * rms + (1.0f - rmsCoeff) * b * b;

        const float levelDb = rms > 1.0e-12f ? 10.0f * std::log10 (rms) : -120.0f;

        if (levelDb > params.gateDb)
        {
            const float want = juceClamp (params.targetDb - levelDb,
                                          -params.rangeDb, params.rangeDb);

            // ograniczenie predkosci — sedno calego modulu
            if (want > gainDb)      gainDb = std::min (want, gainDb + stepPerSample);
            else if (want < gainDb) gainDb = std::max (want, gainDb - stepPerSample);
        }

        return x * dbToGain (gainDb);
    }

    float currentGainDb() const noexcept { return gainDb; }

private:
    static float juceClamp (float v, float lo, float hi) noexcept
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    double sr = 48000.0;
    Biquad kShelf, kHp;
    float rms = 0.0f, rmsCoeff = 0.0f, gainDb = 0.0f, stepPerSample = 0.0f;
};

//==============================================================================
/** Duckowanie: podklad schodzi pod glosem.

    Bez tego mowione slowo na muzyce nie dziala — albo muzyka zaglusza, albo
    jest tak cicha, ze nie ma jej po co puszczac.

    Dwie rzeczy, ktore odrozniaja to od zwyklego kompresora ze wpustem:

      PRZYTRZYMANIE  po ostatnim slowie podklad NIE wraca od razu. Miedzy
                     zdaniami muzyka podskakiwalaby przy kazdym oddechu, co
                     jest slyszalne natychmiast i brzmi tanio. Domyslne 400 ms
                     przykrywa normalna pauze oddechowa.
      WYPRZEDZENIE   podklad schodzi ZANIM padnie pierwsze slowo. Bez tego
                     poczatek zdania wchodzi w jeszcze glosna muzyke — czyli
                     dokladnie tam, gdzie zrozumialosc jest najwazniejsza.
*/
class Ducker
{
public:
    struct Params
    {
        float  duckDb      = -12.0f;   ///< o ile schodzi podklad
        float  thresholdDb = -38.0f;   ///< od jakiego poziomu glosu
        // Wyprzedzenie musi byc DLUZSZE niz atak, inaczej nie ma po co go
        // miec: przy ataku 60 ms i wyprzedzeniu 40 ms podklad w chwili
        // pierwszego slowa zszedl zmierzone 0,2 dB, czyli praktycznie nic.
        double attackMs    = 40.0;
        double releaseMs   = 500.0;
        double holdMs      = 400.0;
        double lookaheadMs = 90.0;
    };

    Params params;

    void prepare (double sampleRate)
    {
        sr = sampleRate;

        voiceBand.setCoeffs (design::bandpass (900.0, sr, 0.5));
        env.prepare (sr);
        env.setTimes (params.attackMs, params.releaseMs);

        look = std::max (1, (int) std::llround (sr * params.lookaheadMs * 0.001));
        delay.assign ((size_t) look, 0.0f);
        holdSamples = (int) std::llround (sr * params.holdMs * 0.001);

        fast = (float) std::exp (-1.0 / (0.010 * sr));
        reset();
    }

    void reset()
    {
        voiceBand.reset();
        env.reset();
        std::fill (delay.begin(), delay.end(), 0.0f);
        pos = 0;
        rms = 0.0f;
        holdLeft = 0;
        lastDuckDb = 0.0f;
    }

    /** WAZNE: wolajacy MUSI opoznic tez glos o latencySamples().

        Wyprzedzenie dziala tak, ze podklad idzie przez linie opozniajaca, a
        wzmocnienie liczy sie z glosu NIEOPOZNIONEGO — czyli z przyszlosci
        wzgledem tego, co wlasnie wychodzi. Jesli glos pojdzie do sluchacza bez
        tego samego opoznienia, podklad rozjedzie sie z nim o dziewiecdziesiat
        milisekund i nic nie zostanie wyprzedzone; przy pierwszym pomiarze
        wyszlo z tego 0,2 dB zamiast dziesieciu.

        @param music  probka podkladu
        @param voice  probka glosu z TEJ SAMEJ chwili — wpust, nie opozniany */
    inline float process (float music, float voice) noexcept
    {
        const float b = voiceBand.process (voice);
        rms = fast * rms + (1.0f - fast) * b * b;

        const float db = rms > 1.0e-12f ? 10.0f * std::log10 (rms) : -120.0f;

        if (db > params.thresholdDb) holdLeft = holdSamples;
        else if (holdLeft > 0)       --holdLeft;

        const float want = (db > params.thresholdDb || holdLeft > 0) ? params.duckDb : 0.0f;
        const float smoothedDb = env.process (want);
        lastDuckDb = smoothedDb;

        const float out = delay[(size_t) pos];
        delay[(size_t) pos] = music;
        pos = (pos + 1) % look;

        return out * dbToGain (smoothedDb);
    }

    float duckDb() const noexcept { return lastDuckDb; }
    int   latencySamples() const noexcept { return look; }

private:
    double sr = 48000.0;
    Biquad voiceBand;
    EnvelopeDb env;
    std::vector<float> delay;
    int look = 1, pos = 0, holdSamples = 0, holdLeft = 0;
    float fast = 0.0f, rms = 0.0f, lastDuckDb = 0.0f;
};

} // namespace mix
