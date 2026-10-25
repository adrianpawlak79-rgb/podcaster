/*
    WizardComponent.h — ekran kreatora sprawdzenia sprzetu.

    To jest ten tryb programu, w ktorym czlowiek MOZE czytac: nie mowi jeszcze
    do mikrofonu, wiec ma uwage na tekst. Dlatego ten ekran wyglada zupelnie
    inaczej niz ekran nagrywania — duzo slow, jeden krok naraz, duze przyciski.

    Zasada: kazdy krok konczy sie zdaniem, co zrobic, i LICZBA, ktora o tym
    zdecydowala. Liczba jest mala i szara, ale jest — zeby dalo sie sprawdzic
    program, a nie tylko mu wierzyc.
*/

#pragma once

#include "Recorder.h"
#include "Look.h"
#include "StepBar.h"
#include "ScreenFrame.h"
#include "DSP/Setup.h"
#include "DSP/Mix/RoomCheck.h"

//==============================================================================
class WizardComponent : public juce::Component,
                        private juce::Timer
{
public:
    /** Wolane, gdy kreator dobiegnie konca. */
    std::function<void()> onFinished;

    /** Czy kreator ZOSTAL PRZEJSCIE DO KONCA, a nie pominiety.

        Rozroznienie nie jest kosmetyczne. Program zapamietuje, ze sprzet jest
        sprawdzony, i przy nastepnym uruchomieniu nie pyta drugi raz. Gdyby
        pominiecie tez sie zapisywalo, wystarczyloby raz kliknac "pomin", zeby
        kreator nigdy wiecej nie wrocil — a wszystkie dalsze decyzje programu
        opieraly sie na pomiarach, ktorych nikt nie zrobil. */
    bool completed() const noexcept { return finishedProperly; }
    bool isDeviceStep() const noexcept { return wizard.step() == tw::SetupStep::Device && ! judged; }
    juce::Rectangle<int> deviceControlsArea() const
    {
        auto c = card().reduced (32, 28);
        c.removeFromTop (18 + 6 + 76 + 8 + 4 + 8);
        return c.removeFromBottom (86);
    }

    /** Jedno zdanie z liczbami, ktore kreator zmierzyl. Do zapisania na
        pozniej: po miesiacu czlowiek chce wiedziec, CO wtedy wyszlo, a nie
        tylko ze "bylo sprawdzane". */
    /** Pomiary pokoju dla miksu: przydzwiek, odleglosc, tlo. Miks sprawdza
        je jeszcze w nagraniu — kreator mowi, czego szukac. */
    mix::RoomCheck roomCheck() const
    {
        const auto& rep = wizard.result();

        mix::RoomCheck r;
        r.known            = true;
        r.noiseFloorDb     = rep.noiseFloorDb;
        r.humHz            = rep.humHz;
        r.humExcessDb      = rep.humExcessDb;
        r.proximityDb      = rep.proximityDb;
        r.proximityLimitDb = wizard.limits.proximityDb;
        r.bleedPercent     = rep.bleedPercent;
        r.bleedLimit       = wizard.limits.bleedPercent;
        r.daysAgo          = 0;
        return r;
    }

    juce::String summaryLine() const
    {
        const auto& rep = wizard.result();

        juce::String s;
        s << PL ("tło ") << juce::String (rep.noiseFloorDb, 1) << " dBFS";

        if (rep.speechPeakDb > -130.0)
            s << PL (", szczyty ") << juce::String (rep.speechPeakDb, 1) << " dBFS";

        if (rep.humHz > 0.0)
            s << PL (", przydźwięk ") << juce::String ((int) rep.humHz) << " Hz";

        return s;
    }

    WizardComponent (Recorder& r) : recorder (r)
    {
        // Pasek krokow zamiast szesciu kropek na dole: kropki mialy 9 pikseli
        // i nie mowily, co jest w kolejnym kroku — nazwa mowi.
        steps.setSteps ({ PL ("Mikrofon"), PL ("Cisza w pokoju"), PL ("Poziom"),
                          PL ("Głoski p i b"), PL ("Słuchawki"), PL ("Odsłuch") });
        addAndMakeVisible (steps);
        addAndMakeVisible (flow);
        flow.setSteps ({ PL("Ekran startowy"), PL("Przygotowanie"), PL("Nagranie"),
                         PL("Odsłuch i montaż"), PL("Miks"), PL("Gotowe") });
        flow.setTileMode();
        flow.setCurrent (1);

        title.setFont (juce::Font (zs::type::heading + 3.0f, juce::Font::bold));   // zdanie, nie nazwa: trzy wiersze w 76 px
        title.setColour (juce::Label::textColourId, zs::Ink::textNow());
        title.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (title);

        counter.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
        counter.setColour (juce::Label::textColourId, zs::Ink::accent());
        addAndMakeVisible (counter);

        body.setFont (juce::Font (zs::type::body + 1.0f));
        body.setColour (juce::Label::textColourId, zs::Ink::textNext());
        body.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (body);

        measured.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), zs::type::small, juce::Font::plain));
        measured.setColour (juce::Label::textColourId, zs::Ink::textDim());
        addAndMakeVisible (measured);
        cumulative.setFont (juce::Font (zs::type::body));
        cumulative.setColour (juce::Label::textColourId, zs::Ink::textNext());
        cumulative.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (cumulative);

        nextButton.setButtonText ("Dalej");
        zs::Look::setKind (nextButton, zs::ui::ButtonKind::Primary);
        zs::Look::setKey (nextButton, "Enter");
        nextButton.onClick = [this] { goNext(); };
        addAndMakeVisible (nextButton);

        againButton.setButtonText (PL ("Jeszcze raz"));
        againButton.onClick = [this] { again(); };
        addAndMakeVisible (againButton);

        skipButton.setButtonText (PL ("Pomiń sprawdzanie"));
        skipButton.onClick = [this] { if (onFinished) onFinished(); };
        addAndMakeVisible (skipButton);

        setWantsKeyboardFocus (true);
        startTimerHz (20);
    }

    ~WizardComponent() override
    {
        recorder.attachWizard (nullptr);
        recorder.setTestTone (false);
        recorder.setCalibrationPlayback (false);
    }

    void begin (double sampleRate)
    {
        wizard.begin (sampleRate);
        judged = false;
        recorder.attachWizard (&wizard);
        updateTexts();

        if (isShowing()) grabKeyboardFocus();
    }

    /** Zanim ruszy pomiar: nie ma karty albo wejscia. Dawniej pusty panel i
        aktywne "Dalej" — poczatkujacy, ktory zapomnial podlaczyc mikrofon, nie
        wiedzial, na co program czeka (sprawdzone 19.10 bez karty). */
    void setWaitingForDevice (bool waiting)
    {
        if (waiting == waitingForDevice) return;
        waitingForDevice = waiting;
        if (waiting)
        {
            title.setText (PL ("Nie widzę mikrofonu"), juce::dontSendNotification);
            body.setText (PL ("Podłącz mikrofon albo kartę dźwiękową. Wybierz kartę i wejście poniżej, powiedz kilka słów i sprawdź poziom sygnału. W razie potrzeby wróć i otwórz „Ustawienia”.\n\n")
                        + PL ("Na Windows sprawdź też: Ustawienia → Prywatność i zabezpieczenia → Mikrofon (dostęp dla aplikacji)."),
                          juce::dontSendNotification);
            measured.setText ({}, juce::dontSendNotification);
        }
        else
        {
            title.setText ({}, juce::dontSendNotification);
            body.setText ({}, juce::dontSendNotification);
        }
        nextButton.setEnabled (! waiting && judged);
        againButton.setEnabled (! waiting && judged);
        repaint();
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (zs::Ink::bg());

        zs::ScreenFrame::paint (g, getLocalBounds(), PL("Sprawdzenie sprzętu"), PL("Przygotuj mikrofon przed nagraniem"));
        zs::ScreenFrame::paintPanel (g, zs::ScreenFrame::sidebar (getLocalBounds()));
        g.setColour (zs::Ink::textNow()); g.setFont(juce::Font(zs::type::title,juce::Font::bold));
        g.drawText(PL("Sprawdzenie sprzętu"),header(),juce::Justification::centredLeft);
        g.setColour (zs::Ink::panel()); g.fillRoundedRectangle(card().toFloat(),6.0f);
        g.setColour (zs::Ink::edge()); g.drawRoundedRectangle(card().toFloat(),6.0f,1.0f);
        auto side = zs::ScreenFrame::sidebar (getLocalBounds()).reduced (22, 24);
        g.setColour (zs::Ink::textNow()); g.setFont (juce::Font (zs::type::heading, juce::Font::bold));
        g.drawText (PL("Wyniki pomiarów"), side.removeFromTop (32), juce::Justification::centredLeft);

        // ---- pasek postepu pomiaru albo kreska werdyktu ---------------------
        auto bar = progressBar();

        if (! judged)
        {
            g.setColour (zs::Ink::edge());
            g.fillRoundedRectangle (bar.toFloat(), 2.0f);

            g.setColour (zs::Ink::accent());
            g.fillRoundedRectangle (bar.withWidth ((int) (bar.getWidth() * wizard.progress())).toFloat(), 2.0f);
        }
        else
        {
            g.setColour (finding.passed ? zs::Ink::green() : zs::Ink::yellow());
            g.fillRoundedRectangle (bar.toFloat(), 2.0f);
        }
    }

    void resized() override
    {
        flow.setBounds (zs::ScreenFrame::stepsArea (getLocalBounds()));
        auto side = zs::ScreenFrame::sidebar(getLocalBounds()).reduced(22,24);
        auto results = side.withTrimmedBottom (170);
        results.removeFromTop (32);
        cumulative.setBounds (results);
        auto stepArea = zs::ScreenFrame::mainArea(getLocalBounds());
        stepArea.removeFromTop(48);
        steps.setBounds(stepArea.removeFromTop(zs::StepBar::preferredHeight));
        skipButton.setBounds(side.removeFromBottom(38)); side.removeFromBottom(10);
        againButton.setBounds(side.removeFromBottom(38)); side.removeFromBottom(10);
        nextButton.setBounds(side.removeFromBottom(56));
        auto c = card().reduced (32, 28);

        counter.setBounds (c.removeFromTop (18));
        c.removeFromTop (6);
        title.setBounds (c.removeFromTop (76));
        c.removeFromTop (8 + 4 + 18);      // pasek postepu rysowany w paint()

        c.removeFromBottom (16);

        measured.setBounds (c.removeFromBottom (20));
        c.removeFromBottom (8);
        if (isDeviceStep()) c.removeFromBottom (94);
        body.setBounds (c);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.getKeyCode() == juce::KeyPress::returnKey && nextButton.isEnabled())
        {
            nextButton.triggerClick();
            return true;
        }

        return false;
    }

