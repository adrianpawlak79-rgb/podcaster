/*
    PrompterComponent.h — ekran do czytania.

    ---------------------------------------------------------------------------
    DLA KOGO TO JEST

    Dla czlowieka, ktory w tej samej chwili MOWI. To jedno zdanie tlumaczy
    kazda decyzje na tym ekranie. Ktos, kto mowi, nie ma uwagi na nic innego:
    nie przeczyta podpowiedzi, nie znajdzie przycisku, nie domysli sie ikony.
    Wszystko, co nie sluzy czytaniu, jest przeszkoda — nawet jesli jest ladne.

    ---------------------------------------------------------------------------
    CELOWNIK, CZYLI DLACZEGO TEKST JEDZIE, A NIE OKO

    Wers czytany stoi zawsze w tym samym miejscu ekranu, a tekst przesuwa sie
    pod nim. Odwrotnie byloby prosciej zaprogramowac i gorzej czytac: oko
    musialoby szukac, gdzie teraz jest. Tak ma sie nauczyc jednego miejsca i
    juz go nie opuszczac.

    ---------------------------------------------------------------------------
    PODSWIETLAMY WERS, NIE SLOWO

    Program wie, gdzie jestes, z dokladnoscia do jednego slowa przez 96% czasu
    (zmierzone na mowie o znanych czasach: 41 slow, mediana bledu 0 slow,
    najgorzej 2). Podswietlenie konkretnego slowa co kilkanascie sekund stoi
    wiec widocznie obok — a to gorsze niz brak podswietlenia, bo czlowiek
    zauwaza pomylke i przestaje ufac. Caly wers znosi ten blad bez mrugniecia.

    ---------------------------------------------------------------------------
    DWIE NIEZALEZNE RZECZY

    Lampka mowi, czy NAGRANIE sie nadaje. Plakietka przy celowniku mowi, czy
    PROMPTER wie, gdzie jest. To sa rozne pytania i moga wypasc w kazdej
    kombinacji: mozna czytac plynnie i przesterowywac naraz. Dlatego lampka
    jest duza i kolorowa, a plakietka mala i szara — nagranie da sie zepsuc
    nieodwracalnie, pozycje w tekscie poprawia sie strzalka w sekunde.
*/

#pragma once

#include "DSP/Follower.h"
#include "DSP/LineLog.h"
#include "DSP/PreRoll.h"
#include "DSP/LineGrade.h"
#include "PunchFile.h"
#include "DSP/PrompterLines.h"
#include "DSP/PrompterText.h"
#include "DSP/SyllableClock.h"
#include "DSP/WordTracker.h"
#include "Recorder.h"
#include "Settings.h"
#include "Look.h"
#include "VoskEngine.h"

