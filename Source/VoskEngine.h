/*
    VoskEngine.h — rozpoznawanie mowy dla promptera.

    ---------------------------------------------------------------------------
    DLACZEGO WCZYTYWANE W TRAKCIE, A NIE LINKOWANE

    Vosk to wyjatek od zasady "tylko JUCE" (AGENTS.md): osobna biblioteka
    (libvosk.dll i trzy DLL MinGW) i model vosk-model-small-pl-0.22 obok
    programu, rozkladane przez instalator. Gdyby .exe byl z nia zlinkowany,
    bez tych plikow nie uruchomilby sie wcale — Windows pokazalby komunikat
    o brakujacej DLL, zanim program zdazylby cokolwiek powiedziec.

    Wczytywana w trakcie (juce::DynamicLibrary) jest zwyklym "jest albo nie
    ma". Nie ma — prompter jedzie na liczniku sylab, jak w 0.4.0, a czlowiek
    dostaje jedno zdanie, dlaczego.

    ---------------------------------------------------------------------------
    WATKI

    Watek dzwieku: tylko dopisuje probki do pierscienia (SpeechFeed.h).
    Watek tego pliku: zabiera probki, schodzi do 16 kHz, karmi Vosk, a jego
    hipotezy wklada do kolejki. Watek okna: odbiera hipotezy i przekazuje je
    do WordTracker. Vosk liczy wolno i nierowno — ani dzwiek, ani okno nigdy
    na niego nie czekaja.
*/

#pragma once

#include <juce_core/juce_core.h>
#include "DSP/SpeechFeed.h"
#include "ModelWords.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

class VoskEngine : private juce::Thread
{
public:
    enum class State { Loading, NoLibrary, NoModel, Ready, Failed };

    struct Hypothesis
    {
        bool final = false;
        std::vector<std::string> words;
        double audioSec = 0.0;   ///< do ktorej chwili dzwieku (od startListening) siega hipoteza
    };

    static constexpr const char* modelFolder = "vosk-model-small-pl-0.22";

    VoskEngine() : juce::Thread ("rozpoznawanie mowy")
    {
        // Surowe probki z karty: przy 48 kHz to 4 s zapasu, przy 96 kHz 2 s.
        ring.allocate (192000);
        startThread();                       // najpierw wczytanie modelu (1-2 s), w tle
    }

    ~VoskEngine() override
    {
        signalThreadShouldExit();
        notify();
        stopThread (4000);

        if (recognizer != nullptr) api.recognizerFree (recognizer);
        if (model != nullptr)      api.modelFree (model);
    }

    State state() const noexcept { return status.load(); }

    /** Jedno zdanie dla czlowieka, gdy rozpoznawania nie ma. */
    juce::String whyNot() const
    {
        switch (state())
        {
            case State::NoLibrary: return juce::String (juce::CharPointer_UTF8 (
                "Brak libvosk — prompter liczy sylaby. Zainstaluj program instalatorem, żeby szedł za słowami."));
            case State::NoModel:   return juce::String (juce::CharPointer_UTF8 (
                "Brak modelu mowy obok programu — prompter liczy sylaby."));
            case State::Failed:    return juce::String (juce::CharPointer_UTF8 (
                "Model mowy nie dał się wczytać — prompter liczy sylaby."));
            default:               return {};
        }
    }

    /** Zamienniki dla slow, ktorych model nie zna (ModelWords.h). Puste,
        dopoki model sie nie wczytal. */
    std::vector<std::pair<std::string, std::string>> substitutesFor (const std::vector<std::string>& vocabulary) const
    {
        if (state() != State::Ready) return {};
        return tw::substitutes (vocabulary, modelWords);    // modelWords nie zmienia sie po Ready
    }

    /** Dla watku dzwieku (przez Recorder::attachSpeechRing). */
    tw::SampleRing& samples() noexcept { return ring; }

    //==========================================================================
    /** Poczatek nagrania: nowy rozpoznawacz ze slownikiem tego skryptu. */
    void startListening (double inputRate, const std::vector<std::string>& vocabulary)
    {
        std::lock_guard<std::mutex> lock (control);

        pendingVocab = vocabulary;
        pendingRate = inputRate;
        restart = true;
        listening = true;
        notify();
    }

    void stopListening()
    {
        std::lock_guard<std::mutex> lock (control);
        listening = false;
    }

    /** Z watku okna. */
    bool nextHypothesis (Hypothesis& h)
    {
        std::lock_guard<std::mutex> lock (out);
        if (results.empty()) return false;
        h = std::move (results.front());
        results.pop_front();
        return true;
    }

private:
    //==========================================================================
    struct Api
    {
        void*       (*modelNew)        (const char*) = nullptr;
        void        (*modelFree)       (void*) = nullptr;
        void*       (*recognizerNewGrm)(void*, float, const char*) = nullptr;
        void        (*recognizerFree)  (void*) = nullptr;
        int         (*acceptWaveform)  (void*, const char*, int) = nullptr;
        const char* (*partialResult)   (void*) = nullptr;
        const char* (*result)          (void*) = nullptr;
        void        (*setLogLevel)     (int) = nullptr;
    };

    static juce::File besideProgram (const juce::String& name)
    {
        return juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                   .getParentDirectory().getChildFile (name);
    }

