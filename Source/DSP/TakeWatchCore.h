/*
    TakeWatchCore.h — the listening part of TakeWatch.

    What it does:  measures a spoken take while it is being recorded, learns what
                   this particular voice on this particular microphone normally
                   looks like, and reports deviations with a verdict attached.
    What it never does:  touch the audio. process() is const with respect to the
                   signal. A recording assistant that can ruin a take is worse
                   than no assistant at all.

    The verdict is the point. An event log tells you "there was a plosive at
    00:04:11". A verdict tells you "stop, that one is unusable, start again from
    00:04:07" — and the second sentence is the one that saves a session.

    Triage:
      Stop   — information was destroyed. No plug-in gets it back.
      Redo   — repairable, but redoing the sentence is cheaper than repairing it.
      Later  — log it, fix it in the edit, do not interrupt the speaker.
      Setup  — the session itself has drifted. Everything from here on is worse.
*/

#pragma once

#include "Primitives.h"
#include <atomic>
#include <vector>

namespace tw
{

//==============================================================================
enum class Severity { Later = 0, Redo = 1, Setup = 2, Stop = 3 };

enum class Kind
{
    DigitalSilence, Clipping, TooHot, Plosive, MouthClick, Breath,
    Sibilance, OffAxis, LevelDrift, NoiseRise, Hum, FalseStart, LowLevel
};

struct Event
{
    Kind     kind      = Kind::Clipping;
    Severity severity  = Severity::Later;
    double   timeSec   = 0.0;   ///< when it happened, on the host timeline
    double   redoFrom  = 0.0;   ///< start of the phrase it belongs to
    float    value     = 0.0f;  ///< the measured number that triggered it
    int      phrase    = -1;    ///< index into the phrase list
};

struct Phrase
{
    double startSec = 0.0;
    double endSec   = 0.0;
    float  rmsDb    = kMinDb;
    float  peakDb   = kMinDb;
    float  lowMinusMid = 0.0f;
    float  sibMinusMid = 0.0f;
    bool   flagged  = false;    ///< carried a Redo or Stop event
    int    verdict  = 0;        ///< highest Severity seen, as int

    static constexpr int kBands = 16;
    static constexpr int kSlots = 32;
    static constexpr double kPrintSeconds = 3.0;

    float    print[kBands * kSlots] {};  ///< band/time fingerprint, doubly mean-removed
    uint32_t slotMask = 0;               ///< which of the 32 slots hold real data
    bool     printValid = false;
};

//==============================================================================
struct Settings
{
    float strictness      = 1.0f;   ///< 0.5 forgiving … 2.0 paranoid

    // hard limits — independent of the voice profile
    float clipDbfs        = -0.1f;  ///< true peak at or above this is clipping
    int   clipRunSamples  = 3;      ///< consecutive samples pinned = hard clip
    float hotDbfs         = -1.5f;  ///< peak above this is uncomfortably close
    float silenceSeconds  = 0.4f;   ///< exact zeros for this long = dead channel

    // profile-relative thresholds, in dB, before strictness scaling
    float plosiveRiseDb   = 18.0f;
    float plosiveExcessDb = 10.0f;
    float clickRiseDb     = 18.0f;
    float sibExcessDb     = 6.0f;
    float offAxisDropDb   = 4.0f;
    float levelDriftDb    = 6.0f;
    float noiseRiseDb     = 8.0f;
    float humProminenceDb = 12.0f;

    double profileSeconds = 20.0;   ///< of *speech*, not of wall clock
};

//==============================================================================
class TakeWatch
{
public:
    TakeWatch() = default;

    //--------------------------------------------------------------------------
    void prepare (double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;

        hop      = std::max (64, (int) std::round (sampleRate / 187.5));   // ~5.3 ms
        fftSize  = 1024;
        while (fftSize < hop * 4) fftSize <<= 1;

        for (int i = 0; i < fftSize; ++i)
            window[i] = 0.5f - 0.5f * std::cos (2.0f * (float) M_PI * (float) i / (float) fftSize);

        lfEnv .setup (sampleRate,  20.0,   120.0,  1.0,  40.0);
        midEnv.setup (sampleRate, 300.0,  3000.0,  4.0,  70.0);
        hiEnv .setup (sampleRate, 3000.0, 9000.0,  0.4,  25.0);

        lfBase .setup (sampleRate, 400.0, 400.0);   // slow both ways
        hiBase .setup (sampleRate, 200.0, 200.0);

        // Narrowband path for mains hum. 50 Hz and 100 Hz are one bin apart in a
        // 1024-point FFT at 48 kHz, which is useless — so the low end gets its
        // own decimated analysis where a bin is 1.5 Hz wide and a hum line is
        // actually a line.
        for (auto& b : decim) b.setLowpass (sampleRate, 300.0, 0.7071);
        humRate = sampleRate / (double) kDecim;

        for (int i = 0; i < kHumFft; ++i)
            humWin[i] = 0.5f - 0.5f * std::cos (2.0f * (float) M_PI * (float) i / (float) kHumFft);

        phrases.reserve (kMaxPhrases);

        peakDetect.prepare();
        reset();
    }

