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
#include "Mp3.h"
#include "MixChainView.h"
#include "ReadMono.h"
#include "CutReviewPanel.h"
#include "Backing.h"
#include "Trace.h"
#include "DSP/BackingEdit.h"
#include "DSP/Mix/MusicBed.h"
#include <limits>
#include <memory>
#include <thread>

class MixStyleCard : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;
    void setText (juce::String title, juce::String description)
    { heading = std::move (title); detail = std::move (description); repaint(); }
    void setSelected (bool s) { selected = s; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    { if (e.mouseWasClicked() && isEnabled() && onClick) onClick(); }
    void paint (juce::Graphics& g) override
    {
        using I = zs::Ink;
        auto r = getLocalBounds().toFloat();
        if (isMouseOver() && isEnabled()) r = r.reduced (0.5f);
        if (! selected && isEnabled()) zs::Depth::shadow (g, r, 8.0f, 5.0f, 0.25f, 2.0f);
        g.setColour (selected ? I::panel().brighter (0.08f) : I::panel());
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (selected ? I::accent() : I::edge());
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, selected ? 2.0f : 1.0f);
        auto copy = getLocalBounds().reduced (12, 8);
        g.setColour (I::textNow()); g.setFont (juce::Font (zs::type::small, juce::Font::bold));
        g.drawText (heading, copy.removeFromTop (24), juce::Justification::centredLeft, true);
        g.setColour (I::textNext()); g.setFont (juce::Font (zs::type::small));
        g.drawFittedText (detail, copy, juce::Justification::topLeft, 2);
    }
