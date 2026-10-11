/*
    ReviewComponent.h — ekran po nagraniu.

    ---------------------------------------------------------------------------
    CO JEST W PANELU BOCZNYM

    Jedno zdanie: czy to sie nadaje. Nie liczby, nie wykres — zdanie. Czlowiek,
    ktory wlasnie skonczyl mowic, potrzebuje najpierw wiedziec, czy moze
    odejsc od mikrofonu. Wszystko inne jest uzasadnieniem tej jednej
    odpowiedzi i stoi nizej w przewijanym panelu.

    ---------------------------------------------------------------------------
    OS CZASU

    Przebieg nagrania, a pod nim kolorowe kawalki: czerwone to rzeczy, ktorych
    miks nie naprawi, zolte i szare to te, ktore naprawil. Klikniecie w os
    przewija odsluch — bo jedyne sensowne pytanie przy takiej liscie to "jak to
    brzmi w tym miejscu", a nie "ile wynosi".

    Przebieg jest rysowany z PO albo z PRZED, zaleznie od tego, co wlasnie
    gra — zeby bylo widac, co kompresja zrobila z dynamika. To jest ta sama
    informacja, ktora w DAW-ie czyta sie z dwoch sciezek obok siebie.

    ---------------------------------------------------------------------------
    PRZED / PO

    Jeden przycisk i spacja. Pozycja jest wspolna, wiec przelaczenie w polowie
    frazy pokazuje te sama fraze w drugiej wersji. To jedyne pytanie na tym
    ekranie, na ktore liczby nie odpowiadaja, a ucho odpowiada w sekunde.

    ---------------------------------------------------------------------------
    DLACZEGO MIKS LICZY SIE W TLE

    Minuta mowy to kilka sekund pracy lancucha. Robione w watku interfejsu
    zamraza okno — a zamrozone okno nie wyglada na "licze", tylko na "zawiesil
    sie" i czlowiek zabija program. Wiec liczy watek roboczy, a ekran pokazuje
    pasek i pisze, na czym stoi.
*/

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "DSP/TakeReview.h"
#include "DSP/CutEdit.h"
#include "Look.h"
#include "ScreenFrame.h"
#include "StepBar.h"
#include "Recorder.h"
#include "Wav.h"
#include <limits>

//==============================================================================
namespace plr
{
    /** Nazwa rodzaju zdarzenia. Ta sama co na liscie w trakcie nagrywania —
        czlowiek ma rozpoznac na osi to, co widzial przed chwila. */
    inline juce::String kind (tw::Kind k)
    {
        switch (k)
        {
            case tw::Kind::DigitalSilence: return "cisza cyfrowa";
            case tw::Kind::Clipping:       return "przester";
            case tw::Kind::TooHot:         return PL ("za wysoki poziom");
            case tw::Kind::Plosive:        return PL("uderzenie powietrza");
            case tw::Kind::MouthClick:     return "mlask";
            case tw::Kind::Breath:         return "oddech";
            case tw::Kind::Sibilance:      return PL("ostre s lub sz");
            case tw::Kind::OffAxis:        return PL ("odsunięcie");
            case tw::Kind::LevelDrift:     return PL ("poziom odjeżdża");
            case tw::Kind::NoiseRise:      return PL ("szum tła");
            case tw::Kind::Hum:            return PL ("przydźwięk");
            case tw::Kind::FalseStart:     return PL ("zły start");
            case tw::Kind::LowLevel:       return "za cicho";
        }
        return "?";
    }

    inline juce::String clock (double s)
    {
        const int t = (int) s;
        return juce::String::formatted ("%d:%02d", t / 60, t % 60);
    }
}