    void reset()
    {
        lfEnv.reset(); midEnv.reset(); hiEnv.reset();
        peakDetect.reset();
        lfBase.setup (sr, 400.0, 400.0);
        hiBase.setup (sr, 200.0, 200.0);

        midFloor = 0.0f;
        midFloorPrimed = false;

        for (auto& b : decim) b.reset();
        decimCount = 0;
        humFill = 0;
        humRuns = 0;
        humLastDb = kMinDb;
        std::memset (humBuf, 0, sizeof (humBuf));

        fifoFill = 0;
        std::memset (fifo, 0, sizeof (float) * (size_t) fftSize);

        sampleCounter   = 0;
        zeroRun         = 0;
        clipRun         = 0;
        speaking        = false;
        speechSamples   = 0;
        gapSamples      = 0;
        profileSpeech   = 0;
        profileLocked   = false;
        phraseCount     = 0;
        phrases.clear();
        writeIndex.store (0, std::memory_order_relaxed);
        readIndex = 0;

        speechRms.clear(); noiseHist.clear();
        lowMidHist.clear(); sibMidHist.clear(); lfMidHist.clear();

        currentPhrase = Phrase{};
        phraseOpen = false;
        clearAccum();

        humFrames = 0;
        noiseRiseGaps = 0;
        offAxisPhrases = 0;
        lastEventSec[0] = lastEventSec[1] = lastEventSec[2] = lastEventSec[3] = -100.0;
        for (auto& t : lastKindSec) t = -100.0;
    }

    Settings settings;

    //--------------------------------------------------------------------------
    /** Analyse a block. Does not modify the audio. playheadSec is the host's
        position for the first sample of the block; if the host does not supply
        one, pass the running elapsed time and the log will still be internally
        consistent. */
    void process (const float* const* channels, int numChannels,
                  int numSamples, double playheadSec)
    {
        blockStartSec = playheadSec;

        for (int n = 0; n < numSamples; ++n)
        {
            float x = 0.0f;
            for (int c = 0; c < numChannels; ++c) x += channels[c][n];
            if (numChannels > 1) x /= (float) numChannels;

            const double t = playheadSec + (double) n / sr;

            perSample (x, t);

            fifo[fifoFill++] = x;
            if (fifoFill == fftSize)
            {
                analyseFrame (t);
                std::memmove (fifo, fifo + hop, sizeof (float) * (size_t) (fftSize - hop));
                fifoFill = fftSize - hop;
            }

            ++sampleCounter;
        }
    }

    //--------------------------------------------------------------------------
    bool profileReady() const noexcept { return profileLocked; }
    float profileProgress() const noexcept
    {
        return std::min (1.0f, (float) ((double) profileSpeech / (sr * settings.profileSeconds)));
    }

    /** Pops the next event for the GUI thread. Returns false if none waiting. */
    bool nextEvent (Event& out) noexcept
    {
        const int w = writeIndex.load (std::memory_order_acquire);
        if (readIndex == w) return false;
        out = events[readIndex % kEventCapacity];
        ++readIndex;
        return true;
    }

    int  numPhrases() const noexcept { return phraseCount; }
    const Phrase& phrase (int i) const noexcept { return phrases[(size_t) i]; }

    float learnedSpeechDb()  const noexcept { return pSpeechDb; }
    float learnedNoiseDb()   const noexcept { return pNoiseDb; }
    float learnedLowMinusMid() const noexcept { return pLowMid; }
    float learnedSibMinusMid() const noexcept { return pSibMid; }

