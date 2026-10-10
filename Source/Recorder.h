/*
    Recorder.h — wejscie dzwieku, nieprzerwany zapis i TakeWatch na zywo.

    Zasada nadrzedna calego programu: NAGRYWANIE SIE NIE ZATRZYMUJE.
    Zapis leci na dysk bez pauz, a "cofniecie o wers" bedzie przesuwac tylko
    prompter i klasc znacznik — nigdy nie zamykac pliku. Zatrzymywanie i
    wznawianie gubi probki na stykach, karta lubi kliknac przy restarcie, a
    nagranie rozpada sie na kawalki, ktorych potem nie da sie porownac.

    Analiza (TakeWatch) chodzi na TYM SAMYM sygnale, ktory idzie na dysk —
    przed jakimkolwiek przetwarzaniem. To warunek, zeby profil glosu uczyl sie
    mikrofonu, a nie czyjegos algorytmu.
*/

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <memory>
#include "DSP/RealtimeSlot.h"
#include "DiskWriter.h"

#include "DSP/TakeWatchCore.h"
#include "DSP/Setup.h"
#include "DSP/SyllableClock.h"
#include "DSP/TakeReview.h"

/** JUCE robi juce::String z const char* przez CharPointer_ASCII — bajt na znak.
    Literaly sa w UTF-8, wiec bez tego "s" rozpada sie na dwa krzaki. */
