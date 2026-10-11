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
        finishTake → PunchFile.h), a pelne nagranie zostaje w "surowe". */
    void repeatLine()
    {
        if (spans.empty()) return;

        const int from = tw::repeatLineStart (spans, follower.currentWord());
        follower.repeatFrom (from);
        words.jumpTo (from, recorder.recordedSeconds());
        shownLine = tw::lineOfWord (spans, from);
        markRestart (shownLine, tw::LineSource::Retake);
        repaint();
    }

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
                startSpeech();
            }
            else
            {
                stopSpeech();
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
                words.hypothesis (now, h.words);
                if (h.final) words.endUtterance();
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
            const bool newSourceLine = ! lastWord
                && script.all()[(size_t) (i + 1)].line != w.line;

            // szerokosc tego, co juz sie uzbieralo, z nastepnym slowem wlacznie
            const int through = lastWord ? i : i + 1;
            const juce::String probe = juce::String::fromUTF8 (
                src.substr (script.all()[(size_t) first].from,
                            script.all()[(size_t) through].to
                              - script.all()[(size_t) first].from).c_str());

            const int words = i - first + 1;

            const bool tooWide  = f.getStringWidth (probe) > width;
            const bool breathes = w.breathAfter >= 0.5f && words >= 3;

            if (lastWord || newSourceLine || tooWide || breathes)
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
        if (lineLog.marks().empty() || wav == juce::File()) return res;

        auto marks = lineLog.marks();
        res = zs::applyPunch (wav, marks);

        tw::LineLog out;
        out.begin();
        for (const auto& m : marks) out.restart (m, m.source);

        juce::String text = juce::String::fromUTF8 (out.toText().c_str());
        if (res.applied)
            text = "# wyciete pomylki (klawisz R): " + juce::String (res.cuts) + ", razem "
                 + juce::String (res.removedSec, 1) + " s; surowe nagranie: " + res.rawCopy.getFileName()
                 + " w folderze surowe"
                 + (res.audibleSeams > 0 ? "; szwy z inna cisza (posluchaj): " + juce::String (res.audibleSeams) : juce::String())
                 + "\n" + text;

        wav.getSiblingFile (wav.getFileNameWithoutExtension() + ".wersy.txt")
           .replaceWithText (text, false, false, "\n");

        lineLog.begin();          // juz zapisane — drugi raz nie
        return res;
    }

private:
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