//==============================================================================
class PrompterComponent : public juce::Component,
                          private juce::Timer
{
public:
    //==========================================================================
    /* Paleta stoi w Ink.h — wspolna dla wszystkich ciemnych ekranow. */
    using Ink = zs::Ink;

    //==========================================================================
    /** Wolane, gdy czlowiek chce wrocic do ekranu z lampka (Escape).

        Bez tego prompter byl pulapka: wchodzilo sie do niego przyciskiem, a
        wychodzilo zamknieciem programu. */
    std::function<void()> onClose;

    /** Esc w trakcie nagrywania: zatrzymaj i ocen. Zatrzymanie robi ekran
        glowny, bo to on trzyma nagranie i ocene. */
    std::function<void()> onStopRequest;

    /** Narzedzia promptera pod tekstem; nagrywanie i lampka sa w panelu bocznym. */
    static constexpr int barHeight = 52;

    //==========================================================================
    PrompterComponent (Recorder& r, Settings& s) : recorder (r), settings (s)
    {
        fontScale = (float) settings.fontScale();
        mirror    = settings.mirrored();

        backButton.setButtonText(PL("Wróć"));
        backButton.onClick=[this] { if(!recorder.isRecording() && onClose)onClose(); };
        smallerButton.setButtonText(PL("A−"));smallerButton.onClick=[this]{zoom(-.1f);};
        largerButton.setButtonText("A+");largerButton.onClick=[this]{zoom(.1f);};
        previousButton.setButtonText(PL("← Cofnij tekst"));previousButton.onClick=[this]{nudge(-4);};
        nextButton.setButtonText(PL("Dalej →"));nextButton.onClick=[this]{nudge(4);};
        mirrorButton.setButtonText(PL("Lustro"));mirrorButton.onClick=[this]{mirror=!mirror;settings.setMirrored(mirror);repaint();};
        for(auto* b:{&backButton,&smallerButton,&largerButton,&previousButton,&nextButton,&mirrorButton})addAndMakeVisible(b);
        setWantsKeyboardFocus (true);
        startTimerHz (30);
    }

    ~PrompterComponent() override
    {
        recorder.attachSyllableClock (nullptr);
        recorder.attachSpeechRing (nullptr);
    }

    /** Rozpoznawanie mowy. Bez niego (nullptr albo brak modelu) prompter
        jedzie na liczniku sylab — tak jak w 0.4.0. */
    void setSpeech (VoskEngine* engine) { speech = engine; }

    //==========================================================================
    void load (const juce::String& utf8)
    {
        script.set (utf8.toStdString());
        follower.begin (&script);
        words.begin (&script);
        scrollNow = scrollTarget = 0.0f;
        shownLine = 0;
        layoutDirty = true;
        repaint();
    }

    bool hasText() const noexcept { return script.wordCount() > 0; }

    void begin (double sampleRate)
    {
        recorder.attachSyllableClock(nullptr);
        clock.prepare (sampleRate);
        clock.reset();
        follower.begin (&script);

        recorder.attachSyllableClock (&clock);

        elapsed = 0.0;
        wasRecording = false;
        running = true;
        grabKeyboardFocus();
    }

    void end()
    {
        running = false;
        recorder.attachSyllableClock (nullptr);
        stopSpeech();
    }

    //==========================================================================
    /** Klawisze.

        Wszystkie dzialaja jedna reka i bez patrzenia, bo druga reka jest przy
        mikrofonie, a oczy w tekscie. Zadnego Ctrl, zadnych kombinacji. */
    bool keyPressed (const juce::KeyPress& k) override
    {
        const int c = k.getKeyCode();

        // Esc i R — patrz zs::ui::keyAction. W 0.4.0 Esc w trakcie nagrywania
        // nie robil nic, "bo to najczesciej pomylka palca", a nagranie
        // zatrzymywal tylko przycisk w rogu ekranu — czyli trzeba bylo oderwac
        // oczy od tekstu i siegnac po mysz. Teraz Esc zatrzymuje i od razu
        // ocenia. Pomylka palca nic nie niszczy: plik zostaje na dysku, a
        // "Nagrywaj dalej" wraca prosto do promptera.
        switch (zs::ui::keyAction (c == juce::KeyPress::escapeKey, (char32_t) k.getTextCharacter(),
                                   recorder.isRecording(), hasText()))
        {
            case zs::ui::KeyAction::StopAndReview: if (onStopRequest) onStopRequest(); return true;
            case zs::ui::KeyAction::RepeatLine:    repeatLine(); return true;
            case zs::ui::KeyAction::LeavePrompter: if (onClose) onClose(); return true;
            case zs::ui::KeyAction::None:          break;
        }

        if (c == juce::KeyPress::upKey)    { nudge (-4); return true; }
        if (c == juce::KeyPress::downKey)  { nudge (+4); return true; }
        if (c == juce::KeyPress::leftKey)  { nudge (-1); return true; }
        if (c == juce::KeyPress::rightKey) { nudge (+1); return true; }

        if (c == juce::KeyPress::homeKey)  { follower.jumpToWord (0); shownLine = 0;
                                             scrollNow = 0.0f; markRestart (0); repaint(); return true; }

        // powiekszanie tekstu: plus i minus, takze z klawiatury numerycznej
        if (k.getTextCharacter() == '+' || k.getTextCharacter() == '=') { zoom (+0.1f); return true; }
        if (k.getTextCharacter() == '-' || k.getTextCharacter() == '_') { zoom (-0.1f); return true; }

        // odbicie lustrzane do szybki promptera
        if (k.getTextCharacter() == 'l' || k.getTextCharacter() == 'L')
        {
            mirror = ! mirror;
            settings.setMirrored (mirror);
            repaint();
            return true;
        }

        return false;
    }

    /** Narzedzia wracaja po zatrzymaniu; klawisze dzialaja przez caly czas. */
    void refreshControls()
    {
        const bool visible = !recorder.isRecording();
        for (auto* b : { &backButton, &smallerButton, &largerButton, &previousButton, &nextButton, &mirrorButton })
            b->setVisible (visible);
    }

    /** "Powtorz wers" (R albo przycisk): na poczatek wersu, ktory jest w
        ustach, i czekamy na glos. Nagranie leci dalej w calosci; po
        zatrzymaniu nieudane podejscie wycina sie samo (punch and roll,
        finishTake → PunchFile.h), a pelne nagranie zostaje w "surowe".

        Tylko z rozpoznawaniem mowy (tw::retakeSource): bez niego prompter
        zbyt czesto nie wie, ktory wers jest czytany, i R tylko cofa tekst. */
    void repeatLine()
    {
        if (spans.empty()) return;

        const int from = tw::repeatLineStart (spans, follower.currentWord());
        follower.repeatFrom (from);
        words.jumpTo (from, recorder.recordedSeconds());
        lastHeard = -1;                       // od teraz slowa ida od poczatku wersu
        shownLine = tw::lineOfWord (spans, from);
        const auto why = tw::retakeSource (listening);
        markRestart (shownLine, why);
        if (why == tw::LineSource::Retake)
        {
            startPreRoll();
            if (onRetake) onRetake();
        }
        else if (recorder.isRecording())
        {
            noCutNoticeAt = juce::Time::getMillisecondCounterHiRes();
        }
        regrade();
        repaint();
    }

    /** Po R: w sluchawkach 2 s tego, co w czystym pliku bedzie przed tym
        wersem, i odliczanie do czytania (DSP/PreRoll.h). Gdy kreator zmierzyl,
        ze sluchawki uciekaja do mikrofonu — samo odliczanie. */
    void startPreRoll()
    {
        if (! recorder.isRecording()) return;

        double startSec = 0.0;
        tw::PreRollPlan plan;
        if (! settings.roomCheck().headphonesLeak())
        {
            const auto history = recorder.recentAudio (30.0, startSec);
            plan = tw::PreRoll().plan (lineLog.marks(), history, recorder.currentSampleRate(), startSec);
        }

        const double sec = (double) plan.audio.size() / juce::jmax (1.0, recorder.currentSampleRate());
        recorder.playPreRoll (std::move (plan.audio));
        countdownStart = juce::Time::getMillisecondCounterHiRes();
        countdownTotal = countdown.totalFor (sec);
    }

public:
    /** Zdarzenie lampki (MainComponent dostaje je z Recordera) — do znaczkow
        przy wersach. Tylko w trakcie nagrania. */
    void noteEvent (const tw::Event& e)
    {
        if (! recorder.isRecording()) return;
        liveEvents.push_back (e);
        regrade();
    }

    /** Tylko zrzut ekranu (--preview-ui): trzy przeczytane wersy, kazdy z
        innym znaczkiem, i odliczanie. */
    void previewMarks()
    {
        if (layoutDirty)                       // wersy powstaja przy pierwszym rysowaniu
        {
            juce::Image img (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
            juce::Graphics tmp (img);
            rebuildLines (tmp);
        }
        if (lines.empty()) return;

        const tw::LineGrade g[] = { tw::LineGrade::Good, tw::LineGrade::Fixable, tw::LineGrade::Redo };
        for (int i = 0; i < 3 && i < (int) lines.size(); ++i) grades[lines[(size_t) i].firstWord] = g[i];
        shownLine = juce::jmin (3, (int) lines.size() - 1);
        scrollNow = scrollTarget = (float) shownLine;
        follower.nudgeWords (lines[(size_t) shownLine].firstWord - follower.currentWord());
        countdownStart = juce::Time::getMillisecondCounterHiRes();
        countdownTotal = 2.0;
        repaint();
    }

    /** Po R: lampka nie trzyma koloru za wycieta pomylke. */
    std::function<void()> onRetake;

private:
    /** Znaczki ✓ / ! / ✗ (DSP/LineGrade.h). Po zatrzymaniu zostaja na
        ekranie do nastepnego nagrania; ostatni wers tez dostaje swoj. */
    void regrade()
    {
        if (lineLog.marks().empty()) return;     // po finishTake: zostaja ostatnie znaczki
        grades = tw::LineGrader().grade (lineLog.marks(), liveEvents, ! recorder.isRecording());
    }

    void drawGrade (juce::Graphics& g, tw::LineGrade grade, juce::Rectangle<float> box)
    {
        if (grade == tw::LineGrade::None) return;

        // Rysowane, nie z czcionki: znaki ✓ i ✗ nie w kazdej czcionce systemu sa.
        const auto c = box.getCentre();
        const float h = juce::jmin (box.getWidth(), box.getHeight()) * 0.5f;
        const auto col = grade == tw::LineGrade::Good ? Ink::green()
                       : grade == tw::LineGrade::Fixable ? Ink::yellow() : Ink::red();
        g.setColour (col);

        juce::Path p;
        if (grade == tw::LineGrade::Good)
        {
            p.startNewSubPath (c.x - h * 0.8f, c.y);
            p.lineTo (c.x - h * 0.2f, c.y + h * 0.6f);
            p.lineTo (c.x + h * 0.9f, c.y - h * 0.7f);
            g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (grade == tw::LineGrade::Redo)
        {
            p.startNewSubPath (c.x - h * 0.7f, c.y - h * 0.7f); p.lineTo (c.x + h * 0.7f, c.y + h * 0.7f);
            p.startNewSubPath (c.x + h * 0.7f, c.y - h * 0.7f); p.lineTo (c.x - h * 0.7f, c.y + h * 0.7f);
            g.strokePath (p, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            g.fillRoundedRectangle (c.x - 2.0f, c.y - h, 4.0f, h * 1.3f, 2.0f);
            g.fillEllipse (c.x - 2.5f, c.y + h * 0.55f, 5.0f, 5.0f);
        }
    }

    void stopPreRoll()
    {
        recorder.playPreRoll ({});
        countdownStart = -1.0;
    }

public:
    void nudge (int by)
    {
        follower.nudgeWords (by);
        words.jumpTo (follower.currentWord(), recorder.recordedSeconds());
        shownLine = lineToShow();
        markRestart (shownLine);
        repaint();
    }

    void zoom (float by)
    {
        fontScale = juce::jlimit (0.6f, 2.2f, fontScale + by);
        settings.setFontScale (fontScale);
        layoutDirty = true;
        repaint();
    }

    //==========================================================================
    void resized() override
    {
        layoutDirty = true;
        auto bar = getLocalBounds().removeFromBottom (barHeight).reduced (12, 8);
        backButton.setBounds (bar.removeFromLeft (64)); bar.removeFromLeft (6);
        smallerButton.setBounds (bar.removeFromLeft (40)); bar.removeFromLeft (6);
        largerButton.setBounds (bar.removeFromLeft (40)); bar.removeFromLeft (6);
        mirrorButton.setBounds (bar.removeFromRight (70)); bar.removeFromRight (6);
        const int w = (bar.getWidth() - 6) / 2;
        previousButton.setBounds (bar.removeFromLeft (w)); bar.removeFromLeft (6);
        nextButton.setBounds (bar);
    }

    void paint (juce::Graphics& g) override
    {
        if (layoutDirty) rebuildLines (g);
        g.fillAll (Ink::panel());
        g.setColour (Ink::edge());
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
        auto r = getLocalBounds();
        if (!recorder.isRecording()) r.removeFromBottom (barHeight);
        auto bottom = r.removeFromBottom (follower.isLost() ? bottomHeight() : 0);
        // Lustro dotyczy czytanego tekstu; przyciski i lampka sa stale.
        juce::Graphics::ScopedSaveState keep (g);
        if (mirror)
            g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f)
                                                 .translated ((float) getWidth(), 0.0f));
        drawScript (g, r);
        if (follower.isLost()) drawLostBanner (g, bottom);
        drawCountdown (g, r);
        drawNoCutNotice (g, r);
    }

private:
    //==========================================================================
    int  bottomHeight() const { return getHeight() >= 1000 ?  96 :  72; }
    int  sideMargin()   const { return 28; }

    float fontNow()  const { return juce::jlimit (26.0f, 48.0f, 34.0f * fontScale); }
    float fontElse() const { return juce::jlimit (26.0f, 48.0f, 30.0f * fontScale); }

    //==========================================================================
    /* Podazanie zaczyna sie razem z NAGRANIEM, nie z otwarciem ekranu.

       Pierwsza wersja ruszala od razu po wczytaniu tekstu. Czlowiek otwieral
       prompter, poprawial krzeslo, chrzakal, ustawial mikrofon — a program
       liczyl mu to wszystko jako sylaby i zanim nacisnal nagrywanie, tekst
       stal juz kilkanascie wersow dalej. Potem caly seans wygladal, jakby
       prompter zgadywal losowo, bo zaczal od zlego miejsca i nigdy go nie
       odzyskal.

       Zegar tez idzie od nagrania, nie od otwarcia okna — inaczej czas, na
       ktorym opiera sie predkosc czytania, skakalby w momencie startu. */
    void timerCallback() override
    {
        if (! running) return;

        const bool rec = recorder.isRecording();
        refreshControls();

        if (rec != wasRecording)
        {
            wasRecording = rec;

            if (rec)
            {
                recorder.attachSyllableClock(nullptr);
                clock.reset();
                recorder.attachSyllableClock(&clock);
                follower.begin (&script);
                words.begin (&script);
                scrollNow = scrollTarget = 0.0f;
                shownLine = 0;
                lineLog.begin();
                heardWords.clear();
                lastHeard = -1;
                liveEvents.clear();
                grades.clear();
                startSpeech();
            }
            else
            {
                stopSpeech();
                stopPreRoll();
                regrade();
            }
        }

        if (! rec)
        {
            clock.takeNewSyllables();      // nie zbieraj tego, co bylo przed
            repaint();
            return;
        }

        const double now = recorder.recordedSeconds();

        follower.advance (now, clock.takeNewSyllables(), clock.pauseLength());

        // Rozpoznane slowa maja pierwszenstwo. Licznik sylab jest rezerwa:
        // dostaje pewna pozycje przy kazdym kroku, wiec gdy rozpoznawanie
        // zamilknie, jedzie dalej od dobrego miejsca.
        if (listening && speech != nullptr)
        {
            // w pauzie pozycja ze slow stoi — patrz WordTracker::speaking()
            words.speaking (now, clock.pauseLength() <= 0.0);

            VoskEngine::Hypothesis h;
            while (speech->nextHypothesis (h))
            {
                const int hitsBefore = words.hitCount();
                words.hypothesis (now, h.words);
                if (h.final) words.endUtterance();
                noteHeard (now, hitsBefore);
            }

            if (words.hasLock (now))
                follower.syncTo (tw::syllableAtWord (script, words.position (now)));
        }

        if (! lines.empty())
            lineLog.observe (markFor (lineToShow(), now,
                                      listening && words.hasLock (now) ? tw::LineSource::Words
                                                                       : tw::LineSource::Syllables));

        repaint();
    }

    void startSpeech()
    {
        if (speech == nullptr || speech->state() != VoskEngine::State::Ready || script.wordCount() == 0) return;

        // Slowa, ktorych model nie zna, dostaja najblizszy znany zamiennik
        // (na "Nic dwa razy": 7 slow, z tego 6 potem uslyszanych).
        for (auto& sb : speech->substitutesFor (words.vocabulary()))
            words.addAlias (sb.second, sb.first);

        speech->startListening (recorder.currentSampleRate(), words.vocabulary());
        recorder.attachSpeechRing (&speech->samples());
        listening = true;
    }

    void stopSpeech()
    {
        recorder.attachSpeechRing (nullptr);
        if (speech != nullptr) speech->stopListening();
        listening = false;
    }

    //==========================================================================
    /* Lamanie tekstu na wersy.

       ---------------------------------------------------------------------
       WERS MA SIE KONCZYC TAM, GDZIE BIERZESZ ODDECH

       Pierwsza wersja sklejala wers ze SLOW i lamala go na szerokosci. Mialo to
       dwie wady i obie widac bylo na pierwszym zrzucie ekranu.

       Po pierwsze gubila sie interpunkcja: slowa wyciaga sie z tekstu po
       literach, wiec przecinki i kropki zostawaly w zrodle. Czlowiek dostawal
       "Mikrofon stoi blisko ust wiec slychac kazdy oddech Czytam spokojnie" —
       bez jednego znaku, ktory mowi, gdzie zrobic pauze. Dla kogos, kto czyta
       na glos, to jest zabranie polowy partytury.

       Po drugie wers konczyl sie tam, gdzie skonczylo sie miejsce. "Czytam
       spokojnie" wypadalo na koncu wersu razem z poprzednim zdaniem, a "nie
       spieszac sie" zaczynalo nastepny — czyli podzial przecinal fraze w
       polowie.

       Teraz wers bierze sie z ZRODLA, znak w znak, od pierwszego do ostatniego
       slowa — wiec interpunkcja jest tam, gdzie ja napisales. A konczy sie
       najchetniej na znaku, przy ktorym i tak zrobisz pauze.
    */
    void rebuildLines (juce::Graphics& g)
    {
        layoutDirty = false;
        lines.clear();
        spans.clear();

        if (script.wordCount() == 0) return;

        const juce::Font f (fontNow(), juce::Font::bold);
        g.setFont (f);

        const int width = getWidth() - 2 * sideMargin() - 24;
        const auto& src = script.source();

        int first = 0;

        for (int i = 0; i < script.wordCount(); ++i)
        {
            const auto& w = script.all()[(size_t) i];

            const bool lastWord = i == script.wordCount() - 1;

            // szerokosc tego, co juz sie uzbieralo, z nastepnym slowem wlacznie
            const int through = lastWord ? i : i + 1;
            const juce::String probe = juce::String::fromUTF8 (
                src.substr (script.all()[(size_t) first].from,
                            script.all()[(size_t) through].to
                              - script.all()[(size_t) first].from).c_str());

            const bool tooWide  = f.getStringWidth (probe) > width;

            if (tw::lineEndsAfter (script, first, i, tooWide))
            {
                Line line;
                line.firstWord = first;
                line.lastWord  = i;

                // od poczatku pierwszego slowa do konca ostatniego — razem z
                // tym, co stoi pomiedzy: przecinkami, myslnikami, cudzyslowami
                line.text = juce::String::fromUTF8 (
                    src.substr (script.all()[(size_t) first].from,
                                w.to - script.all()[(size_t) first].from).c_str()).trim();

                // znak interpunkcyjny stojacy TUZ za slowem nalezy do tego wersu
                size_t tail = w.to;
                while (tail < src.size()
                       && (src[tail] == ',' || src[tail] == '.' || src[tail] == '!'
                        || src[tail] == '?' || src[tail] == ':' || src[tail] == ';'))
                {
                    line.text += juce::String::charToString ((juce::juce_wchar) src[tail]);
                    ++tail;
                }

                lines.push_back (line);
                spans.push_back ({ line.firstWord, line.lastWord });
                first = i + 1;
            }
        }
    }

    /* Czasy wersow (DSP/LineLog.h) — pierwszy krok punch and roll.
       Wers rozpoznajemy po pierwszym slowie, bo numer na ekranie zalezy od
       szerokosci okna. */
    tw::LineMark markFor (int shown, double sec, tw::LineSource src) const
    {
        tw::LineMark m;
        m.sec = sec; m.shown = shown; m.source = src;

        if (shown >= 0 && shown < (int) lines.size())
        {
            m.firstWord = lines[(size_t) shown].firstWord;
            m.lastWord  = lines[(size_t) shown].lastWord;
            m.text      = lines[(size_t) shown].text.toStdString();
        }
        return m;
    }

    /** R bez rozpoznawania mowy: przez kilka sekund mowimy, ze nic nie
        zostanie wyciete samo — zeby nikt nie liczyl na czysty plik. */
    void drawNoCutNotice (juce::Graphics& g, juce::Rectangle<int> r)
    {
        if (noCutNoticeAt < 0.0) return;
        if (juce::Time::getMillisecondCounterHiRes() - noCutNoticeAt > 5000.0) { noCutNoticeAt = -1.0; return; }

        auto box = r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 40, 640), 64)
                    .withY (r.getY() + (int) (r.getHeight() * 0.62f)).toFloat();
        g.setColour (Ink::raised().withAlpha (0.94f));
        g.fillRoundedRectangle (box, 10.0f);
        g.setColour (Ink::yellow());
        g.drawRoundedRectangle (box.reduced (1.0f), 10.0f, 1.5f);
        g.setColour (Ink::textNow());
        g.setFont (juce::Font (zs::type::small + 1.0f));
        g.drawFittedText (PL ("Bez rozpoznawania mowy R tylko cofa tekst. Pomyłkę usuniesz na ekranie Montaż."),
                          box.reduced (14.0f, 6.0f).toNearestInt(), juce::Justification::centred, 2);
    }

    /** Odliczanie po R: duza liczba na srodku tekstu, potem "Czytaj". */
    void drawCountdown (juce::Graphics& g, juce::Rectangle<int> r)
    {
        if (countdownStart < 0.0) return;
        const double elapsed = (juce::Time::getMillisecondCounterHiRes() - countdownStart) / 1000.0;
        const auto label = countdown.label (elapsed, countdownTotal);
        if (label.empty()) { countdownStart = -1.0; return; }

        const bool go = label == "Czytaj";
        // Pod celownikiem, nie na nim: wers do przeczytania ma byc widoczny
        // przez cale odliczanie.
        const int h = 120, w = juce::jmin (r.getWidth() - 40, 300);
        auto box = juce::Rectangle<int> (r.getCentreX() - w / 2, r.getY() + (int) (r.getHeight() * 0.62f) - h / 2, w, h).toFloat();
        g.setColour (Ink::raised().withAlpha (0.94f));
        g.fillRoundedRectangle (box, 12.0f);
        g.setColour (go ? Ink::green() : Ink::edge());
        g.drawRoundedRectangle (box.reduced (1.0f), 12.0f, 2.0f);

        g.setColour (go ? Ink::green() : Ink::textNow());
        g.setFont (juce::Font (go ? 56.0f : 84.0f, juce::Font::bold));
        g.drawText (juce::String (label), box.toNearestInt(), juce::Justification::centred);
    }

    void markRestart (int shown, tw::LineSource why = tw::LineSource::Key)
    {
        if (recorder.isRecording() && ! lines.empty())
            lineLog.restart (markFor (shown, recorder.recordedSeconds(), why), why);
    }

