/*
    SyllableClock.h — liczenie sylab na zywo, w trakcie mowienia.

    ---------------------------------------------------------------------------
    CZYM TO SIE ROZNI OD TEGO, CO JUZ BYLO

    W Prosody.h jest liczenie sylab, sprawdzone na syntezatorze przy trzech
    znanych predkosciach (107 prawdziwych sylab -> 105, 112, 123 wykrytych).
    Ale tamto dziala PO nagraniu: prog bierze z percentyli calego pliku, a
    szczyt rozpoznaje po tym, co bylo przed i po.

    Prompter nie moze czekac do konca. Potrzebuje liczby TERAZ, z opoznieniem
    jednej ramki, i progu, ktory sam sie dostraja do tego, jak glosno ktos
    mowi w tej chwili. Metoda jest ta sama — szczyty energii w pasmie
    samoglosek — ale wszystko, co wymagalo znajomosci przyszlosci, trzeba bylo
    zastapic pamiecia przeszlosci.

    ---------------------------------------------------------------------------
    TRZY RZECZY, KTORE MUSZA BYC SPELNIONE NARAZ

    Zeby uznac szczyt za sylabe:

        1. energia wyraznie ponad tlem          - inaczej liczy sie szum
        2. dolek przed nim glebszy niz 4 dB     - inaczej jedna samogloska
                                                  z falowaniem liczy sie dwa razy
        3. co najmniej 120 ms od poprzedniej    - szybciej nikt nie mowi

    Prog "ponad tlem" jest ruchomy: tlo to wolno opadajace minimum, glos to
    wolno opadajace maksimum. Kiedy ktos zaczyna mowic ciszej, prog schodzi za
    nim w ciagu kilku sekund. Kiedy milknie — prog zostaje tam, gdzie byl, zeby
    cisza nie podniosla czulosci do poziomu, przy ktorym szum staje sie mowa.

    ---------------------------------------------------------------------------
    PAUZA

    Cisza dluzsza niz 350 ms to koniec frazy. Prompter uzywa tego do kasowania
    nazbieranego bledu, wiec pauza musi byc zglaszana osobno i pewnie — lepiej
    przegapic pauze niz zglosic taka, ktorej nie bylo.
*/

#pragma once