static inline juce::String PL (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

//==============================================================================
class Recorder : public juce::AudioIODeviceCallback
{
public:
    Recorder() = default;
    ~Recorder() override { stop(); }

    // Control thread only. The callback sees an immutable session descriptor.
    juce::String start (const juce::File& folder)
    {
        stop();
        std::lock_guard<std::mutex> lock(sessionControl);
        const double rate = sampleRate.load();
        if (rate <= 0) return PL("Brak urządzenia dźwiękowego.");
        if (!folder.createDirectory()) return PL("Nie mogę utworzyć folderu nagrania.");
        const auto stamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
        auto file = folder.getNonexistentChildFile("sesja_" + stamp, ".wav", false);
        std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
        if (!stream || stream->failedToOpen()) return PL("Nie mogę utworzyć pliku ") + file.getFullPathName();
        auto session = std::make_unique<Session>();
        session->mono = !bothChannelsLive.load();
        session->channels = session->mono ? 1 : juce::jlimit(1,2,recordChannels.load());
        session->channel = publishedChannel.load();
        session->rate = rate;
        session->id = ++nextSessionId;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wav.createWriterFor(stream.get(),rate,(unsigned)session->channels,32,{},0));
        if (!writer) return PL("Nie mogę utworzyć zapisu WAV.");
        stream.release();
        session->writer = std::make_unique<DiskWriter>(std::move(writer));
        sessionFile = file;
        samplesWritten.store(0); lostSamples.store(0); firstLost.store(-1);
        writeFailed.store(false); recordingRate.store(rate);
        currentSessionId.store(session->id);
        activeSession = std::move(session);
        recording.store(true);
        sessionSlot.set(activeSession.get());
        return {};
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock(sessionControl);
        sessionSlot.set(nullptr); // drains the last reader before closing its file
        recording.store(false);
        if(activeSession) {
            activeSession->writer->finish();
            if(activeSession->writer->hasFailed()) writeFailed.store(true);
            activeSession.reset();
            if(recordingDamaged()) sessionFile.getSiblingFile(sessionFile.getFileName()+".problem.txt")
                .replaceWithText(PL("UWAGA: podczas zapisu utracono dane. Nagraj materiał ponownie.\n")
                    + "Pierwszy wykryty problem: " + juce::String(firstLossSeconds(),3) + " s\n");
        }
    }

    bool recordingDamaged() const noexcept { return writeFailed.load() || lostSamples.load()>0; }
    double firstLossSeconds() const noexcept {
        return firstLost.load()>=0 && recordingRate.load()>0 ? firstLost.load()/recordingRate.load() : 0;
    }

    bool isRecording() const noexcept        { return recording.load(); }
    juce::File currentFile() const           { return sessionFile; }
    double recordedSeconds() const noexcept
    {
        return recordingRate.load() > 0.0 ? (double) samplesWritten.load() / recordingRate.load() : 0.0;
    }

    float inputLevel() const noexcept        { return level.load(); }
    double currentSampleRate() const noexcept { return sampleRate; }

    //==========================================================================
    /** Podpina kreator. Dopoki jest podpiety, probki ida do niego zamiast do
        zapisu — kreator dziala przed nagrywaniem, nie w jego trakcie. */
    void attachWizard (tw::SetupWizard* w)
    {
        wizardSlot.set(w);
    }

    /** Licznik sylab dla promptera. Dostaje ten sam kanal, co lampka —
        czyli ten, na ktorym naprawde jest glos. */
    void attachSyllableClock (tw::SyllableClock* c)
    {
        syllableSlot.set(c);
    }

    float currentLevel() const noexcept { return level.load(); }

    /** Ton probny na wyjscie — do sprawdzenia przesluchu ze sluchawek. */
    void setTestTone (bool on) { toneOn = on; }

    /** Ktory kanal wejsciowy niesie glos (liczac od 1) — do pokazania czlowiekowi. */
    int   inputInUse()     const noexcept { return publishedChannel.load() + 1; }
    bool  usingBothInputs() const noexcept { return bothChannelsLive.load(); }

    /** Material do kalibracji odsluchu: wlasny glos czlowieka, w petli.

        Przekazywany z watku interfejsu przed rozpoczeciem kroku. Podmiana idzie
        pod blokada, bo watek dzwieku czyta ten sam wektor — a wektor podmieniany
        w trakcie odczytu to trzask albo gorzej. */
    void setCalibrationAudio (std::vector<float> audio)
    {
        calibrationSlot.set(nullptr);
        calibration.audio = std::move(audio);
        calibration.pos = 0;
        calibrationSlot.set(&calibration);
    }

    /** Wlacza odtwarzanie materialu kalibracyjnego. */
    void setCalibrationPlayback (bool on) { calibOn = on; }

    /** Control thread: odpiecie czeka na ostatni odczyt, audio nigdy nie czeka. */
    void attachPlayer (zs::Audition* p) { playerSlot.set(p); }

    /** Odbior zdarzen w watku interfejsu. */
    bool nextEvent (tw::Event& e) noexcept {
        TaggedEvent item;
        while(uiEvents.pop(item)) {
            if(recording.load() && item.session != currentSessionId.load()) continue;
            e=item.event;return true;
        }
        return false;
    }
    int   phrases()         const noexcept   { return publishedPhrases.load(); }
    int   analysisCh()      const noexcept   { return publishedChannel.load(); }
    bool  profileReady()    const noexcept   { return publishedReady.load(); }
    float profileProgress() const noexcept   { return publishedProgress.load(); }

    //==========================================================================
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        prepareDevice(device->getCurrentSampleRate(),device->getCurrentBufferSizeSamples(),
                      juce::jmax(1,device->getActiveInputChannels().countNumberOfSetBits()));
    }

    // Call only while device callbacks are stopped.
    void prepareDevice(double rate,int samples,int channels)
    {
        sampleRate = rate; blockSize = samples; recordChannels = channels;

        watch.prepare (sampleRate.load(), blockSize);
        audioSessionId = 0;
        analysisChannel = 0;
        std::fill(std::begin(chEnv),std::end(chEnv),0.0f);
        publishedChannel.store(0);
        elapsedSamples = 0;
        level = 0.0f;

        // Z zapasem, bo niektore sterowniki daja czasem blok wiekszy od
        // zadeklarowanego. W watku dzwieku nie ma jak dolozyc pamieci, a
        // czytanie za koniec tablicy to nie bledny dzwiek, tylko awaria.
        playBuf.assign ((size_t) juce::jmax (1024, blockSize * 4), 0.0f);
    }

    void audioDeviceStopped() override
    {
        stop();
        sampleRate = 0.0;
    }

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        zs::RealtimeSlot<Session>::Read sessionRead(sessionSlot);
        Session* session = sessionRead.get();
        zs::RealtimeSlot<tw::SetupWizard>::Read wizardRead(wizardSlot);
        auto* wizard = wizardRead.get();
        if(session && audioSessionId != session->id) {
            audioSessionId = session->id;
            elapsedSamples = 0;
            watch.reset();
            analysisChannel = session->channel;
        }
        // ---- poziom wejscia, kanal po kanale --------------------------------
        // Usrednianie kanalow bylo bledem: mikrofon siedzi zwykle na JEDNYM
        // wejsciu interfejsu, a cisza z drugiego zabierala 6 dB. Do analizy
        // idzie ten kanal, na ktorym cos jest.
        float peak = 0.0f;

        // ---- ktory kanal niesie glos ----------------------------------------
        //
        // Interfejs z dwoma wejsciami oddaje dwa kanaly zawsze — takze wtedy,
        // gdy mikrofon jest wpiety tylko w jedno. Drugi kanal niesie wtedy szum
        // otwartego wejscia, i to nie jest cisza: to jest szum, ktory po
        // zsumowaniu do mono podnosi tlo o kilka decybeli, a w odsluchu
        // stereofonicznym siedzi w calosci w jednym uchu.
        //
        // Czlowiek zglosil to na 0.1.7: "moje nagranie slychac tylko w lewym
        // kanale a w prawym jest szum". Program od poczatku WIEDZIAL, ktory
        // kanal jest ktory — uzywal tego do lampki — ale do pliku szly oba,
        // surowo, w kolejnosci z urzadzenia.
        //
        // Decyzja zapada na wolnej obwiedni, a nie na pojedynczym bloku: przy
        // pojedynczym bloku cisza miedzy slowami przerzucalaby wybor na szum.
        // Z histereza 6 dB, zeby nie drgal na granicy. I zamraza sie na czas
        // nagrania, bo zmiana kanalu w polowie pliku to sklejka dwoch roznych
        // zrodel.
        for (int c = 0; c < juce::jmin (numInputChannels, maxTrackedChannels); ++c)
        {
            if (inputChannelData[c] == nullptr) continue;

            float chPeak = 0.0f;

            for (int n = 0; n < numSamples; ++n)
                chPeak = juce::jmax (chPeak, std::abs (inputChannelData[c][n]));

            // narastanie natychmiast, opadanie przez kilka sekund
            chEnv[c] = chPeak > chEnv[c] ? chPeak : chEnv[c] * 0.9985f + chPeak * 0.0015f;
            peak = juce::jmax (peak, chPeak);
        }

        if (session == nullptr)
        {
            int   best = 0;
            float bestEnv = 0.0f;

            for (int c = 0; c < juce::jmin (numInputChannels, maxTrackedChannels); ++c)
                if (chEnv[c] > bestEnv) { bestEnv = chEnv[c]; best = c; }

            const float current = chEnv[juce::jlimit (0, maxTrackedChannels - 1, analysisChannel)];

            // przelacz tylko wtedy, gdy inny kanal jest wyrazniej glosniejszy
            if (bestEnv > 1.0e-4f && bestEnv > current * 2.0f)
                analysisChannel = best;

            // czy oba kanaly niosa material — wtedy to sa dwa mikrofony, a nie
            // mikrofon i puste wejscie
            if (numInputChannels >= 2 && chEnv[0] > 1.0e-4f && chEnv[1] > 1.0e-4f)
            {
                const float lo = juce::jmin (chEnv[0], chEnv[1]);
                const float hi = juce::jmax (chEnv[0], chEnv[1]);
                bothChannelsLive.store (lo > hi * 0.5f);      // w granicach 6 dB
            }
            else
            {
                bothChannelsLive.store (false);
            }
        }

        publishedChannel.store(analysisChannel);

        // opadanie wolniejsze niz narastanie, zeby dalo sie to czytac okiem
        const float prev = level.load();
        level.store (peak > prev ? peak : prev * 0.82f + peak * 0.18f);

        // ---- analiza na surowym sygnale ------------------------------------
        const int ac = juce::jlimit (0, juce::jmax (0, numInputChannels - 1), analysisChannel);

        if (numInputChannels > 0 && inputChannelData[ac] != nullptr)
        {
            const double t = sampleRate.load() > 0.0 ? (double) elapsedSamples / sampleRate.load() : 0.0;
            const float* one[1] = { inputChannelData[ac] };
            watch.process (one, 1, numSamples, t);

            {
                zs::RealtimeSlot<tw::SyllableClock>::Read syllables(syllableSlot);
                if (syllables) syllables->process (inputChannelData[ac], numSamples);
            }
        }

        publishedPhrases.store(watch.numPhrases());
        publishedReady.store(watch.profileReady());
        publishedProgress.store(watch.profileProgress());
        tw::Event event;
        while(watch.nextEvent(event))
            uiEvents.push(TaggedEvent{event,session ? session->id : 0});

        // A failed queue write is permanent damage, never a successful sample.
        if(session) {
            bool valid = numInputChannels > ac && inputChannelData[ac] != nullptr;
            const float* channels[2] = {valid ? inputChannelData[ac] : nullptr,nullptr};
            if(!session->mono) {
                valid = numInputChannels >= session->channels;
                for(int c=0;c<session->channels;++c) {
                    channels[c]=valid ? inputChannelData[c] : nullptr;
                    valid = valid && channels[c]!=nullptr;
                }
            }
            if(valid && session->writer->write(channels,numSamples)) samplesWritten.fetch_add(numSamples);
            else {
                lostSamples.fetch_add(numSamples);
                juce::int64 unset=-1;firstLost.compare_exchange_strong(unset,elapsedSamples);
                writeFailed.store(true);
            }
            if(session->writer->hasFailed()) writeFailed.store(true);
        }
        elapsedSamples += numSamples;
        if(wizard && numInputChannels > ac && inputChannelData[ac])
            wizard->feed(inputChannelData[ac],numSamples);

        // ---- wyjscie: cisza albo ton probny ----------------------------------
        // Ton jest cichy i skladany z dwoch czestotliwosci, ktore nie sa swoimi
        // harmonicznymi — dzieki temu w pomiarze przesluchu nie da sie go
        // pomylic z czymkolwiek, co robi glos.
        const bool tone = toneOn.load();
        const bool cal  = calibOn.load();

        // ---- odsluch po nagraniu --------------------------------------------
        //
        // Idzie przed kalibracja i tonem, ale te trzy i tak nigdy nie wypadaja
        // naraz: kalibracja nalezy do kreatora, ton do pomiaru przesluchu, a
        // odsluch do ekranu po nagraniu.
        zs::RealtimeSlot<zs::Audition>::Read playerRead(playerSlot);
        if (auto* p = playerRead.get())
        {
            if (p->isPlaying() && numSamples <= (int) playBuf.size())
            {
                if (p->pull (playBuf.data(), numSamples))
                {
                    for (int c = 0; c < numOutputChannels; ++c)
                        if (outputChannelData[c] != nullptr)
                            juce::FloatVectorOperations::copy (outputChannelData[c],
                                                              playBuf.data(), numSamples);

                    tonePhase += numSamples;
                    return;
                }
            }
        }

        // ---- kalibracja odsluchu: wlasny glos czlowieka, w petli -------------
        //
        // Idzie przed tonem, bo oba nie moga grac naraz, a kalibracja jest
        // wazniejsza: ton sluzy do pomiaru przesluchu, kalibracja do ustawienia
        // punktu odniesienia dla wszystkiego, co program pozniej mowi.
        zs::RealtimeSlot<Calibration>::Read calibrationRead(calibrationSlot);
        if (cal && calibrationRead)
        {
            auto& calib = calibrationRead->audio;
            auto& calibPos = calibrationRead->pos;

            for (int c = 0; c < numOutputChannels; ++c)
            {
                if (outputChannelData[c] == nullptr) continue;

                if (calib.empty())
                {
                    juce::FloatVectorOperations::clear (outputChannelData[c], numSamples);
                    continue;
                }

                size_t pos = calibPos;

                for (int n = 0; n < numSamples; ++n)
                {
                    outputChannelData[c][n] = calib[pos];
                    if (++pos >= calib.size()) pos = 0;
                }
            }

            if (! calib.empty())
            {
                calibPos = (calibPos + (size_t) numSamples) % calib.size();

                if (wizard != nullptr && numOutputChannels > 0 && outputChannelData[0] != nullptr)
                    wizard->feedReference (outputChannelData[0], numSamples);
            }

            tonePhase += numSamples;
            return;
        }

        for (int c = 0; c < numOutputChannels; ++c)
        {
            if (outputChannelData[c] == nullptr)
                continue;

            if (! tone)
            {
                juce::FloatVectorOperations::clear (outputChannelData[c], numSamples);
                continue;
            }

            for (int n = 0; n < numSamples; ++n)
            {
                const double t = (double) (tonePhase + n) / sampleRate.load();
                outputChannelData[c][n] = (float) (0.25 * std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * t)
                                                 + 0.08 * std::sin (2.0 * juce::MathConstants<double>::pi * 653.0 * t));
            }
        }

        if (tone)
        {
            if (wizard != nullptr && numOutputChannels > 0 && outputChannelData[0] != nullptr)
                wizard->feedReference (outputChannelData[0], numSamples);
        }

        tonePhase += numSamples;
    }