public:
    /** Wolane przez MainComponent tuz po zatrzymaniu nagrania, ZANIM otworzy
        sie ocena: wyciecie pomylek po R (PunchFile.h) i zapis wersow obok
        nagrania (sesja_<data>.wersy.txt). Bez tekstu w prompterze — nic.
        Blad nie zatrzymuje programu: surowe nagranie zostaje nietkniete. */
    zs::PunchResult finishTake (const juce::File& wav)
    {
        zs::PunchResult res;
        regrade();                // znaczki z pelnego nagrania, zanim zapis wersow sie wyczysci
        if (lineLog.marks().empty() || wav == juce::File()) return res;

        auto marks = lineLog.marks();
        res = zs::applyPunch (wav, marks, heardWords, settings.punchAutoCut());

        tw::LineLog out;
        out.begin();
        for (const auto& m : marks) out.restart (m, m.source);

        juce::String text = juce::String::fromUTF8 (out.toText().c_str());
        if (! res.applied)
            for (const auto& c : res.plan)
                text = "# pomylka do wyciecia na ekranie Montaz: " + juce::String (c.from, 2) + "-" + juce::String (c.to, 2)
                     + " s (R; automatyczne wycinanie wylaczone)\n" + text;
        if (res.applied)
            text = "# wyciete pomylki (klawisz R): " + juce::String (res.cuts) + ", razem "
                 + juce::String (res.removedSec, 1) + " s; surowe nagranie: " + res.rawCopy.getFileName()
                 + " w folderze surowe"
                 + (res.audibleSeams > 0 ? "; szwy z inna cisza (posluchaj): " + juce::String (res.audibleSeams) : juce::String())
                 + "\n" + text;
        for (const auto& s : res.skipped)
            text = "# NIE wyciete (" + juce::String (s.cut.from, 2) + "-" + juce::String (s.cut.to, 2) + " s): "
                 + juce::String::fromUTF8 (s.why.c_str()) + " — wytnij recznie na ekranie Montaz\n" + text;

        wav.getSiblingFile (wav.getFileNameWithoutExtension() + ".wersy.txt")
           .replaceWithText (text, false, false, "\n");

        lineLog.begin();          // juz zapisane — drugi raz nie
        return res;
    }

