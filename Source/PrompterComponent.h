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
#include "DSP/PrompterText.h"
#include "DSP/SyllableClock.h"
#include "Recorder.h"
#include "Settings.h"
#include "Ink.h"

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

    ~PrompterComponent() override { recorder.attachSyllableClock (nullptr); }

    //==========================================================================
    void load (const juce::String& utf8)
    {
        script.set (utf8.toStdString());
        follower.begin (&script);
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
    }

    //==========================================================================
    /** Klawisze.

        Wszystkie dzialaja jedna reka i bez patrzenia, bo druga reka jest przy
        mikrofonie, a oczy w tekscie. Zadnego Ctrl, zadnych kombinacji. */
    bool keyPressed (const juce::KeyPress& k) override
    {
        const int c = k.getKeyCode();

        // Wyjscie. W trakcie nagrywania NIE wychodzimy — Escape w polowie wersu
        // to najczesciej pomylka palca, a wyjscie zmienia caly ekran i gubi
        // miejsce w tekscie. Nagranie zatrzymuje sie swoim przyciskiem.
        if (c == juce::KeyPress::escapeKey)
        {
            if (! recorder.isRecording() && onClose)
                onClose();

            return true;
        }

        if (c == juce::KeyPress::upKey)    { nudge (-4); return true; }
        if (c == juce::KeyPress::downKey)  { nudge (+4); return true; }
        if (c == juce::KeyPress::leftKey)  { nudge (-1); return true; }
        if (c == juce::KeyPress::rightKey) { nudge (+1); return true; }

        if (c == juce::KeyPress::homeKey)  { follower.jumpToWord (0); shownLine = 0;
                                             scrollNow = 0.0f; repaint(); return true; }

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

    void nudge (int words)
    {
        follower.nudgeWords (words);
        shownLine = lineToShow();
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
    void resized() override {
        layoutDirty=true;
        auto bar=getLocalBounds().removeFromBottom(46).reduced(16,5);bar.removeFromRight(224);
        backButton.setBounds(bar.removeFromLeft(72));bar.removeFromLeft(8);
        smallerButton.setBounds(bar.removeFromLeft(48));bar.removeFromLeft(6);
        largerButton.setBounds(bar.removeFromLeft(48));bar.removeFromLeft(12);
        previousButton.setBounds(bar.removeFromLeft(132));bar.removeFromLeft(6);
        nextButton.setBounds(bar.removeFromLeft(100));bar.removeFromLeft(12);
        mirrorButton.setBounds(bar.removeFromLeft(76));
    }

    void paint (juce::Graphics& g) override
    {
        if (layoutDirty) rebuildLines (g);

        g.fillAll (Ink::bg());

        // Szybka promptera odbija obraz z monitora, wiec zeby tekst byl
        // czytelny W ODBICIU, na ekranie musi stac odwrotnie. Odbijamy caly
        // rysunek jedna transformacja — inaczej trzeba by pamietac o tym w
        // kazdym miejscu, w ktorym cokolwiek sie rysuje, i predzej czy pozniej
        // jedno by sie zapomnialo.
        juce::Graphics::ScopedSaveState keep (g);

        if (mirror)
            g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f)
                                                 .translated ((float) getWidth(), 0.0f));

        auto r = getLocalBounds();
        r.removeFromBottom(50);
        drawHud (g, r.removeFromTop (hudHeight()));

        // dolny pasek istnieje tylko wtedy, gdy program sie zgubil
        auto bottom = r.removeFromBottom (follower.isLost() ? bottomHeight() : 0);

        // Spis klawiszy stoi na ekranie dokladnie wtedy, kiedy da sie go
        // przeczytac: przed nagraniem. W trakcie mowienia nikt nie czyta
        // podpowiedzi, a kazda linijka, ktora nie jest tekstem do przeczytania,
        // zabiera miejsce wersowi.
        if (! follower.isLost() && ! recorder.isRecording())
            drawKeyHints (g, r.removeFromBottom (30));

        drawScript (g, r);

        if (follower.isLost())
            drawLostBanner (g, bottom);
    }