private:
    juce::String heading, detail;
    bool selected = false;
};

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
    std::function<void(double, double, const std::vector<float>&, double)> onReplaceSelection;
    std::function<void()> onUndoReplacement;
    void setReplacementUndoAvailable (bool available) { replacementUndoAvailable = available; updateMontageButtons(); }
    void showReplacementSeams (double from, double to)
    {
        replacementSeams = { juce::jmax (0.0, from), juce::jmax (from, to) };
        assessment.setText (PL ("Po poprawce zaznaczyłem dwa nowe szwy. Odsłuchaj je przyciskiem „Następne miejsce” albo klawiszem N."), false);
        updateMontageButtons(); repaint();
    }
    /** Przejście do przygotowania z paska kroków; bieżący montaż zostaje w pamięci. */
    std::function<void()> onGoToPreparation;
    /** Powrót do istniejącego ekranu startowego z paska workflow. */
    std::function<void()> onGoToStart;
    std::function<void()> onOpenSettings;
    /** "Nowy projekt" na ekranie Gotowe: do ekranu startowego. */
    std::function<void()> onNewProject;

    /** Co kreator zmierzyl w pokoju. Ustawiac przed load(): miks szuka wtedy
        w nagraniu przydzwieku, ktory slyszal kreator, i porownuje tlo. */
    void setRoom (const mix::RoomCheck& r) { wish.room = r; }

    /** Wzorzec brzmienia z ustawien (pusty = brak). */
    void setVoiceProfile (const mix::VoiceProfile& p) { wish.profile = p; }
    /** "Zapamietaj to brzmienie": tekst wzorca do zapisania w ustawieniach. */
    std::function<void (const juce::String&)> onRememberProfile;

    //==========================================================================
    explicit ReviewComponent (Recorder& r)
        : juce::Thread ("miks po nagraniu"), recorder (r)
    {
        setWantsKeyboardFocus (true);

        settingsButton.setButtonText (PL("Ustawienia"));
        settingsButton.onClick = [this] { if (onOpenSettings) onOpenSettings(); };
        addAndMakeVisible (settingsButton);

        formats.registerBasicFormats();

        setPlayLabel (false);
        playButton.onClick = [this] { togglePlay(); };
        addAndMakeVisible (playButton);

        abButton.onClick = [this] { toggleAb(); };
        addAndMakeVisible (abButton);

        // Trzy brzmienia (Decide::Style), wszystkie do tej samej glosnosci —
        // przelaczenie liczy miks od nowa i zostaje do konca sesji.
        for (int i = 0; i < 3; ++i)
        {
            const auto st = static_cast<mix::Decide::Style> (i);
            styleCards[i].setText (PL (mix::Decide::styleName (st)),
                st == mix::Decide::Style::Delikatny ? PL ("Naturalny głos, tylko lekko wyrównany.")
              : st == mix::Decide::Style::Mocny ? PL ("Gęściej i wyraźniej, jak w radiu.")
                                                : PL ("Równe, bliskie brzmienie do podcastu."));
            styleCards[i].onClick = [this, st] { chooseStyle (st); };
            styleCards[i].setTooltip (st == mix::Decide::Style::Delikatny ? PL ("Naturalnie: głos prawie bez zmian, tylko równiejszy")
                                      : st == mix::Decide::Style::Mocny    ? PL ("Radiowo: gęściej i bliżej")
                                                                           : PL ("Wyrównany: jak w dobrym podcaście"));
            addAndMakeVisible (styleCards[i]);
        }
        updateStyleButtons();

        // Podklad (25.10): glosnosc muzyki wzgledem glosu i sciszanie pod glosem.
        // Zmiana sklada od nowa tylko glos z muzyka — miks glosu zostaje.
        static const char* const levelNames[3] = { "Ciszej", "Średnio", "Głośniej" };
        for (int i = 0; i < 3; ++i)
        {
            musicLevel[i].setButtonText (PL (levelNames[i]));
            musicLevel[i].setTooltip (PL ("Muzyka ") + juce::String (-musicLevels[i], 0) + PL (" dB pod głosem"));
            musicLevel[i].onClick = [this, i] { bedParams.musicDb = musicLevels[i]; remixBed(); };
            addChildComponent (musicLevel[i]);
        }
        duckButton.setButtonText (PL ("Ściszaj pod głosem"));
        duckButton.setTooltip (PL ("Gdy mówisz, muzyka schodzi o 4 dB i wraca w przerwach"));
        duckButton.onClick = [this] { bedParams.duck = duckButton.getToggleState(); remixBed(); };
        addChildComponent (duckButton);
        updateMusicButtons();

        saveButton.setButtonText (PL ("Zapisz miks"));
        saveButton.onClick = [this] { if (editMode) goToMix(); else if (review.isPreview()) acceptPreview(); else save(); };
        addAndMakeVisible (saveButton);

        backButton.setButtonText (PL ("Wróć do nagrywania"));
        backButton.onClick = [this] { if (job == nullptr && onClose) onClose(); };
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
        words.setFont (juce::Font (zs::type::small));
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
        assessment.setIndents (0, 4);                      // rowno z naglowkiem panelu (bylo 5 px w prawo)
        addAndMakeVisible (assessment);
        // Miks na koncu (ustalone 13.10, ZADANIA zad. 11): najpierw odsluch i montaz
        // SUROWEGO nagrania, potem raz liczony miks z wyborem brzmienia, potem zapis.
        flow.setSteps ({ PL("Ekran startowy"), PL("Przygotowanie"), PL("Nagranie"),
                         PL("Odsłuch i montaż"), PL("Miks"), PL("Gotowe") });
        flow.setTileMode();
        flow.canClickStep = [this] (int step)
        {
            if (isThreadRunning() || job != nullptr) return false;
            if (step == 0 || step == 1) return true;
            if (step == 3) return ! editMode;
            if (step == 4) return doneMode && review.stage() == zs::TakeReview::Stage::Ready;
            return false;
        };
        flow.onStepClick = [this] (int step)
        {
            if (step == 0) { if (onGoToStart) onGoToStart(); }
            else if (step == 1) { if (onGoToPreparation) onGoToPreparation(); }
            else if (step == 3) setEditMode (true);
            else if (step == 4) setEditMode (false);
        };
        addAndMakeVisible (flow);

        editToggle.setButtonText(PL("← Wróć do montażu"));
        editToggle.onClick=[this] { setEditMode (true); };
        detailsToggle.setButtonText(PL("Zaawansowane ustawienia"));
        detailsToggle.onClick=[this] { detailsMode=!detailsMode;resized();repaint(); };
        // Tor miksu: kazda zmiana to miks od nowa w tle, jak zmiana brzmienia.
        defaultsButton.setButtonText (PL ("Przywróć ustawienia domyślne"));
        defaultsButton.onClick = [this] { chainView.resetAll(); };
        addChildComponent (defaultsButton);
        chainView.onChange = [this] (const mix::Tweaks& t)
        {
            tweaks = t;
            defaultsButton.setEnabled (chainView.canReset());
            if (! mixReady()) return;
            note = PL ("Miks z Twoimi zmianami liczy się…");
            styleNote = true;
            startMix();
            repaint();
        };
        chainPort.setViewedComponent (&chainView, false);
        chainPort.setScrollBarsShown (true, false);
        addChildComponent (chainPort);
        nextButton.setButtonText(PL("Następne miejsce")); zs::Look::setKey(nextButton,"N");
        nextButton.onClick=[this] { nextTrouble(); };
        // z podkladem: w montazu do wyboru sam glos albo glos z muzyka (Adrian 25.10)
        withMusicButton.setWantsKeyboardFocus (false);
        withMusicButton.onClick = [this] {
            editWithMusic = ! editWithMusic;
            const double at = player.positionSec(); const bool was = player.isPlaying();
            player.pause(); setRawMaterial(); player.seekSec (at); if (was) player.play();
        };
        addChildComponent (withMusicButton);
        // Powiekszenie fali: wokol igly (kolko myszy — wokol kursora).
        zoomIn.setButtonText ("+"); zoomOut.setButtonText (PL ("−")); zoomAllButton.setButtonText (PL ("Całość"));
        zoomIn.setTooltip (PL ("Powiększ falę (kółko myszy albo +)"));
        zoomOut.setTooltip (PL ("Pomniejsz falę (kółko myszy albo −)"));
        zoomAllButton.setTooltip (PL ("Całe nagranie (0)"));
        zoomIn.onClick  = [this] { if (canEdit()) zoomBy (1.0 / zs::ui::review::zoomStep, player.positionSec()); };
        zoomOut.onClick = [this] { if (canEdit()) zoomBy (zs::ui::review::zoomStep, player.positionSec()); };
        zoomAllButton.onClick = [this] { zoomAll(); };
        for (auto* b : { &zoomIn, &zoomOut, &zoomAllButton }) { b->setWantsKeyboardFocus (false); addChildComponent (b); }
        addChildComponent (jobShield);
        for(auto* b:{&editToggle,&detailsToggle,&nextButton})addAndMakeVisible(b);
        markStart.setButtonText(PL("Początek cięcia"));
        markEnd.setButtonText(PL("Koniec cięcia"));
        cutButton.setButtonText(PL("Wytnij zaznaczenie"));
        undoButton.setButtonText(PL("Cofnij cięcie"));
        replaceButton.setButtonText (PL("Nagraj poprawkę"));
        replaceButton.setTooltip (PL("Nagraj ten fragment od nowa. Zaznacz go na fali."));
        replaceButton.onClick = [this] {
            if (! canEdit() || cutFrom < 0.0 || cutTo <= cutFrom || ! onReplaceSelection) return;
            player.pause();
            onReplaceSelection (cutFrom, cutTo, review.rawAudio(), sr);
        };
        markStart.onClick=[this] { if(canEdit()) { player.pause(); cutFrom=player.positionSec(); updateSelection(); } };
        markEnd.onClick=[this] { if(canEdit()) { player.pause(); cutTo=player.positionSec(); updateSelection(); } };
        cutButton.onClick=[this] {
            if(!canEdit())return;
            if(!(cutFrom>=0 && cutTo>cutFrom && cutTo-cutFrom<review.seconds())) {
                note=PL("Wskaż początek, potem późniejszy koniec cięcia na osi czasu.");repaint();return;
            }
            const double from=cutFrom,to=cutTo,rate=sr;
            const bool mute=withMusic;
            restartEdit([this,from,to,rate,mute](std::vector<float>& out) {
                // z podkladem nie usuwamy czasu, tylko wyciszamy: muzyka jest
                // nieruszalna, glos po cieciu zostaje w swoim takcie (25.10)
                if(mute) { auto a=review.rawAudio(); if(!tw::BackingEdit::muteRange(a,rate,from,to)) return false; out=std::move(a); return true; }
                std::vector<float> edited;
                if(!zs::cutRange(review.rawAudio(),rate,from,to,edited)) return false;
                out=std::move(edited); return true;
            }, true);
        };
        undoButton.onClick=[this] {
            if (! canEdit()) return;
            if (replacementUndoAvailable && onUndoReplacement) { onUndoReplacement(); return; }
            if (undoAudio.empty()) return;
            auto back=std::make_shared<std::vector<float>>(std::move(undoAudio)); undoAudio.clear();
            restartEdit([back](std::vector<float>& out) { out=std::move(*back); return true; }, false);
        };
        for(auto* b:{&markStart,&markEnd,&cutButton,&undoButton,&replaceButton}) addAndMakeVisible(b);
        selection.setColour(juce::Label::textColourId,Ink::textNext());
        selection.setText(PL("Przeciągnij po fali, aby zaznaczyć fragment do wycięcia. Oryginał zostaje na dysku."),juce::dontSendNotification);
        // Gdzie jest plik: pytanie, ktore padlo przy pierwszym prawdziwym
        // nagraniu ("nie wiem, gdzie mi sie zapisala sesja").
        folderButton.setButtonText (PL("Pokaż w folderze"));
        folderButton.onClick = [this] { (doneMode ? saved : (saved.existsAsFile() ? saved : file)).revealToUser(); };
        addAndMakeVisible (folderButton);
        doneEditButton.setButtonText (PL("← Wróć do miksu"));
        doneEditButton.onClick = [this] { setEditMode (false); };
        newTakeButton.setButtonText (PL("Nagraj nowe podejście"));
        newTakeButton.setTooltip (PL ("Nowe nagranie od początku. Aby poprawić fragment, zaznacz go na fali."));
        newTakeButton.onClick = [this] { if (job == nullptr && onClose) onClose(); };
        newProjectButton.setButtonText (PL("Nowy projekt"));
        newProjectButton.setTooltip (PL("Ekran startowy: inna nazwa, inny rodzaj nagrania. Ten projekt zostaje na dysku."));
        newProjectButton.onClick = [this] { if (job == nullptr && onNewProject) onNewProject(); };
        addAndMakeVisible (doneEditButton); addAndMakeVisible (newTakeButton); addChildComponent (newProjectButton);
        profileButton.onClick = [this] { rememberProfile(); };
        addAndMakeVisible (profileButton);
        for (auto* editor : { &savedPath, &savedName }) {
            editor->setMultiLine (true); editor->setReadOnly (true);
            editor->setCaretVisible (false); editor->setScrollbarsShown (true);
            editor->setFont (juce::Font (zs::type::small));
            editor->setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
            editor->setColour (juce::TextEditor::textColourId, Ink::textNext());
            editor->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
            editor->setColour (juce::TextEditor::focusedOutlineColourId, Ink::edge());
            editor->setIndents (0, 0);                      // rowno z etykietami nad nim
            addAndMakeVisible (*editor);
        }
        addAndMakeVisible(selection);
        middleToggle.setButtonText (PL("Wytnij fragment ze środka"));
        middleToggle.onClick = [this] { middleMode = !middleMode; resized(); repaint(); };
        addAndMakeVisible (middleToggle);
        for (auto* slider : { &trimStart, &trimEnd }) {
            slider->setSliderStyle (juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider->setColour (juce::Slider::thumbColourId, Ink::accent());
            slider->setColour (juce::Slider::trackColourId, Ink::accent());
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
        abandonJob();
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
               bool watchedLive, bool waitForIt = false, std::function<void()> then = nullptr)
    {
        signalThreadShouldExit();
        waitForThreadToExit(-1);
        abandonJob();
        ready.store(false);
        seamPlayer.clear(); seamPlaying=false;
        player.clear();
        recorder.attachPlayer (nullptr);

        undoAudio.clear(); replacementUndoAvailable = false; replacementSeams.clear(); cutFrom=cutTo=-1;editMode=detailsMode=middleMode=doneMode=false;
        words.clear(); assessment.clear(); reviewSummary.clear(); resized();
        for (auto* b : { &playButton, &abButton, &saveButton, &editToggle, &nextButton,
                         &markStart, &markEnd, &cutButton, &undoButton }) b->setEnabled (false);
        cutPanel.load(wavFile, {}, 48000);
        file  = wavFile;
        musicFile = zs::BackingTrack::musicFor (wavFile);
        withMusic = musicFile != juce::File();
        music = {}; bed = {}; bedRaw = {}; musicEnv = {}; musicProblem.clear();
        musicLufs = std::numeric_limits<double>::quiet_NaN(); queuedStyle = -1;
        cutButton.setButtonText (withMusic ? PL("Wycisz zaznaczenie") : PL("Wytnij zaznaczenie"));
        middleToggle.setButtonText (withMusic ? PL("Wycisz fragment ze środka") : PL("Wytnij fragment ze środka"));
        selection.setText (withMusic ? PL("Przeciągnij po fali, aby zaznaczyć fragment do wyciszenia. Oryginał zostaje na dysku.")
                                     : PL("Przeciągnij po fali, aby zaznaczyć fragment do wycięcia. Oryginał zostaje na dysku."),
                           juce::dontSendNotification);
        saved = juce::File(); savedMp3 = juce::File();
        folderButton.setTooltip ({});
        note.clear();
        // stara kopia nie jest juz potrzebna (godzina = 0,7 GB), a fala nie
        // moze pokazywac poprzedniego nagrania pod napisem "wczytuje"
        review.setTake ({}, 48000.0, {});
        beforeEnv = {}; afterEnv = {};
        editMode = ! waitForIt;         // pod napisem "wczytuje" krok montazu, nie miksu
        resized();

        // Odczyt i przesluchanie w tle: godzina nagrania to ok. minuty pracy.
        struct Out { juce::String error; std::vector<float> mono; double rate = 48000.0;
                     std::vector<tw::Event> events; zs::Envelope env;
                     zs::BackingTrack music; juce::String musicError; };
        auto out = std::make_shared<Out>();
        auto reader = std::shared_ptr<juce::AudioFormatReader> (formats.createReaderFor (wavFile));
        (void) events; (void) watchedLive;   // plik jest wazniejszy niz zdarzenia z nagrywania

        double expect = 0.0;
        if (reader != nullptr && reader->sampleRate > 0)
            expect = (double) reader->lengthInSamples / reader->sampleRate;

        runJob (PL ("Wczytuję i słucham nagrania…"), zs::ui::review::listenEstimateSec (expect),
            [out, reader, stop = &jobStop, musicPath = musicFile]
            {
                if (reader == nullptr) { out->error = "read"; return; }
                if (reader->lengthInSamples <= 0 || reader->lengthInSamples > std::numeric_limits<int>::max()
                    || reader->sampleRate <= 0 || reader->numChannels == 0) { out->error = "size"; return; }

                // Nagrywamy mono (Recorder::writeMono), ale jesli kiedys bedzie inaczej,
                // srednia kanalow jest lepsza niz wziecie pierwszego i zdziwienie.
                // Kawalkami, bez drugiej kopii w buforze (ReadMono.h — godzina to 0,7 GB).
                if (! zs::readMono (*reader, (int) reader->lengthInSamples, out->mono, stop)) { out->error = "part"; return; }
                out->rate = reader->sampleRate;

                // Zdarzenia z nagrywania moga sie spoznic albo zgubic, wiec
                // przesluchujemy plik tym samym wykrywaczem.
                out->events = zs::TakeReview::listenOffline (out->mono, out->rate, stop);
                if (stop->load()) return;
                out->env.build (out->mono, out->rate, 1400);
                // podklad w czestotliwosci nagrania: do fali, odsluchu i miksu
                if (musicPath != juce::File())
                    out->musicError = musicPath.existsAsFile() ? out->music.load (musicPath, out->rate)
                                    : PL ("Nie ma pliku muzyki „") + musicPath.getFileName() + PL ("” — miks bez podkładu.");
            },
            [this, out, waitForIt, then, name = wavFile.getFileName()]
            {
                if (out->error.isNotEmpty())
                {
                    note = out->error == "read" ? PL ("Nie mogę odczytać ") + name + "."
                         : out->error == "size" ? PL ("Plik jest pusty lub za duży do wczytania.")
                                                : PL ("Nie udało się odczytać całego nagrania.");
                    review.setTake ({}, 48000.0, {});
                    editMode = false;
                    resized(); repaint();
                    return;
                }

                sr = out->rate;
                if (! out->events.empty())
                    note = PL ("Nagranie wczytane z pliku — przesłuchałem je od nowa.");

                review.setTake (std::move (out->mono), sr, std::move (out->events));
                ++takeVersion; viewLen = 0.0; viewFrom = 0.0;
                loadedVersion = takeVersion;     // to, co na dysku — nic do stracenia

                cutPanel.load(file, review.rawAudio(), sr);
                beforeEnv = std::move (out->env);
                afterEnv = {};
                music = std::move (out->music);
                musicProblem = out->musicError;
                buildMusicEnv();

                // Krok 3: odsluch i montaz na surowym nagraniu. Miks liczy sie dopiero
                // po "Dalej: miks" — raz, na koncu, a nie po kazdym cieciu.
                editMode = true;
                setRawMaterial();
                resetTrim();
                assessment.setText (montageSummary(), false);
                updateMontageButtons();

                if (waitForIt)                  // tylko zrzuty ekranu przy budowaniu
                {
                    mixTweaks = tweaks;
                    mixBedParams = bedParams;
                    review.run (wish, mixTweaks, previewSec);
                    if (hasMusic() && review.mixValid()) renderBed();
                    adoptResult();
                }

                resized();
                repaint();
                if (then) then();
            },
            waitForIt);
    }

    /** true = krok "Odsluch i montaz" (surowe nagranie), false = krok "Miks". */
    void setEditMode (bool enabled)
    {
        if (! enabled) { goToMix(); return; }
        if (isThreadRunning() || job != nullptr) return;   // miks albo zapis wlasnie trwa
        player.pause(); stopSeam();
        doneMode = false; editMode = true; detailsMode = false;
        setRawMaterial();
        assessment.setText (montageSummary(), false);
        resized(); repaint();
    }

    // Podglad przechodzi ta sama droga zapisu co zwykly przycisk.
    /** --preview-ui tor: szczegoly miksu z torem (Delikatny, jedna zmiana, jeden modul wlaczony recznie). */
    void previewChain()
    {
        if (editMode) return;
        detailsMode = true;
        // Delikatny: wiekszosc modulow nieuzyta; korektor wlaczony recznie
        wish.style = mix::Decide::Style::Delikatny;
        updateStyleButtons();
        tweaks = {};
        tweaks.amount[(size_t) mix::Stage::Odszumianie] = 0.5f;
        tweaks.forced[(size_t) mix::Stage::Korektor] = true;
        mixTweaks = tweaks;
        review.run (wish, mixTweaks);
        adoptResult();
        resized(); repaint();
    }
    void previewDone() { if (! editMode) { previewSave = true; save(); previewSave = false; } }
    void previewReplacementSelection()
    {
        if (! editMode || review.seconds() < 2.0) return;
        middleMode = true;
        cutFrom = review.seconds() * 0.28;
        cutTo = juce::jmin (review.seconds() * 0.40, cutFrom + 3.0);
        updateSelection(); resized(); repaint();
    }

    bool hasTake() const { return review.stage() != zs::TakeReview::Stage::Empty; }

    /** Montaz w pamieci (wyciecie, przyciecie), ktory nie trafil do zapisanego
        miksu — zamkniecie programu by go zgubilo. Oryginal i decyzje po R sa
        na dysku zawsze, wiec ich to nie dotyczy. */
    bool hasUnsavedEdits() const { return takeVersion != loadedVersion && savedVersion != takeVersion; }
    juce::File sourceFile() const { return file; }

    void end()
    {
        zs::trace ("odsluch end: pauza");
        player.pause(); seamPlayer.pause(); seamPlaying=false;
        zs::trace ("odsluch end: odpinam odtwarzanie od karty");
        recorder.attachPlayer (nullptr);
        zs::trace ("odsluch end: odpiete");
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
        if (job != nullptr) return true;      // praca w tle: plik sesji wlasnie sie zmienia
        if (doneMode) {
            if (c == juce::KeyPress::escapeKey) { setEditMode (false); return true; }
            return false;
        }

        if (cutPanel.isVisible() && cutPanel.keyPressed(k)) return true;
        if (c == juce::KeyPress::spaceKey)     { togglePlay(); return true; }
        if (c == juce::KeyPress::tabKey)       { if (! editMode) toggleAb(); return true; }
        // Esc cofa o krok: z miksu do montazu, z montazu do nagrywania
        if (c == juce::KeyPress::escapeKey)    { if (! editMode) setEditMode (true); else if (onClose) onClose(); return true; }

        if (editMode && (c == juce::KeyPress::deleteKey || c == juce::KeyPress::backspaceKey)
            && cutFrom >= 0 && cutTo > cutFrom && cutButton.onClick) { cutButton.onClick(); return true; }
        if (editMode && (c == juce::KeyPress::leftKey || c == juce::KeyPress::rightKey)) {
            trimStart.setValue (trimStart.getValue() + (c == juce::KeyPress::leftKey ? -0.1 : 0.1)); return true;
        }
        if (c == juce::KeyPress::leftKey)  { player.seekSec (player.positionSec() - 2.0); follow (player.positionSec()); return true; }
        if (c == juce::KeyPress::rightKey) { player.seekSec (player.positionSec() + 2.0); follow (player.positionSec()); return true; }
        if (c == juce::KeyPress::homeKey)  { player.seekSec (0.0); follow (0.0); return true; }

        // Skok do nastepnego miejsca, ktore wymaga powtorzenia. Gdy jest ich
        // kilka, przeklikanie listy jest tym, co czlowiek i tak zrobi — wiec
        // niech bedzie jeden klawisz, a nie polowanie myszka na paski.
        if (editMode && canEdit())
        {
            const auto ch = k.getTextCharacter();
            if (ch == '+' || ch == '=') { zoomBy (1.0 / zs::ui::review::zoomStep, player.positionSec()); return true; }
            if (ch == '-')              { zoomBy (zs::ui::review::zoomStep, player.positionSec()); return true; }
            if (ch == '0')              { zoomAll(); return true; }
        }

        if (k.getTextCharacter() == 'n' || k.getTextCharacter() == 'N')
        {
            nextTrouble();
            return true;
        }

        return false;
    }

    /* Mysz na przebiegu.

       Miks: klikniecie = posluchaj od tego miejsca.
       Odsluch i montaz: klikniecie = posluchaj od tego miejsca (po puszczeniu
       przycisku), przeciagniecie zaznacza fragment do wyciecia. Dawniej klikniecie w montazu od razu gralo, a
       granice ciecia ustawialo sie przyciskami w miejscu, w ktorym akurat
       bylo odtwarzanie — Adrian: "nie mam mozliwosci wskazania myszka
       poczatku i konca ciecia". Liczby: zs::ui::review::dragRange. */
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (doneMode) return;
        const auto strip = waveArea();
        dragX0 = -1.0f;

        if (! strip.contains (e.getPosition()) || review.seconds() <= 0.0
            || ! (editMode ? canEdit() : mixReady()))
            return;

        const double t = tAt (e.position.x, strip);

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
        const auto v = view();
        const auto r = zs::ui::review::dragRange (dragX0, e.position.x, strip.getX(),
                                                  strip.getWidth(), v.len);
        if (! r.isDrag()) return;

        cutFrom = v.from + r.from;
        cutTo   = v.from + r.to;

        if (! middleMode) { middleMode = true; resized(); }
        updateSelection();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        // klikniecie bez przeciagniecia w montazu: posluchaj od tego miejsca
        if (editMode && dragX0 >= 0.0f && std::abs (e.position.x - dragX0) < 4.0f && ! player.isPlaying())
            player.play();
        dragX0 = -1.0f;
    }

    /** Montaz: kolko nad fala powieksza wokol kursora, z Shiftem (albo
        kolko poziome) przesuwa widok. */
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const auto strip = waveArea();
        if (! editMode || doneMode || ! strip.contains (e.getPosition()) || ! canEdit()) return;
        const float dx = e.mods.isShiftDown() ? w.deltaY : w.deltaX;
        if (std::abs (dx) > std::abs (e.mods.isShiftDown() ? 0.0f : w.deltaY))
        {
            const auto v = view();
            viewFrom = v.from - dx * v.len * 0.5;
            zoomChanged();
            return;
        }
        if (w.deltaY == 0.0f) return;
        zoomBy (w.deltaY > 0 ? 1.0 / zs::ui::review::zoomStep : zs::ui::review::zoomStep, tAt (e.position.x, strip));
    }

    //==========================================================================
    /* Powiekszenie fali (audyt 14.10). Tylko w montazu — tam sie tnie; miks
       zawsze pokazuje cale nagranie. viewLen 0 = cale nagranie. */
    struct View { double from = 0.0, len = 1.0; };
    /** Os czasu ekranu: w miksie z podkladem do konca muzyki (gra dluzej niz glos). */
    double timelineSec() const
    {
        return ! editMode && ! bed.left.empty() && sr > 0.0 ? juce::jmax (review.seconds(), (double) bed.left.size() / sr)
                                                            : review.seconds();
    }
    View view() const
    {
        const double total = timelineSec();
        if (! editMode || total <= 0.0 || viewLen <= 0.0 || viewLen >= total)
            return { 0.0, juce::jmax (0.001, total) };
        const double len = juce::jlimit (juce::jmin (zs::ui::review::minViewSec, total), total, viewLen);
        return { juce::jlimit (0.0, total - len, viewFrom), len };
    }
    bool zoomed() const { return view().len < review.seconds() - 1.0e-6; }
    float xAt (double t, juce::Rectangle<int> r) const
    {
        const auto v = view();
        return r.getX() + (float) ((t - v.from) / v.len * r.getWidth());
    }
    double tAt (float x, juce::Rectangle<int> r) const
    {
        const auto v = view();
        return v.from + juce::jlimit (0.0, 1.0, (double) (x - r.getX()) / juce::jmax (1, r.getWidth())) * v.len;
    }
    /** factor < 1 = blizej; anchor zostaje w tym samym miejscu ekranu. */
    void zoomBy (double factor, double anchor)
    {
        const double total = review.seconds();
        if (! editMode || total <= 0.0) return;
        const auto v = view();
        const double len = juce::jlimit (juce::jmin (zs::ui::review::minViewSec, total), total, v.len * factor);
        const double rel = (anchor - v.from) / v.len;
        viewLen = len;
        viewFrom = anchor - rel * len;
        zoomChanged();
    }
    void zoomAll() { viewLen = 0.0; viewFrom = 0.0; zoomChanged(); }
    void zoomChanged()
    {
        const auto v = view();
        viewFrom = v.from;
        if (v.len >= review.seconds() - 1.0e-6) viewLen = 0.0;
        resized();      // przyciski i uchwyty (uchwyt poza widokiem znika)
        repaint();
    }
    /** Odtwarzanie albo skok poza widok: widok idzie za igla. */
    void follow (double t)
    {
        if (! zoomed()) return;
        const auto v = view();
        if (t >= v.from && t <= v.from + v.len) return;
        viewFrom = t - v.len * 0.1;
        zoomChanged();
    }
    /** Odtwarzanie: strona dalej tylko wtedy, gdy igla byla w widoku i wyszla
        za prawa krawedz. Dawniej follow() co klatke — nie dalo sie przewinac
        widoku w trakcie odtwarzania, wracal do igly (przeglad 19.10). */
    void followPlayback (double t)
    {
        const auto v = view();
        const bool wasIn = followPrev >= v.from && followPrev <= v.from + v.len;
        followPrev = t;
        if (zoomed() && wasIn && t > v.from + v.len) { viewFrom = t - v.len * 0.1; zoomChanged(); followPrev = t; }
    }
    /** Fala widoku liczona z probek tylko dla tego, co widac. */
    const zs::Envelope& viewEnvelope (int columns)
    {
        const auto v = view();
        if (viewEnvFrom != v.from || viewEnvLen != v.len || viewEnvCols != columns || viewEnvVersion != takeVersion)
        {
            viewEnv.buildRange (review.rawAudio(), sr, columns, v.from, v.from + v.len);
            viewEnvFrom = v.from; viewEnvLen = v.len; viewEnvCols = columns; viewEnvVersion = takeVersion;
        }
        return viewEnv;
    }

    /* W trakcie sluchania i miksu: komunikat na caly ekran.

       Maly napis "Miksuje…" w rogu przeoczyl pierwszy czlowiek, ktory nagral
       4,5 minuty — klikal wszedzie i uznal, ze program sie zawiesil. Miks
       takiego nagrania trwa okolo minuty, wiec ekran ma mowic wprost: pracuje,
       ile minelo, ile zwykle trwa. Procentow nie pokazujemy: lancuch nie
       raportuje postepu w trakcie, pasek stalby na 35% przez pol minuty. */
    void paintOverChildren (juce::Graphics& g) override
    {
        if (job != nullptr) { drawJob (g); return; }
        const auto st = review.stage();
        // tylko gdy watek naprawde liczy: w montazu etap jest "Listening" (miksu
        // jeszcze nie bylo), a nakladka zaslonilaby caly ekran
        if (! isThreadRunning() || (st != zs::TakeReview::Stage::Listening && st != zs::TakeReview::Stage::Mixing)) return;

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
        g.setColour (Ink::accent());
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

    /** Praca w tle (runJob): ten sam wyglad co "Miksuje…", inny napis. */
    void drawJob (juce::Graphics& g)
    {
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
        g.drawText (jobLabel, r.removeFromTop (40), juce::Justification::centredLeft);

        r.removeFromTop (10);
        const auto bar = r.removeFromTop (8).toFloat();
        g.setColour (Ink::raised()); g.fillRoundedRectangle (bar, 4.0f);
        const float phase = (float) (juce::Time::getMillisecondCounter() % 1600u) / 1600.0f;
        const float w = bar.getWidth() * 0.28f;
        const float x = bar.getX() - w + phase * (bar.getWidth() + w);
        g.setColour (Ink::accent());
        g.fillRoundedRectangle (juce::Rectangle<float> (x, bar.getY(), w, bar.getHeight())
                                    .getIntersection (bar), 4.0f);

        r.removeFromTop (16);
        const int elapsed = jobSinceMs == 0 ? 0
                          : (int) ((juce::Time::getMillisecondCounter() - jobSinceMs) / 1000u);
        juce::String text = PL ("Minęło ") + juce::String (elapsed) + " s.";
        if (jobExpectSec > 0) text << PL (" Zwykle trwa to do ") << juce::String (jobExpectSec) << " s.";
        text << "\n" << PL ("Program pracuje — nie zamykaj go. Ekran sam się odświeży.");

        g.setColour (Ink::textNext());
        g.setFont (juce::Font (zs::type::body));
        g.drawFittedText (text, r, juce::Justification::topLeft, 3);
    }

    //==========================================================================
    void resized() override
    {
        jobShield.setBounds (getLocalBounds()); jobShield.toFront (false);
        cutPanel.setVisible(!doneMode && editMode && !cutPanel.empty());   // decyzje po R: w montazu
        chainPort.setVisible (false);                           // tylko w szczegolach miksu
        for (auto& b : styleCards) b.setVisible (false);
        for (auto& b : musicLevel) b.setVisible (false);
        duckButton.setVisible (false); defaultsButton.setVisible (false);
        for (auto* b : { &zoomIn, &zoomOut, &zoomAllButton }) b->setVisible (false);   // tylko w montazu
        withMusicButton.setVisible (false);
        if (doneMode) { layoutDone(); return; }
        doneEditButton.setVisible (false); newTakeButton.setVisible (false); profileButton.setVisible (false); newProjectButton.setVisible (false);
        savedPath.setVisible (false); savedName.setVisible (false);
        playButton.setVisible (true); abButton.setVisible (true);
        saveButton.setVisible (true); editToggle.setVisible (true);
        zs::Look::setKind (folderButton, zs::ui::ButtonKind::Secondary);
        replaceButton.setVisible (false);
        if (editMode) { layoutEdit(); return; }
        namespace L = zs::ui::review;
        for (auto* c : { static_cast<juce::Component*>(&trimStart), static_cast<juce::Component*>(&trimEnd), static_cast<juce::Component*>(&startHandle), static_cast<juce::Component*>(&endHandle), static_cast<juce::Component*>(&middleToggle) }) c->setVisible (false);
        flow.setBounds (zs::ScreenFrame::stepsArea (getLocalBounds()));
        flow.setCurrent (4);                         // krok "Miks"
        settingsButton.setBounds (getWidth() - 196, 14, 174, 36);
        settingsButton.setVisible (true);
        nextButton.setVisible(false); backButton.setVisible(false); detailsToggle.setVisible(true);
        saveButton.setButtonText(review.isPreview() ? PL("Akceptuję — miksuj całość") : PL("Zapisz miks"));
        assessment.setText(reviewSummary, false);
        auto main = zs::ScreenFrame::mainArea (getLocalBounds());
        main.removeFromTop (headH() + waveHeight() + L::strip + 8);
        auto row = main.removeFromTop (L::buttons);
        playButton.setBounds (row.removeFromLeft (132)); row.removeFromLeft (10);
        // Style obok przed/po, gdy sie mieszcza (3 x 96 px); w malym oknie w osobnym
        // wierszu — inaczej przy 960 px byly to puste kafelki po 19 px (przeglad 14.10, nr 4).
        const int styleW = 3 * 104 + 2 * 6, abMin = 200;
        const bool styleInRow = getWidth() >= 1240 && row.getWidth() >= abMin + 16 + styleW;
        abButton.setBounds (row.removeFromLeft (juce::jmin (240, styleInRow ? row.getWidth() - 16 - styleW : row.getWidth())));
        if (styleInRow) row.removeFromLeft (16);
        else { main.removeFromTop (6); row = main.removeFromTop (L::buttons); }
        {
            const int w = juce::jmax (0, (row.getWidth() - 2 * 6) / 3);
            for (int i = 0; i < 3; ++i)
            {
                auto card = row.removeFromLeft (w); row.removeFromLeft (6);
                styleCards[i].setVisible (true);
                styleCards[i].setBounds (card.withHeight (getHeight() < 720 ? 64 : 70));
            }
            main.removeFromTop ((getHeight() < 720 ? 64 : 70) - L::buttons);
        }
        if (hasMusic())
        {
            main.removeFromTop (getHeight() < 720 ? 4 : 8);
            auto mrow = main.removeFromTop (L::buttons);
            musicLabelArea = mrow.removeFromLeft (70);
            const int w = juce::jlimit (0, 104, (mrow.getWidth() - 220) / 3);
            for (auto& b : musicLevel) { b.setVisible (true); b.setBounds (mrow.removeFromLeft (w)); mrow.removeFromLeft (6); }
            mrow.removeFromLeft (10);
            duckButton.setVisible (true);
            duckButton.setBounds (mrow.removeFromLeft (juce::jmin (190, mrow.getWidth())));
            main.removeFromTop (4);
        }
        main.removeFromTop (getHeight() < 720 ? 4 : 8);
        auto tools = main.removeFromBottom (getHeight() < 720 ? 30 : L::tools);
        const int toolW = juce::jmin (220, tools.getWidth() / 2);
        detailsToggle.setButtonText (getWidth() < 1100 ? PL ("Zaawansowane…") : PL ("Zaawansowane ustawienia"));
        detailsToggle.setBounds (tools.removeFromLeft (toolW));
        // obok "Ukryj zaawansowane", nie w rogu nad kaflami - tam nikt go nie widzial (Adrian 25.10)
        defaultsButton.setVisible (detailsMode);
        if (detailsMode) { tools.removeFromLeft (10); defaultsButton.setBounds (tools.removeFromLeft (juce::jmin (250, tools.getWidth() / 2))); defaultsButton.setEnabled (chainView.canReset()); }
        folderButton.setBounds (tools.removeFromRight (juce::jmin (172, tools.getWidth()))); folderButton.setVisible (true);
        tools.removeFromLeft (12); tools.removeFromRight (12); feedbackArea = tools;
        main.removeFromTop (getHeight() < 720 ? 4 : 8);
        if (detailsMode)
        {
            // w malym oknie tor sie przewija; tekst pod nim znika, gdy brak miejsca
            const int need = chainView.heightFor (main.getWidth());
            const int h = juce::jmin (main.getHeight(), need);
            chainPort.setVisible (true);
            chainPort.setBounds (main.removeFromTop (h));
            // bez paska w bok: kafle w dwoch wierszach na szerokosc ekranu (Adrian 25.10)
            const int contentW = main.getWidth() - (need > h ? chainPort.getScrollBarThickness() + 4 : 0);
            chainView.setSize (contentW, chainView.heightFor (contentW));
            main.removeFromTop (8);
        }
        summaryArea = main;
        words.setVisible (detailsMode && main.getHeight() >= 48);
        words.setBounds (main);
        detailsToggle.setEnabled(true);
        if (detailsMode) detailsToggle.setButtonText (getWidth() < 1100 ? PL ("Ukryj zaawansowane") : PL("Ukryj zaawansowane"));

        auto side = zs::ScreenFrame::sidebar (getLocalBounds()).reduced (22, 24);
        side.removeFromTop (32 + 84 + 8);
        auto actions = side.removeFromBottom (80);
        saveButton.setBounds (actions.removeFromBottom (36)); actions.removeFromBottom (8);
        editToggle.setBounds (actions.removeFromBottom (36));
        editToggle.setButtonText (PL("← Wróć do montażu"));
        assessment.setVisible (true);
        assessment.setBounds (side);
        for (auto* b : { &markStart, &markEnd, &cutButton, &undoButton, &replaceButton }) b->setVisible (editMode);
        selection.setVisible (editMode);

    }

    void paint (juce::Graphics& g) override
    {
        if (doneMode) { paintDone (g); return; }
        if (editMode) { paintEdit (g); return; }
        g.fillAll (Ink::bg());
        zs::ScreenFrame::paint (g, getLocalBounds(), file.getFileNameWithoutExtension(),
            PL("Spacja — odtwarzanie · Tab — przed / po · Esc — wróć do montażu"));
        auto side = zs::ScreenFrame::sidebar (getLocalBounds());
        zs::ScreenFrame::paintPanel (g, side);
        auto main = zs::ScreenFrame::paintHead (g, getLocalBounds(), PL("04 / MIKS"), PL("Wybierz brzmienie"), subtitle());
        drawWave (g, main.removeFromTop (waveHeight()));
        drawStrip (g, main.removeFromTop (zs::ui::review::strip));
        if (abButton.isVisible())
            zs::Depth::well (g, abButton.getBounds().toFloat().expanded (4.0f, 3.0f), 9.0f);
        side = side.reduced (22, 24);
        g.setColour (Ink::textNow()); g.setFont (juce::Font (zs::type::heading, juce::Font::bold));
        g.drawText (PL("Co zrobił miks"), side.removeFromTop (32), juce::Justification::centredLeft);
        drawVerdict (g, side.removeFromTop (84));
        g.setColour (Ink::textNext()); g.setFont (juce::Font (zs::type::small));
        if (note.isNotEmpty()) g.drawFittedText (note, feedbackArea, juce::Justification::centredLeft, 2);
        if (hasMusic())
        {
            g.setColour (Ink::textDim()); g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
            g.drawText (PL("MUZYKA"), musicLabelArea, juce::Justification::centredLeft);
        }
        auto summary = summaryArea;
        // sama nazwa pliku — pelna sciezka zajmowala dwa wiersze; folder otwiera "Pokaz w folderze"
        if (!detailsMode && file != juce::File() && summary.getHeight() > 130)
            zs::ScreenFrame::wrapped (g, PL("Nagranie: ") + file.getFileName()
                                        + (saved.existsAsFile() ? PL("   ·   Miks: ") + saved.getFileName() : juce::String()),
                                      summary.removeFromBottom (20), juce::Font (zs::type::small), Ink::textDim());
    }

private:
    juce::Rectangle<int> doneContent() const {
        auto main = zs::ScreenFrame::mainArea (getLocalBounds());
        return main.withSizeKeepingCentre (main.getWidth(), juce::jmin (412, main.getHeight()));
    }
    void layoutDone() {
        for (auto* child : getChildren()) child->setVisible (false);
        flow.setVisible (true);
        flow.setBounds (zs::ScreenFrame::stepsArea (getLocalBounds()));
        flow.setCurrent (5);
        settingsButton.setBounds (getWidth() - 196, 14, 174, 36);
        settingsButton.setVisible (true);
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
        savedName.setBounds (side.removeFromTop (44)); savedName.setVisible (true);
        side = zs::ScreenFrame::sidebar (getLocalBounds()).reduced (22, 24);
        auto actions = side.removeFromBottom (124);
        newProjectButton.setBounds (actions.removeFromBottom (36)); actions.removeFromBottom (8);
        newTakeButton.setBounds (actions.removeFromBottom (36)); actions.removeFromBottom (8);
        doneEditButton.setBounds (actions.removeFromBottom (36));
        newTakeButton.setVisible (true); doneEditButton.setVisible (true); newProjectButton.setVisible (true);
        // nad nimi: wzorzec brzmienia dla kolejnych odcinkow
        side = zs::ScreenFrame::sidebar (getLocalBounds()).reduced (22, 24);
        side.removeFromBottom (124 + 8);
        profileButton.setBounds (side.removeFromBottom (36));
        profileButton.setVisible (true);
        profileButton.setButtonText (profileRemembered ? PL("Brzmienie zapamiętane ✓")
                                   : wish.profile.valid() ? PL("Ustaw to brzmienie jako wzorzec")
                                                          : PL("Zapamiętaj to brzmienie"));
        profileButton.setEnabled (! profileRemembered && saved.existsAsFile());
        profileButton.setTooltip (PL("Następne odcinki dostaną tę barwę — seria brzmi jak z jednego studia. Usunąć wzorzec można w „Mikrofon i sprzęt”."));
    }
    void paintDone (juce::Graphics& g) {
        g.fillAll (Ink::bg());
        zs::ScreenFrame::paint (g, getLocalBounds(), file.getFileNameWithoutExtension(), PL("Plik zapisany na dysku · Esc — wróć do miksu"));
        auto side = zs::ScreenFrame::sidebar (getLocalBounds());
        zs::ScreenFrame::paintPanel (g, side);
        auto main = doneContent();
        // aneks E, pkt 8: kolo #102E26 z obrysem #168B50, znak #39CF7B kreska 5 px
        const auto emblem = main.removeFromTop (68).withSizeKeepingCentre (68, 68).toFloat();
        zs::Depth::glow (g, emblem, emblem.getWidth() / 2, Ink::green(), 10.0f, 0.18f);
        g.setColour (juce::Colour (0xff102e26)); g.fillEllipse (emblem);
        g.setColour (juce::Colour (0xff168b50)); g.drawEllipse (emblem.reduced (0.75f), 1.5f);
        juce::Path check; check.startNewSubPath (emblem.getX()+21, emblem.getY()+34);
        check.lineTo (emblem.getX()+30, emblem.getY()+43); check.lineTo (emblem.getX()+48, emblem.getY()+25);
        g.setColour (Ink::green()); g.strokePath (check, juce::PathStrokeType (5, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        main.removeFromTop (10);
        g.setColour (Ink::textDim()); g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
        g.drawText (PL("05 / GOTOWE"), main.removeFromTop (18), juce::Justification::centred);
        g.setColour (Ink::textNow()); g.setFont (juce::Font (zs::type::title, juce::Font::bold));
        g.drawText (PL("Nagranie gotowe"), main.removeFromTop (40), juce::Justification::centred);
        g.setColour (Ink::textNext()); g.setFont (juce::Font (zs::type::body));
        g.drawFittedText (savedMp3 != juce::File() ? PL("Zapisano WAV do archiwum i MP3 do publikacji. Posłuchaj całości przed wysłaniem.")
                                                   : PL("Zapisano miks jako plik WAV na dysku. Posłuchaj całości przed publikacją."),
                          main.removeFromTop (44).reduced (12, 0), juce::Justification::centred, 2);
        side = side.reduced (22, 24);
        g.setColour (Ink::textNow()); g.setFont (juce::Font (zs::type::heading, juce::Font::bold));
        g.drawText (PL("Plik końcowy"), side.removeFromTop (40), juce::Justification::centredLeft);
        g.setFont (juce::Font (zs::type::small)); g.setColour (Ink::textDim());
        // rowny rytm: etykieta, wartosc, odstep (nazwa pliku w polu do zaznaczenia, 2 wiersze)
        g.drawText (PL("Nazwa"), side.removeFromTop (24), juce::Justification::centredLeft);
        side.removeFromTop (44 + 14);
        // Zarezerwuj miejsce na profil i trzy przyciski. Bez tego szczegoly
        // pliku schodzily pod akcje w niskim oknie (np. 960x640).
        side.removeFromBottom (124 + 8 + 36 + 8);
        const auto row = [&] (const juce::String& label, const juce::String& value, int height) {
            auto r = side.removeFromTop (height);
            g.setColour (Ink::textDim()); g.drawText (label, r.removeFromTop (18), juce::Justification::centredLeft);
            g.setColour (Ink::textNow()); g.drawText (value, r.removeFromTop (22), juce::Justification::centredLeft);
        };
        // jeden wiersz na oba pliki — miejsce na przycisk wzorca brzmienia w 960x640
        row (PL("Pliki"), savedMp3 != juce::File() ? PL("WAV 24-bit i MP3 ") + juce::String (mp3Kbps) + " kbit/s"
                                                   : PL("WAV 24-bit (MP3 nie powstał)"), 54);
        const bool withBed = ! bed.left.empty();
        const double length = withBed ? (double) bed.left.size() / sr : review.seconds();
        row (PL("Długość"), plr::clock (length) + " (" + juce::String (length, 1) + " s)", 54);
        // Audyt 14.10: "LUFS" i "dBTP" na glownym ekranie wymagaja wiedzy audio.
        row (PL("Gotowa głośność"), withBed ? PL("Jak w podcastach (stereo, z muzyką)") : PL("Jak w podcastach (mono)"), 42);
        g.setColour (Ink::textDim());
        g.drawText (juce::String (withBed ? bed.lufs : review.whatIDid().lufsAfter, 1) + PL(" LUFS · szczyt ")
                        + juce::String (withBed ? bed.peakDbtp : review.whatIDid().peakAfter, 1) + " dBTP",
                    side.removeFromTop (20), juce::Justification::centredLeft);
        if (wish.profile.valid())
        {
            side.removeFromTop (14);
            row (PL("Wzorzec brzmienia"), juce::String::fromUTF8 (wish.profile.label.c_str()), 42);
        }
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
    /** Praca w tle: przezroczysta zaslona nad wszystkim. Lapie mysz i
        klawisze, zeby nic (np. panel decyzji po R, ktory ma wlasny fokus)
        nie zmienialo plikow sesji, ktore watek wlasnie przepisuje (przeglad
        19.10). Napis "pracuje" rysuje paintOverChildren. */
    struct JobShield : public juce::Component {
        JobShield() { setWantsKeyboardFocus (true); setInterceptsMouseClicks (true, false); }
        bool keyPressed (const juce::KeyPress&) override { return true; }
    };
    class TrimHandle : public juce::Component {
    public:
        std::function<void(double)> move, step;
        juce::Rectangle<int> axis;
        double seconds = 0, from = 0;   ///< widoczny fragment osi (powiekszenie)
        TrimHandle() { setWantsKeyboardFocus (true); setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); }
        void paint (juce::Graphics& g) override {
            g.setColour (Ink::accent()); g.fillRect (getWidth()/2-1, 0, 2, getHeight());
            g.fillRoundedRectangle (1.f, getHeight()-knobH, getWidth()-2.f, knobH, 4.f);
            if (hasKeyboardFocus (false)) { g.setColour (Ink::textNow()); g.drawRect (getLocalBounds(), 1); }
        }
        /** Uchwyt lapie tylko za galke na srodku (50 px). Dawniej caly pas 14 px
            na wysokosc fali: przeciagniecie zaczete przy krawedzi nagrania
            przycinalo koniec, zamiast zaznaczac fragment do wyciecia (Adrian
            25.10, dwa razy: koniec na 16,9 s i na 7,5 s). Poza galka mysz idzie
            do fali pod spodem. */
        /** Galka na dole fali, nie na srodku: przeciagniecie od srodka fali
            przy krawedzi znow przycinalo koniec do 4,9 s zamiast zaznaczac
            (Adrian 25.10, trzeci raz). */
        static constexpr float knobH = 26.0f;
        bool hitTest (int, int y) override { return y >= getHeight() - (int) knobH - 4; }
        void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }
        void mouseDrag (const juce::MouseEvent& e) override {
            if (move && axis.getWidth()>0) move (from + (e.getEventRelativeTo(getParentComponent()).position.x-axis.getX())/axis.getWidth()*seconds);
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
        const auto v = view();
        for (auto* h : { &startHandle, &endHandle }) { h->axis=axis; h->seconds=v.len; h->from=v.from; }
        // uchwyt poza powiekszonym widokiem chowamy, zamiast kleic go do krawedzi
        auto place = [&] (TrimHandle& h, double t) {
            const int x = (int) xAt (t, axis);
            h.setBounds (x - 7, axis.getY(), 14, axis.getHeight());
            h.setVisible (editMode && ! doneMode && cutPanel.empty()
                          && x >= axis.getX() - 1 && x <= axis.getRight() + 1);
        };
        place (startHandle, trimStart.getValue());
        place (endHandle, trimEnd.getValue());
    }
    /** Krok 3 "Odsluch i montaz": surowe nagranie, bez miksu. Najpierw decyzje
        po R (gdy sa), potem przyciecie poczatku i konca albo wyciecie srodka. */
    void layoutEdit() {
        flow.setBounds (zs::ScreenFrame::stepsArea (getLocalBounds())); flow.setCurrent (3);
        settingsButton.setBounds (getWidth() - 196, 14, 174, 36);
        settingsButton.setVisible (true);
        namespace L = zs::ui::review;
        abButton.setVisible (false); detailsToggle.setVisible (false); words.setVisible (false);
        auto main = zs::ScreenFrame::mainArea (getLocalBounds()); main.removeFromTop (headH() + waveHeight() + L::strip + 8);
        const bool decide = ! cutPanel.empty();
        for (auto* c : { static_cast<juce::Component*>(&trimStart), static_cast<juce::Component*>(&trimEnd),
                         static_cast<juce::Component*>(&startHandle), static_cast<juce::Component*>(&endHandle),
                         static_cast<juce::Component*>(&middleToggle) }) c->setVisible (! decide);
        auto row = main.removeFromTop (36);
        playButton.setBounds (row.removeFromLeft (112)); row.removeFromLeft (8);
        // Waskie okno (np. skalowanie 150% w Windows): najpierw znika "Calosc"
        // (zostaje klawisz 0), potem - i + (zostaje kolko); "Nastepne miejsce"
        // nie mniejsze niz 220 px, bo napis by sie ucial.
        const int zoomSmall = 16 + 40 + 6 + 40, zoomFull = zoomSmall + 6 + 80;
        const bool zoomAll = row.getWidth() >= 260 + zoomFull, zoomPart = ! zoomAll && row.getWidth() >= 220 + zoomSmall;
        nextButton.setVisible (true);
        nextButton.setBounds (row.removeFromLeft (zoomAll ? 260 : zoomPart ? juce::jmin (260, row.getWidth() - zoomSmall)
                                                                         : juce::jmin (260, row.getWidth())));
        if (zoomAll || zoomPart)
        {
            row.removeFromLeft (16);
            zoomOut.setVisible (true); zoomOut.setBounds (row.removeFromLeft (40)); row.removeFromLeft (6);
            zoomIn.setVisible (true);  zoomIn.setBounds (row.removeFromLeft (40));
            if (zoomAll) { row.removeFromLeft (6); zoomAllButton.setVisible (true); zoomAllButton.setBounds (row.removeFromLeft (80)); }
        }
        if (hasMusic() && row.getWidth() >= 196)
        {
            withMusicButton.setButtonText (editWithMusic ? PL ("Słychać: głos z podkładem") : PL ("Słychać: sam głos"));
            withMusicButton.setVisible (true); withMusicButton.setBounds (row.removeFromRight (juce::jmin (230, row.getWidth() - 16)));
        }
        zoomIn.setEnabled (view().len > juce::jmin (zs::ui::review::minViewSec, review.seconds()) + 1.0e-6);
        zoomOut.setEnabled (zoomed()); zoomAllButton.setEnabled (zoomed());
        main.removeFromTop (8);
        if (decide)
        {
            // decyzje po R: czasy odnosza sie do pelnego nagrania, wiec najpierw one
            for (auto* b : { &markStart, &markEnd, &cutButton, &undoButton, &replaceButton }) b->setVisible (false);
            selection.setVisible (false);
            feedbackArea = main.removeFromTop (24);
            cutPanel.setBounds (main); summaryArea = {};
        }
        else
        {
            auto limits = main.removeFromTop (54); auto left = limits.removeFromLeft ((limits.getWidth() - 16) / 2); limits.removeFromLeft (16);
            trimStart.setBounds (left.withTrimmedTop (22)); trimEnd.setBounds (limits.withTrimmedTop (22));
            main.removeFromTop (8);
            middleToggle.setBounds (main.removeFromTop (32).removeFromLeft (juce::jmin (260, main.getWidth())));
            main.removeFromTop (8);
            auto cuts = main.removeFromTop (middleMode ? 32 : 0);
            if (middleMode && getWidth() < 1180)
            {
                const int w = (cuts.getWidth() - 16) / 3;
                juce::TextButton* firstRow[] = { &markStart, &markEnd, &cutButton };
                for (int i = 0; i < 3; ++i)
                { firstRow[i]->setBounds (cuts.removeFromLeft (w)); firstRow[i]->setVisible (true); if (i < 2) cuts.removeFromLeft (8); }
                main.removeFromTop (4);
                auto second = main.removeFromTop (32); const int half = (second.getWidth() - 8) / 2;
                replaceButton.setBounds (second.removeFromLeft (half)); second.removeFromLeft (8);
                undoButton.setBounds (second); replaceButton.setVisible (true); undoButton.setVisible (true);
            }
            else if (middleMode)
            {
                const int w = (cuts.getWidth() - 32) / 5;
                juce::TextButton* all[] = { &markStart, &markEnd, &cutButton, &replaceButton, &undoButton };
                for (int i = 0; i < 5; ++i)
                { all[i]->setBounds (cuts.removeFromLeft (w)); all[i]->setVisible (true); if (i < 4) cuts.removeFromLeft (8); }
            }
            else for (auto* b : { &markStart, &markEnd, &cutButton, &undoButton, &replaceButton }) b->setVisible (false);
            selection.setVisible (middleMode); selection.setBounds (main.removeFromTop (middleMode ? 24 : 0));
            feedbackArea = main.removeFromTop (24); main.removeFromTop (4);
            // cala notka albo zadna — ucieta w polowie wiersza byla nieczytelna (960x640)
            const int noteH = zs::ScreenFrame::wrappedHeight (safeNote(), main.getWidth() - 24, juce::Font (zs::type::small)) + 12;
            summaryArea = noteH <= main.getHeight() ? main.removeFromTop (noteH) : juce::Rectangle<int>();
        }
        auto side = zs::ScreenFrame::sidebar (getLocalBounds()).reduced (22, 24); side.removeFromTop (40);
        auto actions = side.removeFromBottom (124);
        backButton.setVisible (true); backButton.setBounds (actions.removeFromBottom (36)); actions.removeFromBottom (8);
        saveButton.setBounds (actions.removeFromBottom (36)); actions.removeFromBottom (8);
        folderButton.setBounds (actions.removeFromBottom (36)); folderButton.setVisible (true);
        editToggle.setVisible (false);
        assessment.setBounds (side); assessment.setVisible (true);
        saveButton.setButtonText (PL("Dalej: miks →"));
        backButton.setButtonText (PL("Wróć do nagrywania"));
        positionHandles();
    }

    /** Montaz po ludzku: co jest do sprawdzenia, bez parametrow miksu. */
    juce::String montageSummary() const
    {
        juce::String t;
        if (! cutPanel.empty())
            t << PL("Najpierw poprawki po R: przy każdej posłuchaj połączenia i zdecyduj — zaznacz do wycięcia albo zostaw. ")
              << PL("Nagranie zmieni się dopiero po „Wytnij zaznaczone”.\n\n");
        const int marks = (int) review.timeline().size();
        if (marks == 0) t << PL("Program nie zaznaczył miejsc do sprawdzenia.\n\n");
        else t << PL("Paski pod falą to miejsca do sprawdzenia (") << marks
               << PL("). Posłuchaj ich — przycisk „Następne miejsce” albo klawisz N.\n\n");
        if (! review.toRedo().empty())
        {
            t << PL("DO POWTÓRZENIA (nie da się naprawić miksem)\n");
            for (const auto& redo : review.toRedo())
                t << plr::clock (redo.fromSec) << PL("–") << plr::clock (redo.toSec) << "  " << plr::kind (redo.kind) << "\n";
            t << "\n";
        }
        if (withMusic)
            t << PL("Nagranie z podkładem: muzyka się nie przesuwa, więc zaznaczony fragment i początek przed uchwytem są wyciszane, a nie wycinane. Głos zostaje w rytmie muzyki. Koniec można przyciąć normalnie.\n\n");
        else
        t << PL("Możesz przyciąć początek i koniec albo wyciąć fragment ze środka. Oryginał zostaje na dysku.\n\n")
          << PL("Gdy wszystko gra: „Dalej: miks”. Miks powstanie w następnym kroku. Po zmianie cięć policzy się od nowa.");
        return t;
    }
    void paintEdit(juce::Graphics& g) {
        g.fillAll(Ink::bg());
        zs::ScreenFrame::paint(g,getLocalBounds(),file.getFileNameWithoutExtension(),
            cutPanel.empty() ? PL("Klik — posłuchaj · przeciągnij — zaznacz · kółko, + − — powiększ · N — następne · Esc — wróć")
                             : PL("Spacja — połączenie · Enter — zaznacz do wycięcia · Backspace — zostaw · ↑↓ — miejsca · Esc — wróć"));
        auto side=zs::ScreenFrame::sidebar(getLocalBounds());zs::ScreenFrame::paintPanel(g,side);
        auto main=zs::ScreenFrame::paintHead(g,getLocalBounds(),PL("03 / ODSŁUCH I MONTAŻ"),PL("Posłuchaj i popraw"),subtitle());
        g.setColour(Ink::textNext());
        drawWave(g,main.removeFromTop(waveHeight()));
        drawStrip(g,main.removeFromTop(zs::ui::review::strip));main.removeFromTop(8);
        main.removeFromTop(44);
        if(cutPanel.empty()) {
            auto row=main.removeFromTop(54);auto left=row.removeFromLeft((row.getWidth()-16)/2);row.removeFromLeft(16);
            // zwykly kolor: czerwony w programie znaczy "blad", a to tylko czasy
            g.setColour(Ink::textNext());g.setFont(juce::Font(zs::type::small));g.drawText(PL("Początek: ")+juce::String(trimStart.getValue(),1)+" s",left,juce::Justification::topLeft);g.drawText(PL("Koniec: ")+juce::String(trimEnd.getValue(),1)+" s",row,juce::Justification::topLeft);
        }
        g.setColour(Ink::textNow());g.setFont(juce::Font(zs::type::heading,juce::Font::bold));g.drawText(PL("Do sprawdzenia"),side.reduced(22,24).removeFromTop(32),juce::Justification::centredLeft);
        g.setFont(juce::Font(zs::type::small));g.setColour(Ink::textNext());g.drawFittedText(note,feedbackArea,juce::Justification::topLeft,2);
        if(!summaryArea.isEmpty()) {
            g.setColour(Ink::green().withAlpha(.15f));g.fillRoundedRectangle(summaryArea.toFloat(),4.f);g.setColour(Ink::green());
            zs::ScreenFrame::wrapped(g,safeNote(),summaryArea.reduced(12,6),juce::Font(zs::type::small),Ink::green(),juce::Justification::centredLeft);
        }
    }
    // cutRange usuwa zakres: najpierw koniec, potem poczatek, aby czasy
    // wybranego fragmentu pozostaly w osi pierwotnej kopii. DSP bez zmian.
    // Eksport czeka na nowy miks; cofanie przywraca cala kopie sprzed ciecia.
    /** "Dalej: miks": przyciecie poczatku i konca (kopia w pamieci), potem miks
        liczony raz. Gdy ta sama wersja byla juz zmiksowana tym brzmieniem —
        bez liczenia od nowa. */
    void goToMix() {
        if (! canEdit()) return;
        if (! cutPanel.empty()) {
            note = PL("Najpierw zdecyduj o poprawkach po R i kliknij „Wytnij zaznaczone”.");
            if (! editMode) { editMode = true; setRawMaterial(); }
            assessment.setText (montageSummary(), false); resized();
            repaint(); return;
        }
        player.pause(); stopSeam();
        const double from=trimStart.getValue(),to=trimEnd.getValue(),total=review.seconds(),rate=sr;
        if (from > 0.0 || to < total) {
            // przyciecie w tle; po nim ten sam krok jeszcze raz (juz bez przyciecia)
            const bool mute=withMusic;
            restartEdit([this,from,to,total,rate,mute](std::vector<float>& out) {
                std::vector<float> audio=review.rawAudio(), cropped;
                if(to<total) {if(!zs::cutRange(audio,rate,to,total,cropped))return false;audio=std::move(cropped);}
                // z podkladem poczatek sie wycisza, nie znika — inaczej caly glos
                // przesunalby sie wzgledem muzyki
                if(from>0 && mute) {if(!tw::BackingEdit::muteRange(audio,rate,0,from))return false;}
                else if(from>0) {if(!zs::cutRange(audio,rate,0,from,cropped))return false;audio=std::move(cropped);}
                out=std::move(audio); return true;
            }, true, [this] { goToMix(); },
               PL("Nie udało się przyciąć początku i końca. Przesuń uchwyty i spróbuj jeszcze raz."));
            return;
        }
        editMode = false; detailsMode = false; doneMode = false;
        note.clear();   // uwagi z montazu ("Miejsce 3 z 27…") nie dotycza miksu
        if (mixedVersion == takeVersion && mixedStyle == wish.style && mixedTweaks == tweaks
            && review.stage() == zs::TakeReview::Stage::Ready)
            adoptResult();
        else
            startMix();
        resized(); repaint();
    }

    void startMix() {
        player.pause(); recorder.attachPlayer (nullptr); ready.store (false); player.clear();
        for (auto* b : { &playButton, &abButton, &saveButton, &editToggle }) b->setEnabled (false);
        afterEnv = {}; words.clear(); assessment.clear();
        busySinceMs = juce::jmax (1u, juce::Time::getMillisecondCounter());
        mixTweaks = tweaks;                     // watek czyta tylko te kopie
        mixBedParams = bedParams;
        bed = {}; bedRaw = {};
        chainView.setBusy (true); defaultsButton.setEnabled (false);
        startThread();
    }

    /** Odsluch surowego nagrania (krok montazu): przed = po = surowe. */
    void setRawMaterial() {
        const double rate = recorder.currentSampleRate() > 0 ? recorder.currentSampleRate() : sr;
        std::vector<float> src;
        if (editWithMusic && hasMusic())
        {
            // glos z muzyka na dlugosci glosu, zeby fala i czas sie zgadzaly
            if (std::isnan (musicLufs)) musicLufs = mix::MusicBed::lufsOf (music.left, music.right, sr);
            const auto b = mix::MusicBed::render (review.rawAudio(), music.left, music.right, sr, bedParams, false, musicLufs);
            src.assign (review.rawAudio().size(), 0.0f);
            for (size_t i = 0; i < src.size() && i < b.left.size(); ++i) src[i] = 0.5f * (b.left[i] + b.right[i]);
        }
        const auto& voice = src.empty() ? review.rawAudio() : src;
        if (std::abs (rate - sr) > 0.1) {
            mix::Resampler convert; convert.prepare ((int) sr, (int) rate, 64);
            std::vector<float> r; convert.render (voice, r);
            player.setMaterial (r, r, rate, true);
        } else player.setMaterial (voice, voice, sr, true);
        player.showAfter (false);
        recorder.attachPlayer (&player);
        playButton.setEnabled (true);
    }

    void updateMontageButtons() {
        const bool editable = canEdit();
        playButton.setEnabled (editable);
        trimStart.setEnabled(editable);trimEnd.setEnabled(editable);startHandle.setEnabled(editable);endHandle.setEnabled(editable);
        markStart.setEnabled(editable);markEnd.setEnabled(editable);
        cutButton.setEnabled(editable && cutFrom>=0 && cutTo>cutFrom);
        replaceButton.setEnabled (editable && cutFrom >= 0 && cutTo > cutFrom);
        undoButton.setEnabled(editable && (replacementUndoAvailable || !undoAudio.empty()));
        nextButton.setEnabled (editable && (! review.timeline().empty() || ! replacementSeams.empty()));
        saveButton.setEnabled (editable && cutPanel.empty());
    }

    /** Montaz: jest surowe nagranie i nic sie nie liczy w tle. */
    bool canEdit() const { return review.stage() != zs::TakeReview::Stage::Empty && ! isThreadRunning() && job == nullptr; }
    /** Miks: policzony i nic sie nie liczy w tle. */
    bool mixReady() const { return review.stage() == zs::TakeReview::Stage::Ready && ! isThreadRunning() && job == nullptr; }

    /** Brzmienie miksu: ten sam surowy dzwiek, miks od nowa w tle. */
    void chooseStyle (mix::Decide::Style st)
    {
        // Klik w trakcie liczenia miksu przepadal bez sladu (przeklikanie 25.10:
        // "pierwszy klik w Studyjny nie zadzialal"). Teraz czeka w kolejce i
        // rusza, gdy biezacy miks sie skonczy; watek czyta wish, wiec go nie ruszamy.
        if (! mixReady())
        {
            if (review.stage() == zs::TakeReview::Stage::Empty || editMode) return;
            queuedStyle = (int) st;
            for (int i = 0; i < 3; ++i) styleCards[i].setSelected (i == queuedStyle);
            return;
        }
        queuedStyle = -1;
        if (st == wish.style) return;
        wish.style = st;
        updateStyleButtons();
        note = PL ("Miks ") + PL (mix::Decide::styleName (st)).toLowerCase() + PL (" liczy się…");
        styleNote = true;
        startMix();
        repaint();
    }

    void updateStyleButtons()
    {
        for (int i = 0; i < 3; ++i)
            styleCards[i].setSelected (static_cast<mix::Decide::Style> (i) == wish.style);
    }
    void updateSelection() {
        if(cutFrom>=0 && cutTo>cutFrom)
            selection.setText((withMusic ? PL("Do wyciszenia: ") : PL("Do wycięcia: "))+plr::clock(cutFrom)+PL("–")+plr::clock(cutTo)+" ("+juce::String(cutTo-cutFrom,1)+PL(" s) — Delete albo „")+cutButton.getButtonText()+PL("”."),juce::dontSendNotification);
        else
            selection.setText(PL("Cięcie: ")+(cutFrom>=0?juce::String(cutFrom,2)+" s":"—")+"  →  "+(cutTo>=0?juce::String(cutTo,2)+" s":"—"),juce::dontSendNotification);
        // przycisk ciecia za zaznaczeniem: po pierwszym cieciu nic innego go nie
        // odswiezalo i drugie zaznaczenie zostawalo z szarym przyciskiem (Adrian 25.10)
        if (editMode) updateMontageButtons();
        repaint();
    }
    /** Zmiana w montazu (wyciecie, cofniecie, przyciecie): nowa kopia surowego
        nagrania. Miksu nie liczymy — policzy sie raz po "Dalej: miks". */
    /** make: buduje nowa kopie (w tle; czyta review.rawAudio(), nic nie
        zmienia). keepUndo: obecna kopia trafia do "Cofnij cięcie". then: co
        dalej na watku okna, gdy kopia jest gotowa. */
    void restartEdit(std::function<bool(std::vector<float>&)> make, bool keepUndo,
                     std::function<void()> then = nullptr,
                     juce::String failNote = PL("Wskaż początek, potem późniejszy koniec cięcia na osi czasu.")) {
        struct Out { bool ok=false; std::vector<float> audio; std::vector<tw::Event> events; zs::Envelope env; };
        auto out=std::make_shared<Out>();
        const double rate=sr;
        runJob(PL("Stosuję zmianę…"), zs::ui::review::listenEstimateSec(review.seconds()),
            [out,make,rate,stop=&jobStop] {
                if(!make(out->audio)) return;
                out->events=zs::TakeReview::listenOffline(out->audio,rate,stop);
                if(stop->load()) return;
                out->env.build(out->audio,rate,1400);
                out->ok=true;
            },
            [this,out,keepUndo,then,failNote] {
                if(!out->ok) { note=failNote; repaint(); return; }
                player.pause();recorder.attachPlayer(nullptr);ready.store(false);player.clear();
                if(keepUndo) undoAudio=review.rawAudio();
                review.setTake(std::move(out->audio),sr,std::move(out->events));
                ++takeVersion; viewLen=0.0; viewFrom=0.0;
                beforeEnv=std::move(out->env);afterEnv={};resetTrim();
                cutFrom=cutTo=-1;
                selection.setText(PL("Zmiana zastosowana. Posłuchaj miejsca cięcia."),juce::dontSendNotification);
                words.clear();saved=juce::File();savedMp3=juce::File();note=PL("Montaż zmienia kopię w pamięci; oryginał zostaje na dysku.");
                setRawMaterial();
                assessment.setText (montageSummary(), false);
                updateMontageButtons();
                resized();repaint();
                if(then) then();
            });
    }

    //==========================================================================
    /* Praca w tle (audyt 14.10). work: tylko na wlasnych danych i odczyt
       review (w tym czasie canEdit()/mixReady() = false, wiec nikt go nie
       zmienia). finish: na watku okna, z timerCallback. Przy zrzutach
       ekranu (waitForIt) wszystko od razu, bo liczy sie pierwsza klatka. */
    void runJob(const juce::String& label, int expectSec, std::function<void()> work,
                std::function<void()> finish, bool now = false) {
        jassert(job == nullptr);
        player.pause(); stopSeam();
        if(now) { work(); if(finish) finish(); return; }
        jobLabel=label; jobExpectSec=expectSec; jobFinish=std::move(finish);
        jobSinceMs=juce::jmax(1u,juce::Time::getMillisecondCounter());
        jobDone.store(false);
        job=std::make_unique<std::thread>([this,w=std::move(work)] { w(); jobDone.store(true); });
        // przyciski zostaja jak byly — zaslona i tak lapie wszystko; w miksie
        // updateMontageButtons wylaczal "Zapisz" na stale (przeglad 19.10)
        jobShield.setBounds (getLocalBounds()); jobShield.setVisible (true); jobShield.toFront (false);
        if (isShowing()) jobShield.grabKeyboardFocus();
        repaint();
    }
    void finishJob() {
        if(job) { job->join(); job.reset(); }
        jobSinceMs=0;
        jobShield.setVisible(false);
        if(isShowing()) grabKeyboardFocus();
        auto f=std::move(jobFinish); jobFinish=nullptr;
        if(f) f();
        repaint();
    }
    /** Nowe nagranie albo zamkniecie: poczekaj na prace, wyniku nie uzywaj. */
    void abandonJob() {
        // przesluchiwanie i odczyt sprawdzaja to co ok. 1 s nagrania — zamkniecie
        // programu nie czeka juz do minuty (przeglad 19.10)
        jobStop.store(true);
        if(job) { job->join(); job.reset(); }
        jobStop.store(false);
        jobFinish=nullptr; jobSinceMs=0;
        jobShield.setVisible(false);
    }
    //==========================================================================
    /** Rosnie z oknem — liczby i uzasadnienie w zs::ui::review (UiRules.h). */
    int waveHeight() const { if(!cutPanel.empty() && editMode) return 80; const int h=juce::jmax(zs::ui::review::waveMin,zs::ui::review::waveHeight(getHeight())-juce::jmax(0,headH()-78)); return editMode ? juce::jmax(120,h-(middleMode?72:16)) : detailsMode ? juce::jmin(h,getHeight()<720?56:64) : hasMusic() && getHeight()<720 ? h-44 : h; }   // wiersz MUZYKA w malym oknie

    /** Podtytul ekranu: montaz (z decyzjami po R albo bez) albo miks. */
    juce::String subtitle() const
    {
        if (! editMode) return PL("Delikatny — naturalnie · Studyjny — wyrównany · Mocny — radiowy. Ta sama głośność, porównaj przed i po.");
        return cutPanel.empty() ? PL("Surowe nagranie, jeszcze bez miksu. Kliknij falę, aby posłuchać; przeciągnij, aby zaznaczyć fragment.")
                                : PL("Surowe nagranie, jeszcze bez miksu. Najpierw zdecyduj o poprawkach po R.");
    }
    static juce::String safeNote() { return PL("Oryginalne nagranie zostaje bez zmian na dysku. Zmiany dotyczą kopii, z której powstanie miks."); }
    int headH() const { return zs::ScreenFrame::headHeight (getLocalBounds(), subtitle()); }

    juce::Rectangle<int> waveArea() const
    {
        namespace L = zs::ui::review;
        auto r = zs::ScreenFrame::mainArea (getLocalBounds());
        r.removeFromTop (headH());
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
        // dluzsze nagranie: najpierw probka 15 s do odsluchu, calosc po akceptacji
        review.run (wish, mixTweaks, acceptingFull ? 0.0 : previewSec, acceptingFull);

        if (threadShouldExit()) return;
        if (hasMusic() && review.mixValid()) renderBed();

        ready.store (true);
    }

    static juce::String timeText (double t)
    {
        const int s = (int) t;
        return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
    }

    /** Probka sie podoba: cala reszta tymi samymi ustawieniami, potem zapis. */
    void acceptPreview()
    {
        if (! mixReady()) return;
        acceptingFull = true;
        note = PL ("Miksuję całość…");
        startMix();
        repaint();
    }

    void adoptResult()
    {
        const bool thenSave = std::exchange (acceptingFull, false) && ! review.isPreview();
        if (styleNote) { note.clear(); styleNote = false; }   // "Miks ... liczy się…" juz nieaktualne
        mixedVersion = takeVersion; mixedStyle = wish.style; mixedTweaks = mixTweaks;
        chainView.setState (review.whatIDecided().params, review.whatIMixedWith(), mixTweaks);
        chainView.setBusy (false); defaultsButton.setEnabled (chainView.canReset());
        afterEnv.build (review.mixedAudio(), sr, 1400);

        const double playbackRate=recorder.currentSampleRate()>0?recorder.currentSampleRate():sr;
        if(std::abs(playbackRate-sr)>0.1) {
            mix::Resampler convert;convert.prepare((int)sr,(int)playbackRate,64);
            std::vector<float> before,after;
            convert.render(review.rawAudio(),before);convert.render(review.mixedAudio(),after);
            player.setMaterial(std::move(before),std::move(after),playbackRate,true);
        } else player.setMaterial (review.rawAudio(), review.mixedAudio(), sr, true);
        if (! bed.left.empty()) setBedMaterial();
        player.showAfter (true);
        if (review.isPreview()) player.seekSec (review.previewFrom());

        recorder.attachPlayer (&player);

        juce::String all;

        for (const auto& line : review.says())
            all << PL (line.c_str()) << "\n\n";

        words.setText (all.trimEnd(), false);
        // Po prawej skutek prostymi slowami; liczby pod "Szczegoly miksu" (audyt 14.10).
        juce::String summary;
        for (const auto& line : review.plainSays())
            summary << PL ("• ") << PL (line.c_str()) << "\n\n";
        if (! bed.left.empty()) summary << PL ("• ") << bedSummary() << "\n\n";
        else if (musicProblem.isNotEmpty()) summary << PL ("• ") << musicProblem << "\n\n";
        summary << PL ("Liczby i ustawienia: „Zaawansowane ustawienia”.");
        if (review.isPreview())
            summary = PL ("PRÓBKA: zmiksowane tylko ") + juce::String ((int) review.previewSeconds()) + PL (" s od ")
                    + timeText (review.previewFrom()) + PL (". Posłuchaj (Odtwórz zaczyna od próbki). Pasuje? „Akceptuję — miksuj całość”. Inne brzmienie: kliknij kartę obok.\n\n") + summary;
        reviewSummary=summary; assessment.setText (summary, false);
        playButton.setEnabled (true); abButton.setEnabled (true);
        editToggle.setEnabled (true); saveButton.setEnabled (true);

        updateAbText();
        // Wynik dotarl, gdy czlowiek jest w montazu (podglad, powrot z miksu):
        // tam slychac surowe nagranie i widac liste do sprawdzenia, nie miks.
        if (editMode && ! doneMode) { setRawMaterial(); assessment.setText (montageSummary(), false); updateMontageButtons(); }
        // brzmienie klikniete w trakcie liczenia
        if (queuedStyle >= 0 && ! editMode && ! doneMode)
        {
            const auto st = (mix::Decide::Style) std::exchange (queuedStyle, -1);
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ReviewComponent> (this), st]
                                             { if (safe != nullptr) safe->chooseStyle (st); });
        }
        resized();
        repaint();
        if (thenSave && ! editMode && ! doneMode)
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ReviewComponent> (this)]
                                             { if (safe != nullptr) safe->save(); });
    }

    //==========================================================================
    void togglePlay()
    {
        stopSeam();
        if (! (editMode ? canEdit() : mixReady())) return;

        if (player.isPlaying()) player.pause();
        else                    player.play();

        setPlayLabel (player.isPlaying());
    }

    void setPlayLabel (bool playing)
    {
        playButton.setButtonText (playing ? PL ("Pauza") : PL ("Odtwórz"));
        zs::Look::setIcon (playButton, playing ? zs::Look::Icon::Pause : zs::Look::Icon::Play);
    }

    void toggleAb()
    {
        if (editMode || ! mixReady()) return;           // w montazu nie ma jeszcze miksu

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

    /** Nastepne miejsce do sprawdzenia: kazdy pasek pod fala, nie tylko te do
        powtorki (audyt 14.10: "8 miejsc na osi", a przycisk byl wylaczony). */
    void nextTrouble()
    {
        const double now = player.positionSec();
        auto startOf = [] (const auto& t) { return juce::jmax (0.0, (t.mustRedo ? t.redoFrom : t.fromSec) - 0.3); };
        const auto& all = review.timeline();
        if (! replacementSeams.empty())
        {
            size_t index = replacementSeams.size();
            for (size_t i = 0; i < replacementSeams.size(); ++i)
                if (replacementSeams[i] > now + 0.3) { index = i; break; }
            double nextAudio = std::numeric_limits<double>::max();
            for (const auto& t : all)
                if (startOf (t) > now + 0.3) { nextAudio = startOf (t); break; }
            if (index < replacementSeams.size() && replacementSeams[index] - 0.3 <= nextAudio)
            {
                const double at = juce::jmax (0.0, replacementSeams[index] - 0.3);
                player.seekSec (at); follow (at);
                if (! player.isPlaying()) player.play();
                note = PL ("Miejsce do sprawdzenia: szew po poprawce ") + juce::String ((int) index + 1)
                     + PL (" z ") + juce::String ((int) replacementSeams.size()) + PL (". Posłuchaj, czy łączenie brzmi naturalnie.");
                repaint(); return;
            }
            if (all.empty())
            {
                const double at = juce::jmax (0.0, replacementSeams.front() - 0.3);
                player.seekSec (at); follow (at);
                if (! player.isPlaying()) player.play();
                note = PL ("Miejsce do sprawdzenia: szew po poprawce. Posłuchaj, czy łączenie brzmi naturalnie.");
                repaint(); return;
            }
        }
        if (all.empty()) return;
        // Ktore to miejsce, kiedy i dlaczego (audyt 14.10: "Nastepne miejsce
        // do sprawdzenia" z czasem i powodem).
        auto go = [&] (size_t i) {
            const auto& t = all[i];
            player.seekSec (startOf (t));
            follow (startOf (t));
            if (! player.isPlaying()) player.play();
            note = PL ("Miejsce ") + juce::String ((int) i + 1) + PL (" z ") + juce::String ((int) all.size()) + ": "
                 + plr::clock (t.fromSec) + "  " + plr::kind (t.kind)
                 + (t.mustRedo ? PL (" — tego miks nie naprawi, warto nagrać ten fragment jeszcze raz")
                               : PL (" — posłuchaj, czy brzmi dobrze"));
            repaint();
        };
        for (size_t i = 0; i < all.size(); ++i)
            if (startOf (all[i]) > now + 0.3) { go (i); return; }
        // Nie ma nic dalej — wracamy do pierwszego, zeby klawisz zawsze cos robil.
        go (0);
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
        if (! mixReady()) return;
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

        // Bez kopii miksu (godzina = 0,7 GB), z kontrola kazdego kroku zapisu i
        // podmiana pliku dopiero po poprawnym zamknieciu (Wav.h, audyt 14.10).
        // W tle: godzina miksu to kilka sekund zapisu. W tym czasie miks sie
        // nie zmienia (mixReady() = false, styl ani montaz nie ruszy).
        // Obok WAV-a (archiwum, 24 bity) MP3 do publikacji — ta sama nazwa.
        // MP3 nie blokuje niczego: gdy sie nie uda (np. Windows N bez kodera),
        // WAV jest zapisany, a ekran Gotowe mowi, czemu MP3 nie ma.
        auto err = std::make_shared<std::string>(), mp3Err = std::make_shared<std::string>();
        const auto path = target.getFullPathName().toStdString();
        const auto mp3Target = target.withFileExtension ("mp3");
        const auto mp3Path = mp3Target.getFullPathName().toStdString();
        const int rate = (int) sr;
        runJob (PL ("Zapisuję miks…"), 0,
            [this, err, mp3Err, path, mp3Path, rate]
            {
                // z podkladem: stereo glos + muzyka (MusicBed), inaczej sam glos mono
                if (! bed.left.empty()) mix::MusicBed::measureResult (bed, sr);   // dla ekranu Gotowe
                const auto& mixed = review.mixedAudio();
                const std::vector<const float*> ch = bed.left.empty() ? std::vector<const float*> { mixed.data() }
                                                                      : std::vector<const float*> { bed.left.data(), bed.right.data() };
                const size_t frames = bed.left.empty() ? mixed.size() : bed.left.size();
                *err = wavio::writePcm (path, ch, frames, rate, 24);
                if (err->empty())
                    *mp3Err = zs::mp3::write (mp3Path, ch, frames, rate, mp3Kbps);
            },
            [this, err, mp3Err, target, mp3Target]
            {
                if (! err->empty())
                {
                    note = PL ("Nie udało się zapisać miksu — sprawdź wolne miejsce na dysku i dostęp do folderu. ")
                         + PL ("Nagranie jest bezpieczne, możesz spróbować jeszcze raz.");
                    repaint();
                    return;
                }

                saved = target;
                savedMp3 = mp3Err->empty() ? mp3Target : juce::File();
                mp3Problem = juce::String::fromUTF8 (mp3Err->c_str());
                savedVersion = mixedVersion;     // zapisany miks tej wersji montazu
                note  = PL ("Zapisane: ") + target.getFileName()
                      + (savedMp3 != juce::File() ? PL (" i ") + savedMp3.getFileName() : juce::String());
                player.pause();
                savedPath.setText (saved.getFileName() + PL ("\nFolder: ") + saved.getParentDirectory().getFileName(), false);
                juce::String pathTip = saved.getFullPathName();
                if (savedMp3 != juce::File()) pathTip << "\n" << savedMp3.getFullPathName();
                else pathTip << "\n" << PL ("MP3 nie powstał: ") << mp3Problem;
                folderButton.setTooltip (pathTip);
                savedName.setText (saved.getFileName(), false);
                profileRemembered = false;       // nowy plik: wzorca z niego jeszcze nie ma
                doneMode = true;
                resized();
                repaint();
            },
            previewSave);
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
            g.setColour (Ink::accent());
            g.fillRect (bar.withWidth ((int) (bar.getWidth() * review.progress())));
            return;
        }

        // Krok "Miks": werdykt o miksie, nie o miejscach do odsluchu — te byly
        // w montazu. Do powtorki (przester, brak sygnalu) wciaz na czerwono.
        int v = 0;
        juce::String line = review.isPreview() ? PL ("Próbka miksu gotowa. Posłuchaj, wybierz brzmienie i zaakceptuj.")
                                               : PL ("Miks gotowy. Posłuchaj przed i po, wybierz brzmienie i zapisz.");
        if (damagedRecording) { v = 2; line = PL ("Podczas zapisu utracono dane. Nagraj materiał ponownie."); }
        else if (! review.mixValid()) { v = 2; line = PL (review.verdictLine().c_str()); }
        else if (review.nearlySilent())
        {
            v = 1;
            line = PL ("Nagranie jest prawie ciche — mikrofon chyba nie dochodził. Sprawdź „Mikrofon i sprzęt” i nagraj jeszcze raz.");
        }
        else if (! review.toRedo().empty())
        {
            v = 2;
            line = PL ("Miks gotowy, ale w nagraniu zostały fragmenty do nagrania jeszcze raz (")
                 + juce::String ((int) review.toRedo().size()) + PL (") — miks ich nie naprawi.");
        }

        auto c = v == 2 ? Ink::clip() : v == 1 ? Ink::yellow() : Ink::green();

        auto dot = juce::Rectangle<int> (16, 16).withCentre ({ r.getX() + 8, r.getY() + 16 });

        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (dot.toFloat().expanded (7.0f));
        g.setColour (c);
        g.fillEllipse (dot.toFloat());

        auto txt = r.withTrimmedLeft (34);

        g.setColour (c);
        g.setFont (juce::Font (17.0f, juce::Font::bold));
        g.drawFittedText (line, txt.removeFromTop (84), juce::Justification::topLeft, 5);
    }

    //==========================================================================
    void drawWave (juce::Graphics& g, juce::Rectangle<int> r)
    {
        g.setColour (Ink::panel());
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (Ink::edge()); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);

        const bool after = player.showingAfter() && ! afterEnv.peakDb.empty();
        if ((after ? afterEnv : beforeEnv).peakDb.empty() || review.seconds() <= 0.0)
            return;
        // powiekszona fala (tylko montaz, czyli surowe nagranie): z probek widoku
        const auto& env = zoomed() && ! after ? viewEnvelope (juce::jmax (1, r.getWidth() / 3))
                                              : (after ? afterEnv : beforeEnv);

        const float mid = (float) r.getCentreY();
        const float half = r.getHeight() * 0.46f;

        // Przebieg rysujemy w decybelach, nie w probkach. Mowa ma szczyty koło
        // -10 dBFS i pauzy koło -60; w skali liniowej pauzy sa niewidoczna
        // kreska, a to wlasnie w nich siedzi szum i oddechy, czyli to, o co
        // czlowiek pyta.
        const int cols = r.getWidth();
        const auto v = view();

        // podklad pod glosem (Adrian 24.10: "muzyka powinna byc na wykresie"):
        // ta sama os czasu, fiolet, za fala glosu
        if (! musicEnv.peakDb.empty() && musicEnv.seconds > 0.0)
        {
            g.setColour (juce::Colour (0xff8f7cf0).withAlpha (0.30f));
            for (int x = 0; x < cols; x += 3)
            {
                const double t = v.from + (double) x / juce::jmax (1, cols) * v.len;
                const size_t i = (size_t) (t / musicEnv.seconds * musicEnv.peakDb.size());
                if (i >= musicEnv.peakDb.size()) break;
                const float norm = juce::jlimit (0.0f, 1.0f, (musicEnv.peakDb[i] + 66.0f) / 66.0f);
                if (norm > 0.0f) g.fillRect ((float) (r.getX() + x), mid - half * norm, 2.0f, half * norm * 2.0f);
            }
        }

        g.setColour ((after ? Ink::accent() : Ink::textNext()).withAlpha (0.75f));

        // obwiednia pokrywa env.seconds od poczatku widoku; os moze byc dluzsza (muzyka)
        const double envFrom = zoomed() && ! after ? v.from : 0.0;
        for (int x = 0; x < cols; x += 3)
        {
            const double t = v.from + (double) x / juce::jmax (1, cols) * v.len - envFrom;
            const size_t i = env.seconds > 0.0 ? (size_t) (t / env.seconds * env.peakDb.size())
                                               : (size_t) ((double) x / juce::jmax (1, cols) * env.peakDb.size());

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

        const auto rf = r.toFloat();
        auto clampX = [&] (float x) { return juce::jlimit (rf.getX(), rf.getRight(), x); };
        if(cutFrom>=0 && cutTo>cutFrom) {
            const float a=clampX(xAt(cutFrom,r)), b=clampX(xAt(cutTo,r));
            g.setColour(Ink::yellow().withAlpha(.20f));g.fillRect(a,float(r.getY()),b-a,float(r.getHeight()));
        }
        if(! editMode && review.isPreview() && mixReady()) {   // probka miksu na fali
            const float a=clampX(xAt(review.previewFrom(),r)), b=clampX(xAt(review.previewFrom()+review.previewSeconds(),r));
            g.setColour(Ink::accent().withAlpha(.16f));g.fillRect(a,float(r.getY()),b-a,float(r.getHeight()));
            g.setColour(Ink::accent());g.drawRect(a,float(r.getY()),b-a,float(r.getHeight()),1.5f);
            g.setFont(juce::Font(zs::type::small,juce::Font::bold));
            g.drawText(PL("PRÓBKA"),juce::Rectangle<float>(a+4,float(r.getY())+4,juce::jmax(60.0f,b-a),16.0f),juce::Justification::centredLeft);
        }
        if(editMode) {
            const float a=clampX(xAt(trimStart.getValue(),r)), b=clampX(xAt(trimEnd.getValue(),r));
            g.setColour(Ink::accent().withAlpha(.12f));g.fillRect(a,float(r.getY()),b-a,float(r.getHeight()));
            g.setColour(Ink::bg().withAlpha(.75f));g.fillRect(float(r.getX()),float(r.getY()),a-r.getX(),float(r.getHeight()));g.fillRect(b,float(r.getY()),r.getRight()-b,float(r.getHeight()));
        }
        // igla
        const float px = xAt (player.positionSec(), r);
        if (px >= rf.getX() && px <= rf.getRight())
        {
            g.setColour (Ink::textNow());
            g.fillRect (px, (float) r.getY(), 1.5f, (float) r.getHeight());
        }

        // licznik w rogu: ciemne okienko jak w odtwarzaczu, zakres widoku obok
        const juce::Font mono (juce::Font::getDefaultMonospacedFontName(), zs::type::small, juce::Font::bold);
        const juce::String clock = plr::clock (player.positionSec()) + " / " + plr::clock (timelineSec());
        auto box = juce::Rectangle<float> (0.0f, 0.0f, (float) mono.getStringWidth (clock) + 16.0f, mono.getHeight() + 6.0f)
                       .withPosition ((float) r.getRight() - 6.0f, (float) r.getY() + 4.0f);
        box.setX (box.getX() - box.getWidth());
        g.setColour (Ink::bg().darker (0.35f).withAlpha (0.9f));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (Ink::edge());
        g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (Ink::textNow());
        g.setFont (mono);
        g.drawText (clock, box.toNearestInt(), juce::Justification::centred);
        if (zoomed())
        {
            const auto v = view();
            g.setColour (Ink::textDim());
            g.setFont (juce::Font (zs::type::tiny));
            g.drawText (PL ("widać ") + plr::clock (v.from) + PL ("–") + plr::clock (v.from + v.len),
                        box.toNearestInt().withX (r.getX()).withRight ((int) box.getX() - 8), juce::Justification::centredRight);
        }
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

        g.setColour (juce::Colour (0xff141e2a));      // aneks E, pkt 4
        g.fillRect (strip);

        for (const auto& t : review.timeline())
        {
            float x1 = xAt (t.fromSec, strip), x2 = xAt (t.toSec, strip);
            if (x2 < strip.getX() || x1 > strip.getRight()) continue;
            x1 = juce::jmax ((float) strip.getX(), x1); x2 = juce::jmin ((float) strip.getRight(), x2);

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
        for (const auto sec : replacementSeams)
        {
            const float x = xAt (sec, strip);
            if (x >= strip.getX() && x <= strip.getRight())
            { g.setColour (Ink::yellow()); g.fillRect (x - 1.5f, (float) strip.getY(), 3.0f, (float) strip.getHeight()); }
        }

        // Fragmenty do powtorzenia dostaja podpis z czasem — to jedyna rzecz z
        // tego ekranu, ktora czlowiek bedzie przepisywal na kartke.
        g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
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
        if (job != nullptr)
        {
            if (jobDone.load()) finishJob();
            else { repaint(); return; }       // napis "pracuje" i licznik sekund
        }

        if (ready.exchange (false))
            adoptResult();

        cutPanel.setEnabled(canEdit());
        if(seamPlaying && !seamPlayer.isPlaying()) stopSeam();
        folderButton.setEnabled (doneMode ? saved.existsAsFile() : (file.existsAsFile() || saved.existsAsFile()));

        if (editMode && ! doneMode)          // krok montazu: surowe nagranie, bez miksu
        {
            updateMontageButtons();
            const bool playing = player.isPlaying();
            if (playing != wasPlaying) { wasPlaying = playing; setPlayLabel (playing); }
            if (playing) { followPlayback (player.positionSec()); repaint (waveArea()); }
            if (isThreadRunning()) repaint();
            return;
        }

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
        const bool playingNow = player.isPlaying();

        if (playingNow != wasPlaying)
        {
            wasPlaying = playingNow;
            setPlayLabel (playingNow);
        }

        if (playingNow)
            repaint (waveArea());
    }

    //==========================================================================
    void stopSeam() {
        seamPlayer.pause(); seamPlaying=false; recorder.attachPlayer(&player);
    }
    void listenCut(double from, double to) {
        if(!canEdit()) return;
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
        if(!canEdit() || cutPanel.empty()) return;
        // Plik sesji, kopia do "surowe" i lista — w tle (godzina = 0,7 GB na dysku).
        struct Out { zs::PunchResult result; bool archived=false; };
        auto out=std::make_shared<Out>();
        const auto wav=file;
        runJob(PL("Stosuję poprawki…"), 0,
            [out,wav,proposals] {
                out->result=zs::applyCutList(wav,proposals);
                if(out->result.error.isEmpty() && !out->result.applied && tw::acceptedCuts(proposals).empty()) {
                    // Wszystkie decyzje "zostaw": archiwum decyzji i pelnego nagrania,
                    // aby reczny montaz nie odziedziczyl listy ze starymi czasami.
                    const auto dir=wav.getParentDirectory().getChildFile("surowe");
                    const auto raw=dir.getNonexistentChildFile(wav.getFileNameWithoutExtension(),".wav",false);
                    out->archived=dir.createDirectory() && wav.copyFileTo(raw)
                                  && zs::cutListFile(wav).moveFileTo(zs::cutListFile(raw));
                }
            },
            [this,out,proposals] { afterProposals(out->result,out->archived,proposals); });
    }
    void afterProposals(const zs::PunchResult& result,bool archived,const std::vector<tw::CutProposal>& proposals) {
        if(result.error.isNotEmpty()) { note=PL("Nie udało się zastosować poprawek: ")+result.error; repaint(); return; }
        if(!result.applied) {
            if(!tw::acceptedCuts(proposals).empty()) {
                note=PL("Nie udało się zastosować poprawek. Sprawdź plik nagrania."); repaint(); return;
            }
            if(!archived) {
                note=PL("Nie udało się zachować pełnego nagrania i listy w folderze surowe."); repaint(); return;
            }
            cutPanel.load(file,review.rawAudio(),sr); resized();
            note=PL("Wycięto 0 fragmentów. Obie wersje pozostają w nagraniu. Pełne nagranie jest w folderze surowe.");
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,PL("Poprawki po R"),note);
            repaint(); return;
        }
        const int n=result.cuts, d=n%10, dd=n%100;
        const char* frag = n==1 ? " fragment (" : (d>=2 && d<=4 && (dd<12 || dd>14)) ? " fragmenty (" : " fragmentów (";
        juce::String message=PL("Wycięto ")+juce::String(n)+PL(frag)
            +juce::String(result.removedSec,2)+PL(" s). Pełne nagranie jest w folderze surowe.");
        if(result.audibleSeams>0) message+=PL(" Przy ")+juce::String(result.audibleSeams)
            +PL(" połączeniach po cięciu tło się zmieniło — posłuchaj tych miejsc.");
        // Archiwizacja TSV jest wykonywana przez applyCutList. Jesli sie nie
        // udala, nie wolno proponowac starych czasow na juz przycietym pliku.
        const bool staleList=zs::cutListFile(file).existsAsFile() && !zs::writeCutList(file,{});
        if(staleList) {
            message+=PL(" Nie udało się usunąć starej listy cięć — sprawdź plik .ciecia.tsv przed ponownym otwarciem.");
        }
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,PL("Poprawki po R"),message);
        // nowa wersja pliku: wczytanie i przesluchanie znow w tle
        load(file,{},false,false,[this,staleList,message] {
            if(staleList) { cutPanel.load(juce::File(),{},sr); resized(); }
            note=message; repaint();
        });
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
    bool withMusic = false;   ///< nagrane do podkladu: montaz wycisza zamiast wycinac
    int queuedStyle = -1;     ///< brzmienie klikniete, gdy miks jeszcze sie liczyl
    double musicLufs = std::numeric_limits<double>::quiet_NaN();   ///< glosnosc podkladu (raz, w watku)
    juce::File musicFile;                   ///< podklad z "<nagranie>.podklad.txt"
    zs::BackingTrack music;                 ///< w czestotliwosci nagrania; watek miksu tylko czyta
    juce::String musicProblem;              ///< np. brak pliku muzyki
    zs::Envelope musicEnv;
    mix::MusicBed::Params bedParams, mixBedParams;   ///< mixBedParams: kopia dla watku
    mix::MusicBed::Result bed, bedRaw;      ///< glos po miksie / surowy, z muzyka
    static constexpr float musicLevels[3] = { -16.0f, -11.0f, -6.0f };
    juce::TextButton musicLevel[3];
    juce::ToggleButton duckButton;
    juce::Rectangle<int> musicLabelArea;

    bool hasMusic() const { return ! music.left.empty(); }
    juce::TextButton defaultsButton;
    static constexpr double previewSec = 15.0;   ///< probka miksu (Adrian 25.10)
    bool acceptingFull = false;                  ///< po akceptacji probki: calosc, potem zapis
    juce::TextButton withMusicButton;
    bool editWithMusic = false;

    void buildMusicEnv()
    {
        musicEnv = {};
        if (! hasMusic()) return;
        std::vector<float> m (music.left);
        if (! music.right.empty())
            for (size_t i = 0; i < m.size() && i < music.right.size(); ++i) m[i] = 0.5f * (m[i] + music.right[i]);
        musicEnv.build (m, sr, 1400);
    }

    /** Glos z muzyka (MusicBed) — po miksie i surowy, do porownania przed/po.
        Watek miksu albo runJob; ekran czyta wynik dopiero po zakonczeniu. */
    void renderBed()
    {
        // 8:40 muzyki skladalo sie 17 s (przeklikanie 25.10). Teraz: glosnosc
        // muzyki raz, "przed" i "po" naraz w dwoch watkach, pomiar szczytu
        // dopiero przy zapisie (save) — zmierzone 12,6 s -> ok. 2,5 s.
        if (std::isnan (musicLufs)) musicLufs = mix::MusicBed::lufsOf (music.left, music.right, sr);
        std::thread raw ([this] { bedRaw = mix::MusicBed::render (review.rawAudio(), music.left, music.right, sr, mixBedParams, false, musicLufs); });
        bed = mix::MusicBed::render (review.mixedAudio(), music.left, music.right, sr, mixBedParams, false, musicLufs);
        raw.join();
    }

    void setBedMaterial()
    {
        const auto mono = [] (const mix::MusicBed::Result& b, size_t len)
        {
            std::vector<float> m (len, 0.0f);
            for (size_t i = 0; i < b.left.size() && i < len; ++i) m[i] = 0.5f * (b.left[i] + b.right[i]);
            return m;
        };
        const size_t len = juce::jmax (bed.left.size(), bedRaw.left.size());
        auto before = mono (bedRaw, len), after = mono (bed, len);
        const double playbackRate = recorder.currentSampleRate() > 0 ? recorder.currentSampleRate() : sr;
        if (std::abs (playbackRate - sr) > 0.1)
        {
            mix::Resampler convert; convert.prepare ((int) sr, (int) playbackRate, 64);
            std::vector<float> b2, a2;
            convert.render (before, b2); convert.render (after, a2);
            player.setMaterial (std::move (b2), std::move (a2), playbackRate, true);
        }
        else player.setMaterial (std::move (before), std::move (after), sr, true);
        recorder.attachPlayer (&player);
    }

    juce::String bedSummary() const
    {
        return PL ("Muzyka ") + juce::String (-mixBedParams.musicDb, 0) + PL (" dB pod głosem")
             + (mixBedParams.duck ? PL (", ściszana pod mową") : juce::String())
             + PL (". Całość stereo, do końca muzyki.");
    }

    void updateMusicButtons()
    {
        for (int i = 0; i < 3; ++i) zs::Look::setSelected (musicLevel[i], std::abs (musicLevels[i] - bedParams.musicDb) < 0.1f);
        duckButton.setToggleState (bedParams.duck, juce::dontSendNotification);
    }

    /** Zmiana poziomu muzyki albo sciszania: skladamy od nowa tylko glos z muzyka. */
    void remixBed()
    {
        updateMusicButtons();
        if (! hasMusic() || ! mixReady() || bed.left.empty()) return;
        const bool wasAfter = player.showingAfter();
        mixBedParams = bedParams;
        runJob (PL ("Składam głos z muzyką…"), juce::jmax (2, (int) (music.seconds() / 200.0)), [this] { renderBed(); },
            [this, wasAfter]
            {
                setBedMaterial();
                player.showAfter (wasAfter);
                juce::String summary;
                for (const auto& line : review.plainSays()) summary << PL ("• ") << PL (line.c_str()) << "\n\n";
                summary << PL ("• ") << bedSummary() << "\n\n" << PL ("Liczby i ustawienia: „Zaawansowane ustawienia”.");
                reviewSummary = summary; assessment.setText (summary, false);
                repaint();
            });
    }
    juce::File savedMp3;                  ///< MP3 obok zapisanego WAV-a (puste, gdy sie nie udalo)
    juce::String mp3Problem;              ///< dlaczego MP3 nie powstal
    static constexpr int mp3Kbps = 128;   ///< glos mono: nie do odroznienia od WAV-a
    juce::String note;
    double sr = 48000.0;

    std::atomic<bool> ready { false };
    bool wasPlaying = false;

    juce::TextButton playButton, abButton, saveButton, backButton, settingsButton;
    MixStyleCard styleCards[3];
    bool styleNote = false;                 ///< note to "Miks ... liczy się…" z chooseStyle
    int takeVersion = 0, mixedVersion = -1; ///< ktora wersja montazu jest zmiksowana...
    int loadedVersion = 0, savedVersion = -1; ///< ...wczytana z dysku i zapisana jako miks (hasUnsavedEdits)
    mix::Decide::Style mixedStyle = mix::Decide::Style::Studyjny;   ///< ...i jakim brzmieniem
    /** Tor miksu: tweaks — co ustawil czlowiek, mixTweaks — kopia dla watku,
        mixedTweaks — z czym policzono obecny miks. */
    mix::Tweaks tweaks, mixTweaks, mixedTweaks;
    MixChainView chainView;
    juce::Viewport chainPort;
    juce::TextEditor words, assessment;
    zs::StepBar flow;
    juce::TextButton markStart,markEnd,cutButton,undoButton,replaceButton,editToggle,detailsToggle,nextButton;
    bool editMode=false,detailsMode=false,middleMode=false,doneMode=false,replacementUndoAvailable=false;
    juce::TextButton doneEditButton,newTakeButton,newProjectButton;
    juce::TextButton profileButton;           ///< "Zapamietaj to brzmienie" (ekran Gotowe)
    bool profileRemembered = false;           ///< wzorzec wziety z tego miksu

    /** Wzorzec z zapisanego miksu: srednie widmo glosnych fragmentow (pierwsze
        10 minut). W tle, bo przy dlugim nagraniu to kilka sekund FFT. */
    void rememberProfile()
    {
        if (job != nullptr || ! saved.existsAsFile()) return;
        auto out = std::make_shared<mix::VoiceProfile>();
        const auto label = (saved.getFileNameWithoutExtension().upToFirstOccurrenceOf ("__", false, false)
                            + ", " + juce::Time::getCurrentTime().formatted ("%d.%m.%Y")).toStdString();
        runJob (PL ("Zapamiętuję brzmienie…"), 0,
            [this, out, label] { *out = mix::VoiceProfile::measure (review.mixedAudio(), review.sampleRate()); out->label = label; },
            [this, out]
            {
                if (! out->valid()) { note = PL ("Nie udało się zmierzyć brzmienia — za mało mowy w nagraniu."); repaint(); return; }
                wish.profile = *out;
                profileRemembered = true;
                if (onRememberProfile) onRememberProfile (juce::String::fromUTF8 (out->toText().c_str()));
                note = PL ("Zapamiętane. Następne odcinki dostaną tę barwę (Studyjny i Mocny).");
                resized(); repaint();
            });
    }
    juce::TextEditor savedPath,savedName;
    TrimSlider trimStart,trimEnd;
    TrimHandle startHandle,endHandle;
    juce::TextButton middleToggle;
    juce::String reviewSummary;
    std::vector<double> replacementSeams;
    juce::Rectangle<int> summaryArea, feedbackArea;
    juce::Label selection;
    double cutFrom=-1,cutTo=-1;
    std::vector<float> undoAudio;
    juce::TextButton folderButton;
    juce::uint32 busySinceMs = 0;   ///< kiedy zaczelo sie sluchanie/miks; 0 = nic nie trwa

    // Praca w tle poza miksem (audyt 14.10): wczytanie, ciecie, poprawki po R,
    // zapis. Godzina nagrania to ~1 min przesluchiwania — na watku okna
    // program wygladal na zawieszony. Patrz runJob().
    std::unique_ptr<std::thread> job;
    std::atomic<bool> jobDone { false };
    std::atomic<bool> jobStop { false };   ///< "przerwij" dla pracy w tle (abandonJob)
    std::function<void()> jobFinish;
    juce::String jobLabel;
    int jobExpectSec = 0;
    bool previewSave = false;       ///< zrzut ekranu: zapis od razu, bez pracy w tle
    juce::uint32 jobSinceMs = 0;
    float dragX0 = -1.0f;           ///< poczatek przeciagniecia w montazu; < 0 = brak
    double viewFrom = 0.0, viewLen = 0.0;   ///< powiekszenie fali; viewLen 0 = cale nagranie
    zs::Envelope viewEnv;
    double viewEnvFrom = -1.0, viewEnvLen = -1.0;
    int viewEnvCols = 0, viewEnvVersion = -1;
    juce::TextButton zoomIn, zoomOut, zoomAllButton;
    JobShield jobShield;
    double followPrev = -1.0;       ///< igla w poprzedniej klatce (widok idzie za odtwarzaniem)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReviewComponent)
};