#include "Mix/Biquad.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace tw
{

//==============================================================================
class SyllableClock
{
public:
    struct Params
    {
        double bandHz      = 650.0;  ///< pasmo samoglosek
        double bandQ       = 0.55;
        float  aboveFloorDb = 9.0f;  ///< o tyle ponad tlem, zeby to byla mowa
        float  dipDb        = 4.0f;  ///< o tyle musi opasc miedzy sylabami
        double minSyllableSec = 0.12;
        double pauseSec       = 0.35;
    };

    Params params;

    //==========================================================================
    void prepare (double sampleRate)
    {
        sr   = sampleRate;
        hop  = std::max (1, (int) std::llround (sr * 0.010));   // 10 ms
        len  = std::max (hop, (int) std::llround (sr * 0.040)); // 40 ms

        band.setCoeffs (mix::design::bandpass (params.bandHz, sr, params.bandQ));

        ring.assign ((size_t) len, 0.0f);
        reset();
    }

    void reset()
    {
        band.reset();
        std::fill (ring.begin(), ring.end(), 0.0f);

        ringPos = 0;
        sinceHop = 0;

        floorDb = -70.0f;
        loudDb  = -70.0f;

        prevDb = twoAgoDb = -140.0f;
        dipSincePeak = 140.0f;

        framesSincePeak = 1000;
        framesSinceVoice.store (1000);

        syllables = 0;
        pending = 0;
        pauses = 0;
        inPause.store (true);
        lastWasPeak = false;
    }

    //==========================================================================
    /** Dokladaj probki. Zwraca, ile sylab przybylo w tym bloku. */
    int process (const float* in, int numSamples)
    {
        int found = 0;

        for (int n = 0; n < numSamples; ++n)
        {
            const float v = band.process (in[n]);

            ring[(size_t) ringPos] = v;
            ringPos = (ringPos + 1) % len;

            if (++sinceHop < hop) continue;
            sinceHop = 0;

            double sum = 0.0;
            for (int i = 0; i < len; ++i) sum += (double) ring[(size_t) i] * ring[(size_t) i];

            const float db = sum > 1.0e-20 ? (float) (10.0 * std::log10 (sum / len)) : -140.0f;

            if (stepFrame (db)) { ++found; ++syllables; ++pending; }
        }

        return found;
    }

    //==========================================================================
    int  count()        const noexcept { return syllables; }

    /** Ile sylab przybylo od ostatniego pytania.

        Wolane z watku interfejsu, podczas gdy process() chodzi w watku dzwieku.
        Dlatego licznik jest atomowy i odbiera sie go przez wymiane, a nie przez
        odczyt i zerowanie — te dwie operacje osobno gubilyby sylabe za kazdym
        razem, gdy trafi miedzy nie. */
    int takeNewSyllables() noexcept { return pending.exchange (0); }
    int  pauseCount()   const noexcept { return pauses; }
    bool speaking()     const noexcept { return ! inPause.load(); }
    float floorLevelDb() const noexcept { return floorDb; }
    float speechLevelDb() const noexcept { return loudDb; }

    /** Ile sekund trwa biezaca cisza. 0, gdy ktos mowi. */
    double pauseLength() const noexcept
    {
        return inPause.load() ? framesSinceVoice.load() * (hop / sr) : 0.0;
    }

private:
    //==========================================================================
    /** Jedna ramka. @return true, gdy wlasnie zamknela sie sylaba. */
    bool stepFrame (float db)
    {
        // ---- tlo i glos: dwie wolne obwiednie --------------------------------
        //
        // Tlo idzie w dol szybko (zeby zlapac cicha chwile) i w gore bardzo
        // wolno (zeby mowa go nie podniosla). Glos odwrotnie.
        floorDb = db < floorDb ? db : floorDb + 0.004f;
        loudDb  = db > loudDb  ? db : loudDb  - 0.010f;

        floorDb = std::max (floorDb, -120.0f);

        const float thresh = std::max (floorDb + params.aboveFloorDb, loudDb - 25.0f);
        const bool  voiced = db > thresh;

        // ---- pauza ------------------------------------------------------------
        if (voiced)
        {
            framesSinceVoice.store (0);

            if (inPause.load()) inPause.store (false);
        }
        else
        {
            framesSinceVoice.store (framesSinceVoice.load() + 1);

            if (! inPause.load() && framesSinceVoice.load() * (hop / sr) > params.pauseSec)
            {
                inPause.store (true);
                ++pauses;
                dipSincePeak = 140.0f;      // fraza sie skonczyla, dolek od nowa
            }
        }

        ++framesSincePeak;

        // ---- szczyt: poprzednia ramka wyzsza od obu sasiadek ------------------
        //
        // Dlatego decyzja zapada o JEDNA ramke pozniej, czyli 10 ms — tyle
        // wynosi cale opoznienie tego licznika.
        const bool isPeak = prevDb > twoAgoDb && prevDb >= db;

        bool counted = false;

        if (isPeak && prevDb > thresh)
        {
            const bool farEnough = framesSincePeak * (hop / sr) >= params.minSyllableSec;
            const bool deepEnough = syllables == 0 || inPauseJustEnded
                                  || (prevDb - dipSincePeak) >= params.dipDb;

            if (farEnough && deepEnough)
            {
                counted = true;
                framesSincePeak = 0;
                dipSincePeak = prevDb;
            }
        }

        inPauseJustEnded = false;

        // dolek liczymy od ostatniego szczytu
        dipSincePeak = std::min (dipSincePeak, db);

        twoAgoDb = prevDb;
        prevDb = db;

        return counted;
    }

    double sr = 48000.0;
    int    hop = 480, len = 1920;

    mix::Biquad band;
    std::vector<float> ring;
    int ringPos = 0, sinceHop = 0;

    float floorDb = -70.0f, loudDb = -70.0f;
    float prevDb = -140.0f, twoAgoDb = -140.0f, dipSincePeak = 140.0f;

    int  framesSincePeak = 1000;
    int  syllables = 0, pauses = 0;
    std::atomic<int> pending { 0 };
    std::atomic<bool> inPause { true };
    std::atomic<int>  framesSinceVoice { 1000 };
    bool lastWasPeak = false, inPauseJustEnded = false;
};

} // namespace tw