private:
    //==========================================================================
    int  hudHeight()    const { return getHeight() >= 1000 ? 140 : 110; }
    int  bottomHeight() const { return getHeight() >= 1000 ?  96 :  72; }
    int  sideMargin()   const { return getWidth()  >= 1800 ?  96 :  64; }

    float fontNow()  const { return (getHeight() >= 1000 ? 46.0f : 38.0f) * fontScale; }
    float fontElse() const { return (getHeight() >= 1000 ? 42.0f : 34.0f) * fontScale; }

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
        backButton.setEnabled(!rec);

        if (rec != wasRecording)
        {
            wasRecording = rec;

            if (rec)
            {
                recorder.attachSyllableClock(nullptr);
                clock.reset();
                recorder.attachSyllableClock(&clock);
                follower.begin (&script);
                scrollNow = scrollTarget = 0.0f;
                shownLine = 0;
            }
        }

        if (! rec)
        {
            clock.takeNewSyllables();      // nie zbieraj tego, co bylo przed
            repaint();
            return;
        }

        follower.advance (recorder.recordedSeconds(),
                          clock.takeNewSyllables(), clock.pauseLength());
        repaint();
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
                first = i + 1;
            }
        }
    }

    /** Ktory wers podswietlic.

        Nie "ten, w ktorym stoi biezace slowo". Wers zmienia sie wczesniej —
        gdy usta doszly do ostatniej cwiartki wersu, bo oczy sa wtedy juz w
        nastepnym. Prog jest ulamkiem DLUGOSCI WERSU, a nie stala liczba slow:
        wers z dwoch slow i wers z osmiu potrzebuja innego wyprzedzenia.

        Dzieki temu podswietlenie dochodzi do nowego wersu mniej wiecej wtedy,
        kiedy czlowiek zaczyna go czytac oczami — a nie wtedy, kiedy konczy go
        wymawiac.
    */
    int lineToShow() const
    {
        if (lines.empty()) return 0;

        const double pos = follower.syllablePosition();

        for (int i = 0; i < (int) lines.size(); ++i)
        {
            const auto& a = script.all()[(size_t) lines[(size_t) i].firstWord];
            const auto& b = script.all()[(size_t) lines[(size_t) i].lastWord];

            const double from = a.beforeMe;
            const double to   = b.beforeMe + b.syllables;
            const double lead = (to - from) * 0.25;

            if (pos < to - lead) return i;
        }

        return (int) lines.size() - 1;
    }

    int lineOfWord (int word) const
    {
        for (size_t i = 0; i < lines.size(); ++i)
            if (word >= lines[i].firstWord && word <= lines[i].lastWord)
                return (int) i;

        return 0;
    }

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
    }

    //==========================================================================
    void drawStateBadge (juce::Graphics& g, juce::Rectangle<int> guide)
    {
        juce::String label;
        juce::Colour col;

        if (! running || ! recorder.isRecording())
                                        { label = PL ("CZEKA");        col = Ink::textDim(); }
        else if (! follower.hasStarted())
                                        { label = PL ("ZACZNIJ CZYTAĆ"); col = Ink::textNext(); }
        else if (follower.isLost())     { label = PL ("SZUKAM");       col = Ink::lostText(); }
        else if (clock.pauseLength() > 0.3) { label = PL ("PAUZA");    col = Ink::textNext(); }
        else                            { label = PL ("JEDZIE");       col = Ink::green(); }

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
    void drawKeyHints (juce::Graphics& g, juce::Rectangle<int> r)
    {
        g.setColour (Ink::textPast());
        g.setFont (juce::Font (13.0f));
        g.drawText (PL ("strzałki — popraw miejsce   ·   Home — na początek   ·   "
                        "+ / −  — wielkość tekstu   ·   L — odbicie lustrzane   ·   "
                        "Esc — wyjdź"),
                    r, juce::Justification::centred);
    }

    //==========================================================================
    void drawHud (juce::Graphics& g, juce::Rectangle<int> r)
    {
        g.setColour (Ink::panel());
        g.fillRect (r);

        g.setColour (Ink::edge());
        g.fillRect (r.getX(), r.getBottom() - 2, r.getWidth(), 2);

        auto inner = r.reduced (32, 0);

        // ---- lampka: czy NAGRANIE sie nadaje ---------------------------------
        const int d = getHeight() >= 1000 ? 60 : 48;

        auto bulb = juce::Rectangle<int> (d, d).withCentre ({ inner.getX() + d / 2,
                                                             inner.getCentreY() });

        juce::Colour c;
        juce::String title, diag;

        switch (lampState)
        {
            case 3: c = Ink::clip(); title = PL("BŁĄD NAGRANIA");diag=lampMsg;break;
            case 2: c = Ink::red();    title = PL ("POWTÓRZ");  diag = lampMsg; break;
            case 1: c = Ink::yellow(); title = PL ("POPRAW");   diag = lampMsg; break;
            default:
                c = recorder.isRecording() ? Ink::green() : Ink::textNext();
                title = recorder.isRecording() ? PL("NAGRYWASZ") : PL("GOTOWY");
                diag = recorder.isRecording() ? lampMsg : PL("Naciśnij Nagrywaj, kiedy chcesz zacząć.");
                break;
        }

        if (lampDim && recorder.isRecording()) c = c.withMultipliedBrightness (0.65f);

        // poswiata — to ona sprawia, ze lampke widac katem oka
        if (lampState > 0 || running)
        {
            g.setColour (c.withAlpha (0.22f));
            g.fillEllipse (bulb.toFloat().expanded (14.0f));
            g.setColour (c.withAlpha (0.12f));
            g.fillEllipse (bulb.toFloat().expanded (26.0f));
        }

        g.setColour (c);
        g.fillEllipse (bulb.toFloat());

        g.setColour (juce::Colours::white.withAlpha (lampState > 0 ? 0.9f : 0.25f));
        g.drawEllipse (bulb.toFloat(), 2.0f);

        auto txt = inner.withTrimmedLeft (d + 16).withWidth (340);

        g.setColour (c);
        g.setFont (juce::Font (getHeight() >= 1000 ? 28.0f : 20.0f, juce::Font::bold));
        g.drawText (title, txt.removeFromTop (txt.getHeight() / 2).translated (0, 8),
                    juce::Justification::bottomLeft);

        g.setColour (lampState > 0 ? c : Ink::textNext());
        g.setFont (juce::Font (getHeight() >= 1000 ? 15.0f : 13.0f,
                               lampState > 0 ? juce::Font::bold : juce::Font::plain));
        g.drawText (diag, txt.translated (0, 4), juce::Justification::topLeft);

        // ---- czas -------------------------------------------------------------
        drawClock (g, inner);

        // ---- poziom -----------------------------------------------------------
        drawMeter (g, inner.removeFromRight (getHeight() >= 1000 ? 320 : 240));
    }

    //==========================================================================
    void drawClock (juce::Graphics& g, juce::Rectangle<int> inner)
    {
        const double t = recorder.isRecording() ? recorder.recordedSeconds() : 0.0;

        juce::String s;
        s << juce::String ((int) t / 60).paddedLeft ('0', 2) << ":"
          << juce::String ((int) t % 60).paddedLeft ('0', 2) << "."
          << juce::String ((int) (t * 100) % 100).paddedLeft ('0', 2);

        auto box = juce::Rectangle<int> (240, inner.getHeight()).withCentre (inner.getCentre());

        g.setColour (Ink::textNext());
        g.setFont (juce::Font (11.0f));
        g.drawText (PL ("CZAS NAGRANIA"), box.removeFromTop (box.getHeight() / 2 - 4),
                    juce::Justification::centredBottom);

        g.setColour (recorder.isRecording() ? Ink::green() : Ink::textDim());
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(),
                               getHeight() >= 1000 ? 56.0f : 40.0f, juce::Font::bold));
        g.drawText (s, box, juce::Justification::centredTop);
    }

    //==========================================================================
    void drawMeter (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const float lv = recorder.currentLevel();
        const float db = lv > 1.0e-5f ? 20.0f * std::log10 (lv) : -90.0f;

        auto track = juce::Rectangle<int> (r.getWidth(), 14)
                        .withCentre ({ r.getCentreX(), r.getCentreY() + 6 });

        g.setColour (juce::Colour (0xff080b0e));
        g.fillRect (track);
        g.setColour (Ink::edge());
        g.drawRect (track, 1);

        constexpr int segs = 10;
        const int segW = (track.getWidth() - 4 - (segs - 1) * 2) / segs;

        // -60 dBFS na lewym koncu, 0 na prawym
        const float fill = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);

        for (int i = 0; i < segs; ++i)
        {
            auto s = juce::Rectangle<int> (track.getX() + 2 + i * (segW + 2),
                                           track.getY() + 2, segW, track.getHeight() - 4);

            const float at = (i + 1) / (float) segs;

            if (at > fill) { g.setColour (juce::Colour (0xff161e28)); }
            else if (i >= segs - 2) { g.setColour (Ink::clip()); }
            else if (i >= segs - 4) { g.setColour (Ink::yellow()); }
            else { g.setColour (Ink::green()); }

            g.fillRect (s);
        }

        g.setColour (Ink::textNext());
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (db > -89.0f ? juce::String (db, 1) + " dB" : "-",
                    track.withY (track.getY() - 18).withHeight (14),
                    juce::Justification::centredRight);
    }

    //==========================================================================
    void drawLostBanner (juce::Graphics& g, juce::Rectangle<int> r)
    {
        auto box = r.reduced (48, 0).withTrimmedBottom (20);

        g.setColour (Ink::lostBlue().withAlpha (0.08f));
        g.fillRect (box);
        g.setColour (Ink::lostBlue());
        g.drawRect (box, 1);

        g.setColour (juce::Colour (0xffe8eaf6));
        g.setFont (juce::Font (14.0f));
        g.drawText (PL ("Mów dalej — albo popraw miejsce strzałkami."),
                    box.reduced (20, 0), juce::Justification::centredLeft);

        g.setColour (Ink::lostText());
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText (PL ("↑ ↓ wers     ← → słowo"),
                    box.reduced (20, 0), juce::Justification::centredRight);
    }

public:
    /** Lampka przychodzi z zewnatrz — ten ekran jej nie liczy, tylko pokazuje. */
    void setLamp (int state, const juce::String& msg, bool dimmed)
    {
        lampState = state;
        lampMsg = msg;
        lampDim = dimmed;
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

    std::vector<Line> lines;
    bool layoutDirty = true, running = false;

    /* Gdzie obraz JEST i gdzie ma dojechac, w numerach wersow. */
    float scrollNow = 0.0f, scrollTarget = 0.0f;
    mutable int shownLine = 0;

    double elapsed = 0.0;
    bool   wasRecording = false;

    Settings&    settings;
    float        fontScale = 1.0f;
    bool         mirror = false;

    int          lampState = 0;
    juce::String lampMsg;
    bool         lampDim = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrompterComponent)
};
