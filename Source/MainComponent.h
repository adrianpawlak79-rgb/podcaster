/*
    MainComponent.h — to, co widac w trakcie mowienia.

    Regula projektowa: w trakcie nagrywania czlowiek patrzy na JEDNA rzecz —
    lampke. Reszta ma byc dostepna, ale nie ma sie domagac uwagi. Dlatego
    lampka jest wielka i kolorowa, a lista zdarzen szara i cicha.
*/

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "Recorder.h"
#include "LampLogic.h"
#include "PrompterComponent.h"
#include "WizardComponent.h"
#include "ReviewComponent.h"
#include "Settings.h"
#include "InputChoice.h"
#include "StepBar.h"
#include "ScreenFrame.h"

//==============================================================================
namespace pl
{
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
            case tw::Kind::OffAxis:        return PL ("odsunięcie od mikrofonu");
            case tw::Kind::LevelDrift:     return PL ("poziom odjeżdża");
            case tw::Kind::NoiseRise:      return PL ("szum tła rośnie");
            case tw::Kind::Hum:            return PL ("przydźwięk");
            case tw::Kind::FalseStart:     return PL ("zły start");
            case tw::Kind::LowLevel:       return "za cicho";
        }
        return "?";
    }

    /** Staly napis UTF-8 — LampLogic trzyma sam wskaznik, wiec musi przezyc. */
    inline const char* adviceRaw (tw::Kind k)
    {
        switch (k)
        {
            case tw::Kind::DigitalSilence: return "sprawdź kabel i wejście";
            case tw::Kind::Clipping:       return "przykręć wzmocnienie";
            case tw::Kind::TooHot:         return "odsuń się albo przykręć";
            case tw::Kind::Plosive:        return "powtórz — odchyl się od mikrofonu";
            case tw::Kind::MouthClick:     return "powtórz frazę";
            case tw::Kind::Breath:         return "sprawdź po nagraniu";
            case tw::Kind::Sibilance:      return "sprawdź po nagraniu";
            case tw::Kind::OffAxis:        return "wróć na oś mikrofonu";
            case tw::Kind::LevelDrift:     return "poziom spadł — wróć bliżej";
            case tw::Kind::NoiseRise:      return "coś zaczęło szumieć w pokoju";
            case tw::Kind::Hum:            return "przydźwięk z sieci — sprawdź kable";
            case tw::Kind::FalseStart:     return "zacznij frazę od nowa";
            case tw::Kind::LowLevel:       return "mów głośniej albo bliżej";
        }
        return "";
    }

    inline juce::String advice (tw::Kind k) { return PL (adviceRaw (k)); }

    /** Zmierzona liczba, ktora wywolala zdarzenie. Bez niej nie da sie odroznic
        prawdziwego alarmu od falszywego — a to jest jedyne ryzyko, ktore moze
        zabic cale narzedzie. */
    inline juce::String value (tw::Kind k, float v)
    {
        switch (k)
        {
            case tw::Kind::NoiseRise:  return "+" + juce::String (v, 1) + PL (" dB nad tłem");
            case tw::Kind::Clipping:
            case tw::Kind::TooHot:
            case tw::Kind::LowLevel:   return juce::String (v, 1) + " dBFS";
            case tw::Kind::Plosive:
            case tw::Kind::MouthClick:
            case tw::Kind::Sibilance:
            case tw::Kind::OffAxis:
            case tw::Kind::Hum:        return "+" + juce::String (v, 1) + " dB";
            case tw::Kind::LevelDrift: return juce::String (v, 1) + " dB";
            case tw::Kind::FalseStart: return juce::String (v, 2) + " s";
            default:                   return juce::String (v, 1);
        }
    }

    /** Czy tego rodzaju wolno wyciszyc przyciskiem "To bylo OK".

        Przester i cisza cyfrowa NIE. To nie sa opinie programu o tym, jak cos
        brzmi — to zniszczone probki i brak sygnalu. Wyciszenie przestera
        zalatwia objaw i zostawia w pliku nagranie, ktorego nie da sie juz
        naprawic zadnym miksem. Reszta rodzajow to ocena i moze byc bledna,
        wiec te wolno uciszyc. */
    inline bool canMute (tw::Kind k)
    {
        return k != tw::Kind::Clipping && k != tw::Kind::DigitalSilence;
    }

    inline juce::String hhmmss (double s)
    {
        const int t = (int) s;
        return juce::String::formatted ("%02d:%02d", t / 60, t % 60);
    }
}

//==============================================================================
class Lamp : public juce::Component
{
public:
    /** @param dim  sygnalu chwilowo nie ma (pauza miedzy frazami). Kolor zostaje
                    zielony — nic zlego sie nie dzieje — ale gasnie, zeby dalo sie
                    jednym spojrzeniem odroznic "mowisz i jest dobrze" od
                    "cisza, czekam". */
    void setRecording(bool value) { recording=value; }
    void setState (int severity, const juce::String& msg, bool dimmed = false)
    {
        state = severity;
        message = msg;
        dim = dimmed;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (8);
        const int d = juce::jmax(0, juce::jmin (r.getWidth()-20, r.getHeight() - 54));
        auto circle = juce::Rectangle<int> (d, d).withCentre ({ r.getCentreX(), r.getY() + d / 2 + 6 });

        juce::Colour c;
        juce::String label;

        switch (state)
        {
            case 3:  c = zs::Ink::clip();   label = "STOP";         break;
            case 2:  c = zs::Ink::yellow(); label = "POPRAW";       break;
            case 1:  c = zs::Ink::yellow(); label = PL ("POWTÓRZ"); break;
            default: c = zs::Ink::green();  label = PL ("MÓW");     break;
        }

        if(!recording && state==0) { label=PL("GOTOWY"); c=zs::Ink::textDim(); }

        if (dim && state == 0)
            c = c.withMultipliedBrightness (0.72f);

        g.setColour (c.withAlpha (dim && state == 0 ? 0.10f : 0.18f));
        g.fillEllipse (circle.expanded (10).toFloat());
        g.setColour (c);
        g.fillEllipse (circle.toFloat());

        g.setColour (!recording && state==0 ? zs::Ink::bg() : zs::Ink::onAccent());
        g.setFont (juce::Font (juce::jmax (14.0f, d * (recording?0.22f:0.16f)), juce::Font::bold));
        g.drawText (label, circle, juce::Justification::centred);

        g.setColour (zs::Ink::textNext());
        g.setFont (juce::Font (zs::type::body));
        g.drawFittedText (message,
                          r.removeFromBottom (44).reduced (4, 0),
                          juce::Justification::centredTop, 2);
    }

private:
    bool recording=false;
    int state = 0;
    bool dim = false;
    juce::String message = PL ("Naciśnij Nagraj, kiedy będziesz gotowy.");
};

//==============================================================================
class Meter : public juce::Component
{
public:
    void setLevel (float v) { lvl = v; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (zs::Ink::edge());
        g.fillRect (r);

        const float db = lvl > 1.0e-6f ? juce::Decibels::gainToDecibels (lvl) : -60.0f;
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);

        auto bar = r.withWidth (r.getWidth() * norm);
        g.setColour (db > -1.5f ? zs::Ink::clip()
                   : db > -8.0f ? zs::Ink::yellow()
                                : zs::Ink::green());
        g.fillRect (bar);

