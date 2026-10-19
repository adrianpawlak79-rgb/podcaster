/*
    PunchFile.h — po zatrzymaniu: surowe nagranie + klawisz R -> czysty plik.

    Zasady ciecia sa w DSP/Punch.h (test: Tests/punch-test.cpp). Tu tylko pliki:

      1. czytamy surowe nagranie (wszystkie kanaly),
      2. szukamy pauz (CutPoint.h) i liczymy wyciecia z zapisu wersow,
      3. zapisujemy czysty plik do pliku TYMCZASOWEGO obok,
      4. surowe nagranie przenosimy do podfolderu "surowe",
      5. tymczasowy dostaje nazwe nagrania.

    Kolejnosc jest po to, zeby zadna awaria w polowie nie zostawila
    czlowieka bez nagrania: do kroku 4 surowe lezy tam, gdzie lezalo; po
    kroku 4 czysty juz istnieje. Gdy krok 5 sie nie uda, surowe wraca na
    miejsce.
*/

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "DSP/LineLog.h"
#include "DSP/Punch.h"
#include "DSP/CutList.h"
#include "DSP/PrompterLines.h"
#include "DSP/TakeJournal.h"
#include "ReadMono.h"

namespace zs
{

struct PunchResult
{
    bool        applied = false;
    int         cuts = 0;
    double      removedSec = 0.0;
    int         audibleSeams = 0;   ///< szwy, przy ktorych tlo rozni sie o > 1,5 dB
    juce::File  rawCopy;            ///< gdzie trafilo surowe nagranie
    std::vector<tw::Removal> plan;  ///< co wycieto (czas surowego nagrania)
    std::vector<tw::SkippedCut> skipped;   ///< R, po ktorych NIE wycielismy — i dlaczego
    std::vector<tw::CutProposal> proposals;  ///< cala lista dla ekranu Odsluch (pewne + do wskazania)
    juce::String error;
};

/** Wspolne: czytanie nagrania do pamieci. */
struct LoadedTake
{
    juce::AudioBuffer<float> buf;   ///< tylko przy wiecej niz jednym kanale
    std::vector<float> mono;
    int channels = 1;
    double sr = 0.0;
    int bits = 32;
};

inline bool loadTake (const juce::File& wav, LoadedTake& t, juce::String& error)
{
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatReader> reader (
        wavFormat.createReaderFor (wav.createInputStream().release(), true));
    if (reader == nullptr) { error = "nie moge odczytac nagrania"; return false; }

    const int ch = (int) reader->numChannels;
    const auto len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, std::numeric_limits<int>::max());
    t.sr = reader->sampleRate;
    t.bits = (int) reader->bitsPerSample > 0 ? (int) reader->bitsPerSample : 32;
    t.channels = ch;
    // mono kawalkami, bez drugiej kopii (ReadMono.h); bufor tylko przy wielu
    // kanalach — wtedy kazdy kanal jest ciety osobno
    if (! readMono (*reader, len, t.mono)) { error = "nie moge odczytac nagrania"; return false; }
    if (ch > 1)
    {
        t.buf.setSize (ch, len);
        if (! reader->read (&t.buf, 0, len, 0, true, true)) { error = "nie moge odczytac nagrania"; return false; }
    }
    return true;
}

/** Wyciecie podanych kawalkow z pliku: czysty na miejsce nagrania, surowe do
    podfolderu "surowe" (kolejnosc kroków — opis na gorze pliku). marks, gdy
    podane, zostaja przeliczone na czas czystego pliku. */
