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
#include "Ink.h"
#include "DSP/Setup.h"

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

    /** Jedno zdanie z liczbami, ktore kreator zmierzyl. Do zapisania na
        pozniej: po miesiacu czlowiek chce wiedziec, CO wtedy wyszlo, a nie
        tylko ze "bylo sprawdzane". */
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
        title.setFont (juce::Font (23.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, zs::Ink::textNow());
        addAndMakeVisible (title);

        counter.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
        counter.setColour (juce::Label::textColourId, zs::Ink::textNext());
        addAndMakeVisible (counter);

        body.setFont (juce::Font (16.0f));
        body.setColour (juce::Label::textColourId, zs::Ink::textNext());
        body.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (body);

        measured.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain));
        measured.setColour (juce::Label::textColourId, zs::Ink::textNext());
        addAndMakeVisible (measured);

        nextButton.setButtonText ("Dalej");
        nextButton.onClick = [this] { goNext(); };
        addAndMakeVisible (nextButton);

        againButton.setButtonText (PL ("Jeszcze raz"));
        againButton.onClick = [this] { again(); };
        addAndMakeVisible (againButton);

        skipButton.setButtonText (PL ("Pomiń sprawdzanie"));
        skipButton.onClick = [this] { if (onFinished) onFinished(); };
        addAndMakeVisible (skipButton);

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
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (zs::Ink::bg());

        auto r = getLocalBounds().reduced (28);
        r.removeFromTop (150);

        // ---- pasek postepu albo kropka werdyktu -----------------------------
        auto bar = r.removeFromTop (10);

        if (! judged)
        {
            g.setColour (zs::Ink::edge());
            g.fillRect (bar);

            g.setColour (zs::Ink::green());
            g.fillRect (bar.withWidth ((int) (bar.getWidth() * wizard.progress())));
        }
        else
        {
            g.setColour (finding.passed ? zs::Ink::green() : zs::Ink::yellow());
            g.fillRect (bar.withHeight (3));
        }

        // ---- kroki na dole ---------------------------------------------------
        auto dots = getLocalBounds().reduced (28).removeFromBottom (18);
        const int total = 6;
        const int done = (int) wizard.step();

        for (int i = 0; i < total; ++i)
        {
            auto d = juce::Rectangle<int> (dots.getX() + i * 18, dots.getY() + 5, 9, 9);

            g.setColour (i < done ? zs::Ink::green()
                       : i == done ? zs::Ink::textNext()
                                   : zs::Ink::edge());
            g.fillEllipse (d.toFloat());
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (28);

        counter.setBounds (r.removeFromTop (18));
        r.removeFromTop (6);
        title.setBounds (r.removeFromTop (78));
        r.removeFromTop (10);
        r.removeFromTop (10);        // miejsce na pasek rysowany w paint()
        r.removeFromTop (18);

        body.setBounds (r.removeFromTop (110));
        r.removeFromTop (8);
        measured.setBounds (r.removeFromTop (20));

        auto row = r.removeFromBottom (38);
        row.removeFromBottom (4);

        nextButton.setBounds (row.removeFromLeft (140));
        row.removeFromLeft (8);
        againButton.setBounds (row.removeFromLeft (140));
        skipButton.setBounds (row.removeFromRight (170));
    }

private:
    //==========================================================================
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
        const int n = (int) wizard.step() + 1;
        counter.setText (PL ("KROK ") + juce::String (n) + PL (" Z 6"), juce::dontSendNotification);

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

        counter.setText (PL ("GOTOWE"), juce::dontSendNotification);
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
    bool calibArmed = false;
    bool finishedProperly = false;

    juce::Label title, counter, body, measured;
    juce::TextButton nextButton, againButton, skipButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WizardComponent)
};
