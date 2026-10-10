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
            case 3:  c = juce::Colour (0xffa8402e); label = "STOP";     break;
            case 2:  c = juce::Colour (0xffb57b16); label = "POPRAW";   break;
            case 1:  c = juce::Colour (0xffb57b16); label = PL ("POWTÓRZ");  break;
            default: c = juce::Colour (0xff2e7d52); label = PL ("MÓW");      break;
        }

        if(!recording && state==0) { label=PL("GOTOWY"); c=zs::Ink::textDim(); }

        if (dim && state == 0)
            c = c.withMultipliedSaturation (0.45f).withMultipliedBrightness (1.35f);

        g.setColour (c.withAlpha (dim && state == 0 ? 0.10f : 0.18f));
        g.fillEllipse (circle.expanded (10).toFloat());
        g.setColour (c);
        g.fillEllipse (circle.toFloat());

        g.setColour (!recording && state==0 ? zs::Ink::bg() : juce::Colours::white.withAlpha(0.92f));
        g.setFont (juce::Font (juce::jmax (14.0f, d * (recording?0.22f:0.16f)), juce::Font::bold));
        g.drawText (label, circle, juce::Justification::centred);

        g.setColour (zs::Ink::textNext());
        g.setFont (juce::Font (15.0f));
        g.drawFittedText (message,
                          r.removeFromBottom (44).reduced (4, 0),
                          juce::Justification::centredTop, 2);
    }