//==============================================================================
class ReviewComponent : public juce::Component,
                        private juce::Timer,
                        private juce::Thread
{
public:
    using Ink = zs::Ink;

    /** Wolane, gdy czlowiek chce wrocic do nagrywania. */
    std::function<void()> onClose;

    /** Co kreator zmierzyl w pokoju. Ustawiac przed load(): miks szuka wtedy
        w nagraniu przydzwieku, ktory slyszal kreator, i porownuje tlo. */
    void setRoom (const mix::RoomCheck& r) { wish.room = r; }

    //==========================================================================
    explicit ReviewComponent (Recorder& r)
        : juce::Thread ("miks po nagraniu"), recorder (r)
    {
        setWantsKeyboardFocus (true);

        formats.registerBasicFormats();

        playButton.setButtonText (PL ("Odtwórz"));
        playButton.onClick = [this] { togglePlay(); };
        addAndMakeVisible (playButton);

        abButton.onClick = [this] { toggleAb(); };
        addAndMakeVisible (abButton);

        saveButton.setButtonText (PL ("Zapisz miks"));
        saveButton.onClick = [this] { save(); };
        addAndMakeVisible (saveButton);

        backButton.setButtonText (PL ("Wróć do nagrywania"));
        backButton.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (backButton);

        // Jeden przycisk glowny: zapis. Reszta drugorzedna (Look.h).
        zs::Look::setKind (saveButton, zs::ui::ButtonKind::Primary);

        // Zdania programu w polu tekstowym, a nie malowane recznie.
        //
        // Malowane konczyly sie tam, gdzie konczylo sie okno: przy dziewieciu
        // zdaniach i oknie 940x620 ostatnie dwa po prostu nie istnialy na
        // ekranie i nic tego nie zdradzalo. Pole przewija sie samo, a przy
        // okazji pozwala zaznaczyc i skopiowac liczbe — co jest potrzebne
        // dokladnie wtedy, gdy program pisze "zglos to".
        words.setMultiLine (true);
        words.setReadOnly (true);
        words.setScrollbarsShown (true);
        words.setCaretVisible (false);
        words.setFont (juce::Font (13.5f));
        words.setColour (juce::TextEditor::backgroundColourId, Ink::bg());
        words.setColour (juce::TextEditor::textColourId,       Ink::textNext());
        words.setColour (juce::TextEditor::outlineColourId,    juce::Colours::transparentBlack);
        words.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        words.setColour (juce::TextEditor::highlightColourId,  Ink::edge());
        addAndMakeVisible (words);
        assessment.setMultiLine (true);
        assessment.setReadOnly (true);
        assessment.setCaretVisible (false);
        assessment.setScrollbarsShown (true);
        assessment.setFont (juce::Font (zs::type::small));
        assessment.setColour (juce::TextEditor::backgroundColourId, Ink::bar());
        assessment.setColour (juce::TextEditor::textColourId, Ink::textNext());
        assessment.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        assessment.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (assessment);
        flow.setSteps ({ PL("Przygotowanie"), PL("Nagranie"), PL("Odsłuch"), PL("Montaż"), PL("Gotowe") });
        addAndMakeVisible (flow);

        editToggle.setButtonText(PL("Edytuj nagranie"));
        editToggle.onClick=[this] { editMode=!editMode;resized();repaint(); };
        detailsToggle.setButtonText(PL("Szczegóły miksu"));
        detailsToggle.onClick=[this] { detailsMode=!detailsMode;resized();repaint(); };
        nextButton.setButtonText(PL("Następne miejsce do poprawy"));
        nextButton.onClick=[this] { nextTrouble(); };
        for(auto* b:{&editToggle,&detailsToggle,&nextButton})addAndMakeVisible(b);
        markStart.setButtonText(PL("Początek cięcia"));
        markEnd.setButtonText(PL("Koniec cięcia"));
        cutButton.setButtonText(PL("Wytnij zaznaczenie"));
        undoButton.setButtonText(PL("Cofnij cięcie"));
        markStart.onClick=[this] { if(canEdit()) { player.pause(); cutFrom=player.positionSec(); updateSelection(); } };
        markEnd.onClick=[this] { if(canEdit()) { player.pause(); cutTo=player.positionSec(); updateSelection(); } };
        cutButton.onClick=[this] {
            if(!canEdit())return;
            std::vector<float> edited;
            if(!zs::cutRange(review.rawAudio(),sr,cutFrom,cutTo,edited)) {
                note=PL("Wskaż początek, potem późniejszy koniec cięcia na osi czasu.");repaint();return;
            }
            undoAudio=review.rawAudio();
            restartEdit(std::move(edited));
        };
        undoButton.onClick=[this] { if(canEdit() && !undoAudio.empty()) restartEdit(std::move(undoAudio)); };
        for(auto* b:{&markStart,&markEnd,&cutButton,&undoButton}) addAndMakeVisible(b);
        selection.setColour(juce::Label::textColourId,Ink::textNext());
        selection.setText(PL("Montaż: kliknij na osi czasu i oznacz granice cięcia. Oryginał pozostaje na dysku."),juce::dontSendNotification);
        addAndMakeVisible(selection);
        updateAbText();
        startTimerHz (30);
    }

    void setRecordingDamaged(bool bad) { damagedRecording=bad; }

    ~ReviewComponent() override
    {
        recorder.attachPlayer (nullptr);

        // Watek roboczy trzyma wskazniki do naszych wektorow, wiec musi
        // skonczyc, zanim cokolwiek zniknie. stopThread czeka; signalThreadShould
        // Exit sam nie wystarcza, bo lancuch nie sprawdza flagi w srodku
        // przebiegu.
        signalThreadShouldExit();
        waitForThreadToExit(-1);
        ready.store(false);
        player.clear();
    }

    //==========================================================================
    /** Wczytuje nagranie z pliku i zaczyna liczyc miks w tle.

        Z pliku, nie z pamieci. Nagranie i tak lezy juz na dysku — a wziecie go
        stamtad znaczy, ze ekran ocenia DOKLADNIE to, co czlowiek dostal, razem
        ze skutkami zapisu. Gdyby kiedys zapis psul probki, ten ekran to pokaze,
        zamiast przepuscic. */
    /** @param waitForIt  policz miks od razu, zanim ktokolwiek zobaczy ekran.

        Istnieje po to, zeby ten ekran dalo sie OBEJRZEC przy budowaniu. Pod
        Wine, na wirtualnym ekranie, okno maluje sie raz — pierwsze malowanie
        trafia na zrzut i na tym koniec; kazde pozniejsze repaint() nie daje
        juz nowej klatki (sprawdzone: trzy zrzuty nagrania trzyminutowego, co
        dwadziescia sekund, bajt w bajt identyczne, mimo ze etap miksu w tym
        czasie sie zmienil). Czyli wszystko, co pojawia sie PO pierwszej
        klatce, jest dla zrzutu niewidoczne.

        Wiec zamiast zgadywac, jak wyglada gotowy ekran: liczymy przed
        pokazaniem okna i pierwsza klatka jest juz ta wlasciwa. Program
        uruchamiany normalnie nigdy tedy nie idzie. */
    void load (const juce::File& wavFile, std::vector<tw::Event> events,
               bool watchedLive, bool waitForIt = false)
    {
        signalThreadShouldExit();
        waitForThreadToExit(-1);
        ready.store(false);
        player.clear();
        recorder.attachPlayer (nullptr);

        undoAudio.clear();cutFrom=cutTo=-1;editMode=detailsMode=false;
        words.clear(); assessment.clear(); resized();
        for (auto* b : { &playButton, &abButton, &saveButton, &editToggle, &nextButton,
                         &markStart, &markEnd, &cutButton, &undoButton }) b->setEnabled (false);
        file  = wavFile;
        saved = juce::File();
        note.clear();

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (wavFile));

        if (reader == nullptr)
        {
            note = PL ("Nie mogę odczytać ") + wavFile.getFileName() + ".";
            review.setTake ({}, 48000.0, {});
            repaint();
            return;
        }

        if(reader->lengthInSamples<=0 || reader->lengthInSamples>std::numeric_limits<int>::max()
           || reader->sampleRate<=0 || reader->numChannels==0) {
            note=PL("Plik jest pusty lub za duży do wczytania.");review.setTake({},48000,{});repaint();return;
        }
        const int n = (int) reader->lengthInSamples;

        juce::AudioBuffer<float> buf ((int) reader->numChannels, juce::jmax (1, n));
        if(!reader->read (&buf, 0, n, 0, true, true)) {
            note=PL("Nie udało się odczytać całego nagrania.");review.setTake({},48000,{});repaint();return;
        }

        std::vector<float> mono ((size_t) juce::jmax (0, n), 0.0f);

        // Nagrywamy mono (Recorder::writeMono), ale jesli kiedys bedzie inaczej,
        // suma z zapasem jest lepsza niz wziecie pierwszego kanalu i zdziwienie.
        for (int c = 0; c < buf.getNumChannels(); ++c)
        {
            const float* src = buf.getReadPointer (c);
            const float  g   = 1.0f / (float) buf.getNumChannels();

            for (int i = 0; i < n; ++i)
                mono[(size_t) i] += src[i] * g;
        }

        sr = reader->sampleRate;

        // Nagranie otwarte z dysku nie ma listy zdarzen — nikt nie patrzyl
        // wtedy na lampke. Wtedy przesluchujemy je tym samym wykrywaczem.
        //
        // Decyduje o tym, czy nagranie BYLO sledzone, a nie czy lista jest
        // pusta: czyste nagranie tez daje pusta liste, a przesluchiwanie go
        // drugi raz to kilka sekund czekania po kazdym udanym podejsciu.
        // File is authoritative: callback events may have been lost or delayed.
        (void)watchedLive;
        if (true)
        {
            events = zs::TakeReview::listenOffline (mono, sr);

            if (! events.empty())
                note = PL ("Nagranie wczytane z pliku — przesłuchałem je od nowa.");
        }

        review.setTake (std::move (mono), sr, std::move (events));

        beforeEnv.build (review.rawAudio(), sr, 1400);
        afterEnv = {};

        if (waitForIt)
        {
            review.run (wish);
            adoptResult();
        }
        else
        {
            startThread();
        }

        repaint();
    }

    bool hasTake() const { return review.stage() != zs::TakeReview::Stage::Empty; }

    void end()
    {
        player.pause();
        recorder.attachPlayer (nullptr);
    }

    void begin()
    {
        recorder.attachPlayer (&player);
        grabKeyboardFocus();
    }

    //==========================================================================
    bool keyPressed (const juce::KeyPress& k) override
    {
        const int c = k.getKeyCode();

        if (c == juce::KeyPress::spaceKey)     { togglePlay(); return true; }
        if (c == juce::KeyPress::tabKey)       { toggleAb();   return true; }
        if (c == juce::KeyPress::escapeKey)    { if (onClose) onClose(); return true; }

        if (c == juce::KeyPress::leftKey)  { player.seekSec (player.positionSec() - 2.0); return true; }
        if (c == juce::KeyPress::rightKey) { player.seekSec (player.positionSec() + 2.0); return true; }
        if (c == juce::KeyPress::homeKey)  { player.seekSec (0.0); return true; }

        // Skok do nastepnego miejsca, ktore wymaga powtorzenia. Gdy jest ich
        // kilka, przeklikanie listy jest tym, co czlowiek i tak zrobi — wiec
        // niech bedzie jeden klawisz, a nie polowanie myszka na paski.
        if (k.getTextCharacter() == 'n' || k.getTextCharacter() == 'N')
        {
            nextTrouble();
            return true;
        }

        return false;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto strip = waveArea();

        if (strip.contains (e.getPosition()) && review.seconds() > 0.0)
        {
            const double t = (e.position.x - strip.getX()) / juce::jmax (1, strip.getWidth())
                           * review.seconds();

            player.seekSec (t);

            if (! player.isPlaying())
                player.play();

            repaint();
        }
    }

    //==========================================================================
    void resized() override
    {
        namespace L = zs::ui::review;
        flow.setBounds (22, zs::ui::frame::header + 8, getWidth() - 44, zs::StepBar::preferredHeight);
        flow.setCurrent (editMode ? 3 : 2);
        auto main = zs::ScreenFrame::mainArea (getLocalBounds());
        main.removeFromTop (78 + waveHeight() + L::strip + 8);
        auto row = main.removeFromTop (L::buttons);
        playButton.setBounds (row.removeFromLeft (132)); row.removeFromLeft (10);
        abButton.setBounds (row.removeFromLeft (juce::jmin (240, row.getWidth())));
        main.removeFromTop (8);
        auto tools = main.removeFromTop (L::tools);
        detailsToggle.setBounds (tools.removeFromLeft (172));
        tools.removeFromLeft (12); feedbackArea = tools;
        main.removeFromTop (8);
        summaryArea = main;
        words.setVisible (detailsMode);
        words.setBounds (main);
        detailsToggle.setButtonText (detailsMode ? PL("Ukryj szczegóły") : PL("Szczegóły miksu"));

        auto side = zs::ScreenFrame::sidebar (getLocalBounds()).reduced (22, 24);
        side.removeFromTop (32 + 84 + 8);
        auto actions = side.removeFromBottom (168);
        backButton.setBounds (actions.removeFromBottom (36)); actions.removeFromBottom (8);
        saveButton.setBounds (actions.removeFromBottom (36)); actions.removeFromBottom (8);
        editToggle.setBounds (actions.removeFromBottom (36)); actions.removeFromBottom (8);
        nextButton.setBounds (actions.removeFromBottom (36));
        editToggle.setButtonText (editMode ? PL("Wróć do odsłuchu") : PL("Przejdź do montażu →"));
        nextButton.setButtonText (PL("Następne miejsce do powtórki"));
        assessment.setVisible (!editMode);
        assessment.setBounds (side);
        for (auto* b : { &markStart, &markEnd, &cutButton, &undoButton }) b->setVisible (editMode);
        selection.setVisible (editMode);
        if (editMode)
        {
            // Granice i cofanie pozostaja dostepne takze przy 960 x 640.
            auto first = side.removeFromTop (32);
            auto second = side.removeFromTop (32);
            const int width = (side.getWidth() - 6) / 2;
            markStart.setBounds (first.removeFromLeft (width)); first.removeFromLeft (6); markEnd.setBounds (first);
            cutButton.setBounds (second.removeFromLeft (width)); second.removeFromLeft (6); undoButton.setBounds (second);
            selection.setBounds (side);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Ink::bg());
        zs::ScreenFrame::paint (g, getLocalBounds(), file.getFileNameWithoutExtension(),
            PL("Spacja — odtwarzanie · Tab — przed / po · Esc — wróć"));
        auto side = zs::ScreenFrame::sidebar (getLocalBounds());
        g.setColour (Ink::bar()); g.fillRect (side);
        g.setColour (Ink::edge()); g.drawVerticalLine (side.getX(), (float) side.getY(), (float) side.getBottom());
        auto main = zs::ScreenFrame::mainArea (getLocalBounds());
        g.setColour (Ink::textDim()); g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
        g.drawText (editMode ? PL("04 / MONTAŻ") : PL("03 / ODSŁUCH"), main.removeFromTop (18), juce::Justification::centredLeft);
        g.setColour (Ink::textNow()); g.setFont (juce::Font (zs::type::title, juce::Font::bold));
        g.drawText (editMode ? PL("Edytuj nagranie") : PL("Posłuchaj nagrania"), main.removeFromTop (36), juce::Justification::centredLeft);
        g.setColour (Ink::textNext()); g.setFont (juce::Font (zs::type::body));
        g.drawText (PL("Kliknij przebieg, aby posłuchać miejsca. Porównaj dźwięk przed i po miksie."), main.removeFromTop (24), juce::Justification::centredLeft);
        drawWave (g, main.removeFromTop (waveHeight()));
        drawStrip (g, main.removeFromTop (zs::ui::review::strip));
        side = side.reduced (22, 24);
        g.setColour (Ink::textNow()); g.setFont (juce::Font (zs::type::heading, juce::Font::bold));
        g.drawText (PL("Ocena podejścia"), side.removeFromTop (32), juce::Justification::centredLeft);
        drawVerdict (g, side.removeFromTop (84));
        g.setColour (Ink::textNext()); g.setFont (juce::Font (zs::type::small));
        if (note.isNotEmpty()) g.drawFittedText (note, feedbackArea, juce::Justification::centredLeft, 2);
        if (!detailsMode && review.stage() == zs::TakeReview::Stage::Ready)
        {
            g.setColour (Ink::textNext()); g.setFont (juce::Font (zs::type::small));
            g.drawFittedText (PL("Miks został dobrany automatycznie. Odsłuchaj, zanim zapiszesz."),
                              summaryArea, juce::Justification::topLeft, 3);
        }
    }