inline PunchResult cutFile (const juce::File& wav, const LoadedTake& t, const std::vector<tw::Removal>& plan,
                            std::vector<tw::LineMark>* marks)
{
    PunchResult res;
    if (plan.empty()) return res;
    tw::Punch punch;
    const int ch = t.channels;
    const double sr = t.sr;

    // ---- czysty dzwiek, kanal po kanale (mono: prosto z t.mono, bez kopii)
    std::vector<std::vector<float>> clean ((size_t) ch);
    if (ch == 1) clean[0] = punch.render (t.mono, sr, plan);
    else
        for (int c = 0; c < ch; ++c)
        {
            const int len = t.buf.getNumSamples();
            std::vector<float> in (t.buf.getReadPointer (c), t.buf.getReadPointer (c) + len);
            clean[(size_t) c] = punch.render (in, sr, plan);
        }

    // ---- 3. tymczasowy
    juce::WavAudioFormat wavFormat;
    const auto temp = wav.getSiblingFile (wav.getFileNameWithoutExtension() + ".czysty.tmp.wav");
    temp.deleteFile();
    {
        std::unique_ptr<juce::FileOutputStream> out (temp.createOutputStream());
        if (out == nullptr) { res.error = "nie moge zapisac czystego pliku"; return res; }

        std::unique_ptr<juce::AudioFormatWriter> w (wavFormat.createWriterFor (
            out.get(), sr, (unsigned) ch, t.bits, {}, 0));
        if (w == nullptr) { res.error = "nie moge zapisac czystego pliku"; temp.deleteFile(); return res; }
        out.release();

        std::vector<const float*> ptr;
        for (auto& c : clean) ptr.push_back (c.data());
        if (! w->writeFromFloatArrays (ptr.data(), ch, (int) clean[0].size()))
        { w.reset(); temp.deleteFile(); res.error = "nie moge zapisac czystego pliku"; return res; }
    }

    // ---- 4. surowe do podfolderu
    const auto rawDir = wav.getParentDirectory().getChildFile ("surowe");
    rawDir.createDirectory();
    res.rawCopy = rawDir.getNonexistentChildFile (wav.getFileNameWithoutExtension(), ".wav", false);
    if (! wav.moveFileTo (res.rawCopy)) { temp.deleteFile(); res.error = "nie moge przeniesc surowego nagrania"; return res; }

    // ---- 5. czysty na miejsce surowego
    if (! temp.moveFileTo (wav))
    {
        res.rawCopy.moveFileTo (wav);
        temp.deleteFile();
        res.error = "nie moge zapisac czystego pliku";
        return res;
    }

    if (marks != nullptr)
    {
        // zapis wersow surowego nagrania idzie obok niego — do porownania
        tw::LineLog rawLog;
        rawLog.begin();
        for (const auto& m : *marks) rawLog.restart (m, m.source);
        res.rawCopy.getSiblingFile (res.rawCopy.getFileNameWithoutExtension() + ".wersy.txt")
            .replaceWithText (juce::String::fromUTF8 (rawLog.toText().c_str()), false, false, "\n");

        for (auto& m : *marks) m.sec = punch.remap (m.sec, plan);
    }

    for (const auto& sm : punch.seams (t.mono, sr, plan))
        if (sm.audible) ++res.audibleSeams;

    res.applied = true;
    res.plan = plan;
    res.cuts = (int) plan.size();
    for (const auto& p : plan) res.removedSec += p.to - p.from;
    return res;
}

/** Lista ciec obok nagrania (DSP/CutList.h): sesja_<data>.ciecia.tsv. */
inline juce::File cutListFile (const juce::File& wav)
{
    return wav.getSiblingFile (wav.getFileNameWithoutExtension() + ".ciecia.tsv");
}

inline std::vector<tw::CutProposal> readCutList (const juce::File& wav)
{
    return tw::parseCutList (cutListFile (wav).loadFileAsString().toStdString());
}

inline bool writeCutList (const juce::File& wav, const std::vector<tw::CutProposal>& list)
{
    return cutListFile (wav).replaceWithText (juce::String::fromUTF8 (tw::cutListToText (list).c_str()), false, false, "\n");
}

/** Dla ekranu Odsluch: wytnij to, co na liscie zatwierdzone (stan "wytnij").
    Surowe nagranie idzie do "surowe" razem z lista (zeby mozna bylo wrocic);
    obok czystego pliku listy juz nie ma — jego czasy sa inne. */
inline PunchResult applyCutList (const juce::File& wav, const std::vector<tw::CutProposal>& list)
{
    PunchResult res;
    const auto cuts = tw::acceptedCuts (list);
    if (cuts.empty() || ! wav.existsAsFile()) return res;

    LoadedTake t;
    if (! loadTake (wav, t, res.error)) return res;
    res = cutFile (wav, t, cuts, nullptr);
    if (res.applied)
    {
        const auto listNow = cutListFile (wav);
        listNow.moveFileTo (res.rawCopy.getSiblingFile (res.rawCopy.getFileNameWithoutExtension() + ".ciecia.tsv"));
    }
    return res;
}

/** marks: zapis wersow tego nagrania; przy powodzeniu czasy zostaja
    przeliczone na czysty plik.
    heard: slowa uslyszane przez rozpoznawanie mowy (Punch::guard / review).
    reallyCut = false (Settings::punchAutoCut wylaczone): nic nie wycinamy,
    zwracamy tylko propozycje (res.proposals) do listy dla ekranu Odsluch. */