    /** All events emitted so far, for the timeline view and the edit map. */
    int numLoggedEvents() const noexcept { return std::min (loggedCount, kEventCapacity); }
    const Event& loggedEvent (int i) const noexcept { return events[i % kEventCapacity]; }

private:
    //--------------------------------------------------------------------------
    static constexpr int kEventCapacity = 512;
    static constexpr int kMaxPhrases    = 2048;
    static constexpr int kMaxFft        = 2048;

    double sr = 48000.0;
    int    hop = 256, fftSize = 1024;

    float  window[kMaxFft] {};
    float  fifo[kMaxFft] {};
    int    fifoFill = 0;

    float  re[kMaxFft] {}, im[kMaxFft] {};

    static constexpr int kDecim  = 32;
    static constexpr int kHumFft = 1024;
    static constexpr int kHumHop = 256;

    BandEnvelope lfEnv, midEnv, hiEnv;
    SlowFollower lfBase, hiBase;
    TruePeak     peakDetect;

    /** Noise floor of the speech band, tracked with a fast-down / slow-up
        follower. Fast down so it finds the quiet moments, slow up so a long
        loud passage cannot drag the gate threshold along with it. */
    float midFloor = 0.0f;
    bool  midFloorPrimed = false;

    Biquad decim[3];
    int    decimCount = 0;
    double humRate = 1500.0;
    float  humBuf[kHumFft] {}, humWin[kHumFft] {};
    float  humRe[kHumFft] {}, humIm[kHumFft] {};
    int    humFill = 0, humRuns = 0;
    float  humLastDb = kMinDb;

    int64_t sampleCounter = 0;
    int     zeroRun = 0, clipRun = 0;
    double  blockStartSec = 0.0;

    bool    speaking = false;
    int64_t speechSamples = 0, gapSamples = 0, profileSpeech = 0;
    bool    profileLocked = false;

    DbHistogram<576> speechRms, noiseHist, lowMidHist, sibMidHist, lfMidHist;

    float pSpeechDb = -18.0f, pNoiseDb = -60.0f;
    bool  noiseFloorTrusted = false;
    float pLowMid = 0.0f, pSibMid = -12.0f, pLfMid = -18.0f;

    // Reserved once in prepare(); push_back never reallocates because the size
    // is checked against the capacity first. Eight megabytes of fingerprints do
    // not belong on the stack, and an allocation does not belong on the audio
    // thread.
    std::vector<Phrase> phrases;
    Phrase  currentPhrase;
    int     phraseCount = 0;
    bool    phraseOpen = false;

    // per-phrase accumulators (linear energy)
    double aMid = 0.0, aLow = 0.0, aSib = 0.0, aRms = 0.0;
    int    aFrames = 0;
    float  aPeak = 0.0f;
    double printAcc[Phrase::kBands * Phrase::kSlots] {};
    int    printSlotCount[Phrase::kSlots] {};

    Event  events[kEventCapacity];
    std::atomic<int> writeIndex { 0 };
    int    readIndex = 0;
    int    loggedCount = 0;

    double lastEventSec[4] {};
    double lastKindSec[16] {};

    int humFrames = 0, noiseRiseGaps = 0, offAxisPhrases = 0;

    double lastGapEndSec = 0.0;
    double prevPhraseEndSec = -100.0;

    //--------------------------------------------------------------------------
    void clearAccum()
    {
        aMid = aLow = aSib = aRms = 0.0;
        aFrames = 0;
        aPeak = 0.0f;
        std::memset (printAcc, 0, sizeof (printAcc));
        std::memset (printSlotCount, 0, sizeof (printSlotCount));
    }

    float strict (float db) const noexcept
    {
        // higher strictness lowers the bar for a complaint
        return db / std::max (0.25f, settings.strictness);
    }

    void push (Kind k, Severity s, double t, float v)
    {
        const int ki = (int) k;

        // Rate limit by severity, not by a single number. A plosive may be worth
        // repeating after a quarter second; "the room got noisier" is a standing
        // condition and saying it every 1.4 seconds is how a tool gets switched
        // off.
        const double refractory = s == Severity::Setup ? 20.0
                               : s == Severity::Later ? 1.50
                                                      : 0.25;

        if (t - lastKindSec[ki] < refractory) return;
        lastKindSec[ki] = t;

        Event e;
        e.kind     = k;
        e.severity = s;
        e.timeSec  = t;
        e.redoFrom = phraseOpen ? currentPhrase.startSec : t;
        e.value    = v;
        e.phrase   = phraseCount;

        const int w = writeIndex.load (std::memory_order_relaxed);
        events[w % kEventCapacity] = e;
        writeIndex.store (w + 1, std::memory_order_release);
        ++loggedCount;

        if (phraseOpen && (s == Severity::Redo || s == Severity::Stop))
        {
            currentPhrase.flagged = true;
            currentPhrase.verdict = std::max (currentPhrase.verdict, (int) s);
        }
    }