private:
    bool canEdit() const { return review.stage()==zs::TakeReview::Stage::Ready && !isThreadRunning(); }
    void updateSelection() {
        selection.setText(PL("Cięcie: ")+(cutFrom>=0?juce::String(cutFrom,2)+" s":"—")+"  →  "+(cutTo>=0?juce::String(cutTo,2)+" s":"—"),juce::dontSendNotification);
        repaint();
    }
    void restartEdit(std::vector<float> audio) {
        player.pause();recorder.attachPlayer(nullptr);ready.store(false);player.clear();
        auto events=zs::TakeReview::listenOffline(audio,sr);
        review.setTake(std::move(audio),sr,std::move(events));
        beforeEnv.build(review.rawAudio(),sr,1400);afterEnv={};
        cutFrom=cutTo=-1;
        selection.setText(PL("Zmiana zastosowana. Miks liczy się ponownie; odsłuchaj miejsce cięcia."),juce::dontSendNotification);
        words.clear();assessment.clear();saved=juce::File();note=PL("Montaż zmienia kopię w pamięci. Zapisz miks, aby zachować wynik.");
        startThread();repaint();
    }
    //==========================================================================
    /** Rosnie z oknem — liczby i uzasadnienie w zs::ui::review (UiRules.h). */
    int waveHeight() const { return zs::ui::review::waveHeight (getHeight()); }

    juce::Rectangle<int> waveArea() const
    {
        namespace L = zs::ui::review;
        auto r = zs::ScreenFrame::mainArea (getLocalBounds());
        r.removeFromTop (78);
        return r.removeFromTop (waveHeight() + L::strip);
    }

    //==========================================================================
    /* Watek roboczy: cala praca lancucha.

       Po skonczeniu NIE dotyka interfejsu. Ekran i tak patrzy trzydziesci razy
       na sekunde, wiec wystarczy, ze stan zmieni sie w TakeReview — a kazde
       wolanie repaint() z obcego watku jest bledem, ktory ujawnia sie raz na
       kilkaset uruchomien. */
    void run() override
    {
        review.run (wish);

        if (threadShouldExit()) return;

        ready.store (true);
    }

    void adoptResult()
    {
        afterEnv.build (review.mixedAudio(), sr, 1400);

        const double playbackRate=recorder.currentSampleRate()>0?recorder.currentSampleRate():sr;
        if(std::abs(playbackRate-sr)>0.1) {
            mix::Resampler convert;convert.prepare((int)sr,(int)playbackRate,64);
            std::vector<float> before,after;
            convert.render(review.rawAudio(),before);convert.render(review.mixedAudio(),after);
            player.setMaterial(std::move(before),std::move(after),playbackRate,true);
        } else player.setMaterial (review.rawAudio(), review.mixedAudio(), sr, true);
        player.showAfter (true);

        recorder.attachPlayer (&player);

        juce::String all;

        for (const auto& line : review.says())
            all << PL (line.c_str()) << "\n\n";

        words.setText (all.trimEnd(), false);
        juce::String summary;
        if (!review.says().empty()) summary << PL(review.says().front().c_str()) << "\n\n";
        summary << PL("MIEJSCA DO POWTÓRKI\n");
        if (review.toRedo().empty()) summary << PL("Nie ma fragmentów do powtórzenia.\n\n");
        for (const auto& redo : review.toRedo())
            summary << plr::clock (redo.fromSec) << PL("–") << plr::clock (redo.toSec)
                    << "  " << plr::kind (redo.kind) << "\n";
        summary << PL("\nUZASADNIENIE\n");
        for (size_t i = 1; i < review.says().size(); ++i)
            summary << PL(review.says()[i].c_str()) << "\n\n";
        assessment.setText (summary, false);
        playButton.setEnabled (true); abButton.setEnabled (true);
        editToggle.setEnabled (true); saveButton.setEnabled (true);
        nextButton.setEnabled (!review.toRedo().empty());

        updateAbText();
        resized();
        repaint();
    }

    //==========================================================================
    void togglePlay()
    {
        if (review.stage() != zs::TakeReview::Stage::Ready) return;

        if (player.isPlaying()) player.pause();
        else                    player.play();

        playButton.setButtonText (player.isPlaying() ? PL ("Zatrzymaj") : PL ("Odtwórz"));
    }

    void toggleAb()
    {
        if (review.stage() != zs::TakeReview::Stage::Ready) return;

        player.toggle();
        updateAbText();
        repaint();
    }

    void updateAbText()
    {
        abButton.setButtonText (player.showingAfter() ? PL ("słychać: PO miksie")
                                                      : PL ("słychać: PRZED miksem"));

        // Bez osobnego koloru napisu: zielony "PO" i zolty "PRZED" robily z
        // jednego przycisku trzeci i czwarty rodzaj, a zielen i zolcien
        // znacza u nas "dobrze" i "uwaga". Stan mowi napis.
    }

    void nextTrouble()
    {
        const double now = player.positionSec();

        for (const auto& t : review.timeline())
            if (t.mustRedo && t.fromSec > now + 0.3)
            {
                player.seekSec (juce::jmax (0.0, t.redoFrom - 0.3));
                if (! player.isPlaying()) player.play();
                repaint();
                return;
            }

        // Nie ma nic dalej — wracamy do pierwszego, zeby klawisz zawsze cos
        // robil. Milczacy klawisz czlowiek uznaje za zepsuty.
        for (const auto& t : review.timeline())
            if (t.mustRedo)
            {
                player.seekSec (juce::jmax (0.0, t.redoFrom - 0.3));
                repaint();
                return;
            }
    }

    //==========================================================================
    /* Zapis.

       Obok nagrania, pod nazwa z koncowka "__miks". Nigdy NA nagraniu: material
       surowy jest jedyna rzecza, ktorej nie da sie odtworzyc, a miks mozna
       zrobic jeszcze raz w kazdej chwili. Nadpisanie oryginalu przez wlasny
       wynik to blad, ktory robi sie raz i pamieta lata.

       Zapisujemy przez wavio, czyli ten sam kod, ktory sprawdza test
       odsluch-test — a nie przez druga droge, ktorej nikt nie sprawdza. */
    void save()
    {
        if (review.stage() != zs::TakeReview::Stage::Ready) return;
        if (damagedRecording || !review.mixValid()) {
            note=PL("Nie zapisuję wersji do publikacji: nagranie lub miks wymaga sprawdzenia.");
            repaint();return;
        }

        auto target = file.getParentDirectory()
                          .getChildFile (file.getFileNameWithoutExtension() + "__miks.wav");

        // Nie nadpisujemy tez wlasnych starszych miksow bez pytania — kolejne
        // podejscie dostaje numer.
        int n = 2;

        while (target.existsAsFile())
            target = file.getParentDirectory()
                         .getChildFile (file.getFileNameWithoutExtension()
                                        + "__miks-" + juce::String (n++) + ".wav");

        wavio::Audio out;
        out.sampleRate = (int) sr;
        out.channels.push_back (review.mixedAudio());

        const auto err = wavio::write24 (target.getFullPathName().toStdString(), out);

        if (! err.empty())
        {
            note = PL ("Nie udało się zapisać: ") + juce::String (err);
            repaint();
            return;
        }

        saved = target;
        note  = PL ("Zapisane: ") + target.getFileName();
        repaint();
    }

    //==========================================================================
    void drawVerdict (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const auto st = review.stage();

        if (st == zs::TakeReview::Stage::Empty)
        {
            g.setColour (Ink::textDim());
            g.setFont (juce::Font (20.0f));
            g.drawFittedText (note.isNotEmpty() ? note : PL ("Nie ma czego oceniać."),
                             r, juce::Justification::topLeft, 4);
            return;
        }

        if (st != zs::TakeReview::Stage::Ready)
        {
            g.setColour (Ink::textNext());
            g.setFont (juce::Font (20.0f));
            g.drawText (st == zs::TakeReview::Stage::Listening
                            ? PL ("Słucham, co nagrałeś…")
                            : PL ("Miksuję…"),
                        r.removeFromTop (30), juce::Justification::centredLeft);

            auto bar = r.removeFromTop (6).withTrimmedRight (r.getWidth() / 3);

            g.setColour (Ink::panel());
            g.fillRect (bar);
            g.setColour (Ink::green());
            g.fillRect (bar.withWidth ((int) (bar.getWidth() * review.progress())));
            return;
        }

        const int v = damagedRecording ? 2 : review.verdict();

        auto c = v == 2 ? Ink::clip() : v == 1 ? Ink::yellow() : Ink::green();

        auto dot = juce::Rectangle<int> (16, 16).withCentre ({ r.getX() + 8, r.getY() + 16 });

        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (dot.toFloat().expanded (7.0f));
        g.setColour (c);
        g.fillEllipse (dot.toFloat());

        auto txt = r.withTrimmedLeft (34);

        g.setColour (c);
        g.setFont (juce::Font (17.0f, juce::Font::bold));
        g.drawFittedText ((damagedRecording ? PL("Podczas zapisu utracono dane. Nagraj materiał ponownie.") : PL(review.verdictLine().c_str())),
                          txt.removeFromTop (84), juce::Justification::topLeft, 5);
    }

    //==========================================================================
    void drawWave (juce::Graphics& g, juce::Rectangle<int> r)
    {
        g.setColour (Ink::panel());
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (Ink::edge()); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);

        const bool after = player.showingAfter() && ! afterEnv.peakDb.empty();
        const auto& env  = after ? afterEnv : beforeEnv;

        if (env.peakDb.empty() || review.seconds() <= 0.0)
            return;

        const float mid = (float) r.getCentreY();
        const float half = r.getHeight() * 0.46f;

        // Przebieg rysujemy w decybelach, nie w probkach. Mowa ma szczyty koło
        // -10 dBFS i pauzy koło -60; w skali liniowej pauzy sa niewidoczna
        // kreska, a to wlasnie w nich siedzi szum i oddechy, czyli to, o co
        // czlowiek pyta.
        g.setColour ((after ? Ink::green() : Ink::textNext()).withAlpha (0.75f));

        const int cols = r.getWidth();

        for (int x = 0; x < cols; x += 3)
        {
            const size_t i = (size_t) ((double) x / juce::jmax (1, cols) * env.peakDb.size());

            if (i >= env.peakDb.size()) break;

            const float db = env.peakDb[i];
            const float norm = juce::jlimit (0.0f, 1.0f, (db + 66.0f) / 66.0f);

            if (norm <= 0.0f) continue;

            g.fillRect ((float) (r.getX() + x), mid - half * norm, 2.0f, half * norm * 2.0f);
        }

        // Sufit i cel: dwie linie, ktore program obiecal
        if (review.stage() == zs::TakeReview::Stage::Ready && after)
        {
            const float ceil = juce::jlimit (0.0f, 1.0f,
                                   (review.whatIDecided().params.ceilingDbTp + 66.0f) / 66.0f);

            g.setColour (Ink::red().withAlpha (0.35f));
            g.fillRect ((float) r.getX(), mid - half * ceil, (float) r.getWidth(), 1.0f);
            g.fillRect ((float) r.getX(), mid + half * ceil, (float) r.getWidth(), 1.0f);
        }

        if(cutFrom>=0 && cutTo>cutFrom) {
            const float a=r.getX()+float(cutFrom/review.seconds())*r.getWidth();
            const float b=r.getX()+float(cutTo/review.seconds())*r.getWidth();
            g.setColour(Ink::yellow().withAlpha(.20f));g.fillRect(a,float(r.getY()),b-a,float(r.getHeight()));
        }
        // igla
        const double pos = player.positionSec() / juce::jmax (0.001, review.seconds());
        const float  px  = r.getX() + (float) (pos * r.getWidth());

        g.setColour (Ink::textNow());
        g.fillRect (px, (float) r.getY(), 1.5f, (float) r.getHeight());

        g.setColour (Ink::textDim());
        g.setFont (juce::Font (11.0f));
        g.drawText (plr::clock (player.positionSec()) + " / " + plr::clock (review.seconds()),
                    r.reduced (6, 4), juce::Justification::topRight);
    }

    //==========================================================================
    /* Pasek pod przebiegiem: gdzie co sie stalo.

       Czerwone = do powtorzenia. Zolte = miks sie zajal, ale warto wiedziec.
       Szare = drobiazgi. Kolor niesie cala wiadomosc, bo na pasek nie ma jak
       napisac czternastu nazw — nazwa pojawia sie w liscie nizej. */
    void drawStrip (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const double total = review.seconds();

        if (total <= 0.0) return;

        auto strip = r.removeFromTop (22);

        g.setColour (Ink::panel());
        g.fillRect (strip);

        for (const auto& t : review.timeline())
        {
            const float x1 = strip.getX() + (float) (t.fromSec / total * strip.getWidth());
            const float x2 = strip.getX() + (float) (t.toSec   / total * strip.getWidth());

            // Czerwony znaczy "wracaj do mikrofonu" i nic innego. Cisza
            // cyfrowa, ktorej nie liczymy jako powodu do powtorzenia, nie moze
            // wiec byc czerwona — inaczej pasek mowi co innego niz lista pod
            // nim, a czlowiek wierzy temu, co bardziej krzyczy.
            auto c = t.mustRedo ? Ink::clip()
                   : t.severity >= (int) tw::Severity::Redo ? Ink::yellow()
                                                            : Ink::textDim();

            g.setColour (c.withAlpha (t.mustRedo ? 0.95f : 0.65f));
            g.fillRect (x1, (float) strip.getY(), juce::jmax (3.0f, x2 - x1), (float) strip.getHeight());
        }

        // Fragmenty do powtorzenia dostaja podpis z czasem — to jedyna rzecz z
        // tego ekranu, ktora czlowiek bedzie przepisywal na kartke.
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.setColour (Ink::clip());

        // Najwyzej trzy czasy wypisane wprost.
        //
        // Dziewiec nie zmiescilo sie w wierszu i ostatni wychodzil poza ekran
        // jako "powtó…", razem z podpowiedzia o klawiszu. Lista, ktora sama
        // sie urywa, jest gorsza niz krotsza: nie wiadomo, czy czegos nie
        // widac, czy czegos nie ma. Reszte podajemy liczba, a przejscie po
        // nich i tak idzie klawiszem.
        const auto& all = review.toRedo();
        const int shown = juce::jmin (3, (int) all.size());

        juce::String redoText;

        for (int i = 0; i < shown; ++i)
        {
            if (redoText.isNotEmpty()) redoText << "    ";
            redoText << PL ("powtórz ") << plr::clock (all[(size_t) i].fromSec)
                     << PL ("–") << plr::clock (all[(size_t) i].toSec);
        }

        if (redoText.isNotEmpty())
        {
            if ((int) all.size() > shown)
                redoText << PL ("    i jeszcze ") << juce::String ((int) all.size() - shown);

            g.drawText (redoText + PL ("        N — przejdź do następnego"),
                        r.reduced (0, 2), juce::Justification::centredLeft);
        }
    }

    //==========================================================================
    void timerCallback() override
    {
        if (ready.exchange (false))
            adoptResult();

        if (review.stage() != zs::TakeReview::Stage::Ready)
        {
            playButton.setEnabled (false); abButton.setEnabled (false);
            saveButton.setEnabled (false); editToggle.setEnabled (false); nextButton.setEnabled (false);
            repaint();   // pasek postepu
            return;
        }

        const bool editable=canEdit();
        nextButton.setEnabled(editable && !review.toRedo().empty());
        markStart.setEnabled(editable);markEnd.setEnabled(editable);
        cutButton.setEnabled(editable && cutFrom>=0 && cutTo>cutFrom);
        undoButton.setEnabled(editable && !undoAudio.empty());
        const bool playingNow = player.isPlaying();

        if (playingNow != wasPlaying)
        {
            wasPlaying = playingNow;
            playButton.setButtonText (playingNow ? PL ("Zatrzymaj") : PL ("Odtwórz"));
        }

        if (playingNow)
            repaint (waveArea());
    }

    //==========================================================================
    Recorder& recorder;

    juce::AudioFormatManager formats;

    zs::TakeReview review;
    mix::Decide::Wish wish;   ///< w tym wynik kreatora — patrz setRoom()
    zs::Audition   player;
    bool damagedRecording=false;
    zs::Envelope   beforeEnv, afterEnv;

    juce::File file, saved;
    juce::String note;
    double sr = 48000.0;

    std::atomic<bool> ready { false };
    bool wasPlaying = false;

    juce::TextButton playButton, abButton, saveButton, backButton;
    juce::TextEditor words, assessment;
    zs::StepBar flow;
    juce::TextButton markStart,markEnd,cutButton,undoButton,editToggle,detailsToggle,nextButton;
    bool editMode=false,detailsMode=false;
    juce::Rectangle<int> summaryArea, feedbackArea;
    juce::Label selection;
    double cutFrom=-1,cutTo=-1;
    std::vector<float> undoAudio;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReviewComponent)
};