inline PunchResult applyPunch (const juce::File& wav, std::vector<tw::LineMark>& marks,
                               const std::vector<tw::HeardWord>& heard, bool reallyCut = true,
                               const std::vector<int>& ambiguousLines = {})
{
    PunchResult res;

    const bool anyRetake = std::any_of (marks.begin(), marks.end(),
                                        [] (const tw::LineMark& m) { return m.source == tw::LineSource::Retake; });
    if (! anyRetake || ! wav.existsAsFile()) return res;

    LoadedTake t;
    if (! loadTake (wav, t, res.error)) return res;

    tw::CutFinder finder;
    finder.analyse (t.mono.data(), t.mono.size(), t.sr);
    tw::Punch punch;

    std::vector<tw::SkippedCut> noStart;              // R bez ciszy przed wersem
    const auto verdicts = punch.review (punch.plan (marks, finder, &noStart), heard, &finder, ambiguousLines);
    auto proposals = tw::proposalsFrom (verdicts);
    for (const auto& n : noStart)                     // tez do wskazania: czlowiek pokaze, gdzie
    {
        tw::CutProposal p; p.from = n.cut.from; p.to = n.cut.to; p.why = n.why;
        proposals.push_back (p);
    }

    // tylko PEWNE tniemy sami; reszta — do wskazania przez czlowieka
    std::vector<tw::Removal> plan;
    for (const auto& v : verdicts)
    {
        if (v.sure) plan.push_back (v.cut);
        else res.skipped.push_back ({ v.cut, v.why });
    }
    for (const auto& n : noStart) res.skipped.push_back (n);

    if (! reallyCut || plan.empty())
    {
        res.plan = plan;
        res.proposals = proposals;
        return res;
    }

    auto cut = cutFile (wav, t, plan, &marks);
    cut.skipped = res.skipped;
    cut.proposals = proposals;
    return cut;
}

//==============================================================================
/** Dziennik nagrania z tekstem (DSP/TakeJournal.h): prompter pisze go w trakcie
    nagrywania, zwykle zatrzymanie go usuwa. Zostaje tylko po awarii. */
inline juce::File journalFile (const juce::File& wav)
{
    return wav.getSiblingFile (wav.getFileNameWithoutExtension() + ".dziennik.txt");
}

struct JournalRecovery
{
    bool found = false;          ///< byl dziennik
    int proposals = 0;           ///< miejsca po R na ekran Montaz
    juce::String lastLine;       ///< wers, na ktorym czytanie sie urwalo
    juce::String error;
};

/** Po awarii (audyt 14.10): z dziennika propozycje ciec po R (.ciecia.tsv —
    tak, jakby nagranie zatrzymano zwyczajnie z wylaczonym automatycznym
    cieciem) i zapis wersow. Dziennik zostaje jako .dziennik.odzyskany.txt,
    zeby nie przetwarzac go drugi raz. */
inline JournalRecovery recoverJournal (const juce::File& wav)
{
    JournalRecovery r;
    const auto jf = journalFile (wav);
    if (! jf.existsAsFile() || ! wav.existsAsFile()) return r;
    r.found = true;

    auto j = tw::TakeJournal::parse (jf.loadFileAsString().toStdString());
    if (const auto* last = j.lastLine()) r.lastLine = juce::String::fromUTF8 (last->text.c_str()).trim();

    if (j.anyRetake())
    {
        tw::PrompterText script;
        script.set (j.script);
        auto marks = j.marks;
        const auto res = applyPunch (wav, marks, j.heard, false,
                                     tw::similarlyStartingLines (script, tw::lineSpans (script)));
        r.error = res.error;
        if (! res.proposals.empty())
        {
            if (writeCutList (wav, res.proposals)) r.proposals = (int) res.proposals.size();
            else r.error = "nie moge zapisac listy ciec";
        }
    }

    tw::LineLog out;
    out.begin();
    for (const auto& m : j.marks) out.restart (m, m.source);
    wav.getSiblingFile (wav.getFileNameWithoutExtension() + ".wersy.txt")
       .replaceWithText ("# odzyskane z dziennika po awarii\n" + juce::String::fromUTF8 (out.toText().c_str()), false, false, "\n");

    jf.moveFileTo (wav.getSiblingFile (wav.getFileNameWithoutExtension() + ".dziennik.odzyskany.txt"));
    return r;
}

} // namespace zs