    //==========================================================================
    void perSample (float x, double t)
    {
        // ---- dead channel ---------------------------------------------------
        if (x == 0.0f)
        {
            ++zeroRun;
            if (zeroRun == (int) (settings.silenceSeconds * sr))
                push (Kind::DigitalSilence, Severity::Stop,
                      t - settings.silenceSeconds, (float) settings.silenceSeconds);
        }
        else zeroRun = 0;

        // ---- clipping -------------------------------------------------------
        const float tp   = peakDetect.process (x);
        const float tpDb = toDb (tp);
        const float a    = std::abs (x);

        if (a >= 0.99995f) ++clipRun; else clipRun = 0;

        if (clipRun == settings.clipRunSamples)
            push (Kind::Clipping, Severity::Stop, t, toDb (a));
        else if (tpDb >= settings.clipDbfs)
            push (Kind::Clipping, Severity::Redo, t, tpDb);
        else if (tpDb >= settings.hotDbfs)
            push (Kind::TooHot, Severity::Later, t, tpDb);

        if (tp > aPeak) aPeak = tp;

        // ---- band envelopes -------------------------------------------------
        const float lf  = lfEnv .process (x);
        const float mid = midEnv.process (x);
        const float hi  = hiEnv .process (x);

        const float lfDb  = toDb (lf);
        const float midDb = toDb (mid);
        const float hiDb  = toDb (hi);

        const float lfB = toDb (lfBase.process (lf));
        const float hiB = toDb (hiBase.process (hi));

        // ---- plosive --------------------------------------------------------
        // A burst of 20-120 Hz that the midrange did not follow. Real speech
        // does not gain 18 dB of subsonic energy in 20 ms without the voice
        // gaining anything; a puff of air on the capsule does exactly that.
        if (profileLocked
            && lfDb > lfB + strict (settings.plosiveRiseDb)
            && lfDb - midDb > pLfMid + strict (settings.plosiveExcessDb)
            && lfDb > -45.0f)
        {
            const bool severe = (lfDb > -6.0f) || (clipRun > 0);
            push (Kind::Plosive, severe ? Severity::Stop : Severity::Redo, t, lfDb - midDb);
        }

        // ---- mouth click ----------------------------------------------------
        // Short broadband spike while the voice is not sounding. Between words,
        // a 3 ms transient 15 dB over the local high-band floor is a lip or
        // tongue click; during a word it is just a consonant.
        if (profileLocked && ! speaking
            && hiDb > hiB + strict (settings.clickRiseDb)
            && hiDb > pNoiseDb + 12.0f
            && midDb < pSpeechDb - 22.0f)
        {
            push (Kind::MouthClick, Severity::Later, t, hiDb - hiB);
        }

        feedHum (x, t);
    }