private:
    tw::TakeWatch watch;

    struct Session {
        std::unique_ptr<DiskWriter> writer;
        bool mono=true;
        int channels=1, channel=0;
        double rate=0;
        uint64_t id=0;
    };
    struct Calibration { std::vector<float> audio; size_t pos=0; } calibration;
    struct TaggedEvent { tw::Event event; uint64_t session=0; };
    std::mutex sessionControl;
    std::unique_ptr<Session> activeSession;
    zs::RealtimeSlot<Session> sessionSlot;
    zs::RealtimeSlot<tw::SetupWizard> wizardSlot;
    zs::RealtimeSlot<tw::SyllableClock> syllableSlot;
    zs::RealtimeSlot<zs::Audition> playerSlot;
    zs::RealtimeSlot<Calibration> calibrationSlot;
    zs::SpscQueue<TaggedEvent,2048> uiEvents;
    uint64_t nextSessionId=0, audioSessionId=0;
    std::atomic<uint64_t> currentSessionId{0};
    juce::File sessionFile;
    std::atomic<bool> recording{false}, writeFailed{false};
    std::atomic<juce::int64> samplesWritten{0}, lostSamples{0}, firstLost{-1};
    std::atomic<double> sampleRate{0}, recordingRate{0};
    std::atomic<int> recordChannels{1}, publishedChannel{0}, publishedPhrases{0};
    std::atomic<bool> publishedReady{false};
    std::atomic<float> publishedProgress{0}, level{0};
    std::atomic<bool> toneOn{false}, calibOn{false}, bothChannelsLive{false};
    std::vector<float> playBuf;
    juce::int64 tonePhase=0, elapsedSamples=0;
    int blockSize=512, analysisChannel=0;
    static constexpr int maxTrackedChannels=8;
    float chEnv[maxTrackedChannels]{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Recorder)
};
