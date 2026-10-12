/*
    zs-symulacja.cpp — cala sesja nagraniowa bez czlowieka, z raportem.

    Druga polowa symulacji (pierwsza: Tools/zs-scenariusz.py, ktory robi
    nagranie i zapisuje PRAWDE: kiedy jaki wers, gdzie pomylka i R, gdzie
    wada sprzetu). Tu nagranie przechodzi przez TE SAME klasy co w programie:

      Recorder (zapis, TakeWatch, licznik sylab)  ->  lampka (LampLogic)
      prompter: Follower + PrompterLines + LineLog, R jak w PrompterComponent
      pre-roll po R (PreRoll.h), znaczki (LineGrade.h)
      po zatrzymaniu: wyciecie pomylek (PunchFile.h), ocena i miks (TakeReview.h)

    i porownuje wynik z prawda. Raport w Markdownie: liczby, a przy kazdej
    "dobrze / zle" z progiem, ktory jest wpisany obok.

        zs-symulacja KATALOG [raport.md]

    Czego NIE sprawdza: wygladu (zrzuty --preview-ui), rozpoznawania mowy
    (Vosk — tu prompter jedzie na samych sylabach, jak bez modelu) i tego,
    jak cos BRZMI. Mowa jest z syntezatora.

    Czas symulacji to czas nagrania: bloki ida przez Recorder jeden za drugim,
    bez czekania, a "klatka" interfejsu (30 na sekunde) liczy sie z probek.
*/

#include <juce_audio_utils/juce_audio_utils.h>

#include "Recorder.h"
#include "PunchFile.h"
#include "LampLogic.h"
#include "DSP/Follower.h"
#include "DSP/LineGrade.h"
#include "DSP/LineLog.h"
#include "DSP/PreRoll.h"
#include "DSP/PrompterLines.h"
#include "DSP/PrompterText.h"
#include "DSP/SyllableClock.h"
#include "DSP/TakeReview.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

//==============================================================================
struct Truth
{
    std::string kind;          // mowa, R, wada
    double from = 0, to = 0;
    int line = 0;              // wiersz tekstu (od 0)
    std::string what, extra;
};

static std::vector<Truth> readTruth (const juce::File& f)
{
    std::vector<Truth> out;
    std::istringstream in (f.loadFileAsString().toStdString());
    for (std::string l; std::getline (in, l);)
    {
        if (l.empty() || l[0] == '#') continue;
        std::vector<std::string> c;
        std::stringstream ss (l);
        for (std::string x; std::getline (ss, x, '\t');) c.push_back (x);
        while (c.size() < 6) c.push_back ({});
        Truth t;
        t.kind = c[0]; t.from = std::atof (c[1].c_str()); t.to = std::atof (c[2].c_str());
        t.line = std::atoi (c[3].c_str()); t.what = c[4]; t.extra = c[5];
        out.push_back (t);
    }
    return out;
}

static std::string f1 (double v) { char b[32]; std::snprintf (b, sizeof (b), "%.1f", v); return b; }
static std::string f2 (double v) { char b[32]; std::snprintf (b, sizeof (b), "%.2f", v); return b; }
static std::string pct (double a, double b) { return b > 0 ? f1 (100.0 * a / b) + "%" : "-"; }

static const char* kindName (tw::Kind k)
{
    switch (k)
    {
        case tw::Kind::DigitalSilence: return "brak sygnalu";
        case tw::Kind::Clipping:       return "przester";
        case tw::Kind::TooHot:         return "za goraco";
        case tw::Kind::Plosive:        return "plozja";
        case tw::Kind::MouthClick:     return "mlask";
        case tw::Kind::Breath:         return "oddech";
        case tw::Kind::Sibilance:      return "sybilanty";
        case tw::Kind::OffAxis:        return "odsuniecie";
        case tw::Kind::LevelDrift:     return "dryf poziomu";
        case tw::Kind::NoiseRise:      return "wzrost tla";
        case tw::Kind::Hum:            return "przydzwiek";
        case tw::Kind::FalseStart:     return "falstart";
        case tw::Kind::LowLevel:       return "za cicho";
    }
    return "?";
}

static bool kindMatches (const std::string& defect, tw::Kind k)
{
    if (defect == "przester") return k == tw::Kind::Clipping || k == tw::Kind::TooHot;
    if (defect == "plozja")   return k == tw::Kind::Plosive;
    if (defect == "brak")     return k == tw::Kind::DigitalSilence;
    if (defect == "mlaski")   return k == tw::Kind::MouthClick;
    return false;
}

