/*
    SpeechChain.h — kolejnosc, w jakiej to wszystko idzie, i dlaczego wlasnie taka.

      poza strumieniem, na calym pliku:
        1. usuwanie mlaskow          pojedyncze impulsy, naprawiane interpolacja
        2. odszumianie widmowe       szum POD mowa, ktorego brama nie ruszy

      probka po probce:
        3. gornoprzepustowy 80 Hz    stukanie w biurko, kroki, wiatr z ust
        4. wyciecie przydzwieku      50 Hz i harmoniczne, tylko gdy wykryty
        5. tlumik plozyw             grzmot z "p" i "b"
        6. brama                     tlo pokoju miedzy zdaniami
        7. jazda glosnoscia          zdanie wobec zdania, w sekundach
        8. wyrownywacz 2:1           sylaba wobec sylaby, w milisekundach
        9. de-esser                  sybilanty
       10. barwa                     dol, bloto, obecnosc, powietrze — z pomiaru glosu
       11. nasycenie                 cieplo, z nadprobkowaniem
       12. dociskacz 4:1             pojedyncze szczyty
       13. wyrownanie do LUFS        wobec ZMIERZONEJ glosnosci calosci
       14. limiter -1 dBTP           zeby nic nie wyszlo ponad sufit

    Decyzje w tej kolejnosci, ktore nie sa oczywiste:

    ODSZUMIANIE NA POCZATKU, PRZED WSZYSTKIM. Kazdy stopien dynamiki podnosi to,
    co ciche. Szum przepuszczony przez kompresor przestaje byc stacjonarny —
    zaczyna oddychac razem z mowa — a wtedy odszumianie, ktore opiera sie na
    tym, ze szum jest staly, juz go nie znajdzie.

    BRAMA PRZED KOMPRESJA, z tego samego powodu w druga strone: kompresor
    puszczony pierwszy podnioslby tlo pokoju do poziomu, na ktorym brama nie
    odrozni go od mowy.

    JAZDA GLOSNOSCIA PRZED KOMPRESJA, nie zamiast niej. Najpierw wyrownaj
    zdania wobec siebie powolnym suwakiem, potem dopiero domknij dynamike
    wewnatrz zdania. Odwrotnie kompresor musialby nadrabiac cale roznice miedzy
    zdaniami i bylo by go slychac.

    DE-ESSER PO WYROWNYWACZU. Kompresja zmienia proporcje miedzy sybilantem a
    reszta glosu. De-esser przed nia tlumi to, co za chwile i tak zostanie
    sciszone, a przepuszcza to, co zostanie podniesione.

    WYROWNANIE GLOSNOSCI NA KONCU I ITERACYJNIE. Nie da sie zgadnac docelowego
    wzmocnienia z gory: zalezy od tego, co zrobily wszystkie stopnie wyzej i ile
    zabierze limiter. Wiec: policz, zmierz wynik, dołóż roznice, powtorz.
*/

#pragma once