private:
    /** Slowa tekstu, ktore rozpoznawanie naprawde uslyszalo — do straznika
        wyciecia (Punch::guard). Trafienie przesuwa sie zwykle o slowo albo dwa;
        brakujace po drodze slowa (rozpoznane w jednej hipotezie) dopisujemy. */
    void noteHeard (double now, int hitsBefore)
    {
        if (! recorder.isRecording() || words.hitCount() <= hitsBefore) return;
        const int w = words.lastHitWord();
        if (w < 0) return;
        if (lastHeard >= 0 && w > lastHeard && w - lastHeard <= 3)
            for (int k = lastHeard + 1; k < w; ++k) heardWords.push_back ({ now, k });
        heardWords.push_back ({ now, w });
        lastHeard = w;
    }

    /* Ktory wers podswietlic i w ktorym lezy slowo — DSP/PrompterLines.h,
       zeby sprawdzal to test, a nie oko. */
    int lineToShow() const { return tw::lineToShow (spans, script, follower.syllablePosition()); }
    int lineOfWord (int word) const { return tw::lineOfWord (spans, word); }

    //==========================================================================
    void drawScript (juce::Graphics& g, juce::Rectangle<int> r)
    {
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(r);
        if (lines.empty())
        {
            g.setColour (Ink::textDim());
            g.setFont (juce::Font (fontElse() * 0.6f));
            g.drawText (PL ("Wczytaj tekst, zeby czytac z promptera."),
                        r, juce::Justification::centred);
            return;
        }

        // Wyswietlacz nigdy nie cofa sie sam — tak samo jak pozycja slowa.
        const int want = lineToShow();

        if (want > shownLine) shownLine = want;
        if (want < shownLine - 1) shownLine = want;     // duza poprawka: wolno

        const int now = shownLine;

        const float lineH = fontNow() * 1.35f;
        const float gap   = getHeight() >= 1000 ? 32.0f : 20.0f;
        const float step  = lineH + gap;

        // celownik: stale miejsce, w ktorym stoi czytany wers
        const int guideY = r.getY() + (int) (r.getHeight() * 0.28f);

        auto guide = juce::Rectangle<int> (sideMargin() - 16, guideY,
                                           getWidth() - 2 * (sideMargin() - 16),
                                           (int) lineH + 16);

        const juce::Colour guideCol = follower.isLost() ? Ink::lostBlue() : Ink::green();

        // Dopoki prompter nie uslyszal mowy, celownik jest przygaszony — zeby
        // bylo widac, ze tekst stoi CELOWO, a nie dlatego, ze program zamarl.
        const bool waiting = running && recorder.isRecording() && ! follower.hasStarted();
        const float strength = waiting ? 0.35f : 1.0f;

        g.setColour (guideCol.withAlpha ((follower.isLost() ? 0.08f : 0.09f) * strength));
        g.fillRect (guide);

        g.setColour (guideCol.withAlpha (strength));
        g.fillRect (guide.getX(), guide.getY(), 4, guide.getHeight());
        g.fillRect (guide.getRight() - 4, guide.getY(), 4, guide.getHeight());

        // Przewijanie plynne, nie skokowe.
        //
        // Pozycja zmienia wers skokiem — i tak musi, bo wers jest jednostka
        // niepodzielna. Ale gdyby tekst przeskakiwal razem z nia, kazda zmiana
        // wersu bylaby szarpnieciem calego ekranu w polu widzenia kogos, kto w
        // tej chwili mowi. Dlatego obraz dojezdza do nowej pozycji w okolo
        // dwie dziesiate sekundy: wystarczajaco szybko, zeby nie zostawac w
        // tyle, wystarczajaco wolno, zeby oko nadazylo.
        scrollTarget = (float) now;
        scrollNow += (scrollTarget - scrollNow) * 0.18f;

        if (std::abs (scrollTarget - scrollNow) < 0.002f) scrollNow = scrollTarget;

        for (int i = 0; i < (int) lines.size(); ++i)
        {
            const float y = guideY + 8.0f + (i - scrollNow) * step;

            if (y < r.getY() - step || y > r.getBottom()) continue;

            const bool isNow = i == now;

            g.setFont (juce::Font (isNow ? fontNow() : fontElse(),
                                   isNow ? juce::Font::bold : juce::Font::plain));

            g.setColour (i < now ? Ink::textPast()
                       : isNow   ? Ink::textNow()
                                 : Ink::textNext());

            g.drawText (lines[(size_t) i].text,
                        juce::Rectangle<float> ((float) sideMargin(), y,
                                                (float) (getWidth() - 2 * sideMargin()), lineH),
                        juce::Justification::centredLeft, false);

            const auto gr = grades.find (lines[(size_t) i].firstWord);
            if (gr != grades.end())
                drawGrade (g, gr->second, juce::Rectangle<float> (2.0f, y, (float) sideMargin() - 6.0f, lineH));
        }

        drawStateBadge (g, guide);

        // Przed nagraniem: jedno zdanie, gdy rozpoznawania mowy nie ma.
        if (! recorder.isRecording() && speech != nullptr && speech->whyNot().isNotEmpty())
        {
            g.setColour (Ink::textDim());
            g.setFont (juce::Font (zs::type::small));
            g.drawText (speech->whyNot(), guide.withY (guide.getBottom() + 4).withHeight (20).reduced (12, 0),
                        juce::Justification::centredRight);
        }
    }

    //==========================================================================
    void drawStateBadge (juce::Graphics& g, juce::Rectangle<int> guide)
    {
        juce::String label;
        juce::Colour col;

        if (! running || ! recorder.isRecording())
                                        { label = PL ("CZEKA");        col = Ink::textDim(); }
        else if (follower.waitingToResume())
                                        { label = PL ("CZEKAM NA GŁOS"); col = Ink::textNext(); }
        else if (! follower.hasStarted())
                                        { label = PL ("ZACZNIJ CZYTAĆ"); col = Ink::textNext(); }
        else if (follower.isLost())     { label = PL ("SZUKAM");       col = Ink::lostText(); }
        else if (clock.pauseLength() > 0.3) { label = PL ("PAUZA");    col = Ink::textNext(); }
        else                            { label = PL ("JEDZIE");       col = Ink::green(); }

        // Skad prompter wie, gdzie jestes: ze slow czy z sylab. Male, ale
        // widoczne — bez tego nie da sie ocenic, czy rozpoznawanie pomaga.
        if (running && recorder.isRecording() && listening)
            label << (words.hasLock (recorder.recordedSeconds()) ? PL ("  ·  SŁOWA") : PL ("  ·  SYLABY"));

        g.setFont (juce::Font (11.0f, juce::Font::bold));

        const int w = g.getCurrentFont().getStringWidth (label) + 18;
        auto box = juce::Rectangle<int> (guide.getRight() - w - 12, guide.getY() + 6, w, 18);

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRect (box);

        g.setColour (col.withAlpha (0.55f));
        g.drawRect (box, 1);

        g.setColour (col);
        g.drawText (label, box, juce::Justification::centred);
    }

    //==========================================================================
    void drawLostBanner (juce::Graphics& g, juce::Rectangle<int> r)
    {
        auto box = r.reduced (12, 0).withTrimmedBottom (8);

        g.setColour (Ink::lostBlue().withAlpha (0.08f));
        g.fillRect (box);
        g.setColour (Ink::lostBlue());
        g.drawRect (box, 1);

        g.setColour (juce::Colour (0xffe8eaf6));
        g.setFont (juce::Font (14.0f));
        g.drawText (PL ("Mów dalej — albo popraw miejsce strzałkami."),
                    box.reduced (12, 0).removeFromTop (30), juce::Justification::centredLeft);

        g.setColour (Ink::lostText());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText (PL ("↑ ↓ wers     ← → słowo"),
                    box.reduced (12, 0).removeFromBottom (30), juce::Justification::centredLeft);
    }