static const char* gradeName (tw::LineGrade g)
{
    return g == tw::LineGrade::Good ? "ok" : g == tw::LineGrade::Fixable ? "!" : g == tw::LineGrade::Redo ? "X" : "-";
}

//==============================================================================
struct Report
{
    std::string md;
    int checks = 0, fails = 0;

    void line (const std::string& s = {}) { md += s + "\n"; }
    void check (bool ok, const std::string& what, const std::string& got, const std::string& want)
    {
        ++checks; if (! ok) ++fails;
        md += std::string ("| ") + (ok ? "✅" : "❌") + " | " + what + " | " + got + " | " + want + " |\n";
    }
};

//==============================================================================
/** Jeden przebieg: nagranie przez program, porownanie z prawda, rozdzialy raportu.
    ideal = false: prompter na samych sylabach (jak bez modelu mowy);
    ideal = true:  pozycja jak z rozpoznawania mowy (prawda + 0,78 s spoznienia),
                   zeby R, wyciecie i znaczki ocenic niezaleznie od sledzenia. */
static void pass (const std::vector<Truth>& truth, const std::string& text,
                  const juce::AudioBuffer<float>& input, double sr, int len,
                  const juce::File& dir, bool ideal, Report& rep)
{
    juce::WavAudioFormat wavFormat;
    // ---- prompter ----------------------------------------------------------------
    tw::PrompterText script;
    script.set (text);
    const auto spans = tw::lineSpans (script);   // jak w szerokim oknie
    const double lagSec = 0.78;
    auto firstWordOf = [&] (int line) { for (int i = 0; i < script.wordCount(); ++i) if (script.all()[(size_t) i].line == line) return i; return -1; };
    auto lastWordOf  = [&] (int line) { int r = -1; for (int i = 0; i < script.wordCount(); ++i) if (script.all()[(size_t) i].line == line) r = i; return r; };
    auto sourceLineOf = [&] (int shown)
    {
        return shown >= 0 && shown < (int) spans.size() ? script.all()[(size_t) spans[(size_t) shown].firstWord].line : -1;
    };
    auto markFor = [&] (int shown, double sec, tw::LineSource src)
    {
        tw::LineMark m; m.sec = sec; m.shown = shown; m.source = src;
        if (shown >= 0 && shown < (int) spans.size())
        { m.firstWord = spans[(size_t) shown].firstWord; m.lastWord = spans[(size_t) shown].lastWord; }
        return m;
    };

    // ---- nagrywanie ----------------------------------------------------------------
    const auto out = dir.getChildFile (ideal ? "wynik-mowa" : "wynik-sylaby");
    out.deleteRecursively();
    Recorder rec;
    const int block = 512;
    rec.prepareDevice (sr, block, 1);
    std::vector<float> in (block, 0.0f), outBuf (block, 0.0f);
    const float* inP[] = { in.data() };
    float* outP[] = { outBuf.data() };
    auto tick = [&] { rec.audioDeviceIOCallbackWithContext (inP, 1, outP, 1, block, {}); };

    for (int k = 0; k < (int) (sr / block); ++k) tick();      // karta chodzi przed nagraniem

    tw::SyllableClock clock;
    clock.prepare (sr);
    clock.reset();
    tw::Follower follower;
    tw::LineLog lineLog;
    zs::LampLogic lamp;
    rec.attachSyllableClock (&clock);
    follower.begin (&script);
    lineLog.begin();
    lamp.reset();

    const auto err = rec.start (out);
    if (err.isNotEmpty()) { rep.line ("Nagrywanie nie ruszylo: " + err.toStdString()); ++rep.fails; return; }

    std::vector<tw::Event> events;
    std::vector<tw::HeardWord> heard;
    struct Frame { double t; int sourceLine; int lamp; };
    std::vector<Frame> frames;
    struct Key { double t; tw::PreRollPlan plan; int line; };
    std::vector<Key> keys;
    std::vector<double> rTimes;
    for (const auto& t : truth) if (t.kind == "R") rTimes.push_back (t.from);
    size_t nextR = 0;

    const int frameLen = (int) (sr / 30.0);
    int sinceFrame = 0;

    for (int pos = 0; pos < len; pos += block)
    {
        const int n = std::min (block, len - pos);
        std::fill (in.begin(), in.end(), 0.0f);
        std::copy (input.getReadPointer (0) + pos, input.getReadPointer (0) + pos + n, in.begin());
        tick();

        sinceFrame += block;
        if (sinceFrame < frameLen) continue;
        sinceFrame -= frameLen;

        // ---- klatka interfejsu: to samo co PrompterComponent::timerCallback
        // ---- i MainComponent::timerCallback
        const double now = rec.recordedSeconds();
        follower.advance (now, clock.takeNewSyllables(), clock.pauseLength());

        // Przebieg "jak z rozpoznawaniem mowy": pozycja z prawdy, spozniona
        // o 0,78 s (mediana spoznienia Vosk, pomiar w CutPoint.h) — dokladnie
        // tak, jak PrompterComponent dostaje ja z WordTracker (syncTo).
        // W pauzach pozycja stoi, jak przy prawdziwym rozpoznawaniu.
        if (ideal)
            for (const auto& t : truth)
                if (t.kind == "mowa" && now - lagSec >= t.from && now - lagSec < t.to)
                {
                    const int a = firstWordOf (t.line), b = lastWordOf (t.line);
                    if (a < 0) break;
                    // pomylka: czlowiek doszedl najwyzej do 60% wersu (zs-scenariusz.py)
                    const double span = (t.what == "pomylka" ? 0.6 : 1.0) * (b - a + 1);
                    const double word = a + span * (now - lagSec - t.from) / (t.to - t.from);
                    follower.syncTo (tw::syllableAtWord (script, word));
                    // co "uslyszalo rozpoznawanie": slowo, ktore wlasnie sie skonczylo
                    const int w = (int) word - 1;
                    if (w >= a && (heard.empty() || heard.back().word != w)) heard.push_back ({ now, w });
                    break;
                }
        const int shown = tw::lineToShow (spans, script, follower.syllablePosition());
        lineLog.observe (markFor (shown, now, ideal ? tw::LineSource::Words : tw::LineSource::Syllables));

        tw::Event e;
        while (rec.nextEvent (e))
        {
            events.push_back (e);
            if ((int) e.severity >= 1) lamp.event (e.kind, (int) e.severity, "");
        }
        const auto view = lamp.tick (1.0 / 30.0, true, now, rec.inputLevel(), rec.profileReady(), rec.profileProgress());

        // R: PrompterComponent::repeatLine
        if (nextR < rTimes.size() && now >= rTimes[nextR])
        {
            ++nextR;
            const int from = tw::repeatLineStart (spans, follower.currentWord());
            follower.repeatFrom (from);
            const int line = tw::lineOfWord (spans, from);
            // jak PrompterComponent::repeatLine: wycina tylko z rozpoznawaniem mowy
            const auto why = tw::retakeSource (ideal);
            lineLog.restart (markFor (line, now, why), why);

            tw::PreRollPlan plan;
            if (why == tw::LineSource::Retake)
            {
                double startSec = 0.0;
                const auto hist = rec.recentAudio (30.0, startSec);
                plan = tw::PreRoll().plan (lineLog.marks(), hist, sr, startSec);
                lamp.afterCut();
            }
            keys.push_back ({ now, plan, sourceLineOf (line) });
        }

        frames.push_back ({ now, sourceLineOf (tw::lineToShow (spans, script, follower.syllablePosition())), view.state });
    }

    const auto raw = rec.currentFile();
    rec.stop();
    rec.attachSyllableClock (nullptr);

    // ---- po zatrzymaniu ------------------------------------------------------------
    auto marks = lineLog.marks();
    const auto grades = tw::LineGrader().grade (marks, events, true);
    // jak PrompterComponent::finishTake: tnie tylko PEWNE, reszta do wskazania
    const auto punched = zs::applyPunch (raw, marks, heard, true, tw::similarlyStartingLines (script, spans));
    const auto cleanEvents = punched.applied ? tw::Punch().remapEvents (events, punched.plan) : events;

    std::vector<float> clean;
    double cleanSr = sr;
    {
        std::unique_ptr<juce::AudioFormatReader> r (wavFormat.createReaderFor (raw.createInputStream().release(), true));
        if (r != nullptr)
        {
            cleanSr = r->sampleRate;
            juce::AudioBuffer<float> b ((int) r->numChannels, (int) r->lengthInSamples);
            r->read (&b, 0, b.getNumSamples(), 0, true, true);
            clean.assign (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
        }
    }
    zs::TakeReview review;
    review.setTake (clean, cleanSr, cleanEvents);
    review.run();


    rep.line (ideal ? "# Przebieg B: prompter jak z rozpoznawaniem mowy"
                    : "# Przebieg A: prompter na samych sylabach");
    rep.line();
    rep.line (ideal ? "Pozycja w tekście z prawdy, spóźniona o 0,78 s — tak jak z rozpoznawania mowy (Vosk) bez pomyłek. "
                      "Sprawdza R, wycięcie i znaczki niezależnie od jakości śledzenia."
                    : "Tak działa program bez modelu rozpoznawania mowy. Błędy śledzenia ciągną za sobą R, wycięcie i znaczki.");
    rep.line();
    {
        int during = 0, right = 0, nearRight = 0;
        for (const auto& fr : frames)
            for (const auto& t : truth)
                if (t.kind == "mowa" && t.what == "dobrze" && fr.t >= t.from + 0.5 && fr.t < t.to)
                {
                    ++during;
                    if (fr.sourceLine == t.line) ++right;
                    if (std::abs (fr.sourceLine - t.line) <= 1) ++nearRight;
                }
        rep.line ("### 1. Prompter (śledzenie wersu)");
        rep.line();
        rep.line ("Liczone co klatkę (1/30 s) w czasie dobrego czytania, od 0,5 s po początku wersu.");
        rep.line();
        rep.line ("| | co | wynik | próg |");
        rep.line ("|---|---|---|---|");
        rep.check (right >= 0.6 * during, "pokazuje czytany wiersz", pct (right, during), "≥ 60%");
        rep.check (nearRight >= 0.9 * during, "najwyżej wiersz obok", pct (nearRight, during), "≥ 90%");
        rep.line();
    }

    // ---- 2. lampka --------------------------------------------------------------------
    {
        rep.line ("### 2. Lampka i zdarzenia");
        rep.line();
        rep.line ("| | wada | czas | co zgłosił program | próg |");
        rep.line ("|---|---|---|---|---|");
        for (const auto& t : truth)
        {
            if (t.kind != "wada") continue;
            if (t.what == "mlaski")
            {
                // mlaskow lampka nie zglasza (waga "pozniej") — ma je usunac miks
                const int fixedClicks = review.whatIDid().clicksRepaired;
                rep.check (fixedClicks >= 3, t.what, f1 (t.from) + " s",
                           "miks naprawił trzasków: " + std::to_string (fixedClicks) + " | naprawione w miksie (≥ 3)");
                continue;
            }
            std::string got;
            bool hit = false;
            for (const auto& e : events)
                if (e.timeSec >= t.from - 0.5 && e.timeSec <= t.to + 1.5)
                {
                    got += std::string (got.empty() ? "" : ", ") + kindName (e.kind);
                    hit = hit || kindMatches (t.what, e.kind);
                }
            rep.check (hit, t.what, f1 (t.from) + " s", (got.empty() ? "nic" : got) + " | zgłoszone");
        }
        int falseAlarms = 0;
        std::string list;
        for (const auto& e : events)
        {
            if ((int) e.severity < 1) continue;
            // Zdarzenia frazy (dryf poziomu, odsuniecie, sybilanty) maja czas
            // POCZATKU frazy, nie wady — wiec wada "tlumaczy" zdarzenie, gdy
            // lezy w tym samym przeczytanym kawalku, a nie tylko obok.
            bool near = false;
            for (const auto& t : truth)
            {
                if ((t.kind == "wada" || (t.kind == "mowa" && t.what == "pomylka"))
                    && e.timeSec >= t.from - 0.5 && e.timeSec <= t.to + 1.5) near = true;
                if (t.kind == "wada")
                    for (const auto& m : truth)
                        if (m.kind == "mowa" && t.from >= m.from && t.from < m.to
                            && e.timeSec >= m.from - 0.5 && e.timeSec < m.to) near = true;
            }
            if (! near) { ++falseAlarms; list += std::string (list.empty() ? "" : ", ") + kindName (e.kind) + " " + f1 (e.timeSec) + " s"; }
        }
        rep.line();
        rep.line ("| | co | wynik | próg |");
        rep.line ("|---|---|---|---|");
        rep.check (falseAlarms <= 1, "fałszywe alarmy (lampka bez wady)", std::to_string (falseAlarms) + (list.empty() ? "" : " (" + list + ")"), "≤ 1");
        rep.line();
    }

    // ---- 3. R i pre-roll ----------------------------------------------------------------
    {
        rep.line ("### 3. Klawisz R i pre-roll");
        rep.line();
        rep.line ("| | R w | wers (prawda → program) | pre-roll kończy się | próg |");
        rep.line ("|---|---|---|---|---|");
        size_t k = 0;
        for (const auto& t : truth)
        {
            if (t.kind != "R" || k >= keys.size()) continue;
            const auto& key = keys[k++];
            // pauza przed pierwszym podejsciem do tego wersu
            double lineStart = -1, prevEnd = 0;
            for (const auto& s : truth)
                if (s.kind == "mowa")
                {
                    if (s.line == t.line) { lineStart = s.from; break; }
                    prevEnd = s.to;
                }
            if (! ideal)
            {
                rep.check (key.plan.audio.empty(), "R " + f1 (t.from) + " s", "pre-roll: " + std::string (key.plan.audio.empty() ? "nie gra" : "gra"),
                           "nie gra | bez modelu mowy R tylko cofa tekst");
                continue;
            }
            const bool lineOk = key.line == t.line;
            const bool inGap = ! key.plan.audio.empty() && key.plan.toSec >= prevEnd - 0.05 && key.plan.toSec <= lineStart + 0.02;
            rep.check (lineOk && inGap, "R " + f1 (t.from) + " s",
                       std::to_string (t.line + 1) + " → " + std::to_string (key.line + 1),
                       (key.plan.audio.empty() ? std::string ("brak pre-rollu") : f2 (key.plan.toSec) + " s (pauza " + f2 (prevEnd) + "–" + f2 (lineStart) + ")")
                       + " | ten sam wers, koniec w pauzie przed nim");
        }
        rep.line();
    }

    // ---- 4. wyciecie pomylek ------------------------------------------------------------
    {
        rep.line ("### 4. Wycięcie pomyłek (punch and roll)");
        rep.line();
        double mistake = 0, mistakeCut = 0, goodCut = 0, cutTotal = 0;
        auto overlap = [] (double a0, double a1, double b0, double b1) { return std::max (0.0, std::min (a1, b1) - std::max (a0, b0)); };
        for (const auto& c : punched.plan) cutTotal += c.to - c.from;
        for (const auto& t : truth)
        {
            if (t.kind != "mowa") continue;
            double cut = 0;
            for (const auto& c : punched.plan) cut += overlap (t.from, t.to, c.from, c.to);
            if (t.what == "pomylka") { mistake += t.to - t.from; mistakeCut += cut; }
            else goodCut += cut;
        }
        rep.line ("| | co | wynik | próg |");
        rep.line ("|---|---|---|---|");
        if (! ideal)
        {
            // bez modelu mowy R tylko cofa tekst (tw::retakeSource) — nic nie moze zniknac samo
            rep.check (! punched.applied && goodCut <= 0.0, "bez modelu mowy R niczego nie wycina",
                       punched.applied ? "wycięto " + f2 (cutTotal) + " s" : "nic", "nic | pomyłki zostają do Montażu");
        }
        else
        {
            rep.check (punched.applied, "plik po wycięciu powstał", punched.applied ? "tak, cięć: " + std::to_string (punched.cuts) : "nie: " + punched.error.toStdString(), "tak");
            // pomylka nie wycieta, ale pokazana czlowiekowi (do wskazania) tez jest w porzadku
            double flagged = 0;
            for (const auto& t : truth)
                if (t.kind == "mowa" && t.what == "pomylka")
                    for (const auto& sk : punched.skipped) flagged += overlap (t.from, t.to, sk.cut.from, sk.cut.to);
            rep.check (mistakeCut + flagged >= 0.95 * mistake, "pomyłki wycięte albo do wskazania",
                       f2 (mistakeCut) + " s wycięte + " + f2 (flagged) + " s do wskazania z " + f2 (mistake) + " s", "≥ 95%");
            for (const auto& sk : punched.skipped)
                rep.line ("| ℹ️ | do wskazania " + f2 (sk.cut.from) + "–" + f2 (sk.cut.to) + " s | " + sk.why + " | |");
            rep.check (goodCut <= 0.05, "wycięte z dobrego czytania", f2 (goodCut) + " s", "≤ 0,05 s");
            rep.check (punched.audibleSeams == 0, "słyszalne szwy (zmiana tła)", std::to_string (punched.audibleSeams), "0");
        }
        rep.line();
        rep.line ("Razem wycięto " + f2 (cutTotal) + " s (pomyłki + pauzy po R).");
        rep.line();
    }

    // ---- 5. znaczki ---------------------------------------------------------------------
    {
        rep.line ("### 5. Znaczki przy wersach");
        rep.line();
        rep.line ("| | wiersz | znaczek | oczekiwany | próg |");
        rep.line ("|---|---|---|---|---|");
        std::map<int, std::string> want;
        for (const auto& t : truth) if (t.kind == "mowa") want[t.line] = "ok";
        for (const auto& t : truth) if (t.kind == "wada") want[t.line] = t.extra;

        for (const auto& w : want)
        {
            // znaczek wiersza = najgorszy znaczek jego wersow promptera
            tw::LineGrade worst = tw::LineGrade::None;
            for (size_t s = 0; s < spans.size(); ++s)
                if (sourceLineOf ((int) s) == w.first)
                {
                    const auto it = grades.find (spans[s].firstWord);
                    if (it != grades.end() && it->second > worst) worst = it->second;
                }
            const std::string got = gradeName (worst);
            rep.check (got == w.second, "wiersz " + std::to_string (w.first + 1), got, w.second + " | zgodny");
        }
        rep.line();
    }

    // ---- 6. ocena i miks ---------------------------------------------------------------
    {
        rep.line ("### 6. Ocena po nagraniu i miks");
        rep.line();
        const auto& did = review.whatIDid();
        int unfixable = 0;
        for (const auto& t : truth) if (t.kind == "wada" && t.extra == "X") ++unfixable;
        rep.line ("| | co | wynik | próg |");
        rep.line ("|---|---|---|---|");
        rep.check (std::fabs (did.lufsAfter - (-16.0)) <= 1.0, "głośność po miksie", f1 (did.lufsAfter) + " LUFS", "−16 ± 1");
        rep.check (did.peakAfter <= -0.9, "szczyt po miksie", f1 (did.peakAfter) + " dBTP", "≤ −1");
        rep.check ((int) review.toRedo().size() >= 1 && (int) review.toRedo().size() <= unfixable,
                   "miejsca do powtórzenia", std::to_string (review.toRedo().size()), "1–" + std::to_string (unfixable) + " (wady nie do naprawy)");
        rep.line();
        rep.line ("Co program mówi na ekranie oceny:");
        rep.line();
        for (const auto& s : review.says()) rep.line ("> " + s);
        rep.line();
    }

    rep.line ("### Wszystkie zdarzenia z nagrania");
    rep.line();
    {
        std::string all;
        for (const auto& e : events)
            all += std::string (all.empty() ? "" : ", ") + kindName (e.kind) + " " + f1 (e.timeSec) + " s (" + std::to_string ((int) e.severity) + ")";
        rep.line (all.empty() ? "brak" : all);
        rep.line();
        rep.line ("(w nawiasie waga: 0 później, 1 powtórz, 2 ustawienia, 3 stop; lampka zgłasza od 1)");
        rep.line();
    }

}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::fprintf (stderr, "zs-symulacja KATALOG [raport.md]\n"); return 2; }

    const juce::File dir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const auto truth = readTruth (dir.getChildFile ("prawda.tsv"));
    const auto text  = dir.getChildFile ("tekst.txt").loadFileAsString().toStdString();

    // ---- nagranie wejsciowe ---------------------------------------------------
    juce::WavAudioFormat inFormat;
    std::unique_ptr<juce::AudioFormatReader> reader (
        inFormat.createReaderFor (dir.getChildFile ("sesja.wav").createInputStream().release(), true));
    if (reader == nullptr) { std::fprintf (stderr, "brak sesja.wav\n"); return 2; }
    const double sr = reader->sampleRate;
    const int len = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> input (1, len);
    reader->read (&input, 0, len, 0, true, false);
    reader.reset();

    Report rep;
    rep.line ("# Symulacja sesji — raport");
    rep.line();
    rep.line ("Nagranie: " + f1 ((double) len / sr) + " s, mowa z syntezatora (espeak-ng), scenariusz z `prawda.tsv`. "
              "Progi przy każdym wierszu — do dyskusji, nie z normy.");
    rep.line();

    pass (truth, text, input, sr, len, dir, false, rep);
    pass (truth, text, input, sr, len, dir, true,  rep);

    rep.line ("---");
    rep.line ("**Wynik: " + std::to_string (rep.checks - rep.fails) + " z " + std::to_string (rep.checks) + " sprawdzeń zgodnych z prawdą.**");

    const juce::File reportFile = argc > 2 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[2])
                                           : dir.getChildFile ("raport.md");
    reportFile.replaceWithText (juce::String::fromUTF8 (rep.md.c_str()), false, false, "\n");
    std::printf ("%s", rep.md.c_str());
    return 0;
}
