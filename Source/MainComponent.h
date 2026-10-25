/*
    MainComponent.h — to, co widac w trakcie mowienia.

    Regula projektowa: w trakcie nagrywania czlowiek patrzy na JEDNA rzecz —
    lampke. Reszta ma byc dostepna, ale nie ma sie domagac uwagi. Dlatego
    lampka jest wielka i kolorowa, a lista zdarzen szara i cicha.
*/

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "Recorder.h"
#include "WavRepair.h"
#include "LampLogic.h"
#include "PrompterComponent.h"
#include "WizardComponent.h"
#include "ReviewComponent.h"
#include "StartComponent.h"
#include "Backing.h"
#include "Trace.h"
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
    /** Sygnal jeszcze nieoceniony: szara lampka "SLUCHAM", nie zielone "MOW"
        (audyt 14.10: zielony ma znaczyc "dobrze", a nie "czekam"). */
    void setWaiting(bool value) { if (waiting != value) { waiting = value; repaint(); } }
    void setCompact (bool value) { if (compact != value) { compact = value; repaint(); } }
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
        // specyfikacja E: lampka do 132 px, poswiata tylko, gdy cos mowi
        const int d = compact ? juce::jmax (0, juce::jmin (56, r.getWidth() - 24, r.getHeight() - 10))
                              : juce::jmax (0, juce::jmin (132, r.getWidth() - 40, r.getHeight() - 70));
        // lampka i zdanie pod nia razem, na srodku miejsca (zdanie nie odjezdza na dol)
        const int block = compact ? d : d + 10 + 18 + 48;
        const int top = r.getY() + juce::jmax (0, (r.getHeight() - block) / 2);
        auto circle = juce::Rectangle<int> (d, d).withCentre ({ r.getCentreX(), top + 5 + d / 2 });

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
        const bool listening = recording && state==0 && waiting;
        if(listening) { label=PL("SŁUCHAM"); c=zs::Ink::textDim(); }

        if (dim && state == 0)
            c = c.withMultipliedBrightness (0.72f);

        const bool idle = (! recording && state == 0) || listening;
        const auto cf = circle.toFloat();
        if (! idle) zs::Depth::glow (g, cf, cf.getWidth() / 2, c, 12.0f, dim && state == 0 ? 0.12f : 0.24f);
        // pierscien: ciemna szczelina i obwodka w kolorze stanu (makieta "Wtyczka")
        g.setColour (zs::Ink::bg());
        g.fillEllipse (cf.expanded (5.0f));
        g.setColour (idle ? zs::Ink::edge() : c.withAlpha (0.85f));
        g.drawEllipse (cf.expanded (4.0f), 2.0f);
        // dysk: swiatlo z gory po lewej
        const auto lit = idle ? juce::Colour (0xff2a3542) : c;
        g.setGradientFill (juce::ColourGradient (lit.brighter (0.45f), cf.getX() + cf.getWidth() * 0.38f, cf.getY() + cf.getHeight() * 0.3f,
                                                 lit.darker (0.25f), cf.getRight(), cf.getBottom(), true));
        g.fillEllipse (cf);

        g.setColour (idle ? zs::Ink::textNext() : juce::Colour (0xff06140c));
        g.setFont (juce::Font (compact ? 10.5f : juce::jmax (14.0f, d * (recording && ! listening ? 0.22f : 0.15f)), juce::Font::bold));
        g.drawText (label, circle, juce::Justification::centred);

        g.setColour (zs::Ink::textNext());
        g.setFont (juce::Font (zs::type::body));
        if (! compact)
            g.drawFittedText (message,
                              juce::Rectangle<int> (r.getX(), circle.getBottom() + 18, r.getWidth(), 48).reduced (4, 0),
                              juce::Justification::centredTop, 2);
    }

private:
    bool recording=false;
    bool waiting=false;
    bool compact=false;
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
        // aneks specyfikacji E, pkt 3: tlo #263242, sygnal #32B9FF, znacznik
        // progu pomaranczowy, przester czerwony powyzej progu; bez poswiaty
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xff263242));
        g.fillRoundedRectangle (r, 3.0f);

        const float db = lvl > 1.0e-6f ? juce::Decibels::gainToDecibels (lvl) : -60.0f;
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);

        // znacznik zalecanego szczytu: -12 dBFS
        const float mark = r.getX() + r.getWidth() * ((-12.0f + 60.0f) / 60.0f);
        auto bar = r.withWidth (r.getWidth() * norm);
        g.setColour (zs::Ink::accent());
        g.fillRoundedRectangle (bar.withRight (juce::jmin (bar.getRight(), mark)), 3.0f);
        if (bar.getRight() > mark)
        {
            g.setColour (db > -1.5f ? zs::Ink::clip() : zs::Ink::yellow());
            g.fillRoundedRectangle (bar.withLeft (mark), 3.0f);
        }
        g.setColour (zs::Ink::yellow());
        g.fillRect (mark, r.getY(), 1.0f, r.getHeight());

        g.setColour (zs::Ink::textNext());
        g.setFont (juce::Font (zs::type::tiny));
        g.drawText (db <= -59.0f ? juce::String ("cisza")
                                 : juce::String (db, 1) + " dBFS",
                    getLocalBounds().reduced (5, 0), juce::Justification::centredRight);
    }

private:
    float lvl = 0.0f;
};

//==============================================================================
//==============================================================================
/** Miernik segmentowy (specyfikacja E): segmenty od -60 do 0 dBFS, niebieskie
    nisko, zielone w zakresie mowy, pomaranczowe przy progu, czerwone przy
    przesterze; linia szczytu trzyma 0,9 s i opada 12 dB/s; skala i liczba. */
class SegMeter : public juce::Component
{
public:
    static constexpr int height = 50;

    void setLevel (float lin)
    {
        const float db = lin > 1.0e-6f ? juce::Decibels::gainToDecibels (lin) : -90.0f;
        const auto now = juce::Time::getMillisecondCounter();
        const float dt = lastMs == 0 ? 0.0f : (float) (now - lastMs) / 1000.0f;
        lastMs = now;
        levelDb = db;
        if (db >= peakDb) { peakDb = db; holdUntil = now + 900; }
        else if (now > holdUntil) peakDb = juce::jmax (db, peakDb - 12.0f * dt);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto bar = r.removeFromTop (16.0f);
        const int n = 30;
        const float gap = 3.0f, w = (bar.getWidth() - gap * (n - 1)) / n;
        const auto xOf = [&] (float db) { return bar.getX() + bar.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); };
        for (int i = 0; i < n; ++i)
        {
            const float db = -60.0f + 60.0f * (i + 0.5f) / n;
            const juce::Colour c (db > -1.5f ? 0xffe64a55 : db > -6.0f ? 0xfff0a33e : db > -24.0f ? 0xff39cf7b : 0xff30b5fc);
            const bool lit = db <= levelDb;
            g.setColour (lit ? c : juce::Colour (0xff303a46).withAlpha (0.58f));
            g.fillRoundedRectangle (bar.getX() + i * (w + gap), bar.getY(), w, bar.getHeight(), 2.0f);
        }
        if (peakDb > -60.0f)
        {
            g.setColour (zs::Ink::textNow());
            g.fillRect (xOf (peakDb) - 0.5f, bar.getY() - 2.0f, 1.5f, bar.getHeight() + 4.0f);
        }
        r.removeFromTop (3.0f);
        auto scale = r.removeFromTop (14.0f);
        g.setColour (juce::Colour (0xff8996a5));
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        for (float db : { -48.0f, -24.0f, -12.0f, -6.0f, 0.0f })
        {
            const float x = xOf (db);
            const juce::String t = juce::String ((int) db);
            const float tw = 30.0f;
            g.drawText (t, juce::Rectangle<float> (juce::jlimit (scale.getX(), scale.getRight() - tw, x - tw / 2), scale.getY(), tw, scale.getHeight()),
                        db >= 0.0f ? juce::Justification::centredRight : juce::Justification::centred);
        }
        g.setColour (zs::Ink::textNext());
        g.setFont (juce::Font (zs::type::small));
        g.drawText (peakDb > -60.0f ? PL ("szczyt ") + juce::String (peakDb, 1) + " dBFS" : PL ("cisza"),
                    r.toNearestInt(), juce::Justification::centredLeft);
    }