    //==========================================================================
    void analyseFrame (double t)
    {
        for (int i = 0; i < fftSize; ++i) { re[i] = fifo[i] * window[i]; im[i] = 0.0f; }
        fft (re, im, fftSize);

        const int half = fftSize / 2;
        const float binHz = (float) sr / (float) fftSize;
        const float norm  = 2.0f / (float) fftSize;

        float magSq[kMaxFft / 2];
        for (int i = 0; i < half; ++i)
        {
            const float mr = re[i] * norm, mi = im[i] * norm;
            magSq[i] = mr * mr + mi * mi;
        }

        auto bandEnergy = [&] (float lo, float hi)
        {
            const int a = std::max (1, (int) (lo / binHz));
            const int b = std::min (half - 1, (int) (hi / binHz));
            float sum = 0.0f;
            for (int i = a; i <= b; ++i) sum += magSq[i];
            return sum;
        };

        const float eLf   = bandEnergy (20.0f,    120.0f);
        const float eLow  = bandEnergy (120.0f,   300.0f);
        const float eMid  = bandEnergy (300.0f,  3000.0f);
        const float eSib  = bandEnergy (5500.0f,10000.0f);

        // The gate listens across the whole voice, not just its body. A run of
        // /s/ and /f/ carries almost nothing between 300 Hz and 3 kHz, so a gate
        // built on the midrange alone goes deaf in the middle of a word and
        // splits one sentence into three — which then breaks the redo point, the
        // fingerprint and the edit map downstream.
        const float eSpeech = bandEnergy (200.0f, 8000.0f);

        float total = 0.0f;
        for (int i = 1; i < half; ++i) total += magSq[i];

        const float rmsDb = toDbEnergy (total * 0.5f);
        const float midDb = toDbEnergy (eMid);
        const float lowDb = toDbEnergy (eLow);
        const float sibDb = toDbEnergy (eSib);
        const float lfDb  = toDbEnergy (eLf);

        // ---- speech gate ----------------------------------------------------
        if (! midFloorPrimed) { midFloor = eSpeech; midFloorPrimed = true; }
        else
        {
            // ~0.1 s down, ~15 s up, in the energy domain
            const float c = eSpeech < midFloor
                          ? (float) std::exp (-(double) hop / (0.100 * sr))
                          : (float) std::exp (-(double) hop / (15.0  * sr));
            midFloor = eSpeech + c * (midFloor - eSpeech);
        }

        const float speechDb = toDbEnergy (eSpeech);
        const float floorDb  = toDbEnergy (std::max (midFloor, 1.0e-13f));
        const bool  voiced   = speechDb > floorDb + 12.0f && speechDb > -95.0f;

        if (voiced)  { speechSamples += hop; gapSamples = 0; }
        else         { gapSamples += hop; }

        const bool wasSpeaking = speaking;

        if (! speaking && voiced && speechSamples > (int64_t) (0.04 * sr))
            speaking = true;
        else if (speaking && ! voiced && gapSamples > (int64_t) (0.45 * sr))
        {
            speaking = false;
            speechSamples = 0;
        }

        if (speaking && ! wasSpeaking) openPhrase (t - 0.15);
        if (! speaking && wasSpeaking) closePhrase (t - 0.45);

        // ---- profile --------------------------------------------------------
        if (speaking)
        {
            if (! profileLocked)
            {
                profileSpeech += hop;
                speechRms .add (rmsDb);
                lowMidHist.add (lowDb - midDb);
                sibMidHist.add (sibDb - midDb);
                lfMidHist .add (lfDb  - midDb);

                if ((double) profileSpeech >= sr * settings.profileSeconds)
                    lockProfile();
            }

            aMid += eMid; aLow += eLow; aSib += eSib;
            aRms += (double) total;
            ++aFrames;
            addToPrint (magSq, half, binHz, t);
        }
        else
        {
            if (! profileLocked) noiseHist.add (rmsDb);
            checkGap (rmsDb, t);
        }
    }

    //--------------------------------------------------------------------------
    void lockProfile()
    {
        pSpeechDb = speechRms.percentile (0.5f);

        // Szum tla znamy tylko wtedy, gdy uzbieralo sie dosc pomiarow ciszy.
        // Wczesniejsze zgadywanie "45 dB ponizej mowy" jest o kilkanascie dB
        // za optymistyczne dla prawdziwego pokoju i powodowalo falszywy alarm
        // NoiseRise trzy sekundy po zamknieciu profilu: przy mowie -28 dBFS
        // zaklada -73, a pokoj ma realnie -48, czyli 25 dB nad progiem 8 dB.
        // Kiedy nie wiemy, lepiej milczec niz kazac naprawiac sprawny pokoj.
        noiseFloorTrusted = noiseHist.count > 40;
        pNoiseDb = noiseFloorTrusted ? noiseHist.percentile (0.2f) : pSpeechDb - 30.0f;
        pLowMid   = lowMidHist.median();
        pSibMid   = sibMidHist.percentile (0.85f);   // normal variation is not a fault
        pLfMid    = lfMidHist.percentile (0.9f);   // headroom over normal breathiness
        profileLocked = true;
    }

    //--------------------------------------------------------------------------
    void openPhrase (double t)
    {
        currentPhrase = Phrase{};
        currentPhrase.startSec = std::max (0.0, t);
        phraseOpen = true;
        clearAccum();
    }

