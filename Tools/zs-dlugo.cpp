/*
    zs-dlugo.cpp — dluga sesja (domyslnie 60 min) bez czlowieka: pamiec, czas, plik.

    Plan przed 1.0 (rozmowa 13.10, punkt 3): czy godzina nagrania nie zapcha
    pamieci, nie spowolni promptera i czy po zatrzymaniu wyciecie, ocena i
    miks dadza rade. Wejscie: nagrania lektorow (katalogi z zs-prawdziwe.py)
    puszczone po kolei w kolko, tekst promptera = ich teksty sklejone, wiec
    prompter ma tyle slow, ile godzina czytania.

    Te same klasy co w programie: Recorder (zapis, pamiec 300 s, lampka),
    Follower + LineLog + PrompterLines, co klatke (30/s) to, co rysuje pasek
    z fala (recentAudio + EditedView), co 5 min pauza z wyborem wersu
    (PauseCut) i wznowienie, po zatrzymaniu applyPunch i ekran oceny
    (wczytanie, listenOffline, TakeReview::run = miks).

        zs-dlugo KATALOG_Z_NAGRANIAMI WYNIK_DIR [minuty] [raport.md]

    Liczby: czas jest procesora tej maszyny (nie Windowsa Adriana), pamiec to
    VmRSS / VmHWM z /proc — tylko Linux.
*/

#include <juce_audio_utils/juce_audio_utils.h>

#include "Recorder.h"
#include "PunchFile.h"
#include "DSP/EditedView.h"
#include "DSP/Follower.h"
#include "DSP/LineLog.h"
#include "DSP/PauseCut.h"
#include "DSP/PrompterLines.h"
#include "DSP/PrompterText.h"
#include "DSP/SyllableClock.h"
#include "DSP/TakeReview.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static double memMb (const char* key)
{
    std::ifstream f ("/proc/self/status");
    for (std::string l; std::getline (f, l);)
        if (l.rfind (key, 0) == 0) return std::atof (l.c_str() + std::strlen (key)) / 1024.0;
    return -1.0;
}

struct Clock
{
    using C = std::chrono::steady_clock;
    C::time_point t = C::now();
    double ms() const { return std::chrono::duration<double, std::milli> (C::now() - t).count(); }
};

struct Stat
{
    double sum = 0, max = 0; long n = 0; std::vector<double> all;
    void add (double v) { sum += v; max = std::max (max, v); ++n; all.push_back (v); }
    double avg() const { return n ? sum / (double) n : 0.0; }
    double p99()
    {
        if (all.empty()) return 0.0;
        std::sort (all.begin(), all.end());
        return all[(size_t) (0.99 * (double) (all.size() - 1))];
    }
};

static std::string f1 (double v) { char b[32]; std::snprintf (b, sizeof (b), "%.1f", v); return b; }
static std::string f2 (double v) { char b[32]; std::snprintf (b, sizeof (b), "%.2f", v); return b; }