private:
    float levelDb = -90.0f, peakDb = -90.0f;
    juce::uint32 holdUntil = 0, lastMs = 0;
};

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
        selector->setItemHeight (34);                  // jak pola i przyciski programu (JUCE: 24)
        addAndMakeVisible (*selector);

        addAndMakeVisible (lamp);

        prompterButton.onClick = [this] { if (preparation) openScript(); else openLastOrChoose(); };
        addAndMakeVisible (prompterButton);

        otherTextButton.setButtonText (PL ("Zmień tekst"));
        otherTextButton.onClick = [this] { openScript(); };
        addAndMakeVisible (otherTextButton);

        musicButton.setButtonText (PL ("Wybierz podkład"));
        musicButton.onClick = [this] { chooseBacking(); };
        addChildComponent (musicButton);
        editScriptButton.setButtonText (PL ("Wpisz lub wklej tekst"));
        editScriptButton.onClick = [this] {
            scriptEditorOpen = ! scriptEditorOpen;
            editScriptButton.setButtonText (scriptEditorOpen ? PL ("Ukryj edytor") : PL ("Wpisz lub wklej tekst"));
            refreshHomeVisibility(); resized(); repaint();
        };
        addChildComponent (editScriptButton);

        forgetProfileButton.setButtonText (PL ("Usuń wzorzec"));
        forgetProfileButton.setTooltip (PL ("Kolejne miksy bez dociągania barwy do wzorca"));
        forgetProfileButton.onClick = [this] { settings.setVoiceProfile ({}); forgetProfileButton.setVisible (false); repaint(); };
        addChildComponent (forgetProfileButton);
        checkButton.setButtonText (PL ("Sprawdź sprzęt"));
        checkButton.onClick = [this] { enterWizard(); };
        addAndMakeVisible (checkButton);

        settingsButton.setButtonText (PL("Ustawienia"));
        settingsButton.onClick = [this] {
            if (settingsOpen)
            {
                settingsOpen = false;
                const bool returnToReview = settingsReturnToReview;
                const bool returnToPrompter = settingsReturnToPrompter;
                settingsReturnToReview = settingsReturnToPrompter = false;
                refreshHomeVisibility(); resized(); repaint();
                if (returnToReview) showReview (true);
                else if (returnToPrompter) showPrompter (true);
                return;
            }
            if (inPrompter)
            {
                settingsReturnToPrompter = true;
                showPrompter (false);
            }
            settingsOpen = true; refreshHomeVisibility(); resized(); repaint();
        };
        addAndMakeVisible(settingsButton);
        detailsButton.setButtonText (PL("Pokaż uwagi"));
        detailsButton.onClick = [this] {
            detailsOpen = !detailsOpen; refreshHomeVisibility(); resized(); repaint();
        };
        addAndMakeVisible(detailsButton);
        // Otwieranie nagran mieszka teraz na ekranie startowym (lista ostatnich).
        openButton.setButtonText (PL("Ekran startowy"));
        openButton.onClick = [this] { showStart (true); };
        addAndMakeVisible(openButton);
        zs::Look::setKind (recButton, zs::ui::ButtonKind::Record);
        updatePrompterButton();
        addAndMakeVisible (meter);
        addChildComponent (segMeter);

        recButton.setButtonText ("Nagraj");
        recButton.onClick = [this] { toggleRecord(); };
        // Bez fokusu: inaczej po kliknieciu spacja nie trafia do promptera
        // (pauza), tylko tu — i konczy nagranie (przeglad 14.10, nr 7).
        recButton.setWantsKeyboardFocus (false);
        addAndMakeVisible (recButton);

        // "Powtorz wers" istnieje tylko w trakcie nagrywania z tekstem — bez
        // tekstu nie ma wersu, do ktorego mozna by wrocic.
        repeatButton.setButtonText (PL ("Powtórz wers"));
        zs::Look::setKey (repeatButton, "R");
        repeatButton.onClick = [this] { repeatLine(); };
        repeatButton.setWantsKeyboardFocus (false);
        addChildComponent (repeatButton);

        hints.setColour (juce::Label::textColourId, zs::Ink::textNext());
        hints.setFont (juce::Font (zs::type::small));
        hints.setJustificationType (juce::Justification::centredLeft);
        hints.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (hints);

        flow.setSteps ({ PL ("Ekran startowy"), PL ("Przygotowanie"), PL ("Nagranie"),
                         PL ("Odsłuch i montaż"), PL ("Miks"), PL ("Gotowe") });
        flow.setTileMode();
        flow.canClickStep = [this] (int step)
        { return (step == 0 || step == 1) && ! recorder.isRecording() && ! inWizard && ! inReview; };
        flow.onStepClick = [this] (int step)
        {
            if (step == 0) showStart (true);
            else if (step == 1 && preparationButton.isVisible()) preparationButton.onClick();
        };
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

        nameInput.setFont (juce::Font (zs::type::body));   // przed setText: font dotyczy tylko nowego tekstu
        nameInput.setText (PL("Nowy odcinek"));
        nameInput.setIndents (10, 10);                 // tekst na srodku pola 40 px, nie przy gornej krawedzi
        nameInput.onTextChange = [this] { repaint(); };
        scriptInput.setMultiLine (true);
        scriptInput.setFont (juce::Font (zs::type::body));
        scriptInput.setReturnKeyStartsNewLine (true);
        scriptInput.setTextToShowWhenEmpty (PL("Tu możesz wpisać tekst lub otworzyć plik TXT."), zs::Ink::textDim());
        scriptInput.onTextChange = [this] { repaint(); };
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
            if (startWithMusic && ! backingFile.existsAsFile())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon, PL ("Dodaj podkład"),
                    PL ("Wybrano nagrywanie z podkładem. Dodaj plik muzyczny, aby przejść dalej."));
                return;
            }
            preparation = false; settingsOpen = false; detailsOpen = false;
            ensurePrompter();
            prompter->load (startWithPrompter ? scriptInput.getText().trim() : juce::String());
            showPrompter (true);
        };
        addAndMakeVisible (prepareButton);
        preparationButton.setButtonText (PL("← Przygotowanie"));
        preparationButton.onClick = [this] {
            preparation = true; settingsOpen = false; if (inPrompter) showPrompter (false); resized(); repaint();
        };
        addAndMakeVisible (preparationButton);
        sessionFolder = settings.sessionFolder();
        if (! previewMode) recoverInterrupted();   // nie w zrzutach ekranu

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
            if (! previewMode) showStart (true);

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
            showRecordingUi (true);
            if (! previewMode) showStart (true);
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
            preparationAfterWizard = false;
            showStart (false);
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

    /** Zamkniecie okna (X, Alt+F4, wylogowanie). Nic nie ginie po cichu:
        w trakcie nagrywania — "Zakoncz i zamknij" konczy nagranie tak samo
        jak Esc (plik zamkniety, R do montazu), niezapisany montaz — pytanie.
        quitNow wolane dopiero, gdy wolno. */
    void requestClose (std::function<void()> quitNow)
    {
        if (closeAsked) return;                       // okno z pytaniem juz jest
        const auto ask = zs::ui::closing::ask (recorder.isRecording(),
                                               review != nullptr && review->hasUnsavedEdits());
        if (ask == zs::ui::closing::Ask::None) { quitNow(); return; }

        closeAsked = true;
        const bool rec = ask == zs::ui::closing::Ask::Recording;
        juce::AlertWindow::showOkCancelBox (juce::AlertWindow::QuestionIcon,
            rec ? PL ("Trwa nagrywanie") : PL ("Niezapisane zmiany"),
            rec ? PL ("Zakończyć nagrywanie i zamknąć program?\n\nNagranie zostanie zapisane, a pomyłki po R zaczekają w montażu.")
                : PL ("Montaż (cięcia, przycięcie) nie trafił do zapisanego miksu i zniknie po zamknięciu.\n\n")
                  + PL ("Oryginalne nagranie zostaje na dysku bez zmian."),
            rec ? PL ("Zakończ i zamknij") : PL ("Zamknij bez zapisu"),
            rec ? PL ("Nagrywaj dalej") : PL ("Wróć"), this,
            juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<MainComponent> (this), rec, quitNow] (int result)
            {
                if (safe == nullptr) return;
                safe->closeAsked = false;
                if (result != 1) return;
                if (rec && safe->recorder.isRecording()) safe->toggleRecord();   // jak Esc: zapis i R
                quitNow();
            }));
    }

    /** Dla okna, ktore chce zapamietac swoje polozenie. */
    Settings& userSettings() noexcept { return settings; }
    void previewSettings() { settingsOpen=true;resized();repaint(); }
    void previewPreparation (bool music = false, bool withPrompter = true)
    {
        inStart = false; preparation = true; settingsOpen = false;
        startWithMusic = music; startWithPrompter = withPrompter;
        backingFile = juce::File();
        refreshHomeVisibility(); resized(); repaint();
    }
    /** --preview-ui witaj: ekran startowy z nagraniami z folderu przykladu. */
    void previewStart (const juce::File& folder)
    {
        showStart (true);
        if (startScreen != nullptr) startScreen->setFolder (folder);
    }
    void previewPrompter() {
        preparation = false;
        recorder.prepareDevice(48000,512,1);
        prompter=std::make_unique<PrompterComponent>(recorder,settings);
        prompter->onClose=[this]{showPrompter(false);};
        prompter->setSpeech(&speech);
        prompter->onStopRequest=[this]{ if(recorder.isRecording()) toggleRecord(); else if (correctionMode) cancelReplacement(); };
        prompter->onStartStop=[this]{ startStop(); };
        prompter->onRetake=[this]{ logic.afterCut(); };
        addAndMakeVisible(*prompter);
        prompter->load(PL("Dziś porozmawiamy o tym, co naprawdę warto usłyszeć.\nZatrzymaj się na chwilę. Weź spokojny oddech.\nTo jest Twoje miejsce i Twój głos.\nNie musisz się spieszyć. Dobra opowieść potrzebuje czasu."));
        showPrompter(true);
    }



    void paint (juce::Graphics& g) override
    {
        using I = zs::Ink;
        g.fillAll (I::bg());
        if (inWizard || inReview || inStart) return;
        const bool rec = recorder.isRecording();
        const auto name = recordingName();
        zs::ScreenFrame::paint (g, getLocalBounds(), inPrompter ? juce::String() : name,
            inPrompter ? PL ("Spacja — nagraj / pauza  ·  R — powtórz wers  ·  Esc — zakończ")
            : rec ? PL ("Spacja albo Esc — zakończ i oceń") : preparation ? PL("Wybierz mikrofon i przygotuj tekst") : PL ("Nagraj, a potem odsłuchaj podejście"),
            inPrompter);
        auto side = zs::ScreenFrame::sidebar (getLocalBounds());
        zs::ScreenFrame::paintPanel (g, side);
        if (settingsOpen) { paintHardwareSettings (g); return; }
        if (preparation) { paintPreparation (g); return; }
        if (inPrompter) { paintPrompterSide (g); return; }
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
    }

    void resized() override
    {
        // miernik segmentowy tylko na ekranie nagrania z prompterem. Ustawiany na
        // koncu resized(), ale uklady Przygotowania i Ustawien wracaja wczesniej —
        // zostawal z dawnymi granicami i zaslanial "Ekran startowy" / "Sprawdz
        // sprzet" (przeklikanie 25.10, zrzut Adriana)
        segMeter.setVisible (inPrompter && prompter != nullptr && ! inReview && ! inWizard && ! settingsOpen && ! inStart);
        if (wizard) wizard->setBounds (getLocalBounds());
        if (inWizard)
        {
            const bool deviceStep = wizard != nullptr && wizard->isDeviceStep();
            auto area = deviceStep ? wizard->deviceControlsArea().translated (wizard->getX(), wizard->getY()) : juce::Rectangle<int>();
            auto deviceRow = area.removeFromTop (30);
            const int half = juce::jmax (0, (deviceRow.getWidth() - 8) / 2);
            deviceBox.setBounds (deviceRow.removeFromLeft (half)); deviceRow.removeFromLeft (8);
            inputBox.setBounds (deviceRow.removeFromLeft (half));
            auto level = area.removeFromBottom (20);
            meter.setBounds (level.removeFromLeft (juce::jmax (0, level.getWidth() - 82)));
            heardLabel.setBounds (level.removeFromRight (82));
            for (auto* c : { (juce::Component*) &deviceBox, (juce::Component*) &inputBox, (juce::Component*) &meter, (juce::Component*) &heardLabel })
            {
                c->setVisible (deviceStep);
                if (deviceStep) c->toFront (false);
            }
            return;
        }
        if (inReview && review) { review->setBounds (getLocalBounds()); return; }
        if (startScreen != nullptr) startScreen->setBounds (getLocalBounds());
        if (inStart) return;
        const bool rec = recorder.isRecording();
        refreshHomeVisibility();
        settingsButton.setBounds (getWidth() - 196, 14, 174, 36);
        // zakladki krokow w naglowku: miejsce do przyciskow po prawej (1 albo 2)
        flow.setBounds (zs::ScreenFrame::stepsArea (getLocalBounds()));
        if (settingsOpen) { layoutHardwareSettings(); return; }
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
        auto actions = side.removeFromBottom (inPrompter ? 0 : rec ? 42 : detailsOpen ? 126 : 82);
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
        hints.setBounds (card.withY (card.getBottom() + 8).withHeight (28)); fitHintsLabel();
        if (detailsOpen)
        {
            auto area = card.reduced (18);
            okButton.setBounds (area.removeFromBottom (34).removeFromRight (206));
            area.removeFromBottom (8); events.setBounds (area);
        }
        segMeter.setVisible (inPrompter && prompter != nullptr);
        if (inPrompter && prompter)
        {
            // jeden zegar, lampka, miernik — przyciski sa w transporcie promptera
            const auto P = prompterSide();
            lamp.setCompact (! P.musicCap.isEmpty() && P.lamp.getHeight() < 200);   // mala lampka tylko, gdy muzyka zabrala miejsce
            lamp.setBounds (P.lamp);
            segMeter.setBounds (P.meter);
            for (auto* c : std::initializer_list<juce::Component*> { &recButton, &repeatButton, &meter, &status, &hints })
                c->setVisible (false);
            for (auto* c : std::initializer_list<juce::Component*> { &lamp, &segMeter, &flow, &settingsButton })
                c->toFront (false);
        }
    }

    /** Esc na ekranie nagrywania (bez promptera). Prompter ma swoje klawisze
        i wola te same funkcje. */
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (inStart) return false;          // ekran startowy: spacja i Esc nic tu nie nagrywaja
        const auto a = zs::ui::keyAction (k.getKeyCode() == juce::KeyPress::escapeKey,
                                          (char32_t) k.getTextCharacter(),
                                          recorder.isRecording(), inPrompter && prompter != nullptr && prompter->hasText());

        if (a == zs::ui::KeyAction::StopAndReview) { toggleRecord(); return true; }
        // W prompterze spacja w trakcie nagrania to pauza, nie koniec — nawet
        // gdy klawisz trafil tu, a nie do promptera.
        if (a == zs::ui::KeyAction::StartStop && inPrompter && prompter != nullptr && recorder.isRecording())
        { prompter->spacePressed(); return true; }
        if (a == zs::ui::KeyAction::StartStop && startStop()) return true;
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

    void previewBacking (bool pause)
    {
        previewBackingEnabled = true;
        previewBackingName = PL ("Nocne_swiatla_demo");
        previewBackingDuration = 220.0;
        previewBackingPosition = 72.0;
        if (prompter != nullptr)
        {
            std::vector<float> envelope (22000);
            for (size_t i = 0; i < envelope.size(); ++i)
            {
                const float pulse = 0.18f + 0.52f * std::abs (std::sin ((float) i * 0.071f));
                const float phrase = (i % 4300 < 900 || i % 4300 > 3900) ? 0.28f : 1.0f;
                envelope[i] = pulse * phrase;
            }
            prompter->setBackingEnvelope (std::move (envelope), previewBackingPosition);
        }
        if (pause && prompter != nullptr) prompter->spacePressed();
        resized();
        repaint();
    }

    /** --preview-ui ... prompter-znaczki: znaczki przy wersach i odliczanie po R. */
    void previewMarks() { if (prompter != nullptr) prompter->previewMarks(); }
    void previewAim()   { if (prompter != nullptr) prompter->previewAimScreen(); }
    void previewLive()  { if (prompter != nullptr) prompter->previewLiveScreen(); }
    void previewReplacementRecording()
    {
        previewPrompter();
        std::vector<float> audio ((size_t) (18.0 * 48000.0));
        uint32_t seed = 0x51a7u;
        for (size_t i = 0; i < audio.size(); ++i)
        {
            seed = seed * 1664525u + 1013904223u;
            const float noise = (float) ((int) (seed >> 9) - (1 << 22)) / (float) (1 << 22);
            const double phase = std::fmod ((double) i / 48000.0, 2.7);
            const float env = (phase < 1.7 ? 0.16f : 0.025f) * (0.55f + 0.45f * (float) std::sin ((double) i * 0.00017));
            audio[i] = noise * env;
        }
        prompter->configureReplacement (std::move (audio), 48000.0, 8.0, 11.0, 4);
    }
    void previewReplacementSelection (const juce::File& wav)
    {
        if (! wav.existsAsFile()) return;
        openReview (wav, true, true);
        if (review != nullptr) { review->setEditMode (true); review->previewReplacementSelection(); }
    }
    void previewReplacementAfter (const juce::File& wav)
    {
        if (! wav.existsAsFile()) return;
        openReview (wav, true, true);
        if (review != nullptr)
        { review->setEditMode (true); review->previewReplacementSelection(); review->showReplacementSeams (8.0, 11.0); }
    }

    void previewCuts(const juce::File& wav) {
        if(!wav.existsAsFile()) return;
        // Wylacznie kopia podgladu; nie dopisujemy przykladow do pliku czlowieka.
        auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("zs-preview-ciecia");
        dir.createDirectory(); auto copy=dir.getChildFile("przyklad.wav"); wav.copyFileTo(copy);
        std::vector<tw::CutProposal> list(3);
        list[0].from=1; list[0].to=2; list[0].sure=true; list[0].state=tw::CutProposal::State::Accepted;
        list[1].from=3; list[1].to=4; list[1].sure=true; list[1].state=tw::CutProposal::State::Accepted;
        list[2].from=5; list[2].to=6; list[2].why="Nie rozpoznano początku powtórzonego wersu.";
        zs::writeCutList(copy,list); previewReview(copy, true);
    }
    void previewChain (const juce::File& wav)
    {
        if (wav.existsAsFile()) { openReview (wav, true, true); if (review) { review->setEditMode (false); review->previewChain(); } }
    }
    void previewReview (const juce::File& wav, bool edit = false, bool done = false)
    {
        if (wav.existsAsFile()) { openReview (wav, true, true); if (review) { review->setEditMode(edit); if (done) review->previewDone(); } }
    }

