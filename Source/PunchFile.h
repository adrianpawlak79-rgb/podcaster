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
    juce::String error;
};

/** marks: zapis wersow tego nagrania; przy powodzeniu czasy zostaja
    przeliczone na czysty plik. */
inline PunchResult applyPunch (const juce::File& wav, std::vector<tw::LineMark>& marks)
{
    PunchResult res;

    const bool anyRetake = std::any_of (marks.begin(), marks.end(),
                                        [] (const tw::LineMark& m) { return m.source == tw::LineSource::Retake; });
    if (! anyRetake || ! wav.existsAsFile()) return res;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatReader> reader (
        wavFormat.createReaderFor (wav.createInputStream().release(), true));
    if (reader == nullptr) { res.error = "nie moge odczytac nagrania"; return res; }

    const int ch = (int) reader->numChannels;
    const auto len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, std::numeric_limits<int>::max());
    const double sr = reader->sampleRate;
    const int bits = (int) reader->bitsPerSample;

    juce::AudioBuffer<float> buf (ch, len);
    if (! reader->read (&buf, 0, len, 0, true, true)) { res.error = "nie moge odczytac nagrania"; return res; }
    reader.reset();

    std::vector<float> mono ((size_t) len, 0.0f);
    for (int c = 0; c < ch; ++c)
        for (int i = 0; i < len; ++i) mono[(size_t) i] += buf.getSample (c, i) / (float) ch;

    tw::CutFinder finder;
    finder.analyse (mono.data(), mono.size(), sr);
    tw::Punch punch;
    const auto plan = punch.plan (marks, finder);
    if (plan.empty()) return res;

    // ---- czysty dzwiek, kanal po kanale
    std::vector<std::vector<float>> clean ((size_t) ch);
    for (int c = 0; c < ch; ++c)
    {
        std::vector<float> in (buf.getReadPointer (c), buf.getReadPointer (c) + len);
        clean[(size_t) c] = punch.render (in, sr, plan);
    }

    // ---- 3. tymczasowy
    const auto temp = wav.getSiblingFile (wav.getFileNameWithoutExtension() + ".czysty.tmp.wav");
    temp.deleteFile();
    {
        std::unique_ptr<juce::FileOutputStream> out (temp.createOutputStream());
        if (out == nullptr) { res.error = "nie moge zapisac czystego pliku"; return res; }

        std::unique_ptr<juce::AudioFormatWriter> w (wavFormat.createWriterFor (
            out.get(), sr, (unsigned) ch, bits > 0 ? bits : 32, {}, 0));
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

    // zapis wersow surowego nagrania idzie obok niego — do porownania
    tw::LineLog rawLog;
    rawLog.begin();
    for (const auto& m : marks) rawLog.restart (m, m.source);
    res.rawCopy.getSiblingFile (res.rawCopy.getFileNameWithoutExtension() + ".wersy.txt")
        .replaceWithText (juce::String::fromUTF8 (rawLog.toText().c_str()), false, false, "\n");

    for (auto& m : marks) m.sec = punch.remap (m.sec, plan);

    for (const auto& sm : punch.seams (mono, sr, plan))
        if (sm.audible) ++res.audibleSeams;

    res.applied = true;
    res.plan = plan;
    res.cuts = (int) plan.size();
    for (const auto& p : plan) res.removedSec += p.to - p.from;
    return res;
}

} // namespace zs