    void closePhrase (double t)
    {
        if (! phraseOpen) return;
        phraseOpen = false;

        currentPhrase.endSec = t;
        const double dur = currentPhrase.endSec - currentPhrase.startSec;

        if (aFrames > 0)
        {
            currentPhrase.rmsDb       = toDbEnergy ((float) (aRms / aFrames) * 0.5f);
            currentPhrase.lowMinusMid = toDbEnergy ((float) (aLow / aFrames))
                                      - toDbEnergy ((float) (aMid / aFrames));
            currentPhrase.sibMinusMid = toDbEnergy ((float) (aSib / aFrames))
                                      - toDbEnergy ((float) (aMid / aFrames));
        }

        currentPhrase.peakDb = toDb (aPeak);
        finishPrint();

        if (profileLocked && aFrames > 6)
        {
            // sibilance
            if (currentPhrase.sibMinusMid > pSibMid + strict (settings.sibExcessDb))
                push (Kind::Sibilance, Severity::Later, currentPhrase.startSec,
                      currentPhrase.sibMinusMid - pSibMid);

            // off-axis / distance: proximity effect is the first thing to go
            if (currentPhrase.lowMinusMid < pLowMid - strict (settings.offAxisDropDb))
            {
                if (++offAxisPhrases >= 3)
                    push (Kind::OffAxis, Severity::Setup, currentPhrase.startSec,
                          pLowMid - currentPhrase.lowMinusMid);
            }
            else offAxisPhrases = 0;

            // level drift
            const float drift = currentPhrase.rmsDb - pSpeechDb;
            if (std::abs (drift) > strict (settings.levelDriftDb) && dur > 0.6)
                push (drift < 0.0f ? Kind::LowLevel : Kind::LevelDrift,
                      Severity::Setup, currentPhrase.startSec, drift);

            // false start: a very short utterance abandoned into a long pause
            if (dur < 0.55 && prevPhraseEndSec > 0.0
                && currentPhrase.startSec - prevPhraseEndSec > 0.5)
                push (Kind::FalseStart, Severity::Redo, currentPhrase.startSec, (float) dur);
        }

        prevPhraseEndSec = currentPhrase.endSec;

        if (phrases.size() < phrases.capacity())
        {
            phrases.push_back (currentPhrase);
            ++phraseCount;
        }
    }

    //--------------------------------------------------------------------------
    void checkGap (float rmsDb, double t)
    {
        if (! profileLocked) return;
        if (gapSamples < (int64_t) (0.35 * sr)) return;   // wait for the tail to die

        // Dopoki nie mamy zmierzonego szumu, uczymy sie go dalej z przerw i nie
        // oceniamy niczego. Prog liczony z domyslu to nie prog, tylko zgadywanka.
        if (! noiseFloorTrusted)
        {
            noiseHist.add (rmsDb);

            if (noiseHist.count > 40)
            {
                pNoiseDb = noiseHist.percentile (0.2f);
                noiseFloorTrusted = true;
            }

            return;
        }

        if (rmsDb > pNoiseDb + strict (settings.noiseRiseDb))
        {
            if (t - lastGapEndSec > 1.0)
            {
                lastGapEndSec = t;
                if (++noiseRiseGaps >= 5)
                    push (Kind::NoiseRise, Severity::Setup, t, rmsDb - pNoiseDb);
            }
        }
        else if (t - lastGapEndSec > 1.0)
        {
            lastGapEndSec = t;
            noiseRiseGaps = 0;
        }
    }