private:
    bool recording=false;
    int state = 0;
    bool dim = false;
    juce::String message = PL ("Naciśnij nagrywanie.");
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
        g.setColour (db > -1.5f ? juce::Colour (0xffa8402e)
                   : db > -8.0f ? juce::Colour (0xffb57b16)
                                : juce::Colour (0xff2e7d52));
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
                      private juce::Timer
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
        if (!previewMode) {
            deviceManager.initialiseWithDefaultDevices (2, 2);
            deviceManager.addAudioCallback (&recorder);
        }

        selector = std::make_unique<juce::AudioDeviceSelectorComponent> (
                       deviceManager, 1, 2, 0, 2, false, false, true, false);
        addAndMakeVisible (*selector);

        addAndMakeVisible (lamp);

        prompterButton.onClick = [this] { openLastOrChoose(); };
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
        recButton.setColour(juce::TextButton::buttonColourId,zs::Ink::green());
        recButton.setColour(juce::TextButton::textColourOffId,zs::Ink::bg());
        updatePrompterButton();
        addAndMakeVisible (meter);

        recButton.setButtonText ("Nagrywaj");
        recButton.onClick = [this] { toggleRecord(); };
        addAndMakeVisible (recButton);

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
            lamp.setState (0, PL ("Naciśnij nagrywanie."), true);

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

        setSize (1060, 720);
        startTimerHz (30);
    }

    ~MainComponent() override
    {
        deviceManager.removeAudioCallback (&recorder);
    }

    /** Dla okna, ktore chce zapamietac swoje polozenie. */
    Settings& userSettings() noexcept { return settings; }
    void previewSettings() { settingsOpen=true;resized();repaint(); }
    void previewPrompter() {
        recorder.prepareDevice(48000,512,1);
        prompter=std::make_unique<PrompterComponent>(recorder,settings);
        prompter->onClose=[this]{showPrompter(false);};addAndMakeVisible(*prompter);
        prompter->load(PL("Dziś porozmawiamy o tym, co naprawdę warto usłyszeć.\nZatrzymaj się na chwilę. Weź spokojny oddech.\nTo jest Twoje miejsce i Twój głos.\nNie musisz się spieszyć. Dobra opowieść potrzebuje czasu."));
        showPrompter(true);
    }



    void paint (juce::Graphics& g) override
    {
        using I = zs::Ink;
        g.fillAll(I::bg());
        if(inWizard || inReview || inPrompter) return;
        g.setColour(I::textNow()); g.setFont(juce::Font(25.0f,juce::Font::bold));
        g.drawText(PL("Zielone światło"),28,20,360,34,juce::Justification::centredLeft);
        g.setColour(I::textNext());g.setFont(juce::Font(14.0f));
        g.drawText(PL("Twoje miejsce na dobry podcast."),28,57,400,24,juce::Justification::centredLeft);
        auto card=homeCard();
        g.setColour(I::panel());g.fillRoundedRectangle(card.toFloat(),16.0f);
        auto text=card.reduced(26);text.removeFromLeft(154);
        g.setColour(I::textNow());g.setFont(juce::Font(27.0f,juce::Font::bold));
        g.drawFittedText(recorder.isRecording()?PL("Nagrywasz. Mów spokojnie."):PL("Co dziś nagrywasz?"),
                         text.removeFromTop(50),juce::Justification::centredLeft,2);
        g.setColour(I::textNext());g.setFont(juce::Font(15.0f));
        g.drawFittedText(PL("Mów z głowy lub otwórz tekst w prompterze.\nPo zatrzymaniu odsłuchasz nagranie i zapiszesz miks."),
                         text.removeFromTop(66),juce::Justification::topLeft,3);
        g.setColour(I::textNext());g.setFont(juce::Font(13.0f));
        g.drawText(PL("1  Przygotuj     /     2  Nagraj     /     3  Odsłuchaj     /     4  Zapisz"),
                   28,94,getWidth()-56,25,juce::Justification::centredLeft);
        if(settingsOpen) {
            auto box=juce::Rectangle<int>(getWidth()-344,136,316,getHeight()-164);
            g.setColour(I::panel());g.fillRoundedRectangle(box.toFloat(),12.0f);
            g.setColour(I::textNow());g.setFont(juce::Font(18.0f,juce::Font::bold));
            g.drawText(PL("Mikrofon i sprzęt"),box.reduced(18).removeFromTop(28),juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        if(wizard) wizard->setBounds(getLocalBounds());
        if(inWizard) return;
        if(inReview && review) { review->setBounds(getLocalBounds());return; }
        if(inPrompter && prompter) {
            prompter->setBounds(getLocalBounds());
            recButton.setBounds(getWidth()-224,getHeight()-46,208,36);
            recButton.toFront(false);return;
        }
        settingsButton.setBounds(getWidth()-202,26,174,38);
        auto card=homeCard();
        lamp.setBounds(card.getX()+22,card.getY()+20,132,152);
        auto row=card.reduced(26);row.removeFromTop(156);
        meter.setBounds(row.removeFromTop(22));row.removeFromTop(20);
        recButton.setBounds(row.removeFromTop(54));row.removeFromTop(12);
        auto textRow=row.removeFromTop(40);
        if(settings.lastScript().existsAsFile()) {
            otherTextButton.setBounds(textRow.removeFromRight(108));textRow.removeFromRight(8);
        }
        prompterButton.setBounds(textRow);
        row.removeFromTop(12);status.setBounds(row.removeFromTop(24));
        auto foot=juce::Rectangle<int>(card.getX(),card.getBottom()+14,card.getWidth(),36);
        openButton.setBounds(foot.removeFromLeft(170));foot.removeFromLeft(10);
        detailsButton.setBounds(foot.removeFromLeft(142));
        if(detailsOpen) {
            auto area=juce::Rectangle<int>(card.getX(),foot.getBottom()+14,card.getWidth(),juce::jmax(40,getHeight()-foot.getBottom()-42));
            okButton.setBounds(area.removeFromBottom(34).removeFromRight(206));
            area.removeFromBottom(8);events.setBounds(area);
        }
        if(settingsOpen) {
            auto panel=juce::Rectangle<int>(getWidth()-326,190,280,getHeight()-218);
            folderButton.setBounds(panel.removeFromBottom(38));panel.removeFromBottom(10);
            checkButton.setBounds(panel.removeFromBottom(38));panel.removeFromBottom(12);
            selector->setBounds(panel);
        }
        refreshHomeVisibility();
    }

private:
    juce::Rectangle<int> homeCard() const {
        return {28,136,getWidth()-(settingsOpen?400:56),366};
    }
    void refreshHomeVisibility() {
        const bool home=!inWizard && !inReview && !inPrompter;
        settingsButton.setVisible(home);detailsButton.setVisible(home);openButton.setVisible(home);
        settingsButton.setButtonText(settingsOpen?PL("Zamknij ustawienia"):PL("Mikrofon i sprzęt"));
        detailsButton.setButtonText(detailsOpen?PL("Ukryj uwagi"):PL("Pokaż uwagi"));
        selector->setVisible(home && settingsOpen);
        folderButton.setVisible(home && settingsOpen);checkButton.setVisible(home && settingsOpen);
        events.setVisible(home && detailsOpen);okButton.setVisible(home && detailsOpen);
        const bool idle=!recorder.isRecording();
        settingsButton.setEnabled(idle);openButton.setEnabled(idle);folderButton.setEnabled(idle);checkButton.setEnabled(idle);
    }

    void toggleRecord()
    {
        if (recorder.isRecording())
        {
            const auto justRecorded = recorder.currentFile();

            recorder.stop();
            recButton.setButtonText ("Nagrywaj");
            recButton.setColour(juce::TextButton::buttonColourId,zs::Ink::green());
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

        const auto err = recorder.start (sessionFolder);

        if (err.isNotEmpty())
        {
            append ("BLAD: " + err);
            lamp.setState (3, err);
            return;
        }

        settingsOpen=false;
        recButton.setButtonText (PL("Zatrzymaj i odsłuchaj"));
        recButton.setColour(juce::TextButton::buttonColourId,zs::Ink::red());
        resized();repaint();
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

            if (prompter != nullptr)
                prompter->setLamp (v.state, PL (v.msg), v.dim);

        if(recorder.isRecording() && recorder.recordingDamaged()) {
            lamp.setState(3,PL("Utracono dane. Zatrzymaj nagrywanie i sprawdź dysk."),false);
            if(prompter)prompter->setLamp(3,PL("Błąd zapisu nagrania"),false);
        }
        // ---- pasek stanu ----------------------------------------------------
        juce::String s;

        if (recorder.isRecording())
        {
            s << "nagrywam " << pl::hhmmss (recorder.recordedSeconds());

            // szczyt z podtrzymaniem: wskaznik opada, zanim czlowiek zdazy na
            // niego spojrzec, wiec ustawienie wzmocnienia bylo zgadywaniem
            if (peakHold > 1.0e-6f)
            {
                const float ph = juce::Decibels::gainToDecibels (peakHold);
                s << PL ("   ·   szczyt ") << juce::String (ph, 1) << " dBFS";

                // Szczyt 0,0 dBFS znaczy, ze probki dotknely sufitu i czesc
                // przebiegu zostala scieta. Pasek stanu ma o tym mowic do konca
                // sesji, a nie tylko przez trzy sekundy zapalonej lampki —
                // inaczej czlowiek dowiaduje sie o tym dopiero przy miksie.
                if (ph > -0.2f)
                    s << PL ("  (przester — nagranie ścięte)");
            }

            if (! recorder.profileReady())
                s << PL ("   ·   profil głosu ") << (int) (recorder.profileProgress() * 100.0f) << "%";
            else
                s << PL ("   ·   profil gotowy");

            s << PL ("   ·   fraz: ") << recorder.phrases()
              << PL ("   ·   kanał ") << (recorder.analysisCh() + 1);
        }
        else
        {
            s << "gotowy";
        }

        if (recorder.currentSampleRate() > 0.0)
            s << PL ("   ·   ") << juce::String (recorder.currentSampleRate() / 1000.0, 1) << " kHz";

        status.setText (s, juce::dontSendNotification);
    }

    juce::AudioDeviceManager deviceManager;
    Recorder recorder;
    Settings settings;

    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
    std::unique_ptr<juce::FileChooser> chooser;

    Lamp lamp;
    Meter meter;
    juce::TextButton recButton, folderButton, okButton, settingsButton, detailsButton, openButton;
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
        lamp.setState (0, PL ("Naciśnij nagrywanie."), true);

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
        if (prompter == nullptr)
        {
            prompter = std::make_unique<PrompterComponent> (recorder, settings);
            prompter->onClose = [this] { showPrompter (false); };
            addAndMakeVisible (*prompter);
        }

        prompter->load (f.loadFileAsString());

        settings.setLastScript (f);
        updatePrompterButton();

        showPrompter (true);
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
            lamp.setState (0, PL ("Naciśnij nagrywanie."), true);
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