        // znacznik zalecanego szczytu: -12 dBFS
        const float mark = r.getX() + r.getWidth() * ((-12.0f + 60.0f) / 60.0f);
        g.setColour (zs::Ink::textNext());
        g.fillRect (mark, r.getY(), 1.0f, r.getHeight());

        g.setColour (zs::Ink::textNext());
        g.setFont (juce::Font (11.0f));
        g.drawText (db <= -59.0f ? juce::String ("cisza")
                                 : juce::String (db, 1) + " dBFS",
                    getLocalBounds().reduced (5, 0), juce::Justification::centredRight);
    }

private:
    float lvl = 0.0f;
};

//==============================================================================
class MainComponent : public juce::Component,
                      private juce::Timer,
                      private juce::ChangeListener
{
public:
    /** @param openForReview  nagranie do oceny od razu po starcie, zamiast
                              ekranu nagrywania. Z linii polecen: --ocena plik.wav.

        Po co to jest, poza wygoda: ekran po nagraniu dotad nie dal sie
        uruchomic inaczej niz przez prawdziwy mikrofon, czyli nie dal sie
        sprawdzic przy budowaniu. Teraz da sie — a czlowiek zyskuje mozliwosc
        obejrzenia wczorajszego nagrania bez nagrywania nowego. */
    explicit MainComponent (const juce::File& openForReview = {}, bool reviewNow = false, bool previewMode = false)
    {
        noDevice = previewMode;
        if (!previewMode) {
            // Karta i wejscie z poprzedniego uruchomienia; gdy jej nie ma —
            // domyslna systemu (initialise sam wraca do domyslnej).
            const auto saved = juce::parseXML (settings.audioState());
            deviceManager.initialise (2, 2, saved.get(), true);
            deviceManager.addAudioCallback (&recorder);
            deviceManager.addChangeListener (this);
        }

        deviceBox.onChange = [this] { chooseDevice(); };
        inputBox.onChange  = [this] { chooseInput(); };
        heardLabel.setFont (juce::Font (zs::type::small));
        heardLabel.setColour (juce::Label::textColourId, zs::Ink::accent());
        heardLabel.setJustificationType (juce::Justification::centredRight);
        for (auto* c : { (juce::Component*) &deviceBox, (juce::Component*) &inputBox, (juce::Component*) &heardLabel })
            addChildComponent (*c);
        refreshInputBoxes();

        selector = std::make_unique<juce::AudioDeviceSelectorComponent> (
                       deviceManager, 1, 2, 0, 2, false, false, true, false);
        addAndMakeVisible (*selector);

        addAndMakeVisible (lamp);

        prompterButton.onClick = [this] { if (preparation) openScript(); else openLastOrChoose(); };
        addAndMakeVisible (prompterButton);

        otherTextButton.setButtonText (PL ("Zmień tekst"));
        otherTextButton.onClick = [this] { openScript(); };
        addAndMakeVisible (otherTextButton);

        checkButton.setButtonText (PL ("Sprawdź sprzęt"));
        checkButton.onClick = [this] { enterWizard(); };
        addAndMakeVisible (checkButton);

        settingsButton.setButtonText (PL("Mikrofon i sprzęt"));
        settingsButton.onClick = [this] {
            settingsOpen = !settingsOpen; refreshHomeVisibility(); resized(); repaint();
        };
        addAndMakeVisible(settingsButton);
        detailsButton.setButtonText (PL("Pokaż uwagi"));
        detailsButton.onClick = [this] {
            detailsOpen = !detailsOpen; refreshHomeVisibility(); resized(); repaint();
        };
        addAndMakeVisible(detailsButton);
        openButton.setButtonText (PL("Otwórz nagranie"));
        openButton.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>(PL("Wybierz nagranie"),sessionFolder,"*.wav;*.flac;*.aiff");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safe=juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& fc) {
                    if(safe && fc.getResult().existsAsFile()) safe->openReview(fc.getResult(),false,true);
                });
        };
        addAndMakeVisible(openButton);
        zs::Look::setKind (recButton, zs::ui::ButtonKind::Record);
        updatePrompterButton();
        addAndMakeVisible (meter);

        recButton.setButtonText ("Nagraj");
        recButton.onClick = [this] { toggleRecord(); };
        addAndMakeVisible (recButton);

        // "Powtorz wers" istnieje tylko w trakcie nagrywania z tekstem — bez
        // tekstu nie ma wersu, do ktorego mozna by wrocic.
        repeatButton.setButtonText (PL ("Powtórz wers"));
        zs::Look::setKey (repeatButton, "R");
        repeatButton.onClick = [this] { repeatLine(); };
        addChildComponent (repeatButton);

        hints.setColour (juce::Label::textColourId, zs::Ink::textNext());
        hints.setFont (juce::Font (zs::type::small));
        hints.setJustificationType (juce::Justification::centredLeft);
        hints.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (hints);

        flow.setSteps ({ PL ("Przygotowanie"), PL ("Nagranie"), PL ("Odsłuch"), PL ("Montaż"), PL ("Gotowe") });
        addAndMakeVisible (flow);

        setWantsKeyboardFocus (true);

        folderButton.setButtonText (PL ("Folder sesji…"));
        folderButton.onClick = [this] { chooseFolder(); };
        addAndMakeVisible (folderButton);

        // Jedno klikniecie po falszywym alarmie. Bez suwakow, bez ustawien,
        // bez grzebania w progach — ten rodzaj przestaje przerywac w tej sesji,
        // ale nadal trafia na liste.
        okButton.setButtonText (PL ("Wycisz ten typ uwagi"));
        okButton.setEnabled (false);
        okButton.onClick = [this]
        {
            if (! pl::canMute (lastFlagged))
            {
                append (PL ("--- ") + pl::kind (lastFlagged)
                        + PL (" zostaje — tego nie da się naprawić po nagraniu"));
                okButton.setEnabled (false);
                return;
            }

            mutedKinds.add ((int) lastFlagged);
            append (PL ("--- wyciszone w tej sesji: ") + pl::kind (lastFlagged));
            okButton.setEnabled (false);
        };
        addAndMakeVisible (okButton);

        events.setMultiLine (true);
        events.setReadOnly (true);
        events.setScrollbarsShown (true);
        events.setCaretVisible (false);
        events.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
        events.setColour (juce::TextEditor::backgroundColourId, zs::Ink::panel());
        events.setColour (juce::TextEditor::textColourId,       zs::Ink::textNext());
        events.setColour (juce::TextEditor::outlineColourId,    zs::Ink::edge());
        addAndMakeVisible (events);

        status.setJustificationType (juce::Justification::centredLeft);
        status.setColour (juce::Label::textColourId, zs::Ink::textNext());
        addAndMakeVisible (status);

        nameInput.setText (PL("Nowy odcinek"));
        nameInput.onTextChange = [this] { repaint(); };
        scriptInput.setMultiLine (true);
        scriptInput.setReturnKeyStartsNewLine (true);
        scriptInput.setTextToShowWhenEmpty (PL("Tu możesz wpisać tekst lub otworzyć plik TXT."), zs::Ink::textDim());
        if (settings.lastScript().existsAsFile()) scriptInput.setText (settings.lastScript().loadFileAsString());
        for (auto* input : { &nameInput, &scriptInput }) {
            input->setColour (juce::TextEditor::backgroundColourId, zs::Ink::panel());
            input->setColour (juce::TextEditor::textColourId, zs::Ink::textNow());
            input->setColour (juce::TextEditor::outlineColourId, zs::Ink::edge());
            addAndMakeVisible (*input);
        }
        prepareButton.setButtonText (PL("Przejdź do nagrania →"));
        zs::Look::setKind (prepareButton, zs::ui::ButtonKind::Primary);
        prepareButton.onClick = [this] {
            preparation = false; settingsOpen = false; detailsOpen = false;
            if (scriptInput.getText().trim().isNotEmpty()) {
                ensurePrompter(); prompter->load (scriptInput.getText()); showPrompter (true);
            } else { resized(); repaint(); }
        };
        addAndMakeVisible (prepareButton);
        preparationButton.setButtonText (PL("← Przygotowanie"));
        preparationButton.onClick = [this] {
            preparation = true; if (inPrompter) showPrompter (false); resized(); repaint();
        };
        addAndMakeVisible (preparationButton);
        sessionFolder = settings.sessionFolder();

        // ---- kreator idzie pierwszy, ale tylko raz --------------------------
        //
        // Program zaczyna od sprawdzenia sprzetu, nie od przycisku "nagrywaj".
        // Rzeczy, ktore kreator znajduje, to dokladnie te, ktorych po nagraniu
        // nie da sie naprawic — a czlowiek, ktory jeszcze nie mowi, ma uwage
        // na czytanie.
        //
        // Ale PRZY DRUGIM uruchomieniu ten sam kreator jest juz tylko bramka
        // miedzy czlowiekiem a nagrywaniem. Sprzet sie nie zmienil, pokoj tez
        // nie, a szesc krokow do przejscia zanim wolno powiedziec pierwsze
        // slowo konczy sie tym, ze ktos klika "pomin" — i wtedy program nie ma
        // ani pomiarow, ani zaufania. Wiec: raz przejdziesz, potem program
        // pamieta i tylko przypomina, kiedy to bylo.
        if (previewMode || settings.hardwareChecked())
        {
            showRecordingUi (true);
            lamp.setState (0, PL ("Naciśnij Nagraj, kiedy będziesz gotowy."), true);

            const int d = settings.daysSinceCheck();

            append (PL ("sprzęt sprawdzony ")
                    + (d <= 0 ? juce::String (PL ("dzisiaj"))
                              : juce::String (d) + PL (d == 1 ? " dzień temu" : " dni temu"))
                    + (settings.hardwareSummary().isNotEmpty()
                           ? PL (": ") + settings.hardwareSummary() : juce::String())
                    + PL ("."));

            // Po miesiacu mikrofon mogl zostac przestawiony, a pokoj
            // umeblowany. Program nie blokuje nagrywania — przypomina.
            if (d >= 30)
                append (PL ("--- minął miesiąc. Jeśli coś zmieniło się w pokoju "
                            "albo w ustawieniu mikrofonu, kliknij „Sprawdź sprzęt”."));
        }
        else
        {
            enterWizard();
        }

        // Nagranie z linii polecen przeskakuje wszystko, takze kreator: nie
        // oceniamy sprzetu, tylko gotowy plik, wiec pytanie o tlo pokoju jest
        // tu bez sensu.
        if (openForReview.existsAsFile())
        {
            if (wizard != nullptr)
            {
                wizard = nullptr;
                wizardStarted = false;
                inWizard = false;
                showRecordingUi (true);
            }

            openReview (openForReview, reviewNow, true);
        }

        setSize (1280, 800);
        startTimerHz (30);
    }

    ~MainComponent() override
    {
        deviceManager.removeChangeListener (this);
        saveAudioState();
        deviceManager.removeAudioCallback (&recorder);
    }

    /** Dla okna, ktore chce zapamietac swoje polozenie. */
    Settings& userSettings() noexcept { return settings; }
    void previewSettings() { settingsOpen=true;resized();repaint(); }
    void previewPrompter() {
        preparation = false;
        recorder.prepareDevice(48000,512,1);
        prompter=std::make_unique<PrompterComponent>(recorder,settings);
        prompter->onClose=[this]{showPrompter(false);};
        prompter->setSpeech(&speech);
        prompter->onStopRequest=[this]{ if(recorder.isRecording()) toggleRecord(); };
        addAndMakeVisible(*prompter);
        prompter->load(PL("Dziś porozmawiamy o tym, co naprawdę warto usłyszeć.\nZatrzymaj się na chwilę. Weź spokojny oddech.\nTo jest Twoje miejsce i Twój głos.\nNie musisz się spieszyć. Dobra opowieść potrzebuje czasu."));
        showPrompter(true);
    }



    void paint (juce::Graphics& g) override
    {
        using I = zs::Ink;
        g.fillAll (I::bg());
        if (inWizard || inReview) return;
        const bool rec = recorder.isRecording();
        const auto name = recordingName();
        zs::ScreenFrame::paint (g, getLocalBounds(), name,
            rec ? PL ("Esc — zatrzymaj i oceń") : preparation ? PL("Wybierz mikrofon i przygotuj tekst") : PL ("Nagraj, a potem odsłuchaj podejście"));
        auto side = zs::ScreenFrame::sidebar (getLocalBounds());
        g.setColour (I::bar()); g.fillRect (side);
        g.setColour (I::edge()); g.drawVerticalLine (side.getX(), (float) side.getY(), (float) side.getBottom());
        if (preparation) { paintPreparation (g); return; }
        auto main = zs::ScreenFrame::mainArea (getLocalBounds());
        g.setFont (juce::Font (zs::type::tiny, juce::Font::bold)); g.setColour (I::textDim());
        g.drawText (PL ("02 / NAGRANIE"), main.removeFromTop (18), juce::Justification::centredLeft);
        g.setFont (juce::Font (zs::type::title, juce::Font::bold)); g.setColour (I::textNow());
        g.drawText (inPrompter ? PL ("Nagranie z prompterem") : PL ("Nagranie bez promptera"),
                    main.removeFromTop (38), juce::Justification::centredLeft);
        g.setFont (juce::Font (zs::type::body)); g.setColour (I::textNext());
        g.drawText (inPrompter ? PL ("Czytaj spokojnie. Tekst podąża za Twoim głosem.")
                              : PL ("Mów swobodnie. Lampka czuwa nad nagraniem."),
                    main.removeFromTop (24), juce::Justification::centredLeft);
        auto card = homeCard();
        g.setColour (I::panel()); g.fillRoundedRectangle (card.toFloat(), 6.0f);
        g.setColour (I::edge()); g.drawRoundedRectangle (card.toFloat(), 6.0f, 1.0f);
        paintRecording (g, side.reduced (zs::ui::frame::sidePadding, 24));
        if (! rec && ! inPrompter && ! detailsOpen)
        {
            g.setColour (I::textNext()); g.setFont (juce::Font (zs::type::body));
            g.drawText (PL ("Naciśnij Nagraj, kiedy będziesz gotowy."),
                        card.withY (card.getBottom() - 74).withHeight (24), juce::Justification::centred);
        }
        if (settingsOpen)
        {
            auto box = settingsBox();
            g.setColour (I::bar()); g.fillRect (box);
            g.setColour (I::textNow()); g.setFont (juce::Font (zs::type::heading, juce::Font::bold));
            g.drawText (PL ("Mikrofon i sprzęt"), box.reduced (22).removeFromTop (30), juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        if (wizard) wizard->setBounds (getLocalBounds());
        if (inWizard) return;
        if (inReview && review) { review->setBounds (getLocalBounds()); return; }
        const bool rec = recorder.isRecording();
        refreshHomeVisibility();
        settingsButton.setBounds (getWidth() - 196, 14, 174, 36);
        flow.setBounds (22, zs::ui::frame::header + 8, getWidth() - 44, zs::StepBar::preferredHeight);
        if (preparation) { layoutPreparation(); return; }
        preparationButton.setBounds (getWidth() - 380, 14, 174, 36);
        auto card = homeCard();
        auto side = zs::ScreenFrame::sidebar (getLocalBounds()).reduced (22, 24);
        side.removeFromTop (114);
        if (inPrompter && prompter)
        {
            prompter->setBounds (card);
            lamp.setBounds (side.removeFromTop (juce::jmin (190, side.getHeight() - (rec ? 178 : 196))));
        }
        else
        {
            prompterButton.setBounds (side.removeFromTop (40)); side.removeFromTop (10);
            otherTextButton.setBounds (side.removeFromTop (32)); side.removeFromTop (22);
            const int d = juce::jlimit (150, 220, card.getHeight() - 110);
            if (rec) lamp.setBounds (card.withSizeKeepingCentre (juce::jmin (card.getWidth() - 40, 340), d + 64));
            else if (! detailsOpen) recButton.setBounds (card.withSizeKeepingCentre (d, d).translated (0, -20));
        }
        auto actions = side.removeFromBottom (rec ? (inPrompter ? 94 : 42) : (inPrompter ? 112 : detailsOpen ? 126 : 82));
        if (rec || inPrompter)
        {
            recButton.setBounds (actions.removeFromBottom (rec ? 42 : 104));
            actions.removeFromBottom (8);
            repeatButton.setBounds (actions.removeFromBottom (40));
        }
        else
        {
            openButton.setBounds (actions.removeFromBottom (36)); actions.removeFromBottom (8);
            detailsButton.setBounds (actions.removeFromBottom (36));
            if (detailsOpen) recButton.setBounds (actions.removeFromTop (34));
        }
        side.removeFromTop (inPrompter ? 20 : 28);
        meter.setBounds (side.removeFromTop (20)); side.removeFromTop (12);
        status.setBounds (side.removeFromTop (juce::jmin (78, side.getHeight())));
        hints.setBounds (card.withY (card.getBottom() + 8).withHeight (28));
        if (detailsOpen)
        {
            auto area = card.reduced (18);
            okButton.setBounds (area.removeFromBottom (34).removeFromRight (206));
            area.removeFromBottom (8); events.setBounds (area);
        }
        if (settingsOpen)
        {
            auto panel = settingsBox().reduced (18); panel.removeFromTop (44);
            folderButton.setBounds (panel.removeFromBottom (38)); panel.removeFromBottom (10);
            checkButton.setBounds (panel.removeFromBottom (38)); panel.removeFromBottom (12);
            selector->setBounds (panel);
        }
        if (inPrompter && prompter)
            for (auto* c : std::initializer_list<juce::Component*> { &lamp, &recButton, &repeatButton, &hints, &flow })
                c->toFront (false);
    }

    /** Esc na ekranie nagrywania (bez promptera). Prompter ma swoje klawisze
        i wola te same funkcje. */
    bool keyPressed (const juce::KeyPress& k) override
    {
        const auto a = zs::ui::keyAction (k.getKeyCode() == juce::KeyPress::escapeKey,
                                          (char32_t) k.getTextCharacter(),
                                          recorder.isRecording(), inPrompter && prompter != nullptr && prompter->hasText());

        if (a == zs::ui::KeyAction::StopAndReview) { toggleRecord(); return true; }
        if (a == zs::ui::KeyAction::RepeatLine) { repeatLine(); return true; }
        return false;
    }

    void previewWizard()
    {
        enterWizard();
        recorder.prepareDevice (48000, 512, 1);
        wizard->begin (48000.0);
        wizardStarted = true;
    }

    /** Zrzut ekranu nagrywania przy budowaniu: nagrywa naprawde, ale do
        katalogu tymczasowego i bez urzadzenia — plik jest pusty i znika. */
    void previewRecording (bool withPrompter)
    {
        preparation = false;
        recorder.prepareDevice (48000, 512, 1);

        if (withPrompter) previewPrompter();

        previewFolder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("zs-podglad-nagrania");
        toggleRecord();
        lamp.setRecording (true);
        lamp.setState (0, PL ("Nagrywasz. Mów spokojnie."), false);
        timerCallback();
        resized();
    }

    void previewReview (const juce::File& wav)
    {
        if (wav.existsAsFile()) openReview (wav, true, true);
    }

private:
    juce::String recordingName() const
    {
        return nameInput.getText().trim().isEmpty() ? PL("Nowy odcinek") : nameInput.getText().trim();
    }

    juce::Rectangle<int> homeCard() const
    {
        auto r = zs::ScreenFrame::mainArea (getLocalBounds());
        r.removeFromTop (98);
        r.removeFromBottom (36);
        return r;
    }

    juce::Rectangle<int> settingsBox() const
    {
        return zs::ScreenFrame::sidebar (getLocalBounds());
    }

    void paintRecording (juce::Graphics& g, juce::Rectangle<int> side)
    {
        using I = zs::Ink;
        g.setColour (I::textDim()); g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
        g.drawText (PL ("CZAS NAGRANIA"), side.removeFromTop (18), juce::Justification::centredLeft);
        const double t = recorder.isRecording() ? recorder.recordedSeconds() : 0.0;
        g.setColour (I::textNow()); g.setFont (juce::Font (46.0f, juce::Font::bold));
        g.drawText (pl::hhmmss (t), side.removeFromTop (60), juce::Justification::centredLeft);
        g.setColour (I::textNext()); g.setFont (juce::Font (zs::type::small));
        g.drawText (recorder.isRecording() ? PL ("Nagrywanie trwa. Patrz na lampkę.") : PL ("Twoje miejsce i Twój głos."),
                    side.removeFromTop (24), juce::Justification::centredLeft);
        if (! inPrompter)
        {
            if (recorder.isRecording())
            {
                g.drawFittedText (PL ("Mów w swoim tempie.\nPo zatrzymaniu program oceni\nnagranie i dobierze miks."),
                                  side.withHeight (110).reduced (0, 18), juce::Justification::topLeft, 3);
            }
            side.removeFromTop (116);
            g.setColour (I::edge()); g.drawHorizontalLine (side.getY(), (float) side.getX(), (float) side.getRight());
            side.removeFromTop (8);
            g.setColour (I::textNext());
            g.drawText (PL ("SYGNAŁ MIKROFONU"), side.removeFromTop (20), juce::Justification::centredLeft);
        }
    }

    void refreshHomeVisibility() {
        const bool rec=recorder.isRecording();
        const bool home=!inWizard && !inReview && !inPrompter;
        const bool idleHome=home && !rec;
        settingsButton.setVisible(!inWizard && !inReview);settingsButton.setEnabled(idleHome);
        detailsButton.setVisible(idleHome && !settingsOpen);openButton.setVisible(idleHome && !settingsOpen);
        settingsButton.setButtonText(settingsOpen?PL("Zamknij ustawienia"):PL("Mikrofon i sprzęt"));
        detailsButton.setButtonText(detailsOpen?PL("Ukryj uwagi"):PL("Pokaż uwagi"));
        selector->setVisible(idleHome && settingsOpen);
        folderButton.setVisible(idleHome && settingsOpen);checkButton.setVisible(idleHome && settingsOpen);
        events.setVisible(idleHome && detailsOpen);okButton.setVisible(idleHome && detailsOpen);
        prompterButton.setVisible(idleHome && !settingsOpen);
        otherTextButton.setVisible(idleHome && !settingsOpen && settings.lastScript().existsAsFile());
        flow.setVisible(!inWizard && !inReview);
        flow.setCurrent(preparation ? 0 : 1);
        const bool prepare = idleHome && preparation;
        nameInput.setVisible(prepare); scriptInput.setVisible(prepare && !detailsOpen);
        prepareButton.setVisible(prepare && !settingsOpen);
        preparationButton.setVisible(!preparation && !rec && !inWizard && !inReview);
        for (auto* c : { (juce::Component*) &deviceBox, (juce::Component*) &inputBox, (juce::Component*) &heardLabel })
            c->setVisible(prepare && !settingsOpen);
        if (prepare) {
            prompterButton.setVisible(!detailsOpen); otherTextButton.setVisible(false);
            checkButton.setVisible(true); folderButton.setVisible(settingsOpen);
        }
        lamp.setVisible(!inWizard && !inReview && (rec || inPrompter));
        meter.setVisible(!inWizard && !inReview && !settingsOpen);
        status.setVisible(!inWizard && !inReview && !settingsOpen);
        zs::Look::setKind(recButton, rec || detailsOpen ? zs::ui::ButtonKind::Primary : zs::ui::ButtonKind::Record);

        const bool hasScript = inPrompter && prompter != nullptr && prompter->hasText();
        if (!preparation) updatePrompterButton();
        if (prompter != nullptr) prompter->refreshControls();
        recButton.setVisible(!inWizard && !inReview && !preparation);
        repeatButton.setVisible(inPrompter && rec && hasScript);
        hints.setVisible((inPrompter || rec) && !inWizard && !inReview);
        hints.setText(PL(zs::ui::hintLine(rec, hasScript).c_str()), juce::dontSendNotification);
        hints.setJustificationType(inPrompter ? juce::Justification::centred : juce::Justification::centredLeft);
    }

    void paintPreparation (juce::Graphics& g)
    {
        auto main = zs::ScreenFrame::mainArea (getLocalBounds());
        auto text = [&g] (const juce::String& value, juce::Rectangle<int> r, float size, bool bold = false) {
            g.setColour (bold ? zs::Ink::textNow() : zs::Ink::textNext());
            g.setFont (juce::Font (size, bold ? juce::Font::bold : juce::Font::plain));
            g.drawFittedText (value, r, juce::Justification::centredLeft, 2);
        };
        text (PL("01 / PRZYGOTOWANIE"), main.removeFromTop(18), zs::type::tiny);
        text (PL("Przygotowanie nagrania"), main.removeFromTop(38), zs::type::title, true);
        text (PL("Ustaw nazwę i mikrofon. Dodaj tekst, jeśli chcesz używać promptera."), main.removeFromTop(36), zs::type::body);
        main.removeFromTop(12);
        text (PL("Nazwa nagrania"), main.removeFromTop(22), zs::type::small);
        main.removeFromTop(56);
        text (PL("TEKST DO PROMPTERA"), main.removeFromTop(38), zs::type::small, true);
        text (PL("Bez tekstu możesz nagrywać swobodnie."), main.removeFromBottom(28), zs::type::small);
        if (!settingsOpen) {
            auto side = settingsBox().reduced(22,24);
            text (PL("Przed nagraniem"), side.removeFromTop(32), zs::type::heading, true);
            side.removeFromTop(4);
            text (settings.hardwareChecked() ? PL("Sprzęt został sprawdzony") : PL("Mikrofon nie został sprawdzony"), side.removeFromTop(32), zs::type::body);
            side.removeFromTop(8);
            text (PL("Karta / urządzenie"), side.removeFromTop(18), zs::type::small);
            side.removeFromTop(30 + 6);
            text (PL("Wejście z mikrofonem"), side.removeFromTop(18), zs::type::small);
            text (PL("Nagranie rozpocznie się po naciśnięciu Nagraj."), settingsBox().reduced(22,12).removeFromBottom(40), zs::type::small);
        } else {
            g.setColour(zs::Ink::textNow()); g.setFont(juce::Font(zs::type::heading,juce::Font::bold));
            g.drawText(PL("Mikrofon i sprzęt"),settingsBox().reduced(22).removeFromTop(30),juce::Justification::centredLeft);
        }
    }

    void layoutPreparation()
    {
        auto main = zs::ScreenFrame::mainArea(getLocalBounds());
        main.removeFromTop(126);
        nameInput.setBounds(main.removeFromTop(40)); main.removeFromTop(16);
        auto row = main.removeFromTop(38);
        prompterButton.setButtonText(PL("Otwórz tekst"));
        prompterButton.setBounds(row.removeFromRight(156)); main.removeFromTop(8);
        main.removeFromBottom(28);
        scriptInput.setBounds(main);
        if (detailsOpen) {
            okButton.setBounds(main.removeFromBottom(34).removeFromRight(206));
            main.removeFromBottom(8); events.setBounds(main);
        }
        auto side = settingsBox().reduced(22,24);
        auto bottom = side.removeFromBottom(settingsOpen ? 0 : 168);
        bottom.removeFromBottom(36);
        prepareButton.setBounds(bottom.removeFromBottom(42)); bottom.removeFromBottom(8);
        openButton.setBounds(bottom.removeFromBottom(32)); bottom.removeFromBottom(8);
        detailsButton.setBounds(bottom.removeFromBottom(32));
        if (settingsOpen) {
            side.removeFromTop(44);
            folderButton.setBounds(side.removeFromBottom(36)); side.removeFromBottom(8);
            checkButton.setBounds(side.removeFromBottom(36)); side.removeFromBottom(8);
            selector->setBounds(side);
        } else {
            // Uklad musi sie zmiescic przy 960 x 640: folder sesji zostaje
            // w "Mikrofon i sprzet", tu sa tylko rzeczy przed kazdym nagraniem.
            side.removeFromTop(32 + 4 + 32 + 8 + 18);
            deviceBox.setBounds(side.removeFromTop(30)); side.removeFromTop(6);
            heardLabel.setBounds(side.removeFromTop(18));
            inputBox.setBounds(side.removeFromTop(30)); side.removeFromTop(8);
            meter.setBounds(side.removeFromTop(20)); side.removeFromTop(8);
            checkButton.setBounds(side.removeFromTop(34));
            folderButton.setVisible(false);
            status.setVisible(false);
        }
    }

    //==========================================================================
    // ---- karta i wejscie z mikrofonem (zasady: InputChoice.h) ------------------

    /** Zmiana karty — z naszych list albo z okna "Mikrofon i sprzet". */
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        refreshInputBoxes();
        saveAudioState();
    }

    void saveAudioState()
    {
        if (noDevice) return;
        if (auto x = deviceManager.createStateXml())
            settings.setAudioState (x->toString());
    }

    /** Listy odtwarzane z tego, co karta ma teraz wlaczone. Bez onChange:
        samo odswiezenie listy nie moze przestawiac karty. */
    void refreshInputBoxes()
    {
        deviceBox.clear (juce::dontSendNotification);
        inputBox.clear (juce::dontSendNotification);
        deviceBox.setTextWhenNothingSelected (PL ("Brak urządzenia"));
        inputBox.setTextWhenNothingSelected (PL ("Brak wejść"));

        auto* type = noDevice ? nullptr : deviceManager.getCurrentDeviceTypeObject();
        auto* device = noDevice ? nullptr : deviceManager.getCurrentAudioDevice();
        deviceBox.setEnabled (type != nullptr);
        inputBox.setEnabled (device != nullptr);
        if (type == nullptr) return;

        const auto names = type->getDeviceNames (true);
        deviceBox.addItemList (names, 1);
        deviceBox.setTextWhenNothingSelected (PL ("Wybierz kartę"));
        deviceBox.setSelectedItemIndex (names.indexOf (deviceManager.getAudioDeviceSetup().inputDeviceName),
                                        juce::dontSendNotification);
        if (device == nullptr) return;

        std::vector<std::string> driver;
        for (const auto& n : device->getInputChannelNames()) driver.push_back (n.toStdString());

        const auto items = zs::input::labels (driver);
        for (size_t i = 0; i < items.size(); ++i)
            inputBox.addItem (juce::String::fromUTF8 (items[i].c_str()), (int) i + 1);

        const auto mask = (uint64_t) device->getActiveInputChannels().toInt64();
        inputBox.setSelectedId (zs::input::choiceFromMask (mask) + 1, juce::dontSendNotification);
        inputBox.setEnabled (driver.size() > 1);
    }

    void chooseDevice()
    {
        auto setup = deviceManager.getAudioDeviceSetup();
        setup.inputDeviceName = deviceBox.getText();
        setup.useDefaultInputChannels = true;       // inna karta: znow automatycznie
        applySetup (setup);
    }

    void chooseInput()
    {
        auto* device = deviceManager.getCurrentAudioDevice();
        if (device == nullptr) return;

        const int n = device->getInputChannelNames().size();
        const auto mask = zs::input::channelMask (inputBox.getSelectedId() - 1, n);

        auto setup = deviceManager.getAudioDeviceSetup();
        setup.useDefaultInputChannels = false;
        setup.inputChannels.clear();
        for (int b = 0; b < n && b < zs::input::maxChannels; ++b)
            if (mask & (uint64_t (1) << b)) setup.inputChannels.setBit (b);

        applySetup (setup);
    }

    void applySetup (const juce::AudioDeviceManager::AudioDeviceSetup& setup)
    {
        // W trakcie nagrania karta sie nie zmienia — plik mialby dwa zrodla.
        if (noDevice || recorder.isRecording()) { refreshInputBoxes(); return; }

        const auto err = deviceManager.setAudioDeviceSetup (setup, true);
        if (err.isNotEmpty())
            append (PL ("Nie mogę przełączyć karty: ") + err);

        refreshInputBoxes();
        saveAudioState();
    }

    /** "slysze wejscie 2" — zeby bylo widac, gdzie jest mikrofon, zanim
        czlowiek wybierze wejscie recznie. Prog ok. -50 dBFS: cos wiecej niz
        szum wejscia. */
    void updateHeard (float level)
    {
        juce::String s;

        if (auto* d = deviceManager.getCurrentAudioDevice())
            if (level > 0.003f)
            {
                const auto mask = (uint64_t) d->getActiveInputChannels().toInt64();
                const int n = zs::input::physicalInput (mask, recorder.analysisCh());
                if (n > 0) s = PL ("słyszę wejście ") + juce::String (n);
            }

        if (heardLabel.getText() != s) heardLabel.setText (s, juce::dontSendNotification);
    }

    void repeatLine()
    {
        if (prompter != nullptr && inPrompter && recorder.isRecording())
            prompter->repeatLine();
    }

    void toggleRecord()
    {
        if (recorder.isRecording())
        {
            const auto justRecorded = recorder.currentFile();

            recorder.stop();
            recButton.setButtonText ("Nagraj");
            zs::Look::setKey (recButton, {});
            append ("--- zatrzymane: " + justRecorded.getFileName());
            logic.reset();
            lamp.setState (0, "Zatrzymane.", true);

            // Ocena idzie SAMA, bez pytania.
            //
            // Czlowiek, ktory wlasnie skonczyl mowic, ma jedno pytanie: czy to
            // sie nadaje. Kazanie mu szukac przycisku, zeby je zadac, znaczy
            // tylko tyle, ze przez pierwsze tygodnie go nie znajdzie, a potem
            // zapomni, ze istnieje — i program zostanie dyktafonem z lampka.
            openReview (justRecorded);
            return;
        }

        preparation = false;
        const auto err = recorder.start (previewFolder != juce::File() ? previewFolder : sessionFolder);

        if (err.isNotEmpty())
        {
            append ("BLAD: " + err);
            lamp.setState (3, err);
            return;
        }

        // Po starcie okragly Nagraj ustepuje przyciskowi Zatrzymaj i ocen.
        // Nagranie ocenia lampka, dlatego przycisk nie zmienia sie na czerwony.
        settingsOpen=false;
        recButton.setButtonText (PL("Zatrzymaj i oceń"));
        zs::Look::setKey (recButton, "Esc");
        resized();repaint();
        if (! inPrompter && isShowing()) grabKeyboardFocus();
        peakHold = 0.0f;
        logic.reset();
        lastTime = -100.0;
        repeatCount = 0;
        mutedKinds.clear();
        takeEvents.clear();
        okButton.setEnabled (false);
        append ("--- nagrywam do " + recorder.currentFile().getFileName());
        lamp.setState (0, PL ("Uczę się Twojego głosu…"), true);
    }

    void chooseFolder()
    {
        chooser = std::make_unique<juce::FileChooser> (PL ("Gdzie zapisywać sesje?"), sessionFolder);

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectDirectories,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f != juce::File())
                {
                    sessionFolder = f;
                    settings.setSessionFolder (f);
                    append ("folder sesji: " + f.getFullPathName());
                }
            });
    }

    void append (const juce::String& line)
    {
        events.moveCaretToEnd();
        events.insertTextAtCaret (line + "\n");
    }

    /** Podmienia ostatnio dopisana linie — do scalania powtorek. */
    void rewriteLastLine (const juce::String& line)
    {
        const auto all = events.getText();

        if (lastLineStart < 0 || lastLineStart > all.length())
        {
            append (line);
            return;
        }

        events.setText (all.substring (0, lastLineStart) + line + "\n", false);
        events.moveCaretToEnd();
    }

    void timerCallback() override
    {
        lamp.setRecording(recorder.isRecording());
        // Kreator potrzebuje czestotliwosci probkowania, a ta jest znana dopiero
        // wtedy, gdy urzadzenie ruszy — stad start z opoznieniem, a nie w
        // konstruktorze.
        if (inWizard)
        {
            if (! wizardStarted && recorder.currentSampleRate() > 0.0)
            {
                wizard->begin (recorder.currentSampleRate());
                wizardStarted = true;
            }

            return;
        }

        // Ekran po nagraniu ma swoj wlasny zegar i nic z tego, co nizej, nie
        // jest wtedy widoczne. Odswiezanie ukrytej lampki to nie blad, ale
        // trzydziesci razy na sekunde przez cala ocene nagrania juz tak.
        if (inReview)
            return;

        const float lvNow = recorder.inputLevel();
        meter.setLevel (lvNow);
        if (preparation && heardLabel.isVisible()) updateHeard (lvNow);

        if (recorder.isRecording())
            repaint();        // zegar i nazwa nagrania we wspolnym szkielecie
        if (recorder.isRecording() && lvNow > peakHold) peakHold = lvNow;

        // ---- zdarzenia z rdzenia -------------------------------------------
        tw::Event e;

        while (recorder.nextEvent (e))
        {
            const int sev = (int) e.severity;

            // Zdarzenia sa zbierane, a nie tylko wypisywane.
            //
            // Dotad wpadaly na liste tekstowa i tam konczyly zycie. Ekran po
            // nagraniu potrzebuje ich wszystkich naraz, zeby powiedziec, KTORY
            // fragment trzeba powtorzyc — a liczenie ich drugi raz z pliku
            // dawaloby dwie odpowiedzi na to samo pytanie, ktore kiedys by sie
            // rozjechaly.
            //
            // Wyciszone przyciskiem "To bylo OK" tez zapisujemy: wyciszenie
            // dotyczy przerywania w trakcie mowienia, nie tego, czy zdarzenie
            // bylo. Przester wyciszony nadal niszczy probki.
            if (recorder.isRecording())
                takeEvents.push_back (e);

            // ---- scalanie -----------------------------------------------------
            // Minuta prawdziwego mowienia dala jedenascie wpisow, w tym cztery
            // razy to samo w ciagu dwoch sekund. Lista zalewajaca czlowieka
            // powtorzeniami przestaje byc czytana — a to jest ta sama choroba,
            // przez ktora ludzie wylaczaja takie narzedzia.
            const juce::String line = pl::hhmmss (e.timeSec) + "  " + pl::kind (e.kind)
                                    + "  " + pl::value (e.kind, e.value)
                                    + PL ("  \xe2\x80\x94 ") + pl::advice (e.kind);

            if (e.kind == lastKind && e.timeSec - lastTime < 4.0)
            {
                ++repeatCount;
                rewriteLastLine (pl::hhmmss (lastTime) + "  " + pl::kind (e.kind)
                                 + PL ("  \xc3\x97") + juce::String (repeatCount + 1)
                                 + "  " + pl::value (e.kind, e.value)
                                 + PL ("  \xe2\x80\x94 ") + pl::advice (e.kind));
            }
            else
            {
                repeatCount = 0;
                lastKind = e.kind;
                lastTime = e.timeSec;
                lastLineStart = events.getText().length();
                append (line);
            }

            const bool muted = mutedKinds.contains ((int) e.kind) && pl::canMute (e.kind);

            if (sev >= 1 && ! muted)
            {
                logic.event (e.kind, sev, pl::adviceRaw (e.kind));
                lastFlagged = e.kind;
                okButton.setEnabled (pl::canMute (e.kind));
            }
        }

        // ---- lampka ----------------------------------------------------------
        // Cala decyzja siedzi w LampLogic — tym samym kodzie, ktory przechodzi
        // przez wygenerowane sygnaly w Tests/lamp-test.cpp. Trzy poprzednie
        // bledy lampki wyszly dopiero na zywym mikrofonie; teraz wychodza przy
        // budowaniu.
        const auto v = logic.tick (1.0 / 30.0, recorder.isRecording(),
                                   recorder.recordedSeconds(), lvNow,
                                   recorder.profileReady(), recorder.profileProgress());

        if (juce::String (v.msg) == "learning")
            lamp.setState (0, PL ("Ucz\xc4\x99 si\xc4\x99 Twojego g\xc5\x82osu: ")
                              + juce::String (logic.learnPercent())
                              + PL ("%  \xe2\x80\x94 m\xc3\xb3w normalnie"), false);
        else
            lamp.setState (v.state, PL (v.msg), v.dim);



        if(recorder.isRecording() && recorder.recordingDamaged()) {
            lamp.setState(3,PL("Utracono dane. Zatrzymaj nagrywanie i sprawdź dysk."),false);

        }
        // ---- pasek stanu ----------------------------------------------------
        juce::String s;

        if (recorder.isRecording())
        {
            if (peakHold <= 1.0e-6f) s << PL ("Czekam na sygnał mikrofonu.");

            // szczyt z podtrzymaniem: wskaznik opada, zanim czlowiek zdazy na
            // niego spojrzec, wiec ustawienie wzmocnienia bylo zgadywaniem
            if (peakHold > 1.0e-6f)
            {
                const float ph = juce::Decibels::gainToDecibels (peakHold);
                s << PL ("Szczyt: ") << juce::String (ph, 1) << " dBFS";

                // Szczyt 0,0 dBFS znaczy, ze probki dotknely sufitu i czesc
                // przebiegu zostala scieta. Pasek stanu ma o tym mowic do konca
                // sesji, a nie tylko przez trzy sekundy zapalonej lampki —
                // inaczej czlowiek dowiaduje sie o tym dopiero przy miksie.
                if (ph > -0.2f)
                    s << PL ("  (przester — nagranie ścięte)");
            }

            if (! recorder.profileReady())
                s << PL ("\nPoznaję Twój głos: ") << (int) (recorder.profileProgress() * 100.0f) << "%";
        }
        else
        {
            s << PL ("Sprawdź mikrofon, zanim zaczniesz.");
        }

        status.setText (s, juce::dontSendNotification);
    }

    juce::AudioDeviceManager deviceManager;
    Recorder recorder;

    /* Rozpoznawanie mowy dla promptera. Model wczytuje sie w tle od startu
       programu (1-2 s), zeby nie czekac na niego przy pierwszym nagraniu.
       Stoi przed prompterem, bo prompter trzyma do niego wskaznik i musi
       zniknac pierwszy. */
    VoskEngine speech;
    Settings settings;

    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
    std::unique_ptr<juce::FileChooser> chooser;

    Lamp lamp;
    Meter meter;
    juce::TextButton recButton, repeatButton, folderButton, okButton, settingsButton, detailsButton, openButton;
    juce::Label hints;
    juce::TextEditor nameInput, scriptInput;
    juce::ComboBox deviceBox, inputBox;   ///< karta i wejscie z mikrofonem
    bool noDevice = false;                ///< --preview-ui: karty nie otwieramy
    juce::Label heardLabel;               ///< "slysze wejscie 2"
    juce::TextButton prepareButton, preparationButton;
    bool preparation = true;
    zs::StepBar flow;
    juce::File previewFolder;   ///< tylko --preview-ui: nagrywanie do katalogu tymczasowego
    bool settingsOpen=false, detailsOpen=false;
    juce::SortedSet<int> mutedKinds;
    tw::Kind lastFlagged = tw::Kind::Clipping;
    juce::TextEditor events;
    juce::Label status;

    /** Przelacza miedzy trybem kreatora a trybem nagrywania. Program wyglada
        w nich inaczej, bo czlowiek ma w nich zupelnie inna zdolnosc czytania. */
    void showRecordingUi (bool on)
    {
        inWizard = ! on;

        lamp.setVisible (on);
        meter.setVisible (on);
        recButton.setVisible (on);
        folderButton.setVisible (on);
        okButton.setVisible (on);
        events.setVisible (on);
        status.setVisible (on);
        prompterButton.setVisible (on);
        checkButton.setVisible (on);
        otherTextButton.setVisible (on && settings.lastScript().existsAsFile());

        if (selector != nullptr)
            selector->setVisible (on);

        if (wizard != nullptr)
            wizard->setVisible (! on);

        refreshHomeVisibility();
        resized();
        repaint();
    }

    void enterWizard()
    {
        if (inPrompter)
            showPrompter (false);

        wizard = std::make_unique<WizardComponent> (recorder);
        wizard->onFinished = [this] { leaveWizard(); };
        addAndMakeVisible (*wizard);

        wizardStarted = false;
        showRecordingUi (false);
    }

    void leaveWizard()
    {
        // Pominiecie nie liczy sie jako sprawdzenie — patrz WizardComponent::completed().
        if (wizard != nullptr && wizard->completed())
        {
            settings.setHardwareChecked (true, wizard->summaryLine());
            append (PL ("sprzęt sprawdzony: ") + wizard->summaryLine() + ".");
        }
        else
        {
            append (PL ("--- sprawdzanie pominięte. Program nagrywa tak czy inaczej, "
                        "ale nie zna tła pokoju ani Twoich szczytów, więc jego oceny "
                        "są zgadywaniem. Kliknij „Sprawdź sprzęt”, kiedy będzie czas."));
        }

        showRecordingUi (true);
        lamp.setState (0, PL ("Naciśnij Nagraj, kiedy będziesz gotowy."), true);

        // Kreator sluchal mikrofonu przez caly swoj czas. Zostawiony przy zyciu
        // slucha go dalej — takze w trakcie nagrywania, ktorego juz nie dotyczy.
        // Usuwamy go, ale nie z wnetrza jego wlasnego przycisku, bo to wlasnie
        // ten przycisk nas tu wywolal.
        juce::Component::SafePointer<MainComponent> safe (this);

        juce::MessageManager::callAsync ([safe]() mutable
        {
            if (safe == nullptr) return;     // okno zamknieto, zanim to doszlo

            safe->wizard = nullptr;
            safe->wizardStarted = false;
            safe->resized();
        });
    }

    juce::File sessionFolder;
    std::unique_ptr<WizardComponent> wizard;
    std::unique_ptr<PrompterComponent> prompter;
    juce::TextButton prompterButton, otherTextButton, checkButton;
    bool inPrompter = false;

    /** Wczytuje tekst i przelacza ekran na prompter.

        Prompter nie jest osobnym trybem nagrywania — nagrywa dokladnie to samo
        i tak samo. Jest innym widokiem TEGO SAMEGO nagrania, bo czlowiek, ktory
        czyta, potrzebuje na ekranie czego innego niz ten, ktory mowi z glowy. */
    void openScript()
    {
        chooser = std::make_unique<juce::FileChooser> (
            PL ("Wybierz plik z tekstem"), settings.lastScript(), "*.txt");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (! f.existsAsFile()) return;

                loadScript (f);
            });
    }

    /** Otwiera ostatni tekst jednym klikniecem.

        Szukanie tego samego pliku w okienku przed kazdym nagraniem to kilka
        sekund i trzy decyzje w chwili, w ktorej czlowiek ma w glowie tekst, a
        nie drzewo katalogow. Przycisk pokazuje nazwe, wiec nie trzeba pamietac,
        co sie otworzy; "inny…" stoi obok dla kazdego innego razu. */
    void openLastOrChoose()
    {
        const auto f = settings.lastScript();

        if (f.existsAsFile())
        {
            loadScript (f);
            return;
        }

        openScript();
    }

    void loadScript (const juce::File& f)
    {
        ensurePrompter();
        scriptInput.setText (f.loadFileAsString());
        prompter->load (scriptInput.getText());
        settings.setLastScript (f);
        updatePrompterButton();
        if (!preparation) showPrompter (true);
        else { resized(); repaint(); }
    }

    void ensurePrompter()
    {
        if (prompter == nullptr)
        {
            prompter = std::make_unique<PrompterComponent> (recorder, settings);
            prompter->onClose = [this] { showPrompter (false); };
            prompter->setSpeech (&speech);
            prompter->onStopRequest = [this] { if (recorder.isRecording()) toggleRecord(); };
            addChildComponent (*prompter);
        }
    }

    void updatePrompterButton()
    {
        const auto f = settings.lastScript();

        prompterButton.setButtonText (f.existsAsFile() ? PL ("Czytaj: ") + f.getFileName()
                                                       : PL ("Otwórz tekst do czytania"));
    }

    void showPrompter (bool on)
    {
        inPrompter = on;

        if (prompter != nullptr)
        {
            prompter->setVisible (on);

            if (on) prompter->begin (recorder.currentSampleRate());
            else    prompter->end();
        }

        lamp.setVisible (! on);
        meter.setVisible (! on);
        folderButton.setVisible (! on);
        okButton.setVisible (! on);
        events.setVisible (! on);
        status.setVisible (! on);
        prompterButton.setVisible (! on);
        checkButton.setVisible (! on);
        otherTextButton.setVisible (! on && settings.lastScript().existsAsFile());

        if (selector != nullptr) selector->setVisible (! on);

        refreshHomeVisibility();
        resized();
        repaint();
    }
    //==========================================================================
    /** Ekran po nagraniu. Powstaje przy pierwszym nagraniu i juz zostaje —
        wczytanie nowego materialu jest tansze niz budowanie okna od nowa. */
    void openReview (const juce::File& wavFile, bool waitForIt = false, bool fromDisk = false)
    {
        if (! wavFile.existsAsFile())
        {
            append (PL ("--- nie ma czego ocenić: plik nie powstał"));
            return;
        }

        if (review == nullptr)
        {
            review = std::make_unique<ReviewComponent> (recorder);
            review->onClose = [this] { closeReview(); };
            addAndMakeVisible (*review);
        }

        preparation = false;
        reviewCameFromPrompter = inPrompter;

        if (inPrompter)
            showPrompter (false);

        review->setRecordingDamaged((!fromDisk && recorder.recordingDamaged()) || wavFile.getSiblingFile(wavFile.getFileName()+".problem.txt").existsAsFile());
        review->load (wavFile, takeEvents, ! fromDisk, waitForIt);
        showReview (true);
    }

    void closeReview()
    {
        showReview (false);

        // Wracamy tam, skad przyszlismy. Czlowiek, ktory czytal z promptera,
        // chce czytac dalej od tego samego miejsca — a nie szukac przycisku
        // promptera po raz drugi.
        if (reviewCameFromPrompter && prompter != nullptr)
            showPrompter (true);
        else
            lamp.setState (0, PL ("Naciśnij Nagraj, kiedy będziesz gotowy."), true);
    }

    void showReview (bool on)
    {
        inReview = on;

        if (review != nullptr)
        {
            review->setVisible (on);

            if (on) review->begin();
            else    review->end();
        }

        if (on)
        {
            lamp.setVisible (false);
            meter.setVisible (false);
            recButton.setVisible (false);
            folderButton.setVisible (false);
            okButton.setVisible (false);
            events.setVisible (false);
            status.setVisible (false);
            prompterButton.setVisible (false);
            otherTextButton.setVisible (false);
            checkButton.setVisible (false);

            if (selector != nullptr) selector->setVisible (false);
        }
        else if (! reviewCameFromPrompter)
        {
            showRecordingUi (true);
        }

        refreshHomeVisibility();
        resized();
        repaint();
    }

    bool inWizard = true, wizardStarted = false;
    bool inReview = false, reviewCameFromPrompter = false;
    std::unique_ptr<ReviewComponent> review;
    std::vector<tw::Event> takeEvents;
    zs::LampLogic logic;
    float  peakHold  = 0.0f;
    tw::Kind lastKind = tw::Kind::Clipping;
    double lastTime  = -100.0;
    int    repeatCount = 0;
    int    lastLineStart = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