    //--------------------------------------------------------------------------
    /** Mains hum, measured on the decimated low band.

        Two things separate hum from a voice: it is narrow, and it does not move.
        A prominence test alone flags every low male fundamental. So a line has
        to clear the local median AND hold its level within a few dB across a
        second of windows. Vowels do not do that; a transformer does. */
    void feedHum (float x, double t)
    {
        float y = x;
        for (auto& b : decim) y = b.process (y);

        if (++decimCount < kDecim) return;
        decimCount = 0;

        humBuf[humFill++] = y;
        if (humFill < kHumFft) return;

        for (int i = 0; i < kHumFft; ++i) { humRe[i] = humBuf[i] * humWin[i]; humIm[i] = 0.0f; }
        fft (humRe, humIm, kHumFft);

        std::memmove (humBuf, humBuf + kHumHop,
                      sizeof (float) * (size_t) (kHumFft - kHumHop));
        humFill = kHumFft - kHumHop;

        const int   half  = kHumFft / 2;
        const float binHz = (float) humRate / (float) kHumFft;
        const float norm  = 2.0f / (float) kHumFft;

        float mag[kHumFft / 2];
        for (int i = 0; i < half; ++i)
        {
            const float mr = humRe[i] * norm, mi = humIm[i] * norm;
            mag[i] = mr * mr + mi * mi;
        }

        float bestFundDb = kMinDb;
        bool  present = false;

        for (float f0 : { 50.0f, 60.0f })
        {
            int   hits = 0;
            float fundDb = kMinDb;

            for (int h = 1; h <= 3; ++h)
            {
                const int c = (int) std::round (f0 * (float) h / binHz);
                if (c < 6 || c > half - 45) continue;

                // take the strongest of the three bins around the line, since
                // mains frequency wanders by a fraction of a hertz
                float peak = kMinDb;
                for (int d = -1; d <= 1; ++d) peak = std::max (peak, toDbEnergy (mag[c + d]));

                float around[64];
                int   count = 0;
                for (int d = 4; d <= 40 && count < 62; ++d)
                {
                    around[count++] = toDbEnergy (mag[c - d]);
                    around[count++] = toDbEnergy (mag[c + d]);
                }

                std::nth_element (around, around + count / 2, around + count);
                const float med = around[count / 2];

                if (peak - med > strict (settings.humProminenceDb)) ++hits;
                if (h == 1) fundDb = peak;
            }

            if (hits >= 2) { present = true; bestFundDb = std::max (bestFundDb, fundDb); }
        }

        if (present)
        {
            const bool steady = (humRuns == 0)
                             || std::abs (bestFundDb - humLastDb) < 4.0f;
            humLastDb = bestFundDb;

            humRuns = steady ? humRuns + 1 : 1;

            if (humRuns >= 8)   // ~1.4 s of an unmoving line
            {
                push (Kind::Hum, Severity::Setup, t, bestFundDb);
                humRuns = 0;
            }
        }
        else
        {
            humRuns = 0;
            humLastDb = kMinDb;
        }
    }

    //--------------------------------------------------------------------------
    /** A coarse spectro-temporal fingerprint of a phrase: 12 log bands by 16
        time slots over the first 2 seconds, level-normalised. Two recordings of
        the same sentence line up here; two different sentences do not. This is
        what lets the edit map say "that was the same line again, keep the last
        one" instead of guessing from timing alone. */
    void addToPrint (const float* magSq, int half, float binHz, double t)
    {
        if (! phraseOpen) return;

        const double rel = t - currentPhrase.startSec;
        if (rel < 0.0 || rel >= Phrase::kPrintSeconds) return;

        const int slot = std::clamp ((int) (rel / Phrase::kPrintSeconds * Phrase::kSlots),
                                     0, Phrase::kSlots - 1);

        for (int b = 0; b < Phrase::kBands; ++b)
        {
            const float lo = 180.0f * std::pow (7500.0f / 180.0f, (float) b / (float) Phrase::kBands);
            const float hi = 180.0f * std::pow (7500.0f / 180.0f, (float) (b + 1) / (float) Phrase::kBands);
            const int a = std::max (1, (int) (lo / binHz));
            const int z = std::min (half - 1, std::max (a, (int) (hi / binHz)));

            float sum = 0.0f;
            for (int i = a; i <= z; ++i) sum += magSq[i];
            printAcc[slot * Phrase::kBands + b] += (double) sum / (double) (z - a + 1);
        }

        ++printSlotCount[slot];
    }