    bool loadLibrary()
    {
       #if JUCE_WINDOWS
        const juce::String name = "libvosk.dll";
       #else
        const juce::String name = "libvosk.so";
       #endif

        // ZS_VOSK_LIB / ZS_VOSK_MODEL: do testow i pomiarow, gdy pliki leza gdzie indziej
        auto path = juce::SystemStats::getEnvironmentVariable ("ZS_VOSK_LIB", besideProgram (name).getFullPathName());

        if (! juce::File (path).existsAsFile() || ! lib.open (path)) return false;

        auto get = [this] (auto& fn, const char* sym)
        {
            fn = reinterpret_cast<std::remove_reference_t<decltype (fn)>> (lib.getFunction (sym));
            return fn != nullptr;
        };

        return get (api.modelNew, "vosk_model_new") && get (api.modelFree, "vosk_model_free")
            && get (api.recognizerNewGrm, "vosk_recognizer_new_grm") && get (api.recognizerFree, "vosk_recognizer_free")
            && get (api.acceptWaveform, "vosk_recognizer_accept_waveform")
            && get (api.partialResult, "vosk_recognizer_partial_result")
            && get (api.result, "vosk_recognizer_result") && get (api.setLogLevel, "vosk_set_log_level");
    }

public:
    /** Slownik dla Vosk jako JSON w czystym UTF-8.

        NIE przez juce::JSON::toString: ten zapisuje polskie litery jako
        kody szesnastkowe (ukosnik, u, 0105), a Vosk ich nie rozkodowuje — kazde slowo z ogonkiem
        ("się", "świata", "choćbyśmy") po cichu wypadalo ze slownika. Na
        "Nic dwa razy" dawalo to 54 trafienia zamiast 104
        (Tests/vosk-silnik-test.cpp). Klucze slow (spokenKey) maja tylko
        litery i cyfry, wiec cudzyslowow ani ukosnikow nie ma czego
        uciekac — ale robimy to i tak. */
    static std::string grammarJson (const std::vector<std::string>& vocabulary)
    {
        std::string j = "[";
        for (const auto& w : vocabulary)
        {
            j += '"';
            for (char c : w) { if (c == '"' || c == '\\') j += '\\'; j += c; }
            j += "\",";
        }
        return j + "\"[unk]\"]";
    }

private:
    static std::vector<std::string> words (const char* json, const char* field)
    {
        std::vector<std::string> w;
        const auto v = juce::JSON::parse (juce::String (juce::CharPointer_UTF8 (json)));
        for (auto& s : juce::StringArray::fromTokens (v.getProperty (field, {}).toString(), " ", ""))
            if (s.isNotEmpty()) w.push_back (s.toStdString());
        return w;
    }

    void publish (bool final, std::vector<std::string> w)
    {
        std::lock_guard<std::mutex> lock (out);
        if (results.size() > 200) results.pop_front();   // okno stanelo — nie roslij bez konca
        results.push_back ({ final, std::move (w), fedSamples / tw::To16k::outRate });
    }

    //==========================================================================
    void run() override
    {
        // ---- wczytanie ------------------------------------------------------
        if (! loadLibrary()) { status = State::NoLibrary; return; }

        const auto modelDir = juce::SystemStats::getEnvironmentVariable (
            "ZS_VOSK_MODEL", besideProgram (modelFolder).getFullPathName());

        if (! juce::File (modelDir).isDirectory()) { status = State::NoModel; return; }

        api.setLogLevel (-1);
        model = api.modelNew (modelDir.toRawUTF8());
        if (model == nullptr) { status = State::Failed; return; }

        // lista slow modelu — po to, by dobrac zamienniki slow, ktorych nie zna
        modelWords = tw::readModelWords (juce::File (modelDir).getChildFile ("graph/Gr.fst")
                                             .getFullPathName().toStdString());

        status = State::Ready;

        // ---- praca ----------------------------------------------------------
        std::vector<float> raw (4096);
        std::vector<int16_t> pcm;
        std::string lastPartial;

        while (! threadShouldExit())
        {
            bool on;
            {
                std::lock_guard<std::mutex> lock (control);
                on = listening;

                if (restart)
                {
                    restart = false;
                    if (recognizer != nullptr) api.recognizerFree (recognizer);

                    recognizer = api.recognizerNewGrm (model, (float) tw::To16k::outRate,
                                                       grammarJson (pendingVocab).c_str());
                    down.prepare (pendingRate);
                    lastPartial.clear();
                    fedSamples = 0;

                    float drop[512];
                    while (ring.pop (drop, 512) > 0) {}      // to, co bylo przed nagraniem
                }
            }

            const size_t n = ring.pop (raw.data(), raw.size());

            if (n == 0 || ! on || recognizer == nullptr) { wait (20); continue; }

            pcm.clear();
            down.process (raw.data(), n, pcm);
            fedSamples += (double) pcm.size();

            if (api.acceptWaveform (recognizer, reinterpret_cast<const char*> (pcm.data()), (int) (pcm.size() * 2)))
            {
                publish (true, words (api.result (recognizer), "text"));
                lastPartial.clear();
            }
            else
            {
                const std::string p = api.partialResult (recognizer);
                if (p != lastPartial) { lastPartial = p; publish (false, words (p.c_str(), "partial")); }
            }
        }
    }

    juce::DynamicLibrary lib;
    Api api;
    void* model = nullptr;
    std::vector<std::string> modelWords;
    void* recognizer = nullptr;

    std::atomic<State> status { State::Loading };
    tw::SampleRing ring;
    tw::To16k down;

    std::mutex control, out;
    std::vector<std::string> pendingVocab;
    double pendingRate = 48000.0;
    double fedSamples = 0.0;           ///< tylko watek Vosk
    bool restart = false, listening = false;
    std::deque<Hypothesis> results;

    JUCE_DECLARE_NON_COPYABLE (VoskEngine)
};