private:
    struct Line
    {
        juce::String text;
        int firstWord = 0, lastWord = 0;
    };

    juce::TextButton backButton,smallerButton,largerButton,previousButton,nextButton,mirrorButton;
    Recorder& recorder;

    tw::PrompterText script;
    tw::SyllableClock clock;
    tw::Follower follower;
    tw::WordTracker words;           ///< pozycja z rozpoznanych slow (Vosk)
    VoskEngine* speech = nullptr;
    bool listening = false;

    std::vector<Line> lines;
    std::vector<tw::LineSpan> spans;     ///< te same wersy, same numery slow
    tw::LineLog lineLog;                 ///< kiedy zaczal sie ktory wers (zapis obok nagrania)
    std::vector<tw::Event> liveEvents;                 ///< zdarzenia lampki tego nagrania
    std::vector<tw::HeardWord> heardWords;             ///< co uslyszalo rozpoznawanie (straznik wyciecia)
    int lastHeard = -1;
    std::map<int, tw::LineGrade> grades;               ///< firstWord -> znaczek
    tw::Countdown countdown;             ///< po R, razem z pre-rollem
    double countdownStart = -1.0, countdownTotal = 0.0;
    double noCutNoticeAt = -1.0;                       ///< ms; R bez modelu mowy   ///< ms; -1 = nie odliczamy
    bool layoutDirty = true, running = false;

    /* Gdzie obraz JEST i gdzie ma dojechac, w numerach wersow. */
    float scrollNow = 0.0f, scrollTarget = 0.0f;
    mutable int shownLine = 0;

    double elapsed = 0.0;
    bool   wasRecording = false;

    Settings&    settings;
    float        fontScale = 1.0f;
    bool         mirror = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrompterComponent)
};