    void finishPrint()
    {
        int filled = 0;
        for (int s = 0; s < Phrase::kSlots; ++s) if (printSlotCount[s] > 0) ++filled;

        // Under about 1.3 s of speech there is not enough shape to identify a
        // sentence, and a weak fingerprint is worse than none: it invites the
        // planner to cut something it should not.
        if (filled < 10) { currentPhrase.printValid = false; return; }

        currentPhrase.slotMask = 0;

        // Pass 1: log energies, minus each slot's own mean across bands.
        for (int s = 0; s < Phrase::kSlots; ++s)
        {
            if (printSlotCount[s] == 0) continue;
            currentPhrase.slotMask |= (1u << s);

            const double n = printSlotCount[s];
            float v[Phrase::kBands];
            float mean = 0.0f;

            for (int b = 0; b < Phrase::kBands; ++b)
            {
                v[b] = std::max (toDbEnergy ((float) (printAcc[s * Phrase::kBands + b] / n)), -100.0f);
                mean += v[b];
            }

            mean /= (float) Phrase::kBands;

            for (int b = 0; b < Phrase::kBands; ++b)
                currentPhrase.print[s * Phrase::kBands + b] = v[b] - mean;
        }

        // Pass 2: subtract each band's own average across the phrase.
        //
        // Without this the fingerprint mostly measures spectral tilt, and every
        // sentence from the same voice through the same microphone has the same
        // tilt — which is how two unrelated sentences came out 98% alike. What
        // survives is the pattern of change from slot to slot, which is the part
        // that is the sentence.
        for (int b = 0; b < Phrase::kBands; ++b)
        {
            float mean = 0.0f;
            int   count = 0;

            for (int s = 0; s < Phrase::kSlots; ++s)
                if (printSlotCount[s] > 0)
                {
                    mean += currentPhrase.print[s * Phrase::kBands + b];
                    ++count;
                }

            if (count == 0) continue;
            mean /= (float) count;

            for (int s = 0; s < Phrase::kSlots; ++s)
                if (printSlotCount[s] > 0) currentPhrase.print[s * Phrase::kBands + b] -= mean;
        }

        currentPhrase.printValid = true;
    }
};

//==============================================================================
/** Similarity of two phrase fingerprints, -1..1.

    Nobody says the same sentence twice at the same speed, so a fixed alignment
    finds nothing: an 8% stretch over three seconds is a quarter of a second of
    drift by the end, which is more than a syllable. The comparison therefore
    searches over both a time scale and an offset and keeps the best correlation.
    That is the difference between "these two look alike" and "this is that
    sentence again".
*/
inline float similarity (const Phrase& a, const Phrase& b) noexcept
{
    if (! a.printValid || ! b.printValid) return 0.0f;

    constexpr int   B = Phrase::kBands;
    constexpr int   S = Phrase::kSlots;

    float best = -1.0f;

    // The search grid is deliberately small. Every extra alignment tried is
    // another chance for two unrelated phrases to line up by accident, and the
    // score keeps the best one — so a generous grid inflates every similarity,
    // not just the true ones.
    for (int si = 0; si < 5; ++si)
    {
        const float scale = 0.90f + 0.055f * (float) si;     // 0.90 … 1.12

        for (int off = -2; off <= 2; ++off)
        {
            double dot = 0.0, na = 0.0, nb = 0.0;
            int    used = 0;

            for (int s = 0; s < S; ++s)
            {
                if (! (a.slotMask & (1u << s))) continue;

                const float pos = (float) s * scale + (float) off;
                const int   p0  = (int) std::floor (pos);
                const int   p1  = p0 + 1;
                if (p0 < 0 || p1 >= S) continue;
                if (! (b.slotMask & (1u << p0)) || ! (b.slotMask & (1u << p1))) continue;

                const float frac = pos - (float) p0;
                ++used;

                // Compare the *change* from slot to slot, not the slots
                // themselves. Two phrases from one voice share a great deal of
                // static structure — the same formant ranges, the same onset and
                // tail shape — and correlating that rewards every pair equally.
                // The trajectory between slots is what a sentence actually is.
                const int sPrev = s - 1;
                if (sPrev < 0 || ! (a.slotMask & (1u << sPrev))) continue;

                const float posP = (float) sPrev * scale + (float) off;
                const int   q0 = (int) std::floor (posP), q1 = q0 + 1;
                if (q0 < 0 || q1 >= S) continue;
                if (! (b.slotMask & (1u << q0)) || ! (b.slotMask & (1u << q1))) continue;
                const float fracP = posP - (float) q0;

                for (int k = 0; k < B; ++k)
                {
                    const float va = a.print[s * B + k] - a.print[sPrev * B + k];
                    const float vb = (b.print[p0 * B + k] * (1.0f - frac)
                                    + b.print[p1 * B + k] * frac)
                                   - (b.print[q0 * B + k] * (1.0f - fracP)
                                    + b.print[q1 * B + k] * fracP);
                    dot += (double) va * vb;
                    na  += (double) va * va;
                    nb  += (double) vb * vb;
                }
            }

            if (used >= 10 && na > 1.0e-9 && nb > 1.0e-9)
                best = std::max (best, (float) (dot / std::sqrt (na * nb)));
        }
    }

    return best < -1.0f ? 0.0f : best;
}

} // namespace tw