private:
    //==========================================================================
    juce::String measuredSummary() const
    {
        const auto& result = wizard.result();
        const int step = (int) wizard.step();
        // Nie wyswietlamy wartosci domyslnych jako pomiaru. Wynik z biezacego
        // kroku jest gotowy dopiero po evaluate(), poprzednie juz sa zapisane.
        const auto measuredStep = [step, this] (tw::SetupStep s) {
            return step > (int) s || (step == (int) s && judged);
        };
        juce::String summary;
        if (measuredStep (tw::SetupStep::Room)) {
            summary << PL("Tło pokoju\n") << juce::String (result.noiseFloorDb, 1) << " dBFS\n\n";
            if (result.humHz > 0.0) summary << PL("Przydźwięk\n") << juce::String ((int) result.humHz) << " Hz\n\n";
        }
        if (measuredStep (tw::SetupStep::Level))
            summary << PL("Szczyty mowy\n") << juce::String (result.speechPeakDb, 1) << " dBFS\n\n";
        if (measuredStep (tw::SetupStep::Bleed))
            summary << PL("Przesłuch słuchawek\n") << juce::String (result.bleedDb, 0) << " dBFS\n\n";
        return summary.isEmpty() ? PL("Tutaj pojawią się wyniki zakończonych pomiarów tła pokoju i głosu.") : summary.trimEnd();
    }

    juce::Rectangle<int> header() const
    {
        return zs::ScreenFrame::mainArea(getLocalBounds()).removeFromTop(38);
    }

    juce::Rectangle<int> card() const
    {
        auto r = zs::ScreenFrame::mainArea(getLocalBounds());
        r.removeFromTop(114);
        return r;
    }

    juce::Rectangle<int> progressBar() const
    {
        auto c = card().reduced (32, 28);
        c.removeFromTop (18 + 6 + 76 + 8);
        return c.removeFromTop (4);
    }

    void timerCallback() override
    {
        if (wizard.step() == tw::SetupStep::Done)
            return;

        // ton probny gra tylko w kroku o przesluchu
        recorder.setTestTone (wizard.step() == tw::SetupStep::Bleed && ! judged);

        // w kroku kalibracji gra wlasny glos czlowieka, w petli
        const bool calibrating = wizard.step() == tw::SetupStep::Monitor && ! judged;

        if (calibrating && ! calibArmed)
        {
            recorder.setCalibrationAudio (wizard.calibrationAudio());
            calibArmed = true;
        }

        recorder.setCalibrationPlayback (calibrating);

        if (! judged && wizard.stepComplete())
        {
            recorder.attachWizard(nullptr);
            finding = wizard.evaluate();
            judged = true;
            recorder.setTestTone (false);
            recorder.setCalibrationPlayback (false);
            updateTexts();
        }

        repaint();
    }

    void goNext()
    {
        if (! judged)
            return;

        recorder.attachWizard(nullptr);
        wizard.advance();
        judged = false;
        calibArmed = false;

        if (wizard.step() == tw::SetupStep::Done)
        {
            recorder.attachWizard (nullptr);
            showSummary();
            return;
        }

        recorder.attachWizard(&wizard);
        updateTexts();
    }

    void again()
    {
        recorder.attachWizard(nullptr);
        wizard.repeat();
        recorder.attachWizard(&wizard);
        judged = false;
        calibArmed = false;
        updateTexts();
    }

    //==========================================================================
    void updateTexts()
    {
        cumulative.setText (measuredSummary(), juce::dontSendNotification);
        const int n = (int) wizard.step() + 1;
        counter.setText (PL ("KROK ") + juce::String (n) + PL (" Z 6"), juce::dontSendNotification);
        steps.setCurrent (n - 1);

        if (! judged)
        {
            title.setText (PL (tw::SetupWizard::instruction (wizard.step())),
                           juce::dontSendNotification);
            body.setText (PL (waitingHint (wizard.step())), juce::dontSendNotification);
            measured.setText ({}, juce::dontSendNotification);
        }
        else
        {
            title.setText (PL (finding.headline.c_str()), juce::dontSendNotification);
            body.setText (PL (finding.detail.c_str()), juce::dontSendNotification);
            measured.setText (PL (finding.measured.c_str()), juce::dontSendNotification);
        }

        nextButton.setEnabled (judged);
        nextButton.setButtonText (judged && ! finding.passed ? PL ("Kontynuuj mimo ostrzeżenia") : "Dalej");
        againButton.setEnabled (judged);
        repaint();
    }

    static const char* waitingHint (tw::SetupStep s)
    {
        switch (s)
        {
            case tw::SetupStep::Device:
                return "Cokolwiek. „Raz, dwa, trzy” wystarczy — sprawdzam tylko, "
                       "czy sygnał w ogóle dociera i z którego wejścia.";
            case tw::SetupStep::Room:
                return "Nie oddychaj w mikrofon, nie przesuwaj krzesła. Mierzę, jak cicho "
                       "jest w pokoju. Im mniej szumu trafi do nagrania, tym mniej trzeba będzie usuwać.";
            case tw::SetupStep::Level:
                return "Mów tak, jak będziesz mówić w nagraniu — nie ciszej „na próbę”. "
                       "Sprawdzam, czy wzmocnienie pasuje do Twojego głosu.";
            case tw::SetupStep::Plosive:
                return "Powiedz to normalnie, nie przesadzając z „p”. Sprawdzam, czy "
                       "powietrze uderza w membranę.";
            case tw::SetupStep::Bleed:
                return "Przez chwilę usłyszysz w słuchawkach cichy dźwięk. Milcz i nie "
                       "zdejmuj ich — sprawdzam, ile z tego wraca do mikrofonu.";
            case tw::SetupStep::Monitor:
                return "Usłyszysz swoje dwa zdania sprzed chwili. Kręć głośnością, aż "
                       "będą brzmiały tak głośno, jak sam mówiłeś — nie ciszej, nie "
                       "głośniej. To jedyna głośność, którą znasz na pamięć, więc to "
                       "ona jest tu miarą.";
            default:
                return "";
        }
    }

    void showSummary()
    {
        const auto& rep = wizard.result();

        cumulative.setText (measuredSummary(), juce::dontSendNotification);
        counter.setText (PL ("GOTOWE"), juce::dontSendNotification);
        steps.setCurrent (6, true);
        title.setText (rep.ready ? PL ("Możesz nagrywać.")
                                 : PL ("Możesz nagrywać, ale nie wszystko jest w porządku."),
                       juce::dontSendNotification);

        juce::String s;
        s << PL ("Tło pokoju ") << juce::String (rep.noiseFloorDb, 1) << " dBFS";

        if (rep.speechPeakDb > -130.0)
            s << PL (", szczyty mowy ") << juce::String (rep.speechPeakDb, 1) << " dBFS";

        if (rep.humHz > 0.0)
            s << PL (", przydźwięk ") << juce::String ((int) rep.humHz) << " Hz";

        s << ".\n\n";

        if (rep.ready)
            s << PL ("Nie zmieniaj już wzmocnienia ani odległości od mikrofonu — "
                     "te ustawienia są teraz punktem odniesienia dla wszystkiego, "
                     "co program robi dalej.");
        else
            s << PL ("Kroki z ostrzeżeniem możesz powtórzyć w każdej chwili. "
                     "Program będzie nagrywał tak czy inaczej — po prostu wie, "
                     "z czym ma do czynienia.");

        body.setText (s, juce::dontSendNotification);
        measured.setText ({}, juce::dontSendNotification);

        nextButton.setButtonText (PL ("Zaczynamy"));
        nextButton.setEnabled (true);
        nextButton.onClick = [this] { finishedProperly = true; if (onFinished) onFinished(); };
        againButton.setEnabled (false);
        skipButton.setVisible (false);

        repaint();
    }

    //==========================================================================
    Recorder& recorder;
    tw::SetupWizard wizard;
    tw::SetupFinding finding;
    bool judged = false;
    bool waitingForDevice = false;
    bool calibArmed = false;
    bool finishedProperly = false;

    zs::StepBar steps, flow;
    juce::Label title, counter, body, measured, cumulative;
    juce::TextButton nextButton, againButton, skipButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WizardComponent)
};
