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
#include "CutReviewPanel.h"
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
        saveButton.onClick = [this] { saveSelection(); };
        addAndMakeVisible (saveButton);

        backButton.setButtonText (PL ("Wróć do nagrywania"));
        backButton.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (backButton);

        savedPath.setJustification (juce::Justification::centred);

        // Jeden przycisk glowny: zapis wybranego fragmentu. Reszta drugorzedna.
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
        editToggle.onClick=[this] { setEditMode (!editMode); };
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
        selection.setText(PL("Przeciągnij po przebiegu, aby zaznaczyć fragment do wycięcia. Oryginał zostaje na dysku."),juce::dontSendNotification);
        // Gdzie jest plik: pytanie, ktore padlo przy pierwszym prawdziwym
        // nagraniu ("nie wiem, gdzie mi sie zapisala sesja").
        folderButton.setButtonText (PL("Pokaż w folderze"));
        folderButton.onClick = [this] { (doneMode ? saved : (saved.existsAsFile() ? saved : file)).revealToUser(); };
        addAndMakeVisible (folderButton);
        doneEditButton.setButtonText (PL("Wróć do montażu"));
        doneEditButton.onClick = [this] { setEditMode (true); };
        newTakeButton.setButtonText (PL("Nagraj nowe podejście"));
        newTakeButton.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (doneEditButton); addAndMakeVisible (newTakeButton);
        for (auto* editor : { &savedPath, &savedName }) {
            editor->setMultiLine (true); editor->setReadOnly (true);
            editor->setCaretVisible (false); editor->setScrollbarsShown (true);
            editor->setFont (juce::Font (zs::type::small));
            editor->setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            editor->setColour (juce::TextEditor::textColourId, Ink::textNext());
            editor->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            editor->setColour (juce::TextEditor::focusedOutlineColourId, Ink::edge());
            addAndMakeVisible (*editor);
        }
        addAndMakeVisible(selection);
        middleToggle.setButtonText (PL("Wytnij fragment ze środka"));
        middleToggle.onClick = [this] { middleMode = !middleMode; resized(); repaint(); };
        addAndMakeVisible (middleToggle);
        for (auto* slider : { &trimStart, &trimEnd }) {
            slider->setSliderStyle (juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider->setColour (juce::Slider::thumbColourId, Ink::green());
            slider->setColour (juce::Slider::trackColourId, Ink::green());
            addAndMakeVisible (slider);
        }
        trimStart.setName (PL("Początek")); trimEnd.setName (PL("Koniec"));
        trimStart.changed = [this] { constrainTrim (true); };
        trimEnd.changed = [this] { constrainTrim (false); };
        startHandle.move = [this] (double t) { trimStart.setValue (t); };
        endHandle.move = [this] (double t) { trimEnd.setValue (t); };
        startHandle.step = [this] (double d) { trimStart.setValue (trimStart.getValue() + d); };
        endHandle.step = [this] (double d) { trimEnd.setValue (trimEnd.getValue() + d); };
        startHandle.setName (PL("Początek wybranego fragmentu"));
        endHandle.setName (PL("Koniec wybranego fragmentu"));
        addAndMakeVisible (startHandle); addAndMakeVisible (endHandle);
        addChildComponent(cutPanel);
        cutPanel.onError = [this](const juce::String& error) { note=error; repaint(); };
        cutPanel.onListen = [this](double from, double to) { listenCut(from,to); };
        cutPanel.onApply = [this](const auto& list) { applyProposals(list); };
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
        seamPlayer.clear(); seamPlaying=false;
        player.clear();
        recorder.attachPlayer (nullptr);

        undoAudio.clear();cutFrom=cutTo=-1;editMode=detailsMode=middleMode=pendingSave=doneMode=false;
        words.clear(); assessment.clear(); reviewSummary.clear(); resized();
        for (auto* b : { &playButton, &abButton, &saveButton, &editToggle, &nextButton,
                         &markStart, &markEnd, &cutButton, &undoButton }) b->setEnabled (false);
        cutPanel.load(wavFile, {}, 48000);
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

        cutPanel.load(file, review.rawAudio(), sr);
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

    void setEditMode (bool enabled) { if(enabled && !cutPanel.empty()) {
        // Montaz czeka na decyzje po R — wracamy do listy decyzji, nie zostajemy
        // na ekranie "Plik zapisany", gdzie tego komunikatu nie widac (test 12.10:
        // "Wroc do montazu" wygladal na martwy).
        note=PL("Najpierw zatwierdź poprawki po R. Ich czasy odnoszą się do pełnego nagrania.");
        doneMode = false; editMode = false; detailsMode = false; resized(); repaint(); return;
    } doneMode = false; editMode = enabled; detailsMode = false; resized(); repaint(); }

    // Podglad przechodzi ta sama droga zapisu co zwykly przycisk.
    void previewDone() { saveSelection(); }

    bool hasTake() const { return review.stage() != zs::TakeReview::Stage::Empty; }

    void end()
    {
        player.pause(); seamPlayer.pause(); seamPlaying=false;
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
        if (doneMode) {
            if (c == juce::KeyPress::escapeKey) { setEditMode (true); return true; }
            return false;
        }

        if (cutPanel.isVisible() && cutPanel.keyPressed(k)) return true;
        if (c == juce::KeyPress::spaceKey)     { togglePlay(); return true; }
        if (c == juce::KeyPress::tabKey)       { toggleAb();   return true; }
        if (c == juce::KeyPress::escapeKey)    { if (onClose) onClose(); return true; }

        if (editMode && (c == juce::KeyPress::deleteKey || c == juce::KeyPress::backspaceKey)
            && cutFrom >= 0 && cutTo > cutFrom && cutButton.onClick) { cutButton.onClick(); return true; }
        if (editMode && (c == juce::KeyPress::leftKey || c == juce::KeyPress::rightKey)) {
            trimStart.setValue (trimStart.getValue() + (c == juce::KeyPress::leftKey ? -0.1 : 0.1)); return true;
        }
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

    /* Mysz na przebiegu.

       Odsluch: klikniecie = posluchaj od tego miejsca (jak dotad).
       Montaz: klikniecie tylko przestawia kursor, a przeciagniecie zaznacza
       fragment do wyciecia. Dawniej klikniecie w montazu od razu gralo, a
       granice ciecia ustawialo sie przyciskami w miejscu, w ktorym akurat
       bylo odtwarzanie — Adrian: "nie mam mozliwosci wskazania myszka
       poczatku i konca ciecia". Liczby: zs::ui::review::dragRange. */
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (doneMode) return;
        const auto strip = waveArea();
        dragX0 = -1.0f;

        if (! strip.contains (e.getPosition()) || review.seconds() <= 0.0
            || review.stage() != zs::TakeReview::Stage::Ready)
            return;

        const double t = (e.position.x - strip.getX()) / juce::jmax (1, strip.getWidth())
                       * review.seconds();

        stopSeam();
        player.seekSec (t);

        if (editMode)
        {
            player.pause();
            dragX0 = e.position.x;
        }
        else if (! player.isPlaying())
        {
            player.play();
        }

        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! editMode || dragX0 < 0.0f || ! canEdit()) return;

        const auto strip = waveArea();
        const auto r = zs::ui::review::dragRange (dragX0, e.position.x, strip.getX(),
                                                  strip.getWidth(), review.seconds());
        if (! r.isDrag()) return;

        cutFrom = r.from;
        cutTo   = r.to;

        if (! middleMode) { middleMode = true; resized(); }
        updateSelection();
    }

    void mouseUp (const juce::MouseEvent&) override { dragX0 = -1.0f; }

    /* W trakcie sluchania i miksu: komunikat na caly ekran.

       Maly napis "Miksuje…" w rogu przeoczyl pierwszy czlowiek, ktory nagral
       4,5 minuty — klikal wszedzie i uznal, ze program sie zawiesil. Miks
       takiego nagrania trwa okolo minuty, wiec ekran ma mowic wprost: pracuje,
       ile minelo, ile zwykle trwa. Procentow nie pokazujemy: lancuch nie
       raportuje postepu w trakcie, pasek stalby na 35% przez pol minuty. */
    void paintOverChildren (juce::Graphics& g) override
    {
        const auto st = review.stage();
        if (st != zs::TakeReview::Stage::Listening && st != zs::TakeReview::Stage::Mixing) return;

        auto area = getLocalBounds().withTrimmedTop (zs::ui::frame::header + zs::ui::frame::steps)
                                    .withTrimmedBottom (zs::ui::frame::footer);
        g.setColour (Ink::bg().withAlpha (0.88f));
        g.fillRect (area);

        auto box = area.withSizeKeepingCentre (juce::jmin (560, area.getWidth() - 40), 196);
        g.setColour (Ink::panel()); g.fillRoundedRectangle (box.toFloat(), 8.0f);
        g.setColour (Ink::edge());  g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 8.0f, 1.0f);

        auto r = box.reduced (28, 24);
        g.setColour (Ink::textNow());
        g.setFont (juce::Font (zs::type::title, juce::Font::bold));
        g.drawText (st == zs::TakeReview::Stage::Listening ? PL ("Słucham nagrania…")
                                                         : PL ("Miksuję nagranie…"),
                    r.removeFromTop (40), juce::Justification::centredLeft);

        // Pasek "pracuje": odcinek wedrujacy w kolko, bez udawania procentow.
        r.removeFromTop (10);
        const auto bar = r.removeFromTop (8).toFloat();
        g.setColour (Ink::raised()); g.fillRoundedRectangle (bar, 4.0f);
        const float phase = (float) (juce::Time::getMillisecondCounter() % 1600u) / 1600.0f;
        const float w = bar.getWidth() * 0.28f;
        const float x = bar.getX() - w + phase * (bar.getWidth() + w);
        g.setColour (Ink::green());
        g.fillRoundedRectangle (juce::Rectangle<float> (x, bar.getY(), w, bar.getHeight())
                                    .getIntersection (bar), 4.0f);

        r.removeFromTop (16);
        const int elapsed = busySinceMs == 0 ? 0
                          : (int) ((juce::Time::getMillisecondCounter() - busySinceMs) / 1000u);
        const int expect  = zs::ui::review::mixEstimateSec (review.seconds());

        juce::String text = PL ("Minęło ") + juce::String (elapsed) + PL (" s. Przy nagraniu ")
                          + plr::clock (review.seconds()) + PL (" trwa to zwykle ok. ")
                          + juce::String (expect) + " s.";
        text << "\n" << (elapsed > expect * 3 / 2
                            ? PL ("Trwa dłużej niż zwykle, ale program pracuje — nie zamykaj go.")
                            : PL ("Ekran sam się odświeży, gdy miks będzie gotowy."));

        g.setColour (Ink::textNext());
        g.setFont (juce::Font (zs::type::body));
        g.drawFittedText (text, r, juce::Justification::topLeft, 3);
    }

    //==========================================================================
    void resized() override
    {
        cutPanel.setVisible(!doneMode && !editMode && !cutPanel.empty());
        if (doneMode) { layoutDone(); return; }
        doneEditButton.setVisible (false); newTakeButton.setVisible (false);
        savedPath.setVisible (false); savedName.setVisible (false);
        playButton.setVisible (true); abButton.setVisible (true);
        saveButton.setVisible (true); editToggle.setVisible (true);
        zs::Look::setKind (folderButton, zs::ui::ButtonKind::Secondary);
        if (editMode) { layoutEdit(); return; }
        namespace L = zs::ui::review;
        for (auto* c : { static_cast<juce::Component*>(&trimStart), static_cast<juce::Component*>(&trimEnd), static_cast<juce::Component*>(&startHandle), static_cast<juce::Component*>(&endHandle), static_cast<juce::Component*>(&middleToggle) }) c->setVisible (false);
        flow.setBounds (22, zs::ui::frame::header + 8, getWidth() - 44, zs::StepBar::preferredHeight);
        flow.setCurrent (editMode ? 3 : 2);
        nextButton.setVisible(true); backButton.setVisible(true);detailsToggle.setVisible(true);
        saveButton.setButtonText(PL("Zapisz miks"));
        assessment.setText(reviewSummary, false);
        auto main = zs::ScreenFrame::mainArea (getLocalBounds());
        main.removeFromTop (78 + waveHeight() + L::strip + 8);
        auto row = main.removeFromTop (L::buttons);
        playButton.setBounds (row.removeFromLeft (132)); row.removeFromLeft (10);
        abButton.setBounds (row.removeFromLeft (juce::jmin (240, row.getWidth())));
        main.removeFromTop (8);
        auto tools = main.removeFromTop (L::tools);
        detailsToggle.setBounds (tools.removeFromLeft (172));
        folderButton.setBounds (tools.removeFromRight (172)); folderButton.setVisible (true);
        tools.removeFromLeft (12); tools.removeFromRight (12); feedbackArea = tools;
        main.removeFromTop (8);
        summaryArea = main;
        cutPanel.setBounds(main);
        words.setVisible (detailsMode && cutPanel.empty());
        words.setBounds (main);
        detailsToggle.setEnabled(cutPanel.empty());
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

    }

    void paint (juce::Graphics& g) override
    {
        if (doneMode) { paintDone (g); return; }
        if (editMode) { paintEdit (g); return; }
        g.fillAll (Ink::bg());
        zs::ScreenFrame::paint (g, getLocalBounds(), file.getFileNameWithoutExtension(),
            cutPanel.empty() ? PL("Spacja — odtwarzanie · Tab — przed / po · Esc — wróć")
                             : PL("Spacja — szew · Enter — wytnij · Backspace — zostaw · ↑↓ — miejsca · Esc — wróć"));
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
        if (cutPanel.empty() && !detailsMode && review.stage() == zs::TakeReview::Stage::Ready)
        {
            g.setColour (Ink::textNext()); g.setFont (juce::Font (zs::type::small));
            g.drawFittedText (PL("Miks został dobrany automatycznie. Odsłuchaj, zanim zapiszesz."),
                              summaryArea, juce::Justification::topLeft, 3);
        }
        if (!detailsMode && file != juce::File())
        {
            g.setColour (Ink::textDim()); g.setFont (juce::Font (zs::type::small));
            g.drawFittedText (PL("Nagranie: ") + file.getFullPathName()
                                + (saved.existsAsFile() ? PL("\nMiks: ") + saved.getFullPathName() : juce::String()),
                              summaryArea.withTrimmedTop (28), juce::Justification::topLeft, 3);
        }
    }

