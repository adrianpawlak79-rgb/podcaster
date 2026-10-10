/*
    vosk-silnik-test.cpp — cala droga dzwieku do rozpoznanych slow, z prawdziwym
    Vosk: pierscien -> 16 kHz -> VoskEngine -> WordTracker.

    Potrzebuje JUCE (VoskEngine), libvosk i modelu, wiec nie jest w
    wszystko.sh; buduje sie przez CMake (cel VoskSilnikTest):

        ZS_VOSK_LIB=.../libvosk.so ZS_VOSK_MODEL=.../vosk-model-small-pl-0.22 \
            VoskSilnikTest skrypt.txt nagranie.wav

    Dzwiek idzie w kawalkach po 10 ms szybciej niz w czasie rzeczywistym,
    ale przez te same klasy, co w programie. Sprawdza: model sie wczytuje,
    zamienniki sie dobieraja, hipotezy przychodza i trafiaja w tekst.
*/

#include <juce_core/juce_core.h>
#include "VoskEngine.h"
#include "DSP/WordTracker.h"
#include "Wav.h"

#include <cstdio>
#include <thread>

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("VoskSilnikTest skrypt.txt nagranie.wav\n"); return 2; }


    tw::PrompterText text;
    text.set (juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])).loadFileAsString().toStdString());

    wavio::Audio a;
    if (! wavio::read (argv[2], a).empty()) { std::printf ("nie moge czytac %s\n", argv[2]); return 2; }
    const auto x = a.toMono();

    VoskEngine engine;
    for (int i = 0; i < 300 && engine.state() == VoskEngine::State::Loading; ++i)
        std::this_thread::sleep_for (std::chrono::milliseconds (50));

    int fails = 0;
    auto check = [&] (bool ok, const char* what, const juce::String& got)
    {
        std::printf ("  %-5s %-46s %s\n", ok ? "OK" : "ZLE", what, got.toRawUTF8());
        if (! ok) ++fails;
    };

    std::printf ("\nVosk w programie, cala droga\n============================\n\n");
    check (engine.state() == VoskEngine::State::Ready, "biblioteka i model wczytane", engine.whyNot());
    if (engine.state() != VoskEngine::State::Ready) return 1;

    tw::WordTracker tr;
    tr.begin (&text);
    const auto subs = engine.substitutesFor (tr.vocabulary());
    for (auto& s : subs) tr.addAlias (s.second, s.first);
    check (true, "zamienniki slow nieznanych modelowi", juce::String ((int) subs.size()));

    engine.startListening (a.sampleRate, tr.vocabulary());
    std::this_thread::sleep_for (std::chrono::milliseconds (100));

    // ZS_ZRZUT=plik.hip: hipotezy w formacie Tools/vosk-hipotezy.py, do zs-sledzenie
    std::unique_ptr<juce::FileOutputStream> dump;
    if (auto path = juce::SystemStats::getEnvironmentVariable ("ZS_ZRZUT", {}); path.isNotEmpty())
    {
        juce::File (path).deleteFile();
        dump = std::make_unique<juce::FileOutputStream> (juce::File (path));
    }
    auto write = [&] (const VoskEngine::Hypothesis& hh)
    {
        if (dump == nullptr) return;
        juce::String line = juce::String (hh.audioSec, 3) + (hh.final ? " F" : " P");
        for (auto& w : hh.words) line << " " << juce::String (juce::CharPointer_UTF8 (w.c_str()));
        *dump << line << "\n";
    };

    const int block = a.sampleRate / 100;
    int hyps = 0;
    for (size_t p = 0; p + (size_t) block <= x.size(); p += (size_t) block)
    {
        engine.samples().push (x.data() + p, block);

        // nie szybciej niz ok. 4x czas rzeczywisty — zeby pierscien nie gubil
        if ((p / (size_t) block) % 4 == 0) std::this_thread::sleep_for (std::chrono::milliseconds (2));

        VoskEngine::Hypothesis h;
        while (engine.nextHypothesis (h))
        {
            ++hyps;
            write (h);
            tr.hypothesis (h.audioSec, h.words);
            if (h.final) tr.endUtterance();
        }
    }

    std::this_thread::sleep_for (std::chrono::milliseconds (1500));
    VoskEngine::Hypothesis h;
    while (engine.nextHypothesis (h)) { ++hyps; write (h); tr.hypothesis (h.audioSec, h.words); if (h.final) tr.endUtterance(); }

    check (engine.samples().droppedSamples() == 0, "pierscien nic nie zgubil",
           juce::String ((juce::int64) engine.samples().droppedSamples()) + " probek");
    check (hyps > 50, "hipotezy przychodza", juce::String (hyps));
    check (tr.hitCount() > text.wordCount() / 3, "trafienia w tekst",
           juce::String (tr.hitCount()) + " trafien, ostatnie w slowie " + juce::String (tr.lastHitWord())
               + " z " + juce::String (text.wordCount()));
    check (tr.lastHitWord() > text.wordCount() * 3 / 4, "doszlo prawie do konca tekstu", juce::String (tr.lastHitWord()));

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