private:
    juce::String recordingName() const
    {
        return nameInput.getText().trim().isEmpty() ? PL("Nowy odcinek") : nameInput.getText().trim();
    }

    juce::Rectangle<int> homeCard() const
    {
        auto r = zs::ScreenFrame::mainArea (getLocalBounds());
        // z prompterem tytul i wiersz podpowiedzi zabieraly ~130 px tekstowi
        // (przeglad ekranu nagrania 24.10) — klawisze stoja w stopce
        if (inPrompter) return r;
        r.removeFromTop (98);
        r.removeFromBottom (36);
        return r;
    }

    juce::Rectangle<int> settingsBox() const
    {
        return zs::ScreenFrame::sidebar (getLocalBounds());
    }

    /** Prawy panel w nagraniu z prompterem (przeglad 24.10): jeden zegar w
        okienku ze stanem, lampka, miernik segmentowy, postep w tekscie. */
    struct PrompterSide { juce::Rectangle<int> nameCap, name, clockCap, clock, limit,
        musicCap, musicName, musicPosition, musicBar, lamp, meterCap, meter, textCap, text, bar; };
    PrompterSide prompterSide() const
    {
        PrompterSide L;
        auto s = zs::ScreenFrame::sidebar (getLocalBounds()).reduced (22, 22);
        L.nameCap = s.removeFromTop (18);
        L.name = s.removeFromTop (26);     s.removeFromTop (12);
        L.clockCap = s.removeFromTop (18); s.removeFromTop (6);
        L.clock = s.removeFromTop (64);    s.removeFromTop (4);
        L.limit = s.removeFromTop (18);    s.removeFromTop (4);
        if (previewBackingEnabled || recorder.hasBacking())
        {
            L.musicCap = s.removeFromTop (16); s.removeFromTop (3);
            L.musicName = s.removeFromTop (20); s.removeFromTop (2);
            L.musicPosition = s.removeFromTop (18); s.removeFromTop (4);
            L.musicBar = s.removeFromTop (4); s.removeFromTop (10);
        }
        L.bar = s.removeFromBottom (6);    s.removeFromBottom (6);
        L.text = s.removeFromBottom (20);
        L.textCap = s.removeFromBottom (18); s.removeFromBottom (18);
        L.meter = s.removeFromBottom (SegMeter::height); s.removeFromBottom (6);
        L.meterCap = s.removeFromBottom (18); s.removeFromBottom (12);
        L.lamp = s;
        return L;
    }

    void paintPrompterSide (juce::Graphics& g)
    {
        using I = zs::Ink;
        const auto L = prompterSide();
        const auto caption = [&] (const juce::String& t, juce::Rectangle<int> r)
        {
            g.setColour (I::textDim()); g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
            g.drawText (t, r, juce::Justification::centredLeft);
        };
        const bool rec = recorder.isRecording();
        const bool paused = rec && prompter != nullptr && prompter->isPausedView();
        const bool cue = paused && prompter->isResuming();
        const double t = rec && prompter != nullptr ? prompter->clockSeconds() : 0.0;
        const bool nearLimit = rec && zs::ui::session::phase (t) != zs::ui::session::Phase::Ok;

        // nazwa nagrania w panelu, nie w stopce (Adrian 25.10)
        caption (PL ("NAGRANIE"), L.nameCap);
        g.setColour (I::textNow()); g.setFont (juce::Font (zs::type::heading, juce::Font::bold));
        g.drawText (recordingName(), L.name, juce::Justification::centredLeft, true);

        caption (PL ("CZAS NAGRANIA"), L.clockCap);
        // okienko z liczba (spec. E); na pauzie pomaranczowe (aneks pkt 7)
        const auto box = L.clock.toFloat();
        if (paused)
        {
            g.setColour (juce::Colour (0xff21190f)); g.fillRoundedRectangle (box, 6.0f);
            g.setColour (juce::Colour (0xff95691e)); g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);
        }
        else zs::Depth::well (g, box, 6.0f);
        auto in = L.clock.reduced (16, 0);
        const juce::String tag = cue ? PL ("ROZBIEG") : paused ? PL ("PAUZA") : rec ? PL ("REC") : PL ("GOTOWY");
        const juce::Font tagFont (zs::type::tiny, juce::Font::bold);
        const auto tagArea = in.removeFromRight (tagFont.getStringWidth (tag) + 4);
        if (rec && ! paused)
        {
            g.setColour (I::red());
            g.fillEllipse (tagArea.toFloat().withSizeKeepingCentre (8.0f, 8.0f).translated (-12.0f - tagFont.getStringWidth (tag) / 2.0f, 0.0f));
        }
        g.setColour (paused ? I::yellow() : rec ? I::red() : I::textDim());
        g.setFont (tagFont);
        g.drawText (tag, tagArea, juce::Justification::centredRight);
        g.setColour (paused || nearLimit ? I::yellow() : rec ? I::textNow() : I::textNext());
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 32.0f, juce::Font::bold));
        g.drawText (pl::hhmmss (t), in, juce::Justification::centredLeft);
        if (nearLimit)
        {
            g.setColour (I::yellow()); g.setFont (juce::Font (zs::type::small));
            g.drawText (PL ("Zostało ") + juce::String (zs::ui::session::minutesLeft (t)) + PL (" min do końca sesji"),
                        L.limit, juce::Justification::centredLeft, true);
        }

        if (! L.musicCap.isEmpty())
        {
            caption (PL ("MUZYKA"), L.musicCap);
            const auto title = previewBackingEnabled ? previewBackingName : backingFile.getFileNameWithoutExtension();
            g.setColour (I::textNow()); g.setFont (juce::Font (zs::type::small, juce::Font::bold));
            g.drawFittedText (title, L.musicName, juce::Justification::centredLeft, 1);
            const double duration = previewBackingEnabled ? previewBackingDuration : recorder.backingSeconds();
            const double rate = recorder.currentSampleRate();
            const double samplePosition = previewBackingEnabled ? previewBackingPosition
                : rate > 0.0 ? (double) recorder.backingIndexNow() / rate : 0.0;
            const double position = juce::jlimit (0.0, duration, samplePosition);
            const bool ended = duration > 0.0 && samplePosition >= duration;
            const juce::String value = ended ? PL ("muzyka się skończyła")
                : pl::hhmmss (position) + PL (" / ") + pl::hhmmss (duration);
            g.setColour (paused ? I::yellow() : I::textNext());
            g.setFont (juce::Font (zs::type::small));
            g.drawFittedText (value, L.musicPosition, juce::Justification::centredLeft, 1);
            g.setColour (I::edge()); g.fillRoundedRectangle (L.musicBar.toFloat(), 2.0f);
            g.setColour (paused ? I::yellow() : I::accent());
            g.fillRoundedRectangle (L.musicBar.toFloat().withWidth (L.musicBar.getWidth() * (float) (duration > 0.0 ? position / duration : 0.0)), 2.0f);
        }

        caption (PL ("POZIOM"), L.meterCap);

        caption (PL ("TEKST"), L.textCap);
        const int n = prompter != nullptr ? prompter->lineTotal() : 0;
        const int k = prompter != nullptr ? prompter->lineNow() + 1 : 0;
        g.setColour (I::textNext()); g.setFont (juce::Font (zs::type::small));
        g.drawText (n > 0 ? PL ("wers ") + juce::String (k) + PL (" z ") + juce::String (n) : PL ("bez tekstu"),
                    L.text, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff263242));
        g.fillRoundedRectangle (L.bar.toFloat(), 3.0f);
        if (n > 0)
        {
            g.setColour (I::accent());
            g.fillRoundedRectangle (L.bar.toFloat().withWidth (L.bar.getWidth() * (float) k / (float) n), 3.0f);
        }
    }

    void paintRecording (juce::Graphics& g, juce::Rectangle<int> side)
    {
        using I = zs::Ink;
        g.setColour (I::textDim()); g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
        g.drawText (PL ("CZAS NAGRANIA"), side.removeFromTop (18), juce::Justification::centredLeft);
        const double t = recorder.isRecording() ? recorder.recordedSeconds() : 0.0;
        const bool nearLimit = recorder.isRecording()
                            && zs::ui::session::phase (t) != zs::ui::session::Phase::Ok;
        g.setColour (nearLimit ? I::yellow() : I::textNow()); g.setFont (juce::Font (46.0f, juce::Font::bold));
        g.drawText (pl::hhmmss (t), side.removeFromTop (60), juce::Justification::centredLeft);
        g.setColour (nearLimit ? I::yellow() : I::textNext()); g.setFont (juce::Font (zs::type::small));
        g.drawFittedText (nearLimit ? PL ("Zostało ") + juce::String (zs::ui::session::minutesLeft (t))
                                        + PL (" min — w 60. minucie nagrywanie zatrzyma się samo.")
                        : recorder.isRecording() ? PL ("Nagrywanie trwa. Patrz na lampkę.") : PL ("Twoje miejsce i Twój głos."),
                          side.removeFromTop (24), juce::Justification::centredLeft, 1, 0.8f);
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
        settingsButton.setVisible(!inWizard && !inReview && !inStart);
        settingsButton.setEnabled(!inWizard && !inStart && !rec);
        detailsButton.setVisible(idleHome && !settingsOpen);openButton.setVisible(idleHome && !settingsOpen);
        settingsButton.setButtonText(settingsOpen?PL("Zamknij ustawienia"):PL("Ustawienia"));
        settingsButton.setTooltip (rec ? PL("Ustawienia będą dostępne po zakończeniu nagrania") : juce::String());
        detailsButton.setButtonText(detailsOpen?PL("Ukryj uwagi"):PL("Pokaż uwagi"));
        selector->setVisible(idleHome && settingsOpen);
        folderButton.setVisible(idleHome && settingsOpen);checkButton.setVisible(idleHome && settingsOpen);
        events.setVisible(idleHome && detailsOpen && !settingsOpen);okButton.setVisible(idleHome && detailsOpen && !settingsOpen);
        prompterButton.setVisible(idleHome && !preparation && !settingsOpen);
        otherTextButton.setVisible(idleHome && !settingsOpen && settings.lastScript().existsAsFile());
        flow.setVisible(!inWizard && !inReview);
        flow.setCurrent (inStart ? 0 : preparation || settingsOpen ? 1 : 2);
        const bool prepare = idleHome && preparation;
        nameInput.setVisible(false);
        scriptInput.setVisible(prepare && startWithPrompter && scriptEditorOpen && !settingsOpen);
        musicButton.setVisible(prepare && startWithMusic && !settingsOpen);
        editScriptButton.setVisible(prepare && startWithPrompter && !settingsOpen);
        prepareButton.setVisible(prepare && !settingsOpen);
        preparationButton.setVisible(!preparation && !rec && !inWizard && !inReview);
        for (auto* c : { (juce::Component*) &deviceBox, (juce::Component*) &inputBox, (juce::Component*) &heardLabel })
            c->setVisible(false);
        if (prepare) {
            prompterButton.setVisible(false); otherTextButton.setVisible(false);
            checkButton.setVisible(!settingsOpen); folderButton.setVisible(settingsOpen);
            detailsButton.setVisible(false); openButton.setVisible(false);
        }
        lamp.setVisible(!inWizard && !inReview && (rec || inPrompter));
        // w Przygotowaniu miernika nie ma (wybor wejscia jest w kreatorze); zostawal
        // z granicami z kreatora i zaslanial "Ekran startowy" (przeklikanie 25.10)
        meter.setVisible(!inWizard && !inReview && !settingsOpen && !prepare);
        status.setVisible(!inWizard && !inReview && !settingsOpen);
        zs::Look::setKind(recButton, rec || detailsOpen ? zs::ui::ButtonKind::Primary : zs::ui::ButtonKind::Record);

        const bool hasScript = inPrompter && prompter != nullptr && prompter->hasText();
        if (!preparation) updatePrompterButton();
        if (prompter != nullptr) prompter->refreshControls();
        // W prompterze jeden zestaw sterowania: transport pod tekstem (Nagraj,
        // Pauza, Odtworz, Zakoncz) i klawisze. Boczne Nagraj / Powtorz wers /
        // Zatrzymaj i ocen dublowaly go i zacieraly stan (audyt 14.10).
        recButton.setVisible(!inWizard && !inReview && !preparation && !settingsOpen && !inPrompter);
        repeatButton.setVisible(false);
        hints.setVisible((inPrompter || rec) && !inWizard && !inReview);
        hintsFull = PL(zs::ui::hintLine(rec, hasScript).c_str()); fitHintsLabel();
        hints.setJustificationType(inPrompter ? juce::Justification::centred : juce::Justification::centredLeft);
    }

    void layoutHardwareSettings()
    {
        // JUCE oddaje tylko 60% szerokosci listom i rezerwuje miejsce na Test.
        // W panelu 260 px nie zostawalo miejsca nawet na "Brak urzadzenia".
        auto main = zs::ScreenFrame::mainArea (getLocalBounds());
        main.removeFromTop (zs::ScreenFrame::headHeight (getLocalBounds(), hardwareSubtitle()) + 8);
        selector->setBounds (main);
        auto side = settingsBox().reduced (22, 24);
        folderButton.setBounds (side.removeFromBottom (36)); side.removeFromBottom (8);
        checkButton.setBounds (side.removeFromBottom (38));
        // pod mikrofonem i sluchawkami (2 x 108 px): wzorzec brzmienia (24 + 48)
        side = settingsBox().reduced (22, 24);
        side.removeFromTop (2 * 108 + 24 + 48 + 4);
        forgetProfileButton.setBounds (side.removeFromTop (34).removeFromLeft (180));
    }

    void paintHardwareSettings (juce::Graphics& g)
    {
        zs::ScreenFrame::paintHead (g, getLocalBounds(), PL("01 / PRZYGOTOWANIE"), PL("Ustawienia"), hardwareSubtitle());
        auto side = settingsBox().reduced (22, 24);
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup (setup);
        auto device = [&] (const juce::String& label, const juce::String& name) {
            g.setColour (zs::Ink::textDim()); g.setFont (juce::Font (zs::type::small));
            g.drawText (label, side.removeFromTop (24), juce::Justification::centredLeft);
            g.setColour (zs::Ink::textNow()); g.setFont (juce::Font (zs::type::body));
            g.drawFittedText (name.isEmpty() ? PL("Brak urządzenia") : name, side.removeFromTop (72), juce::Justification::topLeft, 3);
            side.removeFromTop (12);
        };
        device (PL("MIKROFON"), setup.inputDeviceName);
        device (PL("SŁUCHAWKI"), setup.outputDeviceName);

        // wzorzec brzmienia (ReviewComponent: "Zapamietaj to brzmienie")
        const auto profile = mix::VoiceProfile::fromText (settings.voiceProfile().toStdString());
        g.setColour (zs::Ink::textDim()); g.setFont (juce::Font (zs::type::small));
        g.drawText (PL("WZORZEC BRZMIENIA"), side.removeFromTop (24), juce::Justification::centredLeft);
        zs::ScreenFrame::wrapped (g, profile.valid() ? juce::String::fromUTF8 (profile.label.c_str())
                                                     : PL("Brak. Ustawisz go po zapisaniu miksu, na ekranie Gotowe."),
                                  side.removeFromTop (48), juce::Font (profile.valid() ? zs::type::body : zs::type::small),
                                  profile.valid() ? zs::Ink::textNow() : zs::Ink::textNext());
    }

    static juce::String hardwareSubtitle() { return PL("Urządzenia, słuchawki, wzorzec brzmienia i folder sesji."); }
    juce::String preparationSubtitle() const
    {
        if (startWithMusic && startWithPrompter)
            return PL ("Dodaj podkład i tekst do promptera. Nazwa odcinka jest już ustawiona.");
        if (startWithMusic)
            return PL ("Dodaj podkład muzyczny. Nazwa odcinka jest już ustawiona.");
        if (startWithPrompter)
            return PL ("Dodaj tekst do promptera lub wklej go samodzielnie.");
        return PL ("Wszystko gotowe. Możesz jeszcze sprawdzić mikrofon przed nagraniem.");
    }

    void paintPreparation (juce::Graphics& g)
    {
        using I = zs::Ink;
        auto main = zs::ScreenFrame::paintHead (g, getLocalBounds(), PL("01 / PRZYGOTOWANIE"),
                                               PL("Przygotowanie nagrania"), preparationSubtitle());
        main.removeFromTop (12);
        auto card = [] (juce::Graphics& gg, juce::Rectangle<int> r) {
            gg.setColour (I::panel()); gg.fillRoundedRectangle (r.toFloat(), 8.0f);
            gg.setColour (I::edge()); gg.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        };
        auto label = [&g] (const juce::String& s, juce::Rectangle<int> r) {
            g.setColour (I::textDim()); g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
            g.drawText (s, r, juce::Justification::centredLeft);
        };
        auto body = [&g] (const juce::String& s, juce::Rectangle<int> r, bool strong = false) {
            g.setColour (strong ? I::textNow() : I::textNext()); g.setFont (juce::Font (strong ? zs::type::heading : zs::type::body));
            g.drawFittedText (s, r, juce::Justification::centredLeft, 2);
        };

        auto summary = main.removeFromTop (62);
        card (g, summary);
        auto si = summary.reduced (16, 8);
        label (PL ("ODCINEK"), si.removeFromTop (16));
        body (recordingName(), si, true);
        main.removeFromTop (10);

        if (startWithMusic)
        {
            auto row = main.removeFromTop (84);
            card (g, row);
            auto ri = row.reduced (16, 10);
            label (PL ("PODKŁAD MUZYCZNY"), ri.removeFromTop (16));
            auto fileText = backingFile.existsAsFile() ? backingFile.getFileName()
                                                       : PL ("Nie dodano pliku. Obsługiwane: MP3, WAV, FLAC, AIFF, OGG.");
            ri.removeFromRight (musicButton.getWidth() + 12);
            body (fileText, ri, backingFile.existsAsFile());
            main.removeFromTop (10);
        }

        if (startWithPrompter)
        {
            auto row = main.removeFromTop (88);
            card (g, row);
            auto ri = row.reduced (16, 10);
            label (PL ("TEKST DO PROMPTERA"), ri.removeFromTop (16));
            auto content = scriptInput.getText().trim();
            const auto scriptState = content.isEmpty() ? PL ("Nie dodano tekstu — możesz czytać swobodnie.")
                : PL ("Tekst gotowy · ") + juce::String (content.length()) + PL (" znaków");
            ri.removeFromRight (332);
            body (scriptState, ri.removeFromBottom (26));
            main.removeFromTop (10);
        }
        else
        {
            auto row = main.removeFromTop (56);
            card (g, row);
            body (PL ("Nagrywanie bez promptera — tekst nie jest potrzebny."), row.reduced (16, 8));
            main.removeFromTop (10);
        }

        if (scriptEditorOpen && startWithPrompter)
        {
            auto edit = main.reduced (0, 0);
            card (g, edit);
        }

        if (! settingsOpen)
        {
            auto side = settingsBox().reduced (22, 24);
            label (PL ("PRZED NAGRANIEM"), side.removeFromTop (22));
            side.removeFromTop (8);
            auto* device = deviceManager.getCurrentAudioDevice();
            const auto name = device != nullptr ? inputDeviceName() : PL ("Mikrofon nie wybrany");
            body (name, side.removeFromTop (48), true);
            const auto freshness = hardwareCheckedThisSession ? PL ("Sprawdzone dziś")
                : settings.hardwareChecked() ? PL ("Sprawdzone wcześniej") : PL ("Test jeszcze nie wykonany");
            body (freshness, side.removeFromTop (42));
            side.removeFromTop (12);
            g.setColour (I::edge()); g.drawHorizontalLine (side.getY(), (float) side.getX(), (float) side.getRight());
            side.removeFromTop (12);
            body (startWithMusic ? PL ("Podkład: do słuchawek") : PL ("Tryb: sam głos"), side.removeFromTop (42));
        }
    }

    void layoutPreparation()
    {
        auto main = zs::ScreenFrame::mainArea (getLocalBounds());
        main.removeFromTop (zs::ScreenFrame::headHeight (getLocalBounds(), preparationSubtitle()) + 8 + 12);
        main.removeFromTop (62); main.removeFromTop (10);
        if (startWithMusic)
        {
            auto row = main.removeFromTop (84);
            musicButton.setButtonText (backingFile.existsAsFile() ? PL ("Zmień podkład") : PL ("Wybierz podkład"));
            musicButton.setBounds (row.removeFromRight (156).withSizeKeepingCentre (140, 36));
            main.removeFromTop (10);
        }
        if (startWithPrompter)
        {
            auto row = main.removeFromTop (88);
            auto actions = row.reduced (16, 10).removeFromBottom (32);
            editScriptButton.setBounds (actions.removeFromRight (168)); actions.removeFromRight (8);
            prompterButton.setButtonText (PL ("Wczytaj plik TXT"));
            prompterButton.setBounds (actions.removeFromRight (156));
            main.removeFromTop (10);
        }
        else { main.removeFromTop (56); main.removeFromTop (10); }
        if (scriptEditorOpen && startWithPrompter)
        {
            auto editor = main.reduced (0, 0).reduced (12, 12);
            scriptInput.setBounds (editor);
        }
        else scriptInput.setBounds ({ });

        auto side = settingsBox().reduced(22,24);
        auto bottom = side.removeFromBottom (92);
        prepareButton.setBounds(bottom.removeFromBottom(42)); bottom.removeFromBottom(8);
        checkButton.setBounds (bottom.removeFromBottom (38));
        zs::Look::setKind (prepareButton, zs::ui::ButtonKind::Primary);
        zs::Look::setKind (checkButton, zs::ui::ButtonKind::Secondary);
        checkButton.setButtonText (PL ("Sprawdź sprzęt"));
        prompterButton.setVisible (startWithPrompter && ! settingsOpen);
        editScriptButton.setVisible (startWithPrompter && ! settingsOpen);
        scriptInput.setVisible (startWithPrompter && scriptEditorOpen && ! settingsOpen);
        musicButton.setVisible (startWithMusic && ! settingsOpen);
        folderButton.setVisible(false);
        status.setVisible(false);
    }

    //==========================================================================
    // ---- karta i wejscie z mikrofonem (zasady: InputChoice.h) ------------------

    /** Zmiana karty — z naszych list albo z okna "Mikrofon i sprzet". */
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        refreshInputBoxes();
        saveAudioState();
        if (settingsOpen) repaint();
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
        auto colour = zs::Ink::accent();
        const auto now = juce::Time::getMillisecondCounter();

        if (auto* d = deviceManager.getCurrentAudioDevice())
        {
            if (level > 0.003f)
            {
                quietSinceMs = 0;
                const auto mask = (uint64_t) d->getActiveInputChannels().toInt64();
                const int n = zs::input::physicalInput (mask, recorder.analysisCh());
                if (n > 0) s = PL ("słyszę wejście ") + juce::String (n);
            }
            // 24.10: SSL2 z pokretlem GAIN na zerze dawal same szumy (-84 dBFS),
            // a nagrania wychodzily ciche. Po kilku sekundach ciszy podpowiedz.
            else
            {
                if (quietSinceMs == 0) quietSinceMs = now;
                if (now - quietSinceMs > quietHintMs) { s = PL ("cisza — podkręć GAIN?"); colour = zs::Ink::yellow(); }
            }
        }

        if (heardLabel.getText() != s) heardLabel.setText (s, juce::dontSendNotification);
        heardLabel.setColour (juce::Label::textColourId, colour);
    }

    void repeatLine()
    {
        if (prompter != nullptr && inPrompter && recorder.isRecording())
            prompter->repeatLine();
    }

    /** Spacja: start i stop — tylko tam, gdzie widac przycisk Nagraj (ekran
        nagrywania), zeby spacja w polu tekstu czy w ustawieniach nie startowala
        nagrania. */
    bool startStop()
    {
        // start tylko z ekranu nagrywania: przycisk Nagraj albo prompter (tam
        // Nagraj jest w transporcie pod tekstem)
        const bool onRecordScreen = recButton.isShowing() || (inPrompter && ! inReview && ! inWizard && ! preparation && ! settingsOpen);
        if (! recorder.isRecording() && ! onRecordScreen) return false;
        if (! recorder.isRecording() && ! correctionMode && ! recButton.isEnabled()) return false;
        toggleRecord();
        return true;
    }

    void toggleRecord()
    {
        if (correctionMode)
        {
            if (recorder.isRecording()) finishReplacement();
            else startReplacement();
            return;
        }
        if (recorder.isRecording())
        {
            const auto justRecorded = recorder.currentFile();
            recorder.stop();
            finishStoppedTake (justRecorded);
            return;
        }

        preparation = false;
        startRecording();
    }

    void beginReplacement (double from, double to, const std::vector<float>& audio, double rate)
    {
        if (review == nullptr || to <= from || rate <= 0.0) return;
        ensurePrompter();
        if (review->hasUnsavedEdits())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, PL ("Zapisz montaż"),
                PL ("Najpierw zapisz obecne zmiany w montażu. Potem zaznacz fragment i nagraj poprawkę."));
            return;
        }
        correctionSource = review->sourceFile();
        correctionFrom = from; correctionTo = to;
        correctionFixedLength = zs::BackingTrack::musicFor (correctionSource).existsAsFile();
        if (correctionSource == juce::File() || ! correctionSource.existsAsFile()) return;

        const auto scriptFile = correctionSource.getSiblingFile (correctionSource.getFileNameWithoutExtension() + ".tekst.txt");
        auto script = scriptFile.existsAsFile() ? scriptFile.loadFileAsString()
                                                : (reviewCameFromPrompter ? prompter->sourceTextUtf8() : juce::String());
        if (script.trim().isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon, PL ("Brak tekstu"),
                PL ("Nie mam tekstu przypisanego do tego nagrania. Poprawka wymaga właściwego tekstu; oryginał pozostaje bez zmian."));
            return;
        }
        int firstWord = 0;
        const auto marksFile = correctionSource.getSiblingFile (correctionSource.getFileNameWithoutExtension() + ".wersy.txt");
        if (marksFile.existsAsFile())
        {
            const auto marks = tw::LineLog::parse (marksFile.loadFileAsString().toStdString());
            double distance = std::numeric_limits<double>::max();
            for (const auto& mark : marks)
                if (std::abs (mark.sec - from) < distance) { distance = std::abs (mark.sec - from); firstWord = mark.firstWord; }
        }
        correctionMode = true; correctionWaitingStart = false;
        prompter->load (script);
        showReview (false); showPrompter (true);
        prompter->configureReplacement (audio, rate, from, to, firstWord);
        append (PL ("--- poprawka fragmentu ") + juce::String (from, 2) + PL ("–") + juce::String (to, 2) + PL (" s"));
    }

    void startReplacement()
    {
        if (! correctionMode || correctionWaitingStart || recorder.isRecording() || prompter == nullptr) return;
        if (! noDevice)
        {
            auto* dev = deviceManager.getCurrentAudioDevice();
            if (dev == nullptr || dev->getActiveInputChannels().isZero())
            { juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, PL ("Nie ma mikrofonu"), PL ("Podłącz mikrofon, a potem spróbuj ponownie.")); return; }
        }
        if (correctionFixedLength)
        {
            backingFile = zs::BackingTrack::musicFor (correctionSource);
            if (! prepareBacking() || ! recorder.hasBacking())
            { juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, PL ("Nie mogę odtworzyć podkładu"), PL ("Poprawka nie została rozpoczęta. Oryginalne nagranie pozostaje bez zmian.")); return; }
        }
        if (correctionFixedLength)
        {
            const auto err = recorder.start (sessionFolder, recordingName() + PL (" poprawka"));
            if (err.isNotEmpty()) { append (PL ("--- nie udało się rozpocząć poprawki: ") + err); return; }
            prompter->startReplacementMusicCue();
            correctionWaitingStart = false;
        }
        else
        {
            if (recorder.hasBacking()) recorder.setBacking ({}, {});
            prompter->startReplacementPreRoll();
            correctionWaitingStart = true;
        }
        repaint();
    }

    void finishReplacement()
    {
        if (! correctionMode || ! recorder.isRecording()) return;
        pendingReplacement = recorder.currentFile();
        recorder.stop(); correctionWaitingStart = false;
        if (prompter != nullptr)
        {
            tw::LineLog log; log.begin();
            for (const auto& mark : prompter->replacementMarks()) log.restart (mark, mark.source);
            const auto text = juce::String::fromUTF8 (log.toText().c_str());
            pendingReplacement.getSiblingFile (pendingReplacement.getFileNameWithoutExtension() + ".wersy.txt")
                .replaceWithText (text, false, false, "\n");
            recorder.playPreRoll ({});
        }
        correctionMode = false;
        juce::AlertWindow::showOkCancelBox (juce::AlertWindow::QuestionIcon,
            PL ("Czy podmienić fragment?"),
            PL ("Poprawka jest gotowa. Podmienić nią zaznaczone miejsce? Oryginał zostanie zachowany w folderze surowe."),
            PL ("Podmień fragment"), PL ("Zostaw oryginał"), this,
            juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<MainComponent> (this)] (int result)
            {
                if (safe == nullptr) return;
                if (result == 1) safe->applyReplacement();
                else safe->discardPendingReplacement();
            }));
    }

    void applyReplacement()
    {
        if (! pendingReplacement.existsAsFile()) return;
        const auto result = zs::replaceRecordedRange (correctionSource, pendingReplacement,
                                                       correctionFrom, correctionTo, correctionFixedLength);
        if (! result.applied)
        {
            if (prompter != nullptr) prompter->clearReplacement();
            showPrompter (false); openReview (correctionSource, true, true);
            if (review != nullptr) review->setEditMode (true);
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, PL ("Nie podmieniono fragmentu"),
                result.error + PL (". Oryginalne nagranie pozostało bez zmian."));
            return;
        }
        pendingReplacement = juce::File();
        correctionUndo = result.rawCopy;
        append (PL ("--- podmieniono fragment; poprzednia wersja w folderze surowe"));
        if (prompter != nullptr) prompter->clearReplacement();
        showPrompter (false); openReview (correctionSource, true, true);
        if (review != nullptr)
        {
            review->setEditMode (true);
            review->setReplacementUndoAvailable (true);
            const double newLength = correctionTo - correctionFrom - result.removedSec;
            review->showReplacementSeams (correctionFrom, correctionFrom + newLength);
        }
    }

    void discardPendingReplacement()
    {
        for (const auto* suffix : { ".wersy.txt", ".ciecia.tsv", ".podklad.txt", ".tekst.txt" })
            pendingReplacement.getSiblingFile (pendingReplacement.getFileNameWithoutExtension() + suffix).deleteFile();
        pendingReplacement.deleteFile();
        pendingReplacement = juce::File();
        if (prompter != nullptr) prompter->clearReplacement();
        append (PL ("--- poprawka odrzucona; oryginalne nagranie pozostało bez zmian"));
        showPrompter (false); openReview (correctionSource, true, true);
        if (review != nullptr) review->setEditMode (true);
    }

    void cancelReplacement()
    {
        if (recorder.isRecording()) { recorder.stop(); recorder.playPreRoll ({}); }
        correctionWaitingStart = false; correctionMode = false;
        if (prompter != nullptr) prompter->clearReplacement();
        showPrompter (false); openReview (correctionSource, true, true);
        if (review != nullptr) review->setEditMode (true);
    }

    void undoReplacement()
    {
        if (correctionUndo == juce::File() || ! correctionUndo.existsAsFile() || ! correctionSource.existsAsFile()) return;
        const auto temp = correctionSource.getSiblingFile (correctionSource.getFileNameWithoutExtension() + ".cofnij.tmp.wav");
        temp.deleteFile();
        if (! correctionUndo.copyFileTo (temp))
        { temp.deleteFile(); juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, PL ("Nie udało się cofnąć"), PL ("Kopia poprzedniego nagrania nadal jest w folderze surowe.")); return; }
        correctionSource.deleteFile();
        if (! temp.moveFileTo (correctionSource))
        { correctionUndo.copyFileTo (correctionSource); temp.deleteFile(); juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, PL ("Nie udało się cofnąć"), PL ("Kopia poprzedniego nagrania nadal jest w folderze surowe.")); return; }
        for (const auto* suffix : { ".wersy.txt", ".ciecia.tsv", ".podklad.txt", ".tekst.txt" })
        {
            const auto archived = correctionUndo.getSiblingFile (correctionUndo.getFileNameWithoutExtension() + suffix);
            const auto target = correctionSource.getSiblingFile (correctionSource.getFileNameWithoutExtension() + suffix);
            if (archived.existsAsFile()) archived.copyFileTo (target); else target.deleteFile();
        }
        correctionUndo = juce::File(); openReview (correctionSource, true, true);
        if (review != nullptr) review->setEditMode (true);
    }

    /** Po zatrzymaniu — przez czlowieka albo przez urzadzenie: wyciecie po R,
        lampka, ocena. */
    void finishStoppedTake (const juce::File& justRecorded)
    {
        {
            recButton.setButtonText ("Nagraj");
            zs::Look::setKey (recButton, {});
            append ("--- zatrzymane: " + justRecorded.getFileName());

            // Punch and roll: pomylki po R wycinamy teraz, zanim ocena
            // przeczyta plik (PunchFile.h). Surowe nagranie idzie do "surowe".
            if (prompter != nullptr)
            {
                if (prompter->hasText())
                    justRecorded.getSiblingFile (justRecorded.getFileNameWithoutExtension() + ".tekst.txt")
                        .replaceWithText (prompter->sourceTextUtf8(), false, false, "\n");
                const auto punched = prompter->finishTake (justRecorded);
                if (punched.applied)
                {
                    // ocena czyta czysty plik — zdarzenia na jego czas, bez wycietych
                    takeEvents = tw::Punch().remapEvents (takeEvents, punched.plan);
                    append (PL ("--- wycięte pomyłki (R): ") + juce::String (punched.cuts) + ", "
                            + juce::String (punched.removedSec, 1) + PL (" s; pełne nagranie w folderze „surowe”")
                            + (punched.audibleSeams > 0
                                 ? PL ("; uwaga: przy ") + juce::String (punched.audibleSeams)
                                   + PL (" połączeniu po cięciu tło się zmieniło (np. wentylator) — posłuchaj tych miejsc")
                                 : juce::String()));
                }
                if (! punched.applied && ! punched.plan.empty())
                {
                    juce::String where;
                    for (const auto& c : punched.plan)
                        where << (where.isEmpty() ? "" : ", ") << pl::hhmmss (c.from) << PL ("–") << pl::hhmmss (c.to);
                    append (PL ("--- R: pomyłki do wycięcia na ekranie Montaż: ") + where
                            + PL (" (nagranie zostaje w całości — program sam nie tnie)"));
                }
                for (const auto& s : punched.skipped)
                    append (PL ("--- NIE wycięłem pomyłki ") + juce::String (s.cut.from, 1) + "–" + juce::String (s.cut.to, 1)
                            + PL (" s: ") + PL (s.why.c_str()) + PL (". Wytnij ją na ekranie Montaż."));
                if (punched.error.isNotEmpty())
                    append (PL ("--- nie wycięłem pomyłek: ") + punched.error + PL (" (nagranie zostaje w całości)"));
            }
            logic.reset();
            lamp.setState (0, "Zatrzymane.", true);

            // Ocena idzie SAMA, bez pytania.
            //
            // Czlowiek, ktory wlasnie skonczyl mowic, ma jedno pytanie: czy to
            // sie nadaje. Kazanie mu szukac przycisku, zeby je zadac, znaczy
            // tylko tyle, ze przez pierwsze tygodnie go nie znajdzie, a potem
            // zapomni, ze istnieje — i program zostanie dyktafonem z lampka.
            openReview (justRecorded);
        }
    }

    /** Podklad do Recordera, w czestotliwosci karty (Backing.h). Wczytuje raz
        na plik i czestotliwosc; bez podkladu — zdejmuje stary. false: blad, nie
        nagrywamy (lepiej powiedziec niz nagrac wiersz bez muzyki). */
    bool prepareBacking()
    {
        if (backingFile == juce::File())
        {
            if (recorder.hasBacking()) recorder.setBacking ({}, {});
            loadedBackingKey.clear();
            if (prompter != nullptr) prompter->setBackingEnvelope ({});
            return true;
        }
        const double rate = recorder.currentSampleRate();
        const auto key = backingFile.getFullPathName() + "@" + juce::String (rate);
        if (key == loadedBackingKey && recorder.hasBacking())
        {
            if (prompter != nullptr) prompter->setBackingEnvelope (backingEnvelope);
            return true;
        }
        zs::BackingTrack t;
        juce::String err;
        if (backingPreload != nullptr && backingPreload->key == key)
        {
            // wczytywane w tle od Startu (preloadBacking); zwykle juz gotowe
            while (! backingPreload->done.load()) juce::Thread::sleep (10);
            t = std::move (backingPreload->track);
            err = backingPreload->error;
            backingPreload.reset();
        }
        else err = t.load (backingFile, rate);
        if (err.isNotEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, PL ("Podkład"), err);
            return false;
        }
        backingEnvelope.clear();
        backingEnvelope.reserve ((t.left.size() + (size_t) juce::jmax (1.0, rate / 100.0) - 1)
                                 / (size_t) juce::jmax (1.0, rate / 100.0));
        const size_t hop = (size_t) juce::jmax (1.0, rate / 100.0);
        for (size_t start = 0; start < t.left.size(); start += hop)
        {
            const size_t end = juce::jmin (t.left.size(), start + hop);
            float peak = 0.0f;
            for (size_t i = start; i < end; ++i) peak = juce::jmax (peak, std::abs (t.left[i]));
            backingEnvelope.push_back (peak);
        }
        if (prompter != nullptr) prompter->setBackingEnvelope (backingEnvelope);
        recorder.setBacking (std::move (t.left), std::move (t.right));
        loadedBackingKey = key;
        return true;
    }

    void startRecording()
    {
        // Bez wejscia nie zaczynamy: zamiast "Brak urzadzenia" na lampce mowimy,
        // co zrobic (audyt 14.10).
        if (! noDevice)
        {
            auto* dev = deviceManager.getCurrentAudioDevice();
            if (dev == nullptr || dev->getActiveInputChannels().isZero())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, PL ("Nie ma mikrofonu"),
                    PL ("Program nie widzi żadnego wejścia dźwięku. Podłącz mikrofon albo interfejs, a potem kliknij „Sprawdź sprzęt”."));
                return;
            }
        }
        if (! prepareBacking()) return;
        const auto err = recorder.start (previewFolder != juce::File() ? previewFolder : sessionFolder, recordingName());

        if (err.isNotEmpty())
        {
            append ("BLAD: " + err);
            lamp.setState (3, err);
            return;
        }
        if (recorder.hasBacking())
        {
            zs::BackingTrack::writeNote (recorder.currentFile(), backingFile);
            append (PL ("--- podkład: ") + backingFile.getFileName());
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
        if (correctionMode && recorder.takeStoppedByDevice())
        {
            recorder.stop(); correctionMode = false; correctionWaitingStart = false;
            if (prompter != nullptr) prompter->clearReplacement();
            showPrompter (false); openReview (correctionSource, true, true);
            if (review != nullptr) review->setEditMode (true);
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, PL ("Poprawka przerwana"), PL ("Urządzenie dźwiękowe przestało działać. Poprzednie nagranie pozostało bez zmian."));
            return;
        }
        if (correctionMode && correctionWaitingStart && prompter != nullptr && prompter->replacementCountdownElapsed())
        {
            correctionWaitingStart = false; recorder.playPreRoll ({});
            const auto err = recorder.start (sessionFolder, recordingName() + PL (" poprawka"));
            if (err.isNotEmpty())
            {
                append (PL ("--- nie udało się rozpocząć poprawki: ") + err);
                cancelReplacement(); return;
            }
        }
        if (correctionMode && correctionFixedLength && recorder.isRecording() && ! recorder.isPaused()
            && ! recorder.cueRunning()
            && recorder.recordedSeconds() >= correctionTo - correctionFrom)
        { finishReplacement(); return; }

        // "Usun wzorzec" chodzi razem z ekranem Mikrofon i sprzet (jak "Folder
        // sesji"), ale tylko gdy wzorzec jest
        {
            const bool want = folderButton.isVisible() && settingsOpen && settings.voiceProfile().isNotEmpty();
            if (forgetProfileButton.isVisible() != want) forgetProfileButton.setVisible (want);
        }
        // Mikrofon albo karta przestaly dzialac w trakcie nagrania (audyt 14.10):
        // to, co nagrane, jest zamkniete i zapisane — mowimy wprost i idziemy dalej.
        if (recorder.takeStoppedByDevice())
        {
            const auto justRecorded = recorder.currentFile();
            append (PL ("--- nagrywanie przerwane: urządzenie dźwiękowe przestało działać"));
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, PL ("Nagrywanie przerwane"),
                PL ("Mikrofon albo karta dźwiękowa przestała działać (odłączony kabel, Bluetooth, zmiana w ustawieniach Windows).\n\n")
                + PL ("Wszystko, co nagrałeś do tej chwili, jest zapisane — za chwilę możesz to odsłuchać. Resztę nagrasz po ponownym podłączeniu."));
            finishStoppedTake (justRecorded);
            return;
        }
        // Limit sesji (UiRules.h, zs::ui::session): w 60. minucie stop i ocena,
        // jak przy odlaczonej karcie — wszystko zapisane.
        if (recorder.isRecording()
            && zs::ui::session::phase (recorder.recordedSeconds()) == zs::ui::session::Phase::Stop)
        {
            const auto justRecorded = recorder.currentFile();
            recorder.stop();
            append (PL ("--- nagrywanie zatrzymane: sesja ma 60 minut"));
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon, PL ("Sesja ma 60 minut"),
                PL ("Tyle program sprawdził w jednej sesji, więc nagrywanie zatrzymało się samo. ")
                + PL ("Wszystko jest zapisane — za chwilę możesz to odsłuchać.\n\n")
                + PL ("Dalszą część nagraj jako nową sesję („Wróć do nagrywania”)."));
            finishStoppedTake (justRecorded);
            return;
        }
        if (recovered != juce::File() && ! inWizard && ! inReview && ! recorder.isRecording())
            offerRecovered();

        lamp.setRecording(recorder.isRecording());
        // Kreator potrzebuje czestotliwosci probkowania, a ta jest znana dopiero
        // wtedy, gdy urzadzenie ruszy — stad start z opoznieniem, a nie w
        // konstruktorze.
        if (inWizard)
        {
            const float lvNow = recorder.inputLevel();
            meter.setLevel (lvNow);
            updateHeard (lvNow);
            // Kreator rusza dopiero przy otwartej karcie Z WEJSCIEM; do tego
            // czasu mowi, czego mu brakuje (a nie pusty panel).
            auto* dev = deviceManager.getCurrentAudioDevice();
            const bool haveInput = recorder.currentSampleRate() > 0.0 && dev != nullptr
                                && ! dev->getActiveInputChannels().isZero();
            if (! wizardStarted) wizard->setWaitingForDevice (! haveInput);
            if (! wizardStarted && haveInput)
            {
                wizard->setWaitingForDevice (false);
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
        if (segMeter.isVisible()) segMeter.setLevel (lvNow);
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
            {
                takeEvents.push_back (e);
                if (prompter != nullptr) prompter->noteEvent (e);   // znaczki przy wersach
            }

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
        {
            const juce::String m (v.msg);
            lamp.setWaiting (m.startsWith ("Sprawdzam wej") || m.startsWith ("Nic nie s"));
        }



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
    juce::File recovered;                 ///< odzyskane po awarii, do zaproponowania
    double recoveredSeconds = 0.0;
    juce::String recoveredLine;           ///< ostatni czytany wers (dziennik nagrania)
    int recoveredProposals = 0;           ///< miejsca po R czekajace w montazu
    Recorder recorder;
    bool correctionMode = false, correctionWaitingStart = false, correctionFixedLength = false;
    double correctionFrom = 0.0, correctionTo = 0.0;
    juce::File correctionSource, correctionUndo, pendingReplacement;

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
    SegMeter segMeter;                    ///< miernik segmentowy w nagraniu z prompterem
    juce::TextButton recButton, repeatButton, folderButton, okButton, settingsButton, detailsButton, openButton;
    juce::Label hints;
    juce::String hintsFull;               ///< pelna podpowiedz; w waskim oknie bez ostatnich pozycji
    void fitHintsLabel() { hints.setText (zs::ScreenFrame::fitHints (hintsFull, hints.getFont(), hints.getWidth() - 8), juce::dontSendNotification); }
    juce::TextEditor nameInput, scriptInput;
    juce::ComboBox deviceBox, inputBox;   ///< karta i wejscie z mikrofonem
    bool noDevice = false;                ///< --preview-ui: karty nie otwieramy
    juce::TooltipWindow tooltips { this, 700 };   ///< podpowiedzi po najechaniu (bez tego setTooltip nic nie pokazuje)
    juce::Label heardLabel;               ///< "slysze wejscie 2" albo podpowiedz o ciszy
    juce::uint32 quietSinceMs = 0;        ///< od kiedy wejscie milczy (0 = slychac)
    static constexpr juce::uint32 quietHintMs = 6000;
    juce::TextButton prepareButton, preparationButton, musicButton, editScriptButton;
    bool preparation = true;
    bool scriptEditorOpen = false;
    zs::StepBar flow;
    juce::File previewFolder;   ///< tylko --preview-ui: nagrywanie do katalogu tymczasowego
    bool settingsOpen=false, detailsOpen=false;
    bool settingsReturnToReview=false, settingsReturnToPrompter=false;
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

    /** Ekran startowy: wybór rodzaju sesji, nazwy albo odsłuch ostatniego pliku. */
    void showStart (bool on)
    {
        if (on && (recorder.isRecording() || inWizard)) return;
        if (on && startScreen == nullptr)
        {
            startScreen = std::make_unique<StartComponent>();
            startScreen->onCheckEquipment = [this]
            {
                wizardReturnToStart = true;
                showStart (false);
                enterWizard();
            };
            startScreen->onStart = [this] (const juce::String& name)
            {
                startWithMusic = startScreen->musicChosen();
                startWithPrompter = startScreen->prompterChosen();
                backingFile = juce::File();
                scriptEditorOpen = false;
                nameInput.setText (name.isEmpty() ? PL ("Nowy odcinek") : name, juce::dontSendNotification);
                showStart (false);
                if (! settings.hardwareChecked())
                {
                    preparationAfterWizard = true;
                    enterWizard();
                }
                else { afterStartChoice(); resized(); repaint(); }
            };
            startScreen->onOpen = [this] (const juce::File& f) { showStart (false); openReview (f, false, true); };
            startScreen->onOpenOther = [this]
            {
                chooser = std::make_unique<juce::FileChooser> (PL ("Wybierz nagranie"), sessionFolder, "*.wav;*.flac;*.aiff");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [safe = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& fc)
                    {
                        if (safe && fc.getResult().existsAsFile()) { safe->showStart (false); safe->openReview (fc.getResult(), false, true); }
                    });
            };
            addChildComponent (*startScreen);
        }
        if (on && inPrompter) showPrompter (false);
        if (on && inReview) showReview (false);
        inStart = on;
        if (startScreen != nullptr)
        {
            if (on)
            {
                startScreen->setFolder (sessionFolder);
                startScreen->setRecordingName (juce::String());
                auto* dev = deviceManager.getCurrentAudioDevice();
                const int d = settings.daysSinceCheck();
                const bool checked = settings.hardwareChecked();
                startScreen->setEquipment (PL ("Mikrofon: ") + (dev != nullptr ? inputDeviceName() : PL ("nie wybrany"))
                    + (checked ? PL ("  ·  sprzęt sprawdzony ") + (d <= 0 ? juce::String (PL ("dzisiaj"))
                                                                      : juce::String (d) + PL (d == 1 ? " dzień temu" : " dni temu"))
                               : PL ("  ·  sprzęt niesprawdzony")),
                    checked && dev != nullptr);
                startScreen->setBounds (getLocalBounds());
            }
            startScreen->setVisible (on);
            if (on) { startScreen->toFront (true); startScreen->focusName(); }
        }
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
            hardwareCheckedThisSession = true;
            settings.setHardwareChecked (true, wizard->summaryLine());
            settings.setRoomCheck (wizard->roomCheck());
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
        if (std::exchange (wizardReturnToStart, false)) showStart (true);
        else if (std::exchange (preparationAfterWizard, false)) afterStartChoice();

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
    juce::TextButton forgetProfileButton;     ///< "Usun wzorzec" w Mikrofon i sprzet
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

    /** Plik muzyczny wybiera się dopiero w Przygotowaniu po wskazaniu tego trybu. */
    void chooseBacking()
    {
        chooser = std::make_unique<juce::FileChooser> (PL ("Wybierz podkład muzyczny"),
            backingFile.existsAsFile() ? backingFile : sessionFolder, "*.mp3;*.wav;*.flac;*.aif;*.aiff;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<MainComponent> (this)] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (safe == nullptr || ! f.existsAsFile()) return;
                juce::AudioFormatManager formats;
                formats.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
                if (reader == nullptr || reader->sampleRate <= 0.0)
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, PL ("Podkład"),
                        PL ("Nie umiem otworzyć „") + f.getFileName() + PL ("”. Wybierz WAV, MP3, FLAC albo AIFF."));
                    return;
                }
                safe->backingFile = f;
                safe->preloadBacking();
                safe->resized(); safe->repaint();
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
            prompter->onClose = [this] { if (correctionMode) cancelReplacement(); else showPrompter (false); };
            prompter->setSpeech (&speech);
            prompter->onStopRequest = [this] { if (recorder.isRecording()) toggleRecord(); else if (correctionMode) cancelReplacement(); };
            prompter->onStartStop = [this] { startStop(); };
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

            if (on) { zs::trace ("prompter: begin"); prompter->begin (recorder.currentSampleRate()); zs::trace ("prompter: po begin"); }
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
    /** Nagrania przerwane awaria (zostal znacznik ".nagrywanie"): naprawa
        naglowka (WavRepair.h) i propozycja otwarcia najnowszego. */
    void recoverInterrupted()
    {
        if (! sessionFolder.isDirectory()) return;
        double secs = 0.0;
        for (const auto& marker : sessionFolder.findChildFiles (juce::File::findFiles, true, "*.wav.nagrywanie"))
        {
            const auto wav = marker.getSiblingFile (marker.getFileNameWithoutExtension());
            const auto r = wav.existsAsFile() ? wavio::repairHeader (wav.getFullPathName().toStdString()) : wavio::RepairResult {};
            marker.deleteFile();
            if (! r.ok || r.seconds < 0.5) continue;
            append (PL ("--- odzyskane nagranie: ") + wav.getFileName() + " (" + juce::String (r.seconds, 1) + " s)");
            // nagranie z tekstem: R i miejsce w tekscie z dziennika (audyt 14.10)
            const auto j = zs::recoverJournal (wav);
            if (j.error.isNotEmpty()) append (PL ("--- dziennik nagrania: ") + j.error);
            if (recovered == juce::File() || wav.getLastModificationTime() > recovered.getLastModificationTime())
            { recovered = wav; secs = r.seconds; recoveredLine = j.lastLine; recoveredProposals = j.proposals; }
        }
        recoveredSeconds = secs;
    }

    void offerRecovered()
    {
        const auto wav = recovered;
        recovered = juce::File();
        const int s = (int) recoveredSeconds;
        juce::AlertWindow::showOkCancelBox (juce::AlertWindow::QuestionIcon, PL ("Odzyskane nagranie"),
            PL ("Program zamknął się w trakcie nagrania. Odzyskałem nagranie „") + wav.getFileNameWithoutExtension()
              + PL ("” (") + juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2)
              + PL (") — leży w folderze nagrań.")
              + (recoveredLine.isNotEmpty() ? PL ("\n\nCzytanie urwało się na wersie: „") + recoveredLine + PL ("”.") : juce::String())
              + (recoveredProposals > 0 ? PL ("\nMiejsca po R (") + juce::String (recoveredProposals)
                                          + PL (") czekają na decyzję w montażu.") : juce::String())
              + PL ("\n\nOtworzyć je teraz do odsłuchu?"),
            PL ("Otwórz"), PL ("Później"), this,
            juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<MainComponent> (this), wav] (int result)
            {
                if (result == 1 && safe != nullptr && wav.existsAsFile()) safe->openReview (wav, false, true);
            }));
    }

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
            review->onClose = [this] { zs::trace ("Gotowe: nowe podejscie"); closeReview(); zs::trace ("Gotowe: z powrotem na nagraniu"); };
            review->onReplaceSelection = [this] (double from, double to, const std::vector<float>& audio, double rate)
            { beginReplacement (from, to, audio, rate); };
            review->onUndoReplacement = [this] { undoReplacement(); };
            // "Nowy projekt" z ekranu Gotowe (Adrian 25.10): porzucic projekt i zaczac inny
            review->onNewProject = [this]
            {
                zs::trace ("Gotowe: nowy projekt");
                closeReview();
                if (inPrompter) showPrompter (false);
                preparation = true; settingsOpen = false; detailsOpen = false;
                showStart (true);
                zs::trace ("Gotowe: ekran startowy");
            };
            review->onGoToPreparation = [this]
            {
                closeReview();
                if (preparationButton.onClick) preparationButton.onClick();
            };
            review->onGoToStart = [this] { showStart (true); };
            review->onOpenSettings = [this] {
                settingsReturnToReview = true;
                showReview (false);
                settingsOpen = true;
                refreshHomeVisibility(); resized(); repaint();
            };
            addAndMakeVisible (*review);
        }

        preparation = false;
        reviewCameFromPrompter = inPrompter;

        if (inPrompter)
            showPrompter (false);

        review->setRecordingDamaged((!fromDisk && recorder.recordingDamaged()) || wavFile.getSiblingFile(wavFile.getFileName()+".problem.txt").existsAsFile());
        review->setRoom (settings.roomCheck());
        review->setVoiceProfile (mix::VoiceProfile::fromText (settings.voiceProfile().toStdString()));
        review->onRememberProfile = [this] (const juce::String& t) { settings.setVoiceProfile (t); repaint(); };
        review->load (wavFile, takeEvents, ! fromDisk, waitForIt);
        showReview (true);
    }

    void closeReview()
    {
        zs::trace ("closeReview: chowam odsluch");
        showReview (false);
        zs::trace ("closeReview: odsluch schowany");

        // Wracamy tam, skad przyszlismy. Czlowiek, ktory czytal z promptera,
        // chce czytac dalej od tego samego miejsca — a nie szukac przycisku
        // promptera po raz drugi.
        if (reviewCameFromPrompter && prompter != nullptr)
        {
            showPrompter (true);
            zs::trace ("closeReview: prompter pokazany");
        }
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
    std::unique_ptr<StartComponent> startScreen;
    juce::File backingFile;               ///< podklad muzyczny projektu (pusty = sam glos)
    bool previewBackingEnabled = false;
    juce::String previewBackingName;
    double previewBackingDuration = 0.0, previewBackingPosition = 0.0;
    juce::String loadedBackingKey;        ///< sciezka@czestotliwosc tego, co siedzi w Recorderze
    std::vector<float> backingEnvelope;   ///< obwiednia promptera, liczona poza callbackiem
    bool inStart = false, preparationAfterWizard = false, wizardReturnToStart = false, hardwareCheckedThisSession = false;
    bool startWithPrompter = true, startWithMusic = false;

    /** Nazwa urzadzenia MIKROFONU. AudioIODevice::getName() na Windows (WASAPI,
        DirectSound) to nazwa wyjscia, gdy wejscie i wyjscie sa rozne — Adrian
        25.10 widzial "FxSound" (domyslne sluchawki) zamiast SSL2. */
    juce::String inputDeviceName()
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup (setup);
        if (setup.inputDeviceName.isNotEmpty()) return setup.inputDeviceName;
        auto* dev = deviceManager.getCurrentAudioDevice();
        return dev != nullptr ? dev->getName() : juce::String();
    }

    /** Każdy nowy projekt przechodzi przez Przygotowanie. */
    void afterStartChoice()
    {
        settingsOpen = false; detailsOpen = false;
        preparation = true;
        preparationButton.setVisible (false);
        refreshHomeVisibility(); resized(); repaint();
    }

    /** Podklad wczytywany w tle zaraz po Starcie, nie po Nagraj: dekodowanie
        MP3 i przeliczenie czestotliwosci 3-4 minut muzyki to kilka sekund,
        ktore zamrazaly okno w chwili nagrania (Adrian 25.10). */
    struct BackingPreload
    {
        juce::String key, error;
        zs::BackingTrack track;
        std::atomic<bool> done { false };
    };
    std::shared_ptr<BackingPreload> backingPreload;
    juce::ThreadPool backingPool { 1 };

    void preloadBacking()
    {
        const double rate = recorder.currentSampleRate();
        if (backingFile == juce::File() || rate <= 0.0) return;
        const auto key = backingFile.getFullPathName() + "@" + juce::String (rate);
        if ((key == loadedBackingKey && recorder.hasBacking()) || (backingPreload != nullptr && backingPreload->key == key)) return;
        auto job = std::make_shared<BackingPreload>();
        job->key = key;
        backingPreload = job;
        backingPool.addJob ([job, file = backingFile, rate]
        {
            job->error = job->track.load (file, rate);
            job->done.store (true);
        });
    }
    bool inReview = false, reviewCameFromPrompter = false;
    bool closeAsked = false;              ///< okno "czy zamknac" juz pokazane
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