private:
    juce::Rectangle<int> doneContent() const {
        auto main = zs::ScreenFrame::mainArea (getLocalBounds());
        return main.withSizeKeepingCentre (main.getWidth(), juce::jmin (412, main.getHeight()));
    }
    void layoutDone() {
        for (auto* child : getChildren()) child->setVisible (false);
        flow.setVisible (true);
        flow.setBounds (22, zs::ui::frame::header + 8, getWidth() - 44, zs::StepBar::preferredHeight);
        flow.setCurrent (4);
        auto main = doneContent();
        main.removeFromTop (68 + 10 + 18 + 40 + 44 + 8);
        folderButton.setBounds (main.removeFromTop (40).withSizeKeepingCentre (220, 40));
        folderButton.setVisible (true);
        folderButton.setEnabled (saved.existsAsFile());
        zs::Look::setKind (folderButton, zs::ui::ButtonKind::Primary);
        main.removeFromTop (12);
        savedPath.setBounds (main.removeFromTop (72).reduced (12, 0)); savedPath.setVisible (true);
        auto side = zs::ScreenFrame::sidebar (getLocalBounds()).reduced (22, 24);
        side.removeFromTop (40 + 24);
        savedName.setBounds (side.removeFromTop (60)); savedName.setVisible (true);
        side = zs::ScreenFrame::sidebar (getLocalBounds()).reduced (22, 24);
        auto actions = side.removeFromBottom (80);
        newTakeButton.setBounds (actions.removeFromBottom (36)); actions.removeFromBottom (8);
        doneEditButton.setBounds (actions.removeFromBottom (36));
        newTakeButton.setVisible (true); doneEditButton.setVisible (true);
    }
    void paintDone (juce::Graphics& g) {
        g.fillAll (Ink::bg());
        zs::ScreenFrame::paint (g, getLocalBounds(), file.getFileNameWithoutExtension(), PL("Plik zapisany na dysku · Esc — wróć do montażu"));
        auto side = zs::ScreenFrame::sidebar (getLocalBounds());
        g.setColour (Ink::bar()); g.fillRect (side);
        g.setColour (Ink::edge()); g.drawVerticalLine (side.getX(), side.getY(), side.getBottom());
        auto main = doneContent();
        const auto emblem = main.removeFromTop (68).withSizeKeepingCentre (68, 68).toFloat();
        g.setColour (Ink::green().withAlpha (0.18f)); g.fillEllipse (emblem);
        g.setColour (Ink::green().withAlpha (0.5f)); g.drawEllipse (emblem, 1.5f);
        juce::Path check; check.startNewSubPath (emblem.getX()+21, emblem.getY()+34);
        check.lineTo (emblem.getX()+30, emblem.getY()+43); check.lineTo (emblem.getX()+48, emblem.getY()+25);
        g.setColour (Ink::green()); g.strokePath (check, juce::PathStrokeType (4, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        main.removeFromTop (10);
        g.setColour (Ink::textDim()); g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
        g.drawText (PL("05 / GOTOWE"), main.removeFromTop (18), juce::Justification::centred);
        g.setColour (Ink::textNow()); g.setFont (juce::Font (zs::type::title, juce::Font::bold));
        g.drawText (PL("Nagranie gotowe"), main.removeFromTop (40), juce::Justification::centred);
        g.setColour (Ink::textNext()); g.setFont (juce::Font (zs::type::body));
        g.drawFittedText (PL("Zapisano miks wybranego nagrania jako plik WAV na dysku."), main.removeFromTop (44).reduced (12, 0), juce::Justification::centred, 2);
        side = side.reduced (22, 24);
        g.setColour (Ink::textNow()); g.setFont (juce::Font (zs::type::heading, juce::Font::bold));
        g.drawText (PL("Plik końcowy"), side.removeFromTop (40), juce::Justification::centredLeft);
        g.setFont (juce::Font (zs::type::small)); g.setColour (Ink::textDim());
        g.drawText (PL("Nazwa"), side.removeFromTop (24), juce::Justification::centredLeft);
        side.removeFromTop (60);
        const auto row = [&] (const juce::String& label, const juce::String& value, int height) {
            auto r = side.removeFromTop (height);
            g.setColour (Ink::textDim()); g.drawText (label, r.removeFromTop (18), juce::Justification::centredLeft);
            g.setColour (Ink::textNow()); g.drawText (value, r.removeFromTop (22), juce::Justification::centredLeft);
        };
        row (PL("Format"), "WAV 24-bit", 62); // Miejsce na drugi wiersz formatu.
        row (PL("Długość"), plr::clock (review.seconds()) + " (" + juce::String (review.seconds(), 1) + " s)", 42);
        row (PL("Głośność"), juce::String (review.whatIDid().lufsAfter, 1) + " LUFS", 42);
        row (PL("Szczyt"), juce::String (review.whatIDid().peakAfter, 1) + " dBTP", 42);
    }
    class TrimSlider : public juce::Slider {
    public:
        std::function<void()> changed;
        void valueChanged() override { if(changed)changed(); }
        bool keyPressed(const juce::KeyPress& k) override {
            if(k.getKeyCode()==juce::KeyPress::leftKey || k.getKeyCode()==juce::KeyPress::rightKey) {
                setValue(getValue()+(k.getKeyCode()==juce::KeyPress::leftKey ? -0.1 : 0.1));return true;
            }
            return juce::Slider::keyPressed(k);
        }
    };
    class TrimHandle : public juce::Component {
    public:
        std::function<void(double)> move, step;
        juce::Rectangle<int> axis;
        double seconds = 0;
        TrimHandle() { setWantsKeyboardFocus (true); setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); }
        void paint (juce::Graphics& g) override {
            g.setColour (Ink::green()); g.fillRect (getWidth()/2-1, 0, 2, getHeight());
            g.fillRoundedRectangle (2.f, getHeight()/2.f-25.f, getWidth()-4.f, 50.f, 4.f);
            if (hasKeyboardFocus (false)) { g.setColour (Ink::textNow()); g.drawRect (getLocalBounds(), 1); }
        }
        void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }
        void mouseDrag (const juce::MouseEvent& e) override {
            if (move && axis.getWidth()>0) move ((e.getEventRelativeTo(getParentComponent()).position.x-axis.getX())/axis.getWidth()*seconds);
        }
        bool keyPressed (const juce::KeyPress& k) override {
            if (k.getKeyCode()!=juce::KeyPress::leftKey && k.getKeyCode()!=juce::KeyPress::rightKey) return false;
            if (step) step (k.getKeyCode()==juce::KeyPress::leftKey ? -0.1 : 0.1); return true;
        }
    };
    void resetTrim() {
        const double total = review.seconds();
        if (total <= 0) return;
        trimStart.setRange (0, total, 0); trimEnd.setRange (0, total, 0);
        trimStart.setValue (0, juce::dontSendNotification); trimEnd.setValue (total, juce::dontSendNotification);
    }
    void constrainTrim (bool start) {
        const double gap = juce::jmin (0.1, review.seconds());
        if (start) trimStart.setValue (juce::jlimit (0.0, juce::jmax(0.0,trimEnd.getValue()-gap), trimStart.getValue()), juce::dontSendNotification);
        else trimEnd.setValue (juce::jlimit (juce::jmin(review.seconds(),trimStart.getValue()+gap), review.seconds(), trimEnd.getValue()), juce::dontSendNotification);
        positionHandles(); repaint();
    }
    void positionHandles() {
        auto axis = waveArea().withHeight (waveHeight());
        for (auto* h : { &startHandle, &endHandle }) { h->axis=axis; h->seconds=review.seconds(); }
        const double total = juce::jmax (0.001, review.seconds());
        startHandle.setBounds (axis.getX()+int(trimStart.getValue()/total*axis.getWidth())-7, axis.getY(), 14, axis.getHeight());
        endHandle.setBounds (axis.getX()+int(trimEnd.getValue()/total*axis.getWidth())-7, axis.getY(), 14, axis.getHeight());
    }
    void layoutEdit() {
        flow.setBounds (22, zs::ui::frame::header+8, getWidth()-44, zs::StepBar::preferredHeight); flow.setCurrent (3);
        auto main = zs::ScreenFrame::mainArea(getLocalBounds()); main.removeFromTop (78+waveHeight()+8);
        auto limits = main.removeFromTop (54); auto left=limits.removeFromLeft((limits.getWidth()-16)/2); limits.removeFromLeft(16);
        trimStart.setBounds(left.withTrimmedTop(22)); trimEnd.setBounds(limits.withTrimmedTop(22));
        main.removeFromTop(8); auto row=main.removeFromTop(36);
        playButton.setBounds(row.removeFromLeft(112));row.removeFromLeft(8);abButton.setBounds(row.removeFromLeft(200));row.removeFromLeft(8);middleToggle.setBounds(row);
        main.removeFromTop(8);
        auto cuts=main.removeFromTop(middleMode?32:0);const int w=(cuts.getWidth()-24)/4;
        for(auto* b:{&markStart,&markEnd,&cutButton,&undoButton}) {b->setBounds(cuts.removeFromLeft(w));cuts.removeFromLeft(8);b->setVisible(middleMode);}
        selection.setVisible(middleMode);selection.setBounds(main.removeFromTop(middleMode?24:0));
        feedbackArea=main.removeFromTop(24);summaryArea=main.removeFromTop(juce::jmin(46,main.getHeight()));
        auto side=zs::ScreenFrame::sidebar(getLocalBounds()).reduced(22,24);side.removeFromTop(40);
        auto actions=side.removeFromBottom(124);editToggle.setBounds(actions.removeFromBottom(36));actions.removeFromBottom(8);saveButton.setBounds(actions.removeFromBottom(36));actions.removeFromBottom(8);folderButton.setBounds(actions.removeFromBottom(36));folderButton.setVisible(true);
        assessment.setBounds(side);assessment.setVisible(true);assessment.setText(processingSummary(),false);
        saveButton.setButtonText(PL("Przejdź do zapisu →"));editToggle.setButtonText(PL("Wróć do odsłuchu"));
        nextButton.setVisible(false);backButton.setVisible(false);words.setVisible(false);detailsToggle.setVisible(false);
        for(auto* c:{static_cast<juce::Component*>(&trimStart),static_cast<juce::Component*>(&trimEnd),static_cast<juce::Component*>(&startHandle),static_cast<juce::Component*>(&endHandle),static_cast<juce::Component*>(&middleToggle)})c->setVisible(true);
        positionHandles();
    }
    juce::String processingSummary() const {
        if(review.stage()!=zs::TakeReview::Stage::Ready) return PL("Miks liczy się ponownie…");
        const auto& d=review.whatIDecided();const auto& p=d.params;const auto& r=review.whatIDid();
        juce::String text=PL("Głośność: ")+juce::String(r.lufsAfter,1)+" LUFS / "+juce::String(p.targetLufs,1)+PL(" cel; zapas szczytu ")+juce::String(d.plr,1)+" dB.\n\n";
        const float eq[]={p.lowDb,p.mudDb,p.presenceDb,p.airDb};
        const char* names[]={"Dół", "Błoto", "Obecność", "Powietrze"};
        for(int i=0;i<4;++i) {
            text<<PL(names[i])<<": "<<juce::String(eq[i],1)<<" dB — ";
            if(!d.tone.valid)text<<PL("za mało mowy do pomiaru");
            else if(i==3 && !d.tone.hasAir())text<<PL("brak treści powyżej ")<<juce::String(d.tone.bandwidthHz/1000,1)<<" kHz";
            else text<<PL("pasmo ")<<juce::String(d.tone.db[i],1)<<PL(" dB względem 1 kHz");
            text<<".\n\n";
        }
        text<<PL("Odszumianie: ")<<juce::String(r.denoiseDb,1)<<PL(" dB; tło ")<<juce::String(d.speechDb-d.noiseFloorDb,1)<<PL(" dB pod mową.")<<"\n\n";
        text<<PL("Przydźwięk: ")<<(p.deHum?juce::String(r.humHz,0)+" Hz":PL("bez filtra"))<<PL("; wystaje ")<<juce::String(d.humDb,1)<<" dB.";
        return text;
    }
    void paintEdit(juce::Graphics& g) {
        g.fillAll(Ink::bg());zs::ScreenFrame::paint(g,getLocalBounds(),file.getFileNameWithoutExtension(),PL("Uchwyty: ← / → co 0,1 s · Spacja — odsłuch · Tab — przed / po"));
        auto side=zs::ScreenFrame::sidebar(getLocalBounds());g.setColour(Ink::bar());g.fillRect(side);
        g.setColour(Ink::edge());g.drawVerticalLine(side.getX(),side.getY(),side.getBottom());
        auto main=zs::ScreenFrame::mainArea(getLocalBounds());
        g.setColour(Ink::textDim());g.setFont(juce::Font(zs::type::tiny,juce::Font::bold));g.drawText(PL("04 / MONTAŻ"),main.removeFromTop(18),juce::Justification::centredLeft);
        g.setColour(Ink::textNow());g.setFont(juce::Font(zs::type::title,juce::Font::bold));g.drawText(PL("Dopracuj początek i koniec"),main.removeFromTop(36),juce::Justification::centredLeft);
        g.setColour(Ink::textNext());g.setFont(juce::Font(zs::type::body));g.drawFittedText(PL("Uchwyty przycinają początek i koniec. Przeciągnij po przebiegu, aby wyciąć fragment."),main.removeFromTop(24),juce::Justification::centredLeft,1);
        drawWave(g,main.removeFromTop(waveHeight()));main.removeFromTop(8);
        auto row=main.removeFromTop(54);auto left=row.removeFromLeft((row.getWidth()-16)/2);row.removeFromLeft(16);
        g.setFont(juce::Font(zs::type::small));g.drawText(PL("Początek: ")+juce::String(trimStart.getValue(),1)+" s",left,juce::Justification::topLeft);g.drawText(PL("Koniec: ")+juce::String(trimEnd.getValue(),1)+" s",row,juce::Justification::topLeft);
        g.setColour(Ink::textNow());g.setFont(juce::Font(zs::type::heading,juce::Font::bold));g.drawText(PL("Obróbka głosu"),side.reduced(22,24).removeFromTop(32),juce::Justification::centredLeft);
        g.setFont(juce::Font(zs::type::small));g.setColour(Ink::textNext());g.drawFittedText(note,feedbackArea,juce::Justification::topLeft,2);
        g.setColour(Ink::green().withAlpha(.15f));g.fillRoundedRectangle(summaryArea.toFloat(),4.f);g.setColour(Ink::green());g.drawFittedText(PL("Oryginalne nagranie pozostaje bez zmian. Eksport obejmie wybrany fragment."),summaryArea.reduced(10,4),juce::Justification::centredLeft,3);
    }
    // cutRange usuwa zakres: najpierw koniec, potem poczatek, aby czasy
    // wybranego fragmentu pozostaly w osi pierwotnej kopii. DSP bez zmian.
    // Eksport czeka na nowy miks; cofanie przywraca cala kopie sprzed ciecia.
    void saveSelection() {
        if(!canEdit())return;
        const double from=trimStart.getValue(),to=trimEnd.getValue(),total=review.seconds();
        if(!editMode || (from<=0 && to>=total)) {save();return;}
        std::vector<float> audio=review.rawAudio(), cropped;
        if(to<total) {if(!zs::cutRange(audio,sr,to,total,cropped))return;audio=std::move(cropped);}
        if(from>0) {if(!zs::cutRange(audio,sr,0,from,cropped))return;audio=std::move(cropped);}
        undoAudio=review.rawAudio();pendingSave=true;restartEdit(std::move(audio));
    }
    bool canEdit() const { return review.stage()==zs::TakeReview::Stage::Ready && !isThreadRunning(); }
    void updateSelection() {
        if(cutFrom>=0 && cutTo>cutFrom)
            selection.setText(PL("Do wycięcia: ")+plr::clock(cutFrom)+PL("–")+plr::clock(cutTo)+" ("+juce::String(cutTo-cutFrom,1)+PL(" s) — Delete albo „Wytnij zaznaczenie”."),juce::dontSendNotification);
        else
            selection.setText(PL("Cięcie: ")+(cutFrom>=0?juce::String(cutFrom,2)+" s":"—")+"  →  "+(cutTo>=0?juce::String(cutTo,2)+" s":"—"),juce::dontSendNotification);
        repaint();
    }
    void restartEdit(std::vector<float> audio) {
        player.pause();recorder.attachPlayer(nullptr);ready.store(false);player.clear();
        for(auto* b:{&playButton,&abButton,&saveButton,&editToggle,&markStart,&markEnd,&cutButton,&undoButton})b->setEnabled(false);
        trimStart.setEnabled(false);trimEnd.setEnabled(false);startHandle.setEnabled(false);endHandle.setEnabled(false);
        auto events=zs::TakeReview::listenOffline(audio,sr);
        review.setTake(std::move(audio),sr,std::move(events));
        beforeEnv.build(review.rawAudio(),sr,1400);afterEnv={};resetTrim();resized();
        cutFrom=cutTo=-1;
        selection.setText(PL("Zmiana zastosowana. Miks liczy się ponownie; odsłuchaj miejsce cięcia."),juce::dontSendNotification);
        words.clear();assessment.clear();saved=juce::File();note=PL("Montaż zmienia kopię w pamięci. Zapisz miks, aby zachować wynik.");
        startThread();repaint();
    }
    //==========================================================================
    /** Rosnie z oknem — liczby i uzasadnienie w zs::ui::review (UiRules.h). */
    int waveHeight() const { if(!cutPanel.empty() && !editMode) return getHeight()<720 ? 0 : 80; const int h=zs::ui::review::waveHeight(getHeight()); return editMode ? juce::jmax(120,h-(middleMode?72:16)) : h; }

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
        reviewSummary=summary; assessment.setText (summary, false);
        playButton.setEnabled (true); abButton.setEnabled (true);
        editToggle.setEnabled (true); saveButton.setEnabled (true);
        nextButton.setEnabled (!review.toRedo().empty());

        resetTrim();
        updateAbText();
        resized();
        if(pendingSave) {pendingSave=false;save();}
        repaint();
    }

    //==========================================================================
    void togglePlay()
    {
        stopSeam();
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
        player.pause();
        savedPath.setText (saved.getFullPathName(), false);
        savedName.setText (saved.getFileName(), false);
        doneMode = true;
        resized();
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
        if(editMode) {
            const float a=r.getX()+float(trimStart.getValue()/review.seconds())*r.getWidth();
            const float b=r.getX()+float(trimEnd.getValue()/review.seconds())*r.getWidth();
            g.setColour(Ink::green().withAlpha(.12f));g.fillRect(a,float(r.getY()),b-a,float(r.getHeight()));
            g.setColour(Ink::bg().withAlpha(.75f));g.fillRect(float(r.getX()),float(r.getY()),a-r.getX(),float(r.getHeight()));g.fillRect(b,float(r.getY()),r.getRight()-b,float(r.getHeight()));
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

        cutPanel.setEnabled(review.stage() == zs::TakeReview::Stage::Ready);
        if(seamPlaying && !seamPlayer.isPlaying()) stopSeam();
        folderButton.setEnabled (doneMode ? saved.existsAsFile() : (file.existsAsFile() || saved.existsAsFile()));

        if (review.stage() != zs::TakeReview::Stage::Ready)
        {
            if (busySinceMs == 0) busySinceMs = juce::jmax (1u, juce::Time::getMillisecondCounter());
            playButton.setEnabled (false); abButton.setEnabled (false);
            trimStart.setEnabled(false);trimEnd.setEnabled(false);startHandle.setEnabled(false);endHandle.setEnabled(false);
            markStart.setEnabled(false);markEnd.setEnabled(false);cutButton.setEnabled(false);undoButton.setEnabled(false);
            saveButton.setEnabled (false); editToggle.setEnabled (false); nextButton.setEnabled (false);
            repaint();   // pasek postepu
            return;
        }

        busySinceMs = 0;
        const bool editable=canEdit();
        trimStart.setEnabled(editable);trimEnd.setEnabled(editable);startHandle.setEnabled(editable);endHandle.setEnabled(editable);
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
    void stopSeam() {
        seamPlayer.pause(); seamPlaying=false; recorder.attachPlayer(&player);
    }
    void listenCut(double from, double to) {
        if(review.stage()!=zs::TakeReview::Stage::Ready) return;
        player.pause(); seamPlayer.pause();
        const auto& raw=review.rawAudio();
        const double total=raw.size()/sr;
        from=juce::jlimit(0.0,total,from); to=juce::jlimit(from,total,to);
        std::vector<float> fragment;
        // To tylko odsluch: 3 s z obu stron, srodek pominiety. Plik bez zmian.
        auto append=[&](double a,double b) {
            const size_t first=juce::jmin(raw.size(),(size_t)(a*sr));
            const size_t last=juce::jmin(raw.size(),(size_t)(b*sr));
            fragment.insert(fragment.end(),raw.begin()+first,raw.begin()+last);
        };
        append(juce::jmax(0.0,from-3),from); append(to,juce::jmin(total,to+3));
        const double rate=recorder.currentSampleRate()>0?recorder.currentSampleRate():sr;
        if(std::abs(rate-sr)>.1) {
            mix::Resampler convert; convert.prepare((int)sr,(int)rate,64);
            std::vector<float> converted; convert.render(fragment,converted); fragment=std::move(converted);
        }
        seamPlayer.setMaterial(fragment,fragment,rate);
        recorder.attachPlayer(&seamPlayer); seamPlaying=true; seamPlayer.play();
    }
    void applyProposals(const std::vector<tw::CutProposal>& proposals) {
        if(review.stage()!=zs::TakeReview::Stage::Ready || !cutPanel.decided()) return;
        player.pause(); stopSeam();
        const auto result=zs::applyCutList(file,proposals);
        if(result.error.isNotEmpty()) { note=PL("Nie udało się zastosować poprawek: ")+result.error; repaint(); return; }
        if(!result.applied) {
            if(!tw::acceptedCuts(proposals).empty()) {
                note=PL("Nie udało się zastosować poprawek. Sprawdź plik nagrania."); repaint(); return;
            }
            // Wszystkie decyzje "zostaw": archiwum decyzji i pelnego nagrania,
            // aby reczny montaz nie odziedziczyl listy ze starymi czasami.
            const auto dir=file.getParentDirectory().getChildFile("surowe");
            const auto raw=dir.getNonexistentChildFile(file.getFileNameWithoutExtension(),".wav",false);
            if(!dir.createDirectory() || !file.copyFileTo(raw)
               || !zs::cutListFile(file).moveFileTo(zs::cutListFile(raw))) {
                note=PL("Nie udało się zachować pełnego nagrania i listy w folderze surowe."); repaint(); return;
            }
            cutPanel.load(file,review.rawAudio(),sr); resized();
            note=PL("Wycięto 0 fragmentów. Obie wersje pozostają w nagraniu. Pełne nagranie jest w folderze surowe.");
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,PL("Poprawki po R"),note);
            repaint(); return;
        }
        juce::String message=PL("Wycięto ")+juce::String(result.cuts)+PL(" fragmentów (")
            +juce::String(result.removedSec,2)+PL(" s). Pełne nagranie jest w folderze surowe.");
        if(result.audibleSeams>0) message+=PL(" Przy ")+juce::String(result.audibleSeams)
            +PL(" szwach tło się zmieniło — posłuchaj tych miejsc.");
        // Archiwizacja TSV jest wykonywana przez applyCutList. Jesli sie nie
        // udala, nie wolno proponowac starych czasow na juz przycietym pliku.
        const bool staleList=zs::cutListFile(file).existsAsFile() && !zs::writeCutList(file,{});
        if(staleList) {
            message+=PL(" Nie udało się usunąć starej listy cięć — sprawdź plik .ciecia.tsv przed ponownym otwarciem.");
        }
        load(file,{},false);
        if(staleList) { cutPanel.load(juce::File(),{},sr); resized(); }
        note=message;
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,PL("Poprawki po R"),message);
        repaint();
    }
    CutReviewPanel cutPanel;
    zs::Audition seamPlayer;
    bool seamPlaying=false;
    Recorder& recorder;

    juce::AudioFormatManager formats;

    zs::TakeReview review;
    mix::Decide::Wish wish = zs::TakeReview::podcastWish();   ///< w tym wynik kreatora — patrz setRoom()
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
    bool editMode=false,detailsMode=false,middleMode=false,pendingSave=false,doneMode=false;
    juce::TextButton doneEditButton,newTakeButton;
    juce::TextEditor savedPath,savedName;
    TrimSlider trimStart,trimEnd;
    TrimHandle startHandle,endHandle;
    juce::TextButton middleToggle;
    juce::String reviewSummary;
    juce::Rectangle<int> summaryArea, feedbackArea;
    juce::Label selection;
    double cutFrom=-1,cutTo=-1;
    std::vector<float> undoAudio;
    juce::TextButton folderButton;
    juce::uint32 busySinceMs = 0;   ///< kiedy zaczelo sie sluchanie/miks; 0 = nic nie trwa
    float dragX0 = -1.0f;           ///< poczatek przeciagniecia w montazu; < 0 = brak

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReviewComponent)
};
