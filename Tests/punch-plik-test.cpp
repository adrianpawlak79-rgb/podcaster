/*
    punch-plik-test.cpp — punch and roll na prawdziwych plikach WAV.

    Zasady ciecia sprawdza punch-test (bez JUCE). Tu sprawdzamy to, co moze
    sie nie udac dopiero na dysku: czy surowe nagranie trafia do "surowe"
    nietkniete, czy czysty plik ma nazwe nagrania i da sie go przeczytac,
    czy nie zostaje plik tymczasowy, czy czasy wersow sa przeliczone.
*/

#include <juce_audio_formats/juce_audio_formats.h>
#include "PunchFile.h"

#include <cmath>
#include <iostream>
#include <random>

static int errors = 0, checks = 0;
static void ok (bool c, const juce::String& what) { ++checks; std::cout << (c ? "  OK    " : "  ZLE   ") << what << std::endl; if (! c) ++errors; }

int main()
{

    std::cout << "\npunch-plik-test\n\n";

    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("zs-punch-test");
    dir.deleteRecursively(); dir.createDirectory();
    const auto wav = dir.getChildFile ("sesja_test.wav");

    // ---- nagranie: wers 0, wers 1, pomylka w wersie 2 (300 Hz), R, wers 2, wers 3
    const double sr = 48000.0;
    std::vector<float> x;
    std::mt19937 rng (5); std::normal_distribution<float> n (0.0f, 1.0f);
    auto now = [&] { return (double) x.size() / sr; };
    auto silence = [&] (double s) { for (size_t i = 0, e = (size_t) (s * sr); i < e; ++i) x.push_back (0.001f * n (rng)); };
    auto syl = [&] (int k, double hz) {
        for (int j = 0; j < k; ++j) {
            const size_t len = (size_t) (0.15 * sr);
            for (size_t i = 0; i < len; ++i)
                x.push_back ((float) (0.3 * std::sin (2 * M_PI * hz * i / sr) * std::sin (M_PI * i / len)));
            silence (0.04);
        }
    };
    std::vector<tw::LineMark> marks;
    auto mk = [&] (int line, double sec, tw::LineSource s) { tw::LineMark m; m.firstWord = line * 6; m.lastWord = line * 6 + 5; m.shown = line; m.sec = sec; m.source = s; m.text = "wers " + std::to_string (line); marks.push_back (m); };

    silence (0.6);
    mk (0, now() + 0.78, tw::LineSource::Words); syl (6, 140); silence (0.4);
    mk (1, now() + 0.78, tw::LineSource::Words); syl (6, 140); silence (0.4);
    const double bad = now(); mk (2, now() + 0.78, tw::LineSource::Words); syl (3, 300);
    mk (2, now() + 0.05, tw::LineSource::Retake); silence (0.7);
    const double good = now(); syl (6, 140); silence (0.4);
    mk (3, now() + 0.78, tw::LineSource::Words); syl (6, 140); silence (0.6);

    {
        juce::WavAudioFormat f;
        std::unique_ptr<juce::AudioFormatWriter> w (f.createWriterFor (wav.createOutputStream().release(), sr, 1, 32, {}, 0));
        const float* p[1] = { x.data() };
        w->writeFromFloatArrays (p, 1, (int) x.size());
    }
    const auto rawBytes = wav.getSize();

    const double line1Raw = marks[1].sec, line3Raw = marks[4].sec;
    // co uslyszalo rozpoznawanie (z opoznieniem 0,58 s): pomylka 12-13, potem wers 2 od nowa
    const double lag = tw::Punch().heardLagSec;
    const std::vector<tw::HeardWord> heard { { bad + 0.2 + lag, 12 }, { bad + 0.4 + lag, 13 },
                                             { good + 0.1 + lag, 12 }, { good + 0.3 + lag, 13 }, { good + 0.6 + lag, 14 } };

    // najpierw straznik: gdyby po R czytano od razu wers 3, pomylka zostaje, plik nietkniety
    {
        auto copy = marks;
        const std::vector<tw::HeardWord> skipLine { heard[0], heard[1], { good + 0.1 + lag, 18 } };
        const auto noCut = zs::applyPunch (wav, copy, skipLine);
        ok (! noCut.applied && noCut.skipped.size() == 1 && wav.getSize() == rawBytes,
            "po R nie przeczytano wersu jeszcze raz: nic nie wyciete (" + juce::String::fromUTF8 (noCut.skipped.empty() ? "" : noCut.skipped[0].why.c_str()) + ")");
    }

    auto res = zs::applyPunch (wav, marks, heard);

    ok (res.applied && res.cuts == 1, "jedno R -> jedno wyciecie (" + juce::String (res.removedSec, 2) + " s)");
    ok (res.rawCopy.existsAsFile() && res.rawCopy.getParentDirectory().getFileName() == "surowe",
        "surowe nagranie w podfolderze 'surowe'");
    ok (res.rawCopy.getSize() == rawBytes, "surowe nagranie nietkniete (ten sam rozmiar)");
    ok (! dir.getChildFile ("sesja_test.czysty.tmp.wav").exists(), "nie zostal plik tymczasowy");
    ok (res.rawCopy.getSiblingFile ("sesja_test.wersy.txt").existsAsFile(), "obok surowego jego zapis wersow");

    juce::WavAudioFormat f;
    std::unique_ptr<juce::AudioFormatReader> r (f.createReaderFor (wav.createInputStream().release(), true));
    ok (r != nullptr, "czysty plik ma nazwe nagrania i da sie go przeczytac");
    if (r != nullptr)
    {
        const double cleanSec = (double) r->lengthInSamples / r->sampleRate;
        ok (std::fabs (cleanSec - ((double) x.size() / sr - res.removedSec - 0.02)) < 0.002,
            "czysty krotszy dokladnie o wyciecie (" + juce::String (cleanSec, 3) + " s)");
        ok (r->bitsPerSample == 32 && r->numChannels == 1, "ten sam format co surowe (32 bity, mono)");
    }

    // czasy wersow: wers 3 przesuniety o wyciecie, wers 1 bez zmian
    ok (std::fabs (marks[1].sec - line1Raw) < 1e-9, "wers przed wycieciem: czas bez zmian");
    ok (std::fabs (marks[4].sec - (line3Raw - res.removedSec - 0.02)) < 1e-6,
        "wers po wycieciu: czas mniejszy dokladnie o wyciecie (" + juce::String (line3Raw, 2) + " -> "
        + juce::String (marks[4].sec, 2) + " s)");
    (void) bad; (void) good;

    // ---- lista ciec dla ekranu Odsluch (bez wycinania od razu) --------------------
    {
        const auto copyWav = dir.getChildFile ("sesja_lista.wav");
        res.rawCopy.copyFileTo (copyWav);
        const auto before = copyWav.getSize();
        auto m2 = marks;   // czasy z czystego pliku nie pasuja — bierzemy z surowego zapisu
        auto rawMarks = tw::LineLog::parse (res.rawCopy.getSiblingFile (res.rawCopy.getFileNameWithoutExtension() + ".wersy.txt")
                                               .loadFileAsString().toStdString());
        const auto prop = zs::applyPunch (copyWav, rawMarks, heard, false);
        ok (! prop.applied && copyWav.getSize() == before && prop.proposals.size() == 1
            && prop.proposals[0].sure && prop.proposals[0].state == tw::CutProposal::State::Accepted,
            "bez wycinania: plik nietkniety, jedna pewna propozycja");
        ok (zs::writeCutList (copyWav, prop.proposals) && zs::readCutList (copyWav).size() == 1,
            "lista obok nagrania zapisana i odczytana");

        auto list = zs::readCutList (copyWav);
        list[0].state = tw::CutProposal::State::Rejected;
        ok (! zs::applyCutList (copyWav, list).applied && copyWav.getSize() == before, "przywrocone: nic nie wyciete");
        list[0].state = tw::CutProposal::State::Accepted;
        const auto done = zs::applyCutList (copyWav, list);
        ok (done.applied && copyWav.getSize() < before && ! zs::cutListFile (copyWav).existsAsFile()
            && done.rawCopy.getSiblingFile (done.rawCopy.getFileNameWithoutExtension() + ".ciecia.tsv").existsAsFile(),
            "zatwierdzone: wyciete, surowe i lista w 'surowe'");
        (void) m2;
    }

    // drugi raz to samo nagranie bez R: nic
    std::vector<tw::LineMark> plain { marks[0], marks[1] };
    plain[0].source = plain[1].source = tw::LineSource::Words;
    ok (! zs::applyPunch (wav, plain, heard).applied, "bez R plik zostaje, jak byl");

    dir.deleteRecursively();
    std::cout << "\n" << (errors == 0 ? "wszystko przeszlo" : "SA BLEDY") << " (" << checks << " sprawdzen)\n\n";
    return errors;
}