struct Source { std::vector<float> audio; std::string text; };

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) { std::fprintf (stderr, "zs-dlugo KATALOG WYNIK_DIR [minuty] [raport.md]\n"); return 2; }
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const juce::File inDir = cwd.getChildFile (argv[1]), outDir = cwd.getChildFile (argv[2]);
    const double minutes = argc > 3 ? std::atof (argv[3]) : 60.0;
    const double sr = 48000.0;

    // ---- wejscie -------------------------------------------------------------------
    std::vector<Source> src;
    juce::WavAudioFormat wf;
    for (const auto& d : inDir.findChildFiles (juce::File::findDirectories, false))
    {
        const auto wav = d.getChildFile ("sesja.wav"), txt = d.getChildFile ("tekst.txt");
        if (! wav.existsAsFile() || ! txt.existsAsFile()) continue;
        std::unique_ptr<juce::AudioFormatReader> r (wf.createReaderFor (wav.createInputStream().release(), true));
        if (r == nullptr || r->sampleRate != sr) continue;
        juce::AudioBuffer<float> b (1, (int) r->lengthInSamples);
        r->read (&b, 0, b.getNumSamples(), 0, true, false);
        src.push_back ({ std::vector<float> (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples()),
                         txt.loadFileAsString().toStdString() });
    }
    if (src.empty()) { std::fprintf (stderr, "brak nagran w %s\n", argv[1]); return 2; }
    std::sort (src.begin(), src.end(), [] (const Source& a, const Source& b) { return a.text < b.text; });

    const long total = (long) (minutes * 60.0 * sr);
    std::string text;
    {
        long have = 0;
        for (size_t k = 0; have < total; k = (k + 1) % src.size())
        {
            text += src[k].text + "\n\n";
            have += (long) src[k].audio.size();
        }
    }

    tw::PrompterText script;
    Clock tScript;
    script.set (text);
    const auto spans = tw::lineSpans (script);
    const double scriptMs = tScript.ms();

    // ---- nagrywanie ----------------------------------------------------------------
    outDir.createDirectory();
    const auto out = outDir.getChildFile ("dluga");
    out.deleteRecursively();

    Recorder rec;
    const int block = 512;
    rec.prepareDevice (sr, block, 1);
    std::vector<float> in (block, 0.0f), outBuf (block, 0.0f);
    const float* inP[] = { in.data() };
    float* outP[] = { outBuf.data() };
    auto tick = [&] { rec.audioDeviceIOCallbackWithContext (inP, 1, outP, 1, block, {}); };
    for (int k = 0; k < (int) (sr / block); ++k) tick();

    tw::SyllableClock clock;
    clock.prepare (sr);
    clock.reset();
    tw::Follower follower;
    tw::LineLog lineLog;
    rec.attachSyllableClock (&clock);
    follower.begin (&script);
    lineLog.begin();
    auto markFor = [&] (int shown, double sec, tw::LineSource s)
    {
        tw::LineMark m; m.sec = sec; m.shown = shown; m.source = s;
        if (shown >= 0 && shown < (int) spans.size())
        { m.firstWord = spans[(size_t) shown].firstWord; m.lastWord = spans[(size_t) shown].lastWord; }
        return m;
    };

    const double memStart = memMb ("VmRSS:");
    const auto err = rec.start (out);
    if (err.isNotEmpty()) { std::fprintf (stderr, "start: %s\n", err.toRawUTF8()); return 1; }

    Stat cb, frame, pauseOp, fFollow, fWave;
    std::vector<std::pair<double, double>> memByMin;    // minuta, MB
    tw::EditedView::Spans removed;
    std::vector<tw::HeardWord> heard;
    size_t srcIdx = 0, srcPos = 0;
    const int frameLen = (int) (sr / 30.0);
    int sinceFrame = 0;
    double nextPause = 300.0, resumeIn = -1.0;
    int pauses = 0;
    double droppedSec = 0.0;
    int nextMin = 0;

    for (long pos = 0; pos < total; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            if (srcPos >= src[srcIdx].audio.size()) { srcPos = 0; srcIdx = (srcIdx + 1) % src.size(); }
            in[(size_t) i] = src[srcIdx].audio[srcPos++];
        }
        Clock tc;
        tick();
        cb.add (tc.ms());

        const double tIn = (double) pos / sr;
        if (resumeIn >= 0.0)
        {
            droppedSec += (double) block / sr;
            if (tIn >= resumeIn) { rec.resume(); resumeIn = -1.0; }
            continue;
        }

        sinceFrame += block;
        if (sinceFrame < frameLen) continue;
        sinceFrame -= frameLen;

        // ---- klatka: prompter i pasek z fala, jak PrompterComponent ----------------
        Clock tf;
        const double now = rec.recordedSeconds();
        follower.advance (now, clock.takeNewSyllables(), clock.pauseLength());
        const int shown = tw::lineToShow (spans, script, follower.syllablePosition());
        lineLog.observe (markFor (shown, now, tw::LineSource::Syllables));
        fFollow.add (tf.ms());
        Clock tw2;
        // co mniej wiecej 0,4 s "uslyszane" slowo (rosnie jak przy Vosk)
        if (heard.empty() || now - heard.back().sec > 0.4) heard.push_back ({ now, follower.currentWord() });
        double start = 0.0;
        const auto live = rec.recentAudio (10.0, start);
        const auto view = tw::EditedView::render (live, start, sr, now, removed, 10.0);
        fWave.add (tw2.ms());
        frame.add (tf.ms());
        (void) view;

        if ((int) (tIn / 60.0) >= nextMin)
        {
            memByMin.push_back ({ tIn / 60.0, memMb ("VmRSS:") });
            nextMin += 5;
        }

        // ---- co 5 min: pauza, klik w biezacy wers, wznowienie po 3 s ----------------
        if (now >= nextPause)
        {
            nextPause += 300.0;
            ++pauses;
            Clock tp;
            rec.pause();
            const double atSec = rec.recordedSeconds();
            double hs = 0.0;
            const auto hist = rec.recentAudio (300.0, hs);
            const auto cuts = tw::PauseCut::silences (hist, sr, hs);
            const int line = std::max (0, shown);
            const double cut = std::clamp (tw::PauseCut::forLine (lineLog.marks(), markFor (line, atSec, tw::LineSource::Retake),
                                                                  hist, sr, hs, heard, cuts), hs, atSec);
            lineLog.restart (markFor (line, atSec, tw::LineSource::Retake), tw::LineSource::Retake);
            lineLog.setLastCut (cut, true);
            lineLog.setLastCutTo (atSec);
            if (cut < atSec) removed.push_back ({ cut, atSec });
            follower.repeatFrom (spans[(size_t) line].firstWord);
            pauseOp.add (tp.ms());
            resumeIn = tIn + 3.0;
        }
    }

    const auto raw = rec.currentFile();
    const double recordedSec = rec.recordedSeconds();
    const double memEndRec = memMb ("VmRSS:"), hwmRec = memMb ("VmHWM:");
    rec.stop();
    rec.attachSyllableClock (nullptr);

    // ---- po zatrzymaniu: wyciecie jak PrompterComponent::finishTake -------------------
    auto marks = lineLog.marks();
    Clock tPunch;
    const auto punched = zs::applyPunch (raw, marks, heard, true, {});
    const double punchMs = tPunch.ms(), hwmPunch = memMb ("VmHWM:");
    double cutSec = 0.0;
    for (const auto& c : punched.plan) cutSec += c.to - c.from;

    // ---- ekran oceny: wczytanie, przesluchanie, miks (ReviewComponent::load) ----------
    Clock tLoad;
    std::vector<float> mono;
    double fileSec = 0.0;
    {
        std::unique_ptr<juce::AudioFormatReader> r (wf.createReaderFor (raw.createInputStream().release(), true));
        if (r != nullptr && zs::readMono (*r, (int) r->lengthInSamples, mono))     // jak ReviewComponent::load
            fileSec = (double) mono.size() / r->sampleRate;
    }
    const double loadMs = tLoad.ms();
    Clock tListen;
    auto events = zs::TakeReview::listenOffline (mono, sr);
    const double listenMs = tListen.ms();
    zs::TakeReview review;
    Clock tMix;
    review.setTake (std::move (mono), sr, std::move (events));
    review.run();
    const double mixMs = tMix.ms(), hwmEnd = memMb ("VmHWM:");
    const auto& did = review.whatIDid();

    // ---- raport --------------------------------------------------------------------
    std::string md;
    int checks = 0, fails = 0;
    auto line = [&] (const std::string& s = {}) { md += s + "\n"; };
    auto check = [&] (bool ok, const std::string& what, const std::string& got, const std::string& want)
    {
        ++checks; if (! ok) ++fails;
        md += std::string ("| ") + (ok ? "✅" : "❌") + " | " + what + " | " + got + " | " + want + " |\n";
    };
    const double blockMs = 1000.0 * block / sr;

    line ("# Długa sesja — " + f1 (minutes) + " min");
    line();
    line ("Wejście: " + std::to_string (src.size()) + " nagrań lektorów w kółko; prompter: "
          + std::to_string (script.wordCount()) + " słów, " + std::to_string (spans.size()) + " wersów. "
          + "Czas: procesor maszyny testowej (Linux), nie komputer Adriana — liczy się zapas, nie wartość.");
    line();
    line ("| | co | wynik | próg |");
    line ("|---|---|---|---|");
    check (cb.p99() < 0.25 * blockMs, "wątek audio: czas bloku 512 (99 percentyl)",
           f2 (cb.p99()) + " ms, najdłużej " + f2 (cb.max) + " ms", "< 25% z " + f2 (blockMs) + " ms");
    check (frame.p99() < 8.0, "klatka promptera i fali (99 percentyl)",
           f2 (frame.p99()) + " ms, śr. " + f2 (frame.avg()) + " (prompter " + f2 (fFollow.avg()) + ", fala " + f2 (fWave.avg())
           + "), najdłużej " + f2 (frame.max) + " ms", "< 8 ms (30 klatek/s)");
    check (pauseOp.max < 300.0, "pauza: pamięć 300 s + wybór miejsca", f1 (pauseOp.max) + " ms (" + std::to_string (pauses) + " pauz)", "< 300 ms");
    check (scriptMs < 500.0, "wczytanie tekstu promptera", f1 (scriptMs) + " ms", "< 500 ms");
    {
        const double first = memByMin.size() > 1 ? memByMin[1].second : memEndRec;
        check (memEndRec - first < 50.0, "pamięć w trakcie nagrania nie rośnie",
               f1 (first) + " → " + f1 (memEndRec) + " MB (od 5. min)", "przyrost < 50 MB");
    }
    check (std::fabs (recordedSec - (minutes * 60.0 - droppedSec)) < 0.1, "na pauzach nic się nie zapisało",
           f1 (recordedSec) + " s nagrania, pauzy " + f1 (droppedSec) + " s", "różnica < 0,1 s");
    // kazde ciecie zabiera jeszcze przenikanie (Punch::crossfadeSec, 20 ms)
    const double xfade = tw::Punch().crossfadeSec * (double) punched.plan.size();
    check (punched.applied && std::fabs (fileSec - (recordedSec - cutSec - xfade)) < 0.05, "plik po wycięciu",
           f2 (fileSec) + " s = " + f1 (recordedSec) + " − " + f1 (cutSec) + " s cięć − " + f2 (xfade) + " s przenikań", "zgodny co do 0,05 s");
    check (punchMs < 30000.0, "wycięcie po zatrzymaniu", f1 (punchMs / 1000.0) + " s", "< 30 s");
    check (loadMs + listenMs + mixMs < 0.2 * minutes * 60.0 * 1000.0, "ocena: wczytanie + przesłuchanie + miks",
           f1 (loadMs / 1000.0) + " + " + f1 (listenMs / 1000.0) + " + " + f1 (mixMs / 1000.0) + " s", "< 0,2 długości nagrania");
    const double target = review.whatIDecided().params.targetLufs;   // studyjny: -19 (mono, AES)
    check (std::fabs (did.lufsAfter - target) <= 1.0, "głośność po miksie", f1 (did.lufsAfter) + " LUFS", f1 (target) + " ± 1 (cel miksu)");
    check (hwmEnd < 3000.0, "szczyt pamięci całego przebiegu", f1 (hwmEnd) + " MB", "< 3 GB");
    line();
    line ("Pamięć (VmRSS) co 5 min: ");
    std::string m;
    for (const auto& p : memByMin) m += (m.empty() ? "" : ", ") + f1 (p.first) + " min " + f1 (p.second) + " MB";
    line (m);
    line();
    line ("Szczyt pamięci: start " + f1 (memStart) + " MB, po nagraniu " + f1 (hwmRec) + " MB, po wycięciu "
          + f1 (hwmPunch) + " MB, po miksie " + f1 (hwmEnd) + " MB.");
    line();
    line ("**Wynik: " + std::to_string (checks - fails) + " z " + std::to_string (checks) + ".**");

    const auto rep = argc > 4 ? cwd.getChildFile (argv[4]) : outDir.getChildFile ("raport-dlugo.md");
    rep.replaceWithText (juce::String::fromUTF8 (md.c_str()), false, false, "\n");
    std::printf ("%s", md.c_str());
    return fails == 0 ? 0 : 1;
}