#include "Biquad.h"
#include "Breath.h"
#include "Colour.h"
#include "Dynamics.h"
#include "Level.h"
#include "Limiter.h"
#include "TiltMatch.h"
#include "VoiceProfile.h"
#include "Tame.h"
#include "Loudness.h"
#include "Restore.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mix
{

struct ChainReport
{
    double lufsBefore = 0.0;
    double lufsAfter  = 0.0;
    double peakBefore = 0.0;   ///< dBTP
    double peakAfter  = 0.0;
    double gainAppliedDb = 0.0;

    double noiseFloorDb  = 0.0;
    double speechDb      = 0.0;
    double gateThreshDb  = 0.0;
    double humHz         = 0.0;

    int    clicksRepaired = 0;
    double denoiseDb      = 0.0;   ///< 0 = odszumianie nie bylo potrzebne
    int    breaths        = 0;     ///< ile oddechow sciszono (Breath.h)

    float  gateMaxGrDb    = 0.0f;
    float  riderMaxDb     = 0.0f;
    float  levelMaxGrDb   = 0.0f;
    float  essMaxGrDb     = 0.0f;
    float  tameMaxCutDb   = 0.0f;
    double tameActive     = 0.0;   ///< w jakiej czesci ramek cokolwiek scisnieto
    float  plosiveMaxGrDb = 0.0f;
    float  peakMaxGrDb    = 0.0f;
    float  limitMaxGrDb   = 0.0f;

    /** Ile barwy trzeba bylo cofnac: polka 300, dzwony 500 i 1400, polka 5 kHz. */
    float  tiltDb[4]      = { 0, 0, 0, 0 };
    bool   tiltUsed       = false;

    /** Ile barwy dociagnieto do wzorca brzmienia (te same cztery pasma). */
    float  profileDb[4]   = { 0, 0, 0, 0 };
    bool   profileUsed    = false;
};

//==============================================================================
class SpeechChain
{
public:
    struct Params
    {
        // ---- naprawa
        bool   useDeClick   = true;
        bool   useDenoise   = true;
        float  denoiseDb    = -14.0f;
        bool   usePlosive   = true;
        bool   useBreath    = false;   ///< sciszanie oddechow (Breath.h)
        float  breathDb     = -12.0f;

        // ---- filtry
        double highpassHz   = 80.0;
        bool   steepHighpass = false;  ///< 24 dB/okt Butterwortha zamiast dwoch Q 0,71

        /** Wolne pasma korektora. pre: przed dynamika (odejmowanie, np. bloto);
            notch: po wyrownywaczu, waskie wyciecia rezonansow (zamiast soothe). */
        struct Bell { double hz = 1000.0, q = 1.0; float db = 0.0f; };
        std::vector<Bell> preEq, notchEq;
        bool   deHum        = true;
        double humHz        = 50.0;

        // ---- dynamika
        bool   useGate      = true;
        bool   autoGate     = true;
        float  gateThreshDb = -45.0f;
        float  gateFloorDb  = -14.0f;
        float  gateRatio    = 3.0f;
        double gateAttackMs = 2.0, gateHoldMs = 180.0, gateReleaseMs = 250.0, gateLookaheadMs = 5.0;

        bool   useRider     = true;
        float  riderRangeDb = 9.0f;

        /** Okno i szybkosc suwaka. Domyslne 400 ms i 4 dB/s pozwalaly mu isc
            za fraza — czyli poprawiac wykonanie zamiast wedrowki. Do prostowania
            powolnego odjezdzania potrzeba okna liczonego w sekundach i takiej
            szybkosci, przy ktorej przejscie calego zakresu trwa kilkanascie
            sekund. Wtedy suwak fizycznie nie ma jak zareagowac na zdanie. */
        double riderWindowMs = 400.0;
        float  riderSlopeDbS = 4.0f;

        float  levelerThreshDb = -26.0f;
        float  levelerRatio    = 2.0f;
        double levelerAttackMs = 30.0, levelerReleaseMs = 300.0;

        /** Tlumienie tego, co wystaje ponad wlasna srednia — po kompresji,
            bo to ona podnosi sybilanty. Prog jest liczony z nagrania, wiec
            stala barwa glosu nie jest tlumiona, choćby byla jasna. */
        bool   useTame      = true;
        float  tameThreshDb = 6.0f;
        float  tameMaxCutDb = -10.0f;
        double tameLowHz    = 1200.0;

        bool   useDeEsser   = true;
        float  essThreshDb  = -12.0f;
        float  essMaxCutDb  = -12.0f;
        double essShelfHz   = 6000.0;

        // ---- barwa (Decide ustawia je z pomiaru glosu, VoiceTone.h)
        float  lowDb        = 0.0f;    // polka 150 Hz: dudnienie, efekt zblizeniowy
        float  mudDb        = -2.0f;   // 250 Hz
        float  presenceDb   = 1.5f;    // 3 kHz
        float  airDb        = 1.0f;    // 10 kHz
        double presenceHz   = 3000.0, presenceQ = 0.9;
        double airHz        = 10000.0;

        /** Cofanie barwy, ktora dolozyla kompresja. Domyslnie wlaczone —
            to nie jest efekt, tylko naprawa skutku ubocznego. */
        bool   useTiltMatch  = true;
        float  tiltMaxDb     = 6.0f;

        /** Wzorzec brzmienia (VoiceProfile.h): barwa dociagana do miksu, ktory
            czlowiek zatwierdzil. Pusty = bez wzorca. */
        VoiceProfile profile;
        float  profileMaxDb  = 3.0f;

        bool   useSaturation = true;
        float  saturation    = 0.35f;

        float  peakThreshDb = -12.0f;
        float  peakRatio    = 4.0f;
        double peakAttackMs = 5.0, peakReleaseMs = 80.0;

        // ---- wyjscie
        double targetLufs   = -16.0;
        float  ceilingDbTp  = -1.0f;
        int    outputBits   = 24;      ///< 16 wlacza dither
    };

    Params params;

    void prepare (double sampleRate)
    {
        sr = sampleRate;

        hp1.setCoeffs (design::highpass (params.highpassHz, sr, params.steepHighpass ? 0.5412 : 0.7071));
        hp2.setCoeffs (design::highpass (params.highpassHz, sr, params.steepHighpass ? 1.3066 : 0.7071));

        pre.assign (params.preEq.size(), {});
        for (size_t i = 0; i < pre.size(); ++i)
            pre[i].setCoeffs (design::peaking (params.preEq[i].hz, sr, params.preEq[i].q, params.preEq[i].db));
        notch.assign (params.notchEq.size(), {});
        for (size_t i = 0; i < notch.size(); ++i)
            notch[i].setCoeffs (design::peaking (params.notchEq[i].hz, sr, params.notchEq[i].q, params.notchEq[i].db));

        for (int i = 0; i < 3; ++i)
            hum[i].setCoeffs (design::notch (params.humHz * (i + 1), sr, 25.0));

        low.setCoeffs      (design::lowShelf (150.0, sr, params.lowDb));
        mud.setCoeffs      (design::peaking (250.0, sr, 1.0, params.mudDb));
        presence.setCoeffs (design::peaking (params.presenceHz, sr, params.presenceQ, params.presenceDb));
        air.setCoeffs      (design::highShelf (params.airHz, sr, params.airDb));

        plosive.prepare (sr);

        gate.params.thresholdDb = params.gateThreshDb;
        gate.params.floorDb     = params.gateFloorDb;
        gate.params.ratio       = params.gateRatio;
        gate.params.attackMs    = params.gateAttackMs;
        gate.params.holdMs      = params.gateHoldMs;
        gate.params.releaseMs   = params.gateReleaseMs;
        gate.params.lookaheadMs = params.gateLookaheadMs;
        gate.prepare (sr);

        rider.params.rangeDb     = params.riderRangeDb;
        rider.params.gateDb      = params.gateThreshDb;
        rider.params.windowMs    = params.riderWindowMs;
        rider.params.maxSlopeDbS = params.riderSlopeDbS;
        rider.prepare (sr);

        leveler.params.thresholdDb = params.levelerThreshDb;
        leveler.params.ratio       = params.levelerRatio;
        leveler.params.kneeDb      = 8.0f;
        leveler.params.attackMs    = params.levelerAttackMs;
        leveler.params.releaseMs   = params.levelerReleaseMs;
        leveler.params.rmsDetector = true;
        leveler.prepare (sr);

        ess.params.thresholdDb = params.essThreshDb;
        ess.params.maxCutDb    = params.essMaxCutDb;
        ess.params.shelfHz     = params.essShelfHz;
        ess.prepare (sr);

        sat.params.amount  = params.saturation;
        sat.params.driveDb = 4.0f;
        sat.prepare (sr);

        peaker.params.thresholdDb = params.peakThreshDb;
        peaker.params.ratio       = params.peakRatio;
        peaker.params.kneeDb      = 4.0f;
        peaker.params.attackMs    = params.peakAttackMs;
        peaker.params.releaseMs   = params.peakReleaseMs;
        peaker.prepare (sr);

        limiter.params.ceilingDb = params.ceilingDbTp;
        limiter.prepare (sr, 1);

        dither.reset();
        reset();
    }

    void reset()
    {
        hp1.reset(); hp2.reset();
        for (auto& h : hum) h.reset();
        low.reset(); mud.reset(); presence.reset(); air.reset();
        for (auto& b : pre) b.reset();
        for (auto& b : notch) b.reset();
        plosive.reset(); gate.reset(); rider.reset();
        leveler.reset(); ess.reset(); sat.reset(); peaker.reset();
        tilt.reset();
        limiter.reset();
        rep = {};
    }

    /** Czesc idaca probka po probce. Wszystko poza wyrownaniem glosnosci i
        limiterem — czyli to, co trzeba przepuscic dwa razy. */
    inline float processStage (float x) noexcept
    {
        float y = hp2.process (hp1.process (x));

        if (params.deHum)
            for (auto& h : hum)
                y = h.process (y);

        for (auto& b : pre) y = b.process (y);

        if (params.usePlosive)
        {
            y = plosive.process (y);
            rep.plosiveMaxGrDb = std::min (rep.plosiveMaxGrDb, plosive.gainReductionDb());
        }

        if (params.useGate)
        {
            y = gate.process (y);
            rep.gateMaxGrDb = std::min (rep.gateMaxGrDb, gate.gainReductionDb());
        }

        if (params.useRider)
        {
            y = rider.process (y);
            rep.riderMaxDb = std::max (rep.riderMaxDb, std::fabs (rider.currentGainDb()));
        }

        y = leveler.process (y);
        rep.levelMaxGrDb = std::min (rep.levelMaxGrDb, leveler.gainReductionDb());

        for (auto& b : notch) y = b.process (y);

        if (params.useDeEsser)
        {
            y = ess.process (y);
            rep.essMaxGrDb = std::min (rep.essMaxGrDb, ess.gainReductionDb());
        }

        y = air.process (presence.process (mud.process (low.process (y))));

        if (params.useSaturation)
            y = sat.process (y);

        y = peaker.process (y);
        rep.peakMaxGrDb = std::min (rep.peakMaxGrDb, peaker.gainReductionDb());

        // Na samym koncu, zeby cofac laczny skutek wszystkiego powyzej — a nie
        // polowe skutku, ktory dopiero potem urosnie.
        y = tilt.process (y);

        return y;
    }

    /** Caly przebieg, mono. */
    ChainReport render (const std::vector<float>& in, std::vector<float>& out)
    {
        // ---- najpierw poznaj material, potem go ustawiaj ---------------------
        // Prog bramy jako stala liczba jest zgadywaniem: -45 dBFS jest w sam
        // raz w cichym pokoju z dobrym mikrofonem, a w mieszkaniu przy ulicy
        // lezy PONIZEJ tla i brama nie robi wtedy nic.
        const double noise  = percentileBlockDb (in, 0.10);
        const double speech = percentileBlockDb (in, 0.95);

        if (params.autoGate && speech - noise > 12.0)
        {
            const double wanted = std::min (noise + 8.0, speech - 20.0);
            params.gateThreshDb = (float) std::max (-70.0, std::min (-20.0, wanted));
        }

        prepare (sr);

        const double lufsIn = measureLufs (in);
        const double peakIn = measurePeak (in);

        // ---- naprawa poza strumieniem ----------------------------------------
        std::vector<float> pre = in;
        int clicks = 0;
        double denoised = 0.0;

        if (params.useDeClick)
        {
            DeClick dc;
            std::vector<float> tmp;
            dc.render (pre, tmp);
            pre.swap (tmp);
            clicks = dc.repaired;
        }

        // Odszumiamy tylko wtedy, gdy jest co odszumiac. Material nagrany w
        // cichym pokoju, z tlem czterdziesci kilka decybeli ponizej mowy, nie
        // zyska nic, a kazde przetwarzanie widmowe czyms placi.
        if (params.useDenoise && speech - noise < 45.0 && noise > -95.0)
        {
            SpectralDenoise dn;
            dn.params.reductionDb = params.denoiseDb;
            dn.prepare (sr);

            std::vector<float> tmp;
            dn.render (pre, tmp);
            pre.swap (tmp);

            denoised = dn.didProcess() ? params.denoiseDb : 0.0;
        }

        // Oddechy PO odszumianiu (czysciej widac, co jest oddechem, a co tlem)
        // i PRZED kompresja — inaczej kompresor podnosi je razem z cichymi
        // fragmentami (v3 Adriana: oddechy +2-9 dB wzgledem glosu).
        if (params.useBreath)
        {
            BreathControl bc;
            bc.params.reductionDb = params.breathDb;
            std::vector<float> tmp;
            bc.render (pre, tmp, sr);
            pre.swap (tmp);
            rep.breaths = bc.found;
        }

        // ---- przejscie pierwsze: tylko po to, zeby zmierzyc ------------------
        //
        // Tlumienie odchylen idzie PO kompresji, nie przed. Kompresja podnosi
        // sybilanty (sa ciche, wiec dostaja najwiecej wzmocnienia) — sprzatanie
        // przed nia to sprzatanie przed balaganem.
        std::vector<float> stage (pre.size());

        auto renderStage = [&]
        {
            const size_t delay = (params.useGate ? (size_t) gate.latencySamples() : 0)
                               + (params.useSaturation ? (size_t) sat.latencySamples() : 0);
            for (size_t i = 0; i < pre.size() + delay; ++i)
            {
                const float y = processStage (i < pre.size() ? pre[i] : 0.0f);
                if (i >= delay) stage[i - delay] = y;
            }

            if (params.useTame)
            {
                Tame t;
                t.params.thresholdDb = params.tameThreshDb;
                t.params.maxCutDb    = params.tameMaxCutDb;
                t.params.lowHz       = params.tameLowHz;
                t.prepare (sr);

                std::vector<float> tmp;
                t.render (stage, tmp);
                stage.swap (tmp);

                rep.tameMaxCutDb = t.maxCutUsedDb;
                rep.tameActive   = t.activeRatio;
            }
        };

        renderStage();

        // ---- cofniecie barwy, ktorej nikt nie zamawial -----------------------
        // Kompresja rozjasnia mowe i podbija dol. Zmierzone na wierszu: +5 dB
        // powyzej 4 kHz od samej kompresji, drugie +3 dB od jazdy suwakiem,
        // +3,5 dB ponizej 250 Hz. Dotad zamalowywal to de-esser, scinajac cala
        // gore o 11 dB — i nagranie wychodzilo ciemniejsze niz weszlo.
        //
        // Punktem odniesienia nie jest samo wejscie, tylko wejscie PLUS to, co
        // EQ mialo zrobic celowo. Inaczej korekta cofalaby takze +1 dB
        // powietrza, ktore ktos swiadomie ustawil.
        if (params.useTiltMatch)
        {
            std::vector<char> frames;
            const auto refSp = TiltMatch::analyse (pre, sr, &frames);

            TiltMatch::Curve acc;

            // Dwa podejscia. Dopasowanie trafia w krzywa od razu, ale limiter i
            // kompresor szczytowy po korekcie pracuja juz na innym sygnale i
            // czesc oddaja z powrotem. Drugie podejscie to zbiera.
            for (int round = 0; round < 2 && refSp.valid; ++round)
            {
                const auto gotSp = TiltMatch::analyse (stage, sr, &frames);
                if (! gotSp.valid) break;

                // Gdy de-esser tnie gore, korekta konczy sie pod jego zakresem.
                const double topHz = params.useDeEsser ? 4000.0 : 16000.0;

                auto grid = TiltMatch::compare (refSp, gotSp, sr, topHz);

                for (size_t i = 0; i < grid.hz.size(); ++i)
                    grid.errDb[i] += intendedEqDb (grid.hz[i]);

                // stala jeszcze raz, bo celowy EQ mogl ja przesunac
                {
                    double ws = 0.0, es = 0.0;
                    for (size_t i = 0; i < grid.hz.size(); ++i)
                    { ws += grid.weight[i]; es += grid.weight[i] * grid.errDb[i]; }
                    if (ws > 0.0) { const double m = es / ws; for (auto& e : grid.errDb) e -= m; }
                }

                TiltMatch solver;
                const auto delta = solver.fit (grid, sr, params.tiltMaxDb, params.useDeEsser || params.useTame);

                if (! delta.active) break;

                for (int b = 0; b < TiltMatch::kBands; ++b)
                    acc.gainDb[b] = std::clamp (acc.gainDb[b] + delta.gainDb[b],
                                                -params.tiltMaxDb, params.tiltMaxDb);

                acc.active = acc.maxAbsDb() > 0.25f;
                if (! acc.active) break;

                const auto saved = rep;
                reset();
                rep = saved;

                tilt.prepare (acc, sr);

                renderStage();

                if (delta.maxAbsDb() < 0.3f) break;
            }

            if (acc.active)
            {
                for (int b = 0; b < TiltMatch::kBands; ++b)
                    rep.tiltDb[b] = acc.gainDb[b];

                rep.tiltUsed = true;
            }
        }

        // ---- wzorzec brzmienia -------------------------------------------------
        // Po wszystkim, co lancuch robi sam: porownujemy gotowy ksztalt z
        // wzorcem i dociagamy tym samym filtrem (tilt jest ostatni w stopniu).
        // Ponizej 0,75 dB nic nie ruszamy — taka roznica to rozrzut pomiaru
        // (wzorzec jest z gotowego pliku, po limiterze; tu porownujemy przed
        // nim: na tym samym nagraniu wychodzi do 0,5 dB), nie inny dzien przy
        // mikrofonie.
        if (params.profile.valid())
        {
            TiltMatch::Curve acc;
            if (rep.tiltUsed) for (int b = 0; b < TiltMatch::kBands; ++b) acc.gainDb[b] = rep.tiltDb[b];
            float applied[TiltMatch::kBands] {};

            for (int round = 0; round < 2; ++round)
            {
                const auto gotSp = TiltMatch::analyse (stage, sr);
                if (! gotSp.valid) break;

                // Cale pasmo: polka 5 kHz dopasowana tylko do punktow pod 4 kHz
                // "zgadywala" gore (kontrola na tym samym nagraniu dostawala
                // +0,8 dB). Podbicia gory i tak ogranicza warunek nizej.
                const auto grid = params.profile.against (gotSp, 16000.0);
                TiltMatch solver;
                // Bez "ochrony gory" z TiltMatch: ta odrzuca kazdy ruch, ktory
                // choc o 0,05 dB rusza pasmo nad 4 kHz — a dzwon 1,4 kHz zawsze
                // troche tam siega, wiec glos z bliska (ciemniejszy) nie dostawal
                // zadnej korekty. Gore pilnujemy prosciej: przy de-esserze polka
                // 5 kHz najwyzej +1,5 dB, zeby nie oddawac sybilantow.
                auto delta = solver.fit (grid, sr, params.profileMaxDb, false);
                if (params.useDeEsser || params.useTame)
                    delta.gainDb[3] = std::min (delta.gainDb[3], 1.5f - applied[3]);
                if (! delta.active || delta.maxAbsDb() < (round == 0 ? 0.75f : 0.3f)) break;

                for (int b = 0; b < TiltMatch::kBands; ++b)
                {
                    const float next = std::clamp (applied[b] + delta.gainDb[b], -params.profileMaxDb, params.profileMaxDb);
                    acc.gainDb[b] += next - applied[b];
                    applied[b] = next;
                }
                acc.active = true;

                const auto saved = rep;
                reset();
                rep = saved;
                tilt.prepare (acc, sr);
                renderStage();
            }

            for (int b = 0; b < TiltMatch::kBands; ++b) rep.profileDb[b] = applied[b];
            rep.profileUsed = std::any_of (applied, applied + TiltMatch::kBands, [] (float v) { return std::fabs (v) >= 0.25f; });
        }

        const double afterStage = measureLufs (stage);

        // Kopia robocza nie jest juz potrzebna — tylko jej dlugosc. Przy godzinie
        // nagrania to 0,7 GB (Tools/zs-dlugo.cpp: szczyt 3,2 GB po miksie).
        const size_t n = pre.size();
        std::vector<float>().swap (pre);

        // ---- wyrownanie glosnosci, iteracyjnie -------------------------------
        const bool canNormalise=stage.size()>=size_t(sr*.4) && afterStage>-69.0;
        double gainDb = canNormalise ? params.targetLufs - afterStage : 0.0;
        out.resize (n);

        // Osiem przejsc, nie trzy. Limiter jest jedyna nieliniowoscia po
        // korekcie, ale przy mocnym limitowaniu kazdy dodany decybel wchodzi
        // tylko w polowie — zbieznosc jest wtedy geometryczna z ilorazem 0,5 i
        // trzy przejscia zostawialy 0,8 LU bledu. Zmierzone: -16,8 zamiast -16,0.
        // Sufit jest deklaracja, wiec ma byc dotrzymany, a nie oszacowany.
        //
        // Limiter pilnuje szczytu PROBKOWEGO i zostawia staly zapas 0,5 dB na
        // to, co dzieje sie miedzy probkami. Przy materiale z duza iloscia gory
        // ten zapas nie wystarcza: zmierzone 1,9 dB nadwyzki, czyli wyjscie
        // deklarowane jako -1 dBTP wychodzilo na +0,4 dBTP. To nie jest drobiazg
        // — po kompresji do mp3 albo aac taki plik trzeszczy, a nikt juz wtedy
        // nie wie dlaczego.
        //
        // Wiec zamiast zakladac: mierzymy szczyt miedzyprobkowy po limitowaniu i
        // jesli przekracza sufit, obnizamy sufit o nadwyzke i powtarzamy. Petla
        // i tak juz chodzi po glosnosc.
        double extraMarginDb = 0.0;

        for (int pass = 0; pass < 10; ++pass)
        {
            const float g = dbToGain ((float) gainDb);

            for (size_t i = 0; i < n; ++i)
                out[i] = stage[i] * g;

            limiter.params.ceilingDb = (float) (params.ceilingDbTp - extraMarginDb);
            limiter.prepare (sr, 1);

            const size_t delay = (size_t) limiter.latencySamples();
            out.resize (n + delay, 0.0f);
            float* io[1] = { out.data() };
            limiter.process (io, 1, (int) out.size());
            rep.limitMaxGrDb = limiter.maxGainReductionDb();
            std::move (out.begin() + (ptrdiff_t) delay, out.end(), out.begin());
            out.resize (n);

            const double overTp = measurePeak (out) - params.ceilingDbTp;

            if (overTp > 0.05)
            {
                extraMarginDb += overTp + 0.05;
                continue;                       // najpierw sufit, potem glosnosc
            }

            if(!canNormalise)break;
            const double miss = params.targetLufs - measureLufs (out);

            if (std::fabs (miss) < 0.05)
                break;

            gainDb += miss;
        }

        limiter.params.ceilingDb = params.ceilingDbTp;   // zostaw jak bylo

        // ---- dither tylko przy skracaniu do 16 bitow -------------------------
        if (params.outputBits <= 16)
        {
            dither.reset();
            for (auto& v : out) v = dither.process (v, params.outputBits);
        }

        rep.gainAppliedDb  = gainDb;
        rep.clicksRepaired = clicks;
        rep.denoiseDb      = denoised;
        rep.noiseFloorDb   = noise;
        rep.speechDb       = speech;
        rep.gateThreshDb   = params.gateThreshDb;
        rep.humHz          = params.deHum ? params.humHz : 0.0;   // ekran montazu pokazywal "0 Hz"
        rep.lufsBefore     = lufsIn;
        rep.peakBefore     = peakIn;
        rep.lufsAfter      = measureLufs (out);
        rep.peakAfter      = measurePeak (out);

        return rep;
    }

    double sampleRate() const noexcept { return sr; }
    void   setSampleRate (double v) noexcept { sr = v; }

private:
    /** Poziom skuteczny w blokach 50 ms, wziety z zadanego percentyla.

        Dziesiaty percentyl to tlo pokoju: blok, ponizej ktorego lezy tylko
        dziesiec procent nagrania. Dziewiecdziesiaty piaty to mowa w szczycie
        swojej glosnosci. Roznica miedzy nimi mowi wszystko o warunkach, w
        jakich nagrywano — i to na niej stoja progi. */
    /** Laczna odpowiedz EQ barwy — tego, co ustawil czlowiek — w dB.
        Dopasowanie barwy ma to zostawic w spokoju, bo to jedyna zmiana
        brzmienia w calym lancuchu, ktora jest decyzja, a nie skutkiem. */
    double intendedEqDb (double hz) const
    {
        Biquad l, m, p, a;
        l.setCoeffs (design::lowShelf  (150.0,   sr, params.lowDb));
        m.setCoeffs (design::peaking   (250.0,   sr, 1.0, params.mudDb));
        p.setCoeffs (design::peaking   (params.presenceHz, sr, params.presenceQ, params.presenceDb));
        a.setCoeffs (design::highShelf (params.airHz, sr, params.airDb));

        return 20.0 * std::log10 (std::max (1.0e-9, l.magnitudeAt (hz, sr)
                                                  * m.magnitudeAt (hz, sr)
                                                  * p.magnitudeAt (hz, sr)
                                                  * a.magnitudeAt (hz, sr)));
    }

    double percentileBlockDb (const std::vector<float>& v, double p) const
    {
        const int block = std::max (1, (int) (sr * 0.05));
        std::vector<double> levels;

        for (size_t start = 0; start + (size_t) block <= v.size(); start += (size_t) block)
        {
            double sum = 0.0;

            for (int i = 0; i < block; ++i)
                sum += (double) v[start + (size_t) i] * v[start + (size_t) i];

            levels.push_back (sum > 1.0e-20 ? 10.0 * std::log10 (sum / block) : -180.0);
        }

        if (levels.empty())
            return -180.0;

        std::sort (levels.begin(), levels.end());
        const size_t idx = std::min (levels.size() - 1,
                                     (size_t) (p * (double) (levels.size() - 1)));
        return levels[idx];
    }

    double measureLufs (const std::vector<float>& v)
    {
        LoudnessMeter m;
        m.prepare (sr, 1);
        const float* p[1] = { v.data() };
        m.process (p, 1, (int) v.size());
        return m.integrated();
    }

    double measurePeak (const std::vector<float>& v)
    {
        TruePeak t;
        t.prepare (sr, 1);
        const float* p[1] = { v.data() };
        t.process (p, 1, (int) v.size());
        t.finish();
        return t.dbTP();
    }

    double sr = 48000.0;

    Biquad hp1, hp2, hum[3], low, mud, presence, air;
    std::vector<Biquad> pre, notch;
    TiltMatch tilt;
    DePlosive plosive;
    Gate gate;
    GainRider rider;
    Compressor leveler, peaker;
    DeEsser ess;
    Saturator sat;
    Limiter limiter;
    Dither dither;
    ChainReport rep;
};

} // namespace mix
