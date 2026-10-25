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
#include "DSP/PauseCut.h"
#include "DSP/EditedView.h"
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
#include "Trace.h"

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

    class StripTooltip : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit StripTooltip (PrompterComponent& p) : owner (p) {}
        void mouseDown (const juce::MouseEvent& e) override { owner.mouseDown (e.getEventRelativeTo (&owner)); }
        void mouseDrag (const juce::MouseEvent& e) override { owner.mouseDrag (e.getEventRelativeTo (&owner)); }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        { owner.mouseWheelMove (e.getEventRelativeTo (&owner), w); }
    private:
        PrompterComponent& owner;
    };

    //==========================================================================
    PrompterComponent (Recorder& r, Settings& s) : recorder (r), settings (s), stripTip (*this)
    {
        fontScale = (float) settings.fontScale();
        mirror    = settings.mirrored();

        backButton.setButtonText(PL("Wróć"));
        backButton.onClick=[this] { if(!recorder.isRecording() && onClose)onClose(); };
        smallerButton.setButtonText(PL("A−"));smallerButton.onClick=[this]{zoom(-.1f);};
        largerButton.setButtonText("A+");largerButton.onClick=[this]{zoom(.1f);};
        previousButton.setButtonText(PL("← Cofnij tekst"));previousButton.onClick=[this]{nudge(-4);};
        nextButton.setButtonText(PL("→ Dalej"));nextButton.onClick=[this]{nudge(4);};
        mirrorButton.setButtonText(PL("Lustro"));mirrorButton.onClick=[this]{mirror=!mirror;settings.setMirrored(mirror);repaint();};
        for(auto* b:{&backButton,&smallerButton,&largerButton,&previousButton,&nextButton,&mirrorButton})addAndMakeVisible(b);
        setupTransport();
        addAndMakeVisible (stripTip);
        setWantsKeyboardFocus (true);
        startTimerHz (30);
    }

    ~PrompterComponent() override
    {
        recorder.attachSyllableClock (nullptr);
        recorder.attachSpeechRing (nullptr);
        subsPool.removeAllJobs (true, 20000);   // zadanie czyta slowa modelu z VoskEngine
    }

    /** Rozpoznawanie mowy. Bez niego (nullptr albo brak modelu) prompter
        jedzie na liczniku sylab — tak jak w 0.4.0. */
    void setSpeech (VoskEngine* engine) { speech = engine; }

    /** Lekka obwiednia podkładu: jedna wartość szczytowa na 10 ms.
        Kopia jest przygotowana poza wątkiem audio. */
    void setBackingEnvelope (std::vector<float> envelope, double previewPositionSec = -1.0)
    { backingEnvelope = std::move (envelope); previewBackingPositionSec = previewPositionSec; repaint(); }

    //==========================================================================
    void load (const juce::String& utf8)
    {
        script.set (utf8.toStdString());
        follower.begin (&script);
        words.begin (&script);
        scrollNow = scrollTarget = 0.0f;
        shownLine = 0;
        layoutDirty = true;
        prepareSubstitutes();
        refreshControls();
        resized();
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
        traceFrames = 2;            // slad pierwszych klatek po powrocie (Trace.h)
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
            case zs::ui::KeyAction::StartStop:     spacePressed(); return true;
            case zs::ui::KeyAction::None:          break;
        }

        // Na pauzie strzalki wybieraja wers (jak klik), w odliczaniu nic nie robia —
        // inaczej prompter rozjezdzal sie z czerwona linia (przeglad 14.10, nr 5).
        if (pz.on && (c == juce::KeyPress::upKey || c == juce::KeyPress::downKey || c == juce::KeyPress::leftKey
                      || c == juce::KeyPress::rightKey || c == juce::KeyPress::homeKey))
        {
            if (! pz.resuming && c == juce::KeyPress::upKey)   selectLine (juce::jmax (0, shownLine - 1));
            if (! pz.resuming && c == juce::KeyPress::downKey) selectLine (shownLine + 1);
            return true;
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
        const bool visible = !recorder.isRecording() && ! noText();
        for (auto* b : { &backButton, &smallerButton, &largerButton, &previousButton, &nextButton, &mirrorButton })
            b->setVisible (visible);
    }

    /* ==== Transport jak w DAW (Adrian 13.10) ==================================

       Nagraj / Pauza / Odtworz / Zakoncz — przyciski pod fala, spacja to pauza
       i wznowienie, Esc konczy. Na pauzie nic sie nie zapisuje (Recorder::pause).
       Na pauzie czlowiek klika wers w prompterze albo miejsce na fali — program
       stawia czerwona linie w najblizszej ciszy i gra 2 s przed nia. Nagraj:
       pre-roll, odliczanie, a zapis rusza na "Czytaj"; wszystko od linii do
       chwili pauzy wypada (znacznik R z cutFrom i cutTo — Punch.h).
       R = skrot: pauza, linia przed biezacym wersem, Nagraj. */

    struct PauseState
    {
        bool   on = false;
        double atSec = 0.0;                 ///< czas w nagraniu, w ktorym stanelismy
        std::vector<float> history;         ///< ostatnie minuty nagrania z chwili pauzy
        double historyStart = 0.0, rate = 48000.0;
        std::vector<double> cuts;           ///< miejsca w ciszy, do ktorych przyciaga klik
        double viewEnd = 0.0;               ///< prawy brzeg widoku fali
        bool   selected = false;
        double cutSec = 0.0;                ///< czerwona linia
        int    line = -1;                   ///< od tego wersu nagrywamy
        int    lineWord = -1;               ///< ...jego pierwsze slowo (indeks wersu zmienia sie z oknem i +/-)
        int    pausedWord = -1;             ///< pierwsze slowo wersu z chwili pauzy — dalej wybrac nie mozna
        bool   tooOld = false;              ///< klikniety wers jest dalej niz pamiec nagrania
        bool   ahead = false;               ///< klikniety wers jeszcze nie byl czytany
        bool   resuming = false;            ///< pre-roll i odliczanie, zapis jeszcze stoi
        double playFrom = 0.0, playLen = 0.0, playStartMs = -1.0;
        double endSec = 0.0;                ///< z podkladem: czas nagrania w chwili wznowienia
    };
    PauseState pz;
    static constexpr double pauseViewSec = 20.0;
    static constexpr double backingCueSec = 3.0;   ///< rozbieg muzyki przed wznowieniem

    bool isPausedView() const noexcept { return pz.on; }
    /** Dla prawego panelu (MainComponent): czas nagrania — na pauzie miejsce
        pauzy — i ktory wers z ilu (przeglad ekranu nagrania 24.10). */
    double clockSeconds() const { return pz.on ? pz.atSec : recorder.recordedSeconds(); }
    bool   isResuming() const noexcept { return pz.resuming; }
    int    lineNow() const noexcept   { return lines.empty() ? 0 : juce::jlimit (0, (int) lines.size() - 1, shownLine); }
    int    lineTotal() const noexcept { return (int) lines.size(); }

    void pauseRecording()
    {
        if (! recorder.isRecording() || pz.on) return;
        recorder.pause();
        stopPreRoll();
        pz = {};
        pz.on = true;
        pz.atSec = recorder.recordedSeconds();
        pz.rate = recorder.currentSampleRate();
        pz.history = recorder.recentAudio (300.0, pz.historyStart);
        pz.viewEnd = pz.atSec;
        pz.cuts = tw::PauseCut::silences (pz.history, pz.rate, pz.historyStart);
        if (! lines.empty()) pz.pausedWord = lines[(size_t) juce::jlimit (0, (int) lines.size() - 1, shownLine)].firstWord;
        updateTransport();
        repaint();
    }

    /** Poczatek wersu w nagraniu (szacunek): pierwsze uslyszane slowo tego wersu
        w biezacym podejsciu, inaczej znacznik wersu. */
    /** Klik w wers na pauzie: linia w ciszy przed tym wersem (PauseCut.h —
        ten sam kod sprawdza symulacja). */
    void selectLine (int line, bool preview = true)
    {
        if (! pz.on || pz.resuming || line < 0 || line >= (int) lines.size()) return;
        // dalej niz wers z chwili pauzy nie ma czego nagrywac od nowa (porownanie
        // po slowie: po pierwszym wyborze shownLine stoi juz na wybranym wersie)
        // (przeklikanie 25.10: klik w taki wers nic nie robil i wygladal na blad — mowimy dlaczego)
        if (pz.pausedWord >= 0 && lines[(size_t) line].firstWord > pz.pausedWord) { pz.ahead = true; repaint(); return; }
        pz.ahead = false;
        const double cut = tw::PauseCut::forLine (lineLog.marks(), markFor (line, pz.atSec, tw::LineSource::Retake),
                                                  pz.history, pz.rate, pz.historyStart, heardWords, pz.cuts);
        // Wers starszy niz pamiec (5 min): linia wypadlaby w przypadkowym miejscu
        // na skraju pamieci, zwykle w slowie — nie zaznaczamy, mowimy dlaczego.
        pz.tooOld = cut < pz.historyStart + 0.5;
        if (pz.tooOld) { repaint(); return; }
        setSelection (cut, line, preview);
    }

    /** Klik w fale na pauzie: najblizsza cisza, prompter na wersie, ktory tam sie zaczyna. */
    void selectTime (double t)
    {
        if (! pz.on || pz.resuming || (lines.empty() && ! noText())) return;
        const double best = tw::PauseCut::nearest (t, pz.cuts, pz.atSec, pz.history, pz.rate, pz.historyStart);
        if (noText()) { setSelection (best, -1, true); return; }
        int line = shownLine;
        for (const auto& h : heardWords)
            if (h.sec - tw::Punch().heardLagSec >= best - 0.05) { line = tw::lineOfWord (spans, h.word); break; }
        setSelection (best, juce::jlimit (0, juce::jmax (0, (int) lines.size() - 1), line), true);
    }

    void setSelection (double cut, int line, bool preview)
    {
        // bez tekstu (Adrian 25.10): linia tylko na fali, bez wersu
        if (noText())
        {
            pz.tooOld = false; pz.ahead = false;
            pz.selected = true; pz.lineWord = -1; pz.line = -1;
            pz.cutSec = juce::jlimit (pz.historyStart, pz.atSec, cut);
            if (pz.cutSec < pz.viewEnd - pauseViewSec + 1.0 || pz.cutSec > pz.viewEnd)
                pz.viewEnd = juce::jmin (pz.atSec, pz.cutSec + pauseViewSec * 0.7);
            if (preview) playRange (pz.cutSec - 2.0, 2.0);
            updateTransport();
            repaint();
            return;
        }
        if (lines.empty() || line < 0 || line >= (int) lines.size()) return;
        pz.tooOld = false; pz.ahead = false;
        pz.selected = true;
        pz.lineWord = lines[(size_t) line].firstWord;
        pz.cutSec = juce::jlimit (pz.historyStart, pz.atSec, cut);
        pz.line = line;
        const int from = lines[(size_t) line].firstWord;
        follower.repeatFrom (from);
        words.jumpTo (from, recorder.recordedSeconds());
        lastHeard = -1;
        shownLine = line;
        if (pz.cutSec < pz.viewEnd - pauseViewSec + 1.0 || pz.cutSec > pz.viewEnd)
            pz.viewEnd = juce::jmin (pz.atSec, pz.cutSec + pauseViewSec * 0.7);
        if (preview) playRange (pz.cutSec - 2.0, 2.0);
        updateTransport();
        repaint();
    }

    /** Odtworz na pauzie: od linii (3 s przed nia) albo kilka sekund przed pauza. */
    void togglePlay()
    {
        if (! pz.on || pz.resuming) return;
        if (pz.playStartMs >= 0.0) { recorder.playPreRoll ({}); pz.playStartMs = -1.0; updateTransport(); return; }
        // ze wskazana linia: tylko to, co zostanie przed nia (3 s), jak mowi opis;
        // dawniej gralo dalej przez pomylke az do pauzy (przeklikanie 25.10)
        if (pz.selected) { playRange (pz.cutSec - 3.0, pz.cutSec - juce::jmax (pz.historyStart, pz.cutSec - 3.0)); return; }
        const double from = pz.atSec - 8.0;
        playRange (from, pz.atSec - juce::jmax (pz.historyStart, from));
    }

    void playRange (double from, double len)
    {
        from = juce::jmax (pz.historyStart, from);
        len = juce::jmin (len, pz.atSec - from);
        if (len <= 0.05 || pz.history.empty()) return;
        const size_t a = (size_t) ((from - pz.historyStart) * pz.rate);
        const size_t b = juce::jmin (pz.history.size(), a + (size_t) (len * pz.rate));
        if (b <= a) return;
        std::vector<float> slice (pz.history.begin() + (long) a, pz.history.begin() + (long) b);
        const size_t fade = juce::jmin (slice.size() / 2, (size_t) (0.01 * pz.rate));
        for (size_t k = 0; k < fade; ++k) { const float w = (float) k / (float) fade; slice[k] *= w; slice[slice.size() - 1 - k] *= w; }
        recorder.playPreRoll (std::move (slice));
        pz.playFrom = from; pz.playLen = len; pz.playStartMs = juce::Time::getMillisecondCounterHiRes();
        updateTransport();
        repaint();
    }

    /** Nagraj na pauzie. Bez linii: zapis od razu, tam gdzie stanelismy. */
    void resumeRecording()
    {
        if (! pz.on || pz.resuming) return;
        recorder.playPreRoll ({});
        pz.playStartMs = -1.0;
        if (recorder.hasBacking()) { resumeWithBacking(); return; }
        if (! pz.selected)
        {
            recorder.resume();
            pz = {};
            updateTransport();
            repaint();
            return;
        }
        tw::PreRollPlan p;
        if (! settings.roomCheck().headphonesLeak())
            tw::PreRoll().render (p, pz.history, pz.rate, pz.historyStart, pz.cutSec);
        pz.resuming = true;
        playPreRollPlan (std::move (p));              // odliczanie; zapis nadal stoi
        updateTransport();
        repaint();
    }

    /** Wznowienie z podkladem (Recorder::cueBacking): muzyka gra rozbieg
        konczacy sie dokladnie tam, gdzie glos wypadnie w czystym pliku — przy
        linii: w miejscu ciecia, bez linii: tam, gdzie stanelismy. W tej probce
        nagranie rusza samo (watek audio), wiec glos trafia w bit. Odliczanie
        trwa tyle co rozbieg. Glosu sprzed ciecia nie puszczamy — rozbiegiem
        jest muzyka. */
    void resumeWithBacking()
    {
        const double rate = juce::jmax (1.0, recorder.currentSampleRate());
        pz.endSec = std::max (pz.atSec, recorder.recordedSeconds());   // na pauzie nic juz nie przybywa
        auto spans = removedSpans;
        if (pz.selected && pz.cutSec < pz.endSec) spans.push_back ({ pz.cutSec, pz.endSec });
        const double toSec = tw::EditedView::editedTime (pz.endSec, spans);
        const auto to = (juce::int64) std::llround (toSec * rate);
        const auto from = juce::jmax ((juce::int64) 0, to - (juce::int64) std::llround (backingCueSec * rate));
        pz.resuming = true;
        preRoll = {};
        countdownStart = juce::Time::getMillisecondCounterHiRes();
        countdownTotal = juce::jmax (0.3, (double) (to - from) / rate);
        recorder.cueBacking (from, to);
        updateTransport();
        repaint();
    }

    /** Koniec odliczania: zapis rusza, od linii do pauzy wypada. */
    void finishResume()
    {
        if (! pz.on || ! pz.resuming) return;
        if (! pz.selected)                          // z podkladem, bez linii: dalej, bez ciecia
        {
            recorder.resume();
            pz = {};
            updateTransport();
            repaint();
            return;
        }
        // wers po slowie, nie po indeksie: okno albo +/- na pauzie lamia wersy od nowa
        if (pz.lineWord >= 0 && ! spans.empty()) pz.line = tw::lineOfWord (spans, pz.lineWord);
        // Koniec wyciecia = tyle, ile naprawde zapisano. Blok audio, ktory juz
        // trwal w chwili pauzy, dopisal sie po odczycie atSec (przeglad 14.10,
        // nr 8) — na pauzie nic wiecej nie przybywa, wiec teraz to dokladny koniec.
        // z podkladem nagranie juz ruszylo samo — koniec zapamietany przy wznowieniu
        const double end = pz.endSec > 0.0 ? pz.endSec : std::max (pz.atSec, recorder.recordedSeconds());
        auto m = markFor (pz.line, end, tw::LineSource::Retake);
        lineLog.restart (m, tw::LineSource::Retake);
        lineLog.setLastCut (pz.cutSec, true);
        lineLog.setLastCutTo (end);
        punchMovedByHand = true;
        if (pz.selected && pz.cutSec < end) removedSpans.push_back ({ pz.cutSec, end });
        recorder.resume();
        if (onRetake) onRetake();
        pz = {};
        regrade();
        updateTransport();
        repaint();
    }

    /** Spacja: start, pauza albo wznowienie. */
    void spacePressed()
    {
        if (! recorder.isRecording()) { if (onStartStop) onStartStop(); return; }
        if (pz.resuming) return;
        if (pz.on) resumeRecording(); else pauseRecording();
    }

    /** "Powtorz wers" (R albo przycisk): pauza, linia przed biezacym wersem i od
        razu Nagraj (pre-roll, odliczanie). Bez nagrania — tylko cofa tekst.

        Bez pewnej pozycji z rozpoznanych slow R tylko pauzuje i proponuje
        linie — wznawia czlowiek (spacja), gdy zobaczy, ze to dobre miejsce.
        Symulacja (przebieg C, same sylaby): licznik sylab potrafi byc dwa
        wersy obok i automatyczne wznowienie wycinalo dobre czytanie. */
    void repeatLine()
    {
        if (spans.empty()) return;
        if (! recorder.isRecording())
        {
            const int from = tw::repeatLineStart (spans, follower.currentWord());
            follower.repeatFrom (from);
            shownLine = tw::lineOfWord (spans, from);
            repaint();
            return;
        }
        if (pz.resuming) return;
        const bool sure = listening && words.hasLock (recorder.recordedSeconds());
        if (! pz.on) pauseRecording();
        const int from = tw::repeatLineStart (spans, follower.currentWord());
        selectLine (tw::lineOfWord (spans, from), ! sure);
        if (pz.selected) { if (sure) resumeRecording(); }
        else { recorder.resume(); pz = {}; updateTransport(); }
    }

    //==========================================================================
    /** Uklad karty z fala: fala, slowa, podpowiedz, pasek transportu. */
    struct StripLayout { juce::Rectangle<float> wave, labels, hint; };
    StripLayout stripLayout (juce::Rectangle<int> card) const
    {
        StripLayout L;
        auto inner = card.toFloat().reduced (14.0f, 10.0f);
        L.hint = inner.removeFromBottom (18.0f);
        // slowa w jednym rzedzie (Adrian 24.10: dwa rzedy "schodkami" wygladaly slabo)
        L.labels = inner.removeFromBottom (24.0f);
        L.wave = inner;
        return L;
    }

    juce::TextButton tRecord, tPause, tPlay, tRepeat, tEnd;

    void setupTransport()
    {
        tRecord.setButtonText (PL ("Nagraj"));   zs::Look::setIcon (tRecord, zs::Look::Icon::Record);
        tPause.setButtonText (PL ("Pauza"));     zs::Look::setIcon (tPause, zs::Look::Icon::Pause);
        tPlay.setButtonText (PL ("Odtwórz"));    zs::Look::setIcon (tPlay, zs::Look::Icon::Play);
        tEnd.setButtonText (PL ("Zakończ"));     zs::Look::setIcon (tEnd, zs::Look::Icon::Stop);
        // R byl tylko pod klawiszem (przeglad ekranu nagrania 24.10)
        tRepeat.setButtonText (PL ("Powtórz wers")); zs::Look::setIcon (tRepeat, zs::Look::Icon::Loop);
        tRepeat.setTooltip (PL ("Wróć na początek wersu i przeczytaj go jeszcze raz (R)"));
        tRepeat.onClick = [this] { repeatLine(); };
        tRecord.setTooltip (PL ("Nagraj (spacja)")); tPause.setTooltip (PL ("Pauza (spacja)"));
        tPlay.setTooltip (PL ("Odsłuchaj od zaznaczonego miejsca")); tEnd.setTooltip (PL ("Zakończ nagranie (Esc)"));
        tRecord.onClick = [this] { if (! recorder.isRecording()) { if (onStartStop) onStartStop(); } else resumeRecording(); };
        tPause.onClick  = [this] { pauseRecording(); };
        tPlay.onClick   = [this] { togglePlay(); };
        tEnd.onClick    = [this] { if (recorder.isRecording() && onStopRequest) onStopRequest(); };
        for (auto* b : { &tRecord, &tPause, &tPlay, &tRepeat, &tEnd }) { b->setWantsKeyboardFocus (false); addAndMakeVisible (b); }
        updateTransport();
    }

    void updateTransport()
    {
        const bool rec = recorder.isRecording();
        const bool resumeFromLine = pz.on && pz.selected && ! pz.resuming;
        tRecord.setButtonText (resumeFromLine ? PL ("Nagraj od linii") : PL ("Nagraj"));
        tRecord.setTooltip (resumeFromLine
            ? PL ("Wznowisz od czerwonej linii. Poprzednia wersja zostanie w folderze „surowe”. Kliknij falę, aby zmienić miejsce.")
            : PL ("Nagraj (spacja)"));
        stripTip.setTooltip (pz.on
            ? pz.selected
                ? PL ("PAUZA. Czerwona linia wyznacza początek nowego podejścia. Kliknij falę lub wers, aby wybrać inne miejsce; poprzednia wersja zostanie w folderze „surowe”.")
                : PL ("PAUZA. Kliknij wers albo miejsce na fali, aby wskazać początek wznowienia. Kółkiem myszy przewiniesz nagranie.")
            : juce::String());
        tRecord.setEnabled (! rec || (pz.on && ! pz.resuming));
        tPause.setEnabled (rec && ! pz.on);
        tPlay.setEnabled (pz.on && ! pz.resuming);
        tPlay.setButtonText (pz.playStartMs >= 0.0 ? PL ("Stop") : PL ("Odtwórz"));
        zs::Look::setIcon (tPlay, pz.playStartMs >= 0.0 ? zs::Look::Icon::Stop : zs::Look::Icon::Play);
        tEnd.setEnabled (rec);
        tRepeat.setEnabled (rec && ! pz.resuming && ! spans.empty());
        zs::Look::setKind (tRecord, (! rec || pz.on) ? zs::ui::ButtonKind::Primary : zs::ui::ButtonKind::Secondary);
    }

    void layoutTransport()
    {
        auto row = transportRow();
        // licznik czasu jest raz — w prawym panelu (MainComponent), nie tu
        // Kazdy przycisk tak szeroki jak ikona i napis. Gdy cztery sie nie mieszcza
        // (male okno), zostaja same ikony — jak w odtwarzaczu; nazwy
        // i klawisze stoja w podpowiedzi pod spodem.
        const juce::Font f (zs::type::body, juce::Font::bold);
        const auto widthOf = [&] (juce::Button* b) { return f.getStringWidth (b->getButtonText()) + 32 + zs::Look::iconSpace; };
        // piec rownych przyciskow na cala szerokosc (makieta E, nagranie)
        const int gap = 8;
        const int w = (row.getWidth() - 4 * gap) / 5;
        const int widest = juce::jmax (juce::jmax (widthOf (&tRecord), widthOf (&tPause), widthOf (&tPlay)),
                                       juce::jmax (widthOf (&tRepeat), widthOf (&tEnd)));
        const bool compact = w < widest;
        for (auto* b : { &tRecord, &tPause, &tPlay, &tRepeat, &tEnd })
        {
            zs::Look::setCompact (*b, compact);
            b->setBounds (b == &tEnd ? row : row.removeFromLeft (w)); row.removeFromLeft (gap);
        }
    }

    //==========================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! pz.on || pz.resuming) return;
        const auto L = stripLayout (stripCard());
        if (L.wave.expanded (0.0f, 8.0f).contains (e.position) || L.labels.contains (e.position))
        {
            const double t0 = pz.viewEnd - pauseViewSec;
            selectTime (t0 + (e.position.x - L.wave.getX()) / L.wave.getWidth() * pauseViewSec);
            return;
        }
        if (textCard().contains (e.getPosition()))
        {
            const int line = lineAtY ((float) e.position.y);
            if (line >= 0) selectLine (line);
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (! pz.on || ! stripCard().contains (e.getPosition())) return;
        pz.viewEnd = juce::jlimit (juce::jmin (pz.atSec, pz.historyStart + pauseViewSec), pz.atSec,
                                   pz.viewEnd - (double) (w.deltaY + w.deltaX) * 6.0);
        repaint();
    }

    /** Ktory wers jest pod kursorem (ta sama geometria co drawScript). */
    int lineAtY (float y) const
    {
        if (lines.empty()) return -1;
        auto r = textCard();
        if (! recorder.isRecording() && ! noText()) r.removeFromBottom (barHeight);
        if (follower.isLost()) r.removeFromBottom (bottomHeight());     // jak paint(): baner SZUKAM
        const float lineH = fontNow() * 1.35f;
        const float gap   = getHeight() >= 1000 ? 32.0f : 20.0f;
        const float step  = lineH + gap;
        const float guideY = r.getY() + r.getHeight() * 0.28f;
        const int i = (int) std::floor ((y - guideY - 8.0f + gap * 0.5f) / step + scrollNow);
        return i >= 0 && i < (int) lines.size() ? i : -1;
    }

    /** Pasek z fala pod prompterem — przez cale nagranie. Na zywo: ostatnie
        sekundy po cieciach ("nadpisanie", tw::EditedView). Na pauzie: ostatnie
        minuty, przewijane kolkiem, z czerwona linia "stad nagrasz od nowa". */
    void drawAim (juce::Graphics& g, juce::Rectangle<int> area)
    {
        if (pz.on)
        {
            if (previewBackingPositionSec >= 0.0)
            {
                pz.selected = true;
                pz.cutSec = previewBackingPositionSec - 3.0;
                drawStrip (g, area, pz.history, pz.historyStart, pz.rate,
                           previewBackingPositionSec - pauseViewSec, previewBackingPositionSec, true);
            }
            else drawStrip (g, area, pz.history, pz.historyStart, pz.rate, pz.viewEnd - pauseViewSec, pz.viewEnd, true);
            return;
        }
        if (previewLive)        // tylko zrzut ekranu
        {
            const double now = preRollStartSec + (double) preRollHistory.size() / preRollRate;
            const auto view = tw::EditedView::render (preRollHistory, preRollStartSec, preRollRate, now, removedSpans, liveStripSec);
            viewSpans = &removedSpans; viewNow = now;
            drawStrip (g, area, view, now - liveStripSec, preRollRate, now - liveStripSec, now, false);
            viewSpans = nullptr;
            return;
        }
        if (! recorder.isRecording())
        {
            static const std::vector<float> none;
            const double previewNow = previewBackingPositionSec >= 0.0 ? previewBackingPositionSec : 0.0;
            drawStrip (g, area, none, 0.0, 48000.0, previewNow - liveStripSec, previewNow, false);
            // Bez rozpoznawania mowy: jedno zdanie tam, gdzie po starcie bedzie
            // fala — wczesniej lezalo na tekscie promptera.
            if (speech != nullptr && speech->whyNot().isNotEmpty() && ! noText())
            {
                const auto wave = stripLayout (area).wave.toNearestInt();
                g.setColour (Ink::panel());
                g.fillRect (wave);
                g.setColour (Ink::textDim());
                g.setFont (juce::Font (zs::type::small));
                g.drawFittedText (speech->whyNot(), wave.reduced (4, 0), juce::Justification::centred, 3, 1.0f);
            }
            return;
        }
        double start = 0.0;
        const double rate = recorder.currentSampleRate();
        const auto live = recorder.recentAudio (liveStripSec, start);
        if (live.empty() || rate <= 0.0)
        {
            // pierwsze chwile nagrania: karta z pusta osia, nie dziura
            static const std::vector<float> none;
            const double previewNow = previewBackingPositionSec >= 0.0 ? previewBackingPositionSec : 0.0;
            drawStrip (g, area, none, 0.0, 48000.0, previewNow - liveStripSec, previewNow, false);
            return;
        }
        const double now = start + (double) live.size() / rate;
        auto spans2 = removedSpans;
        const auto view = tw::EditedView::render (live, start, rate, now, spans2, liveStripSec);
        viewSpans = &spans2; viewNow = now;
        drawStrip (g, area, view, now - liveStripSec, rate, now - liveStripSec, now, false);
        viewSpans = nullptr;
    }

    static constexpr double liveStripSec = 10.0;
    bool previewLive = false;

    void drawStrip (juce::Graphics& g, juce::Rectangle<int> area, const std::vector<float>& hist,
                    double histStart, double rate, double t0, double t1, bool paused)
    {
        const double sr = juce::jmax (1.0, rate);
        const auto box = area.toFloat();
        const auto L = stripLayout (area);
        const auto wave = L.wave, labels = L.labels, hint = L.hint;
        // specyfikacja E: tlo #0B1118, obrys #293543 (na pauzie pomaranczowy
        // #B88331 — aneks pkt 6), promien 8
        g.setColour (juce::Colour (paused ? 0xff151d27 : 0xff0b1118));
        g.fillRoundedRectangle (box, 8.0f);
        g.setColour (juce::Colour (paused ? 0xffb88331 : 0xff293543));
        g.drawRoundedRectangle (box.reduced (0.5f), 8.0f, 1.0f);
        // siatka czasu: kreska co sekunde (#283443, alpha .7)
        {
            const float pxPerSec = wave.getWidth() / (float) juce::jmax (0.1, t1 - t0);
            g.setColour (juce::Colour (0xff283443).withAlpha (0.7f));
            for (double s = std::ceil (t0); s < t1; s += 1.0)
                g.fillRect (wave.getX() + (float) (s - t0) * pxPerSec, wave.getY(), 1.0f, wave.getHeight());
        }

        auto xOf = [&] (double t) { return wave.getX() + (float) ((t - t0) / (t1 - t0)) * wave.getWidth(); };

        // Z podkladem dwa pasy z podpisami (Adrian 25.10: "podklad i wokal w
        // osobnych oknach i opisane"): GLOS u gory, PODKLAD pod nim.
        auto voiceArea = wave;
        juce::Rectangle<float> musicArea;
        if (! backingEnvelope.empty())
        {
            musicArea = voiceArea.removeFromBottom (juce::jmax (18.0f, wave.getHeight() * 0.32f));
            voiceArea.removeFromBottom (6.0f);
            g.setColour (juce::Colour (0xff293543));
            g.fillRect (wave.getX(), musicArea.getY() - 3.5f, wave.getWidth(), 1.0f);
        }

        // Podklad rysujemy w dolnym pasmie, przed fala glosu. Indeks muzyki
        // jest atomowo publikowany przez Recorder; przesuniecie wyliczamy z
        // jego biezacej pozycji, bez dostepu do bufora watku audio.
        if (! backingEnvelope.empty())
        {
            const double backingRate = recorder.currentSampleRate();
            const double shift = recorder.hasBacking() && backingRate > 0.0
                ? recorder.recordedSeconds() - (double) recorder.backingIndexNow() / backingRate : 0.0;
            const float musicMid = musicArea.getCentreY();
            const float musicHalf = juce::jmax (2.0f, musicArea.getHeight() * 0.46f);
            const juce::Colour musicColour (0xff8f7cf0);
            for (int px = 0; px < (int) wave.getWidth(); ++px)
            {
                const double t = t0 + (t1 - t0) * (px + 0.5) / wave.getWidth();
                const double musicSec = t - shift;
                const auto bucket = (long) std::floor (musicSec * 100.0);
                if (bucket < 0 || bucket >= (long) backingEnvelope.size()) continue;
                const float peak = backingEnvelope[(size_t) bucket];
                const float h = juce::jlimit (0.0f, 1.0f, peak) * musicHalf;
                if (h < 0.5f) continue;
                const bool afterRestartLine = paused && pz.selected && t > pz.cutSec;
                g.setColour (musicColour.withAlpha (afterRestartLine ? 0.15f : 0.60f));
                g.drawVerticalLine ((int) (wave.getX() + (float) px), musicMid - h, musicMid + h);
            }
        }

        // na pauzie: wyciete wczesniej (przygaszone) i to, co zostanie zastapione
        if (paused)
        {
            for (const auto& sp : removedSpans)
            {
                const float a = juce::jmax (wave.getX(), xOf (sp.first)), b = juce::jmin (wave.getRight(), xOf (sp.second));
                if (b > a) { g.setColour (Ink::bg().withAlpha (0.55f)); g.fillRect (juce::Rectangle<float> (a, wave.getY(), b - a, wave.getHeight())); }
            }
            if (pz.selected)
            {
                const float a = juce::jmax (wave.getX(), xOf (pz.cutSec)), b = juce::jmin (wave.getRight(), xOf (pz.atSec));
                if (b > a) { g.setColour (Ink::red().withAlpha (0.16f)); g.fillRect (juce::Rectangle<float> (a, wave.getY(), b - a, wave.getHeight())); }
            }
        }

        // przebieg: szczyt na kolumne w skali decybeli (jak w DAW); na zywo
        // niebieski #38B5FF, na pauzie szary #98A7B6 (spec. E), linia zera #536170
        const float mid = voiceArea.getCentreY(), half = voiceArea.getHeight() * 0.48f;
        g.setColour (juce::Colour (0xff536170));
        g.drawHorizontalLine ((int) mid, wave.getX(), wave.getRight());
        if (! backingEnvelope.empty())
        {
            // podpisy na ciemnej plakietce, zeby nie ginely w fali
            const juce::Font tag (zs::type::tiny, juce::Font::bold);
            g.setFont (tag);
            const auto plate = [&] (const juce::String& t, juce::Rectangle<float> area, juce::Colour c)
            {
                const auto r = juce::Rectangle<float> (area.getX() + 2.0f, area.getY() + 2.0f, (float) tag.getStringWidth (t) + 10.0f, 14.0f);
                g.setColour (juce::Colour (paused ? 0xff151d27 : 0xff0b1118).withAlpha (0.85f));
                g.fillRoundedRectangle (r, 3.0f);
                g.setColour (c);
                g.drawText (t, r, juce::Justification::centred);
            };
            plate (PL ("GŁOS"), voiceArea, Ink::textNext());
            plate (PL ("PODKŁAD"), musicArea, juce::Colour (0xffb3a6ff));
            g.setColour (paused ? juce::Colour (0xff98a7b6).withAlpha (0.8f) : juce::Colour (0xff38b5ff).withAlpha (0.9f));
        }
        g.setColour (paused ? juce::Colour (0xff98a7b6).withAlpha (0.8f) : juce::Colour (0xff38b5ff).withAlpha (0.9f));
        for (int px = 0; px < (int) wave.getWidth(); ++px)
        {
            const double a = t0 + (t1 - t0) * px / wave.getWidth(), b = t0 + (t1 - t0) * (px + 1) / wave.getWidth();
            const auto i0 = (long) ((a - histStart) * sr), i1 = (long) ((b - histStart) * sr);
            float pk = 0.0f;
            for (long i = juce::jmax (0L, i0); i < i1 && i < (long) hist.size(); ++i)
                pk = juce::jmax (pk, std::abs (hist[(size_t) i]));
            if (pk <= 0.0f) continue;
            const float h = juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (pk) + 60.0f) / 54.0f) * half;
            if (h < 0.5f) continue;
            g.drawVerticalLine ((int) (wave.getX() + (float) px), mid - h, mid + h + 1.0f);
        }

        // szwy na zywo: tu nowe czytanie "nadpisalo" pomylke
        if (viewSpans != nullptr)
            for (const auto& sp : *viewSpans)
            {
                const double v = tw::EditedView::toView (sp.first, viewNow, *viewSpans);
                if (v <= t0 || v >= t1) continue;
                g.setColour (Ink::accent());
                g.fillRect (juce::Rectangle<float> (xOf (v) - 1.5f, wave.getY() - 2.0f, 3.0f, wave.getHeight() + 4.0f));
            }

        if (paused)
        {
            // miejsce pauzy, czerwona linia, glowica odtwarzania
            g.setColour (Ink::yellow());
            if (pz.atSec > t0 && pz.atSec <= t1) g.fillRect (juce::Rectangle<float> (xOf (pz.atSec) - 1.0f, wave.getY(), 2.0f, wave.getHeight()));
            if (pz.selected && pz.cutSec > t0 && pz.cutSec < t1)
            {
                const float x = xOf (pz.cutSec);
                g.setColour (Ink::red());
                g.fillRect (juce::Rectangle<float> (x - 1.5f, wave.getY() - 2.0f, 3.0f, wave.getHeight() + 4.0f));
                const juce::Font f (zs::type::small, juce::Font::bold);
                const auto label = PL ("stąd nagrasz od nowa");
                const float lw = (float) f.getStringWidth (label);
                // przy prawym brzegu podpis po lewej stronie linii — inaczej wychodzil poza karte
                const bool left = x + 6.0f + lw > wave.getRight();
                g.setFont (f);
                g.drawText (label, juce::Rectangle<float> (left ? x - 6.0f - lw : x + 6.0f, wave.getY(), lw, 16.0f),
                            left ? juce::Justification::centredRight : juce::Justification::centredLeft);
            }
            if (pz.playStartMs >= 0.0)
            {
                const double el = (juce::Time::getMillisecondCounterHiRes() - pz.playStartMs) / 1000.0;
                if (el > pz.playLen) { pz.playStartMs = -1.0; updateTransport(); }
                else
                {
                    const double ph = pz.playFrom + el;
                    if (ph > t0 && ph < t1) { g.setColour (Ink::accent()); g.fillRect (juce::Rectangle<float> (xOf (ph) - 1.0f, wave.getY() - 2.0f, 2.0f, wave.getHeight() + 4.0f)); }
                }
            }
        }

        // uslyszane slowa: jeden rzad; slowo, ktore by nachodzilo na poprzednie,
        // nie jest rysowane (zostaje co drugie — i tak widac, gdzie jest tekst)
        {
        juce::Graphics::ScopedSaveState clipWords (g);
        g.reduceClipRegion (labels.withX (wave.getX()).withWidth (wave.getWidth()).toNearestInt());
        const int rows = 1;
        const float wordRow = labels.getHeight() / (float) rows;
        const juce::Font wordFont (zs::type::body, juce::Font::bold);
        g.setFont (wordFont);
        float rowRight[2] = { -1.0e9f, -1.0e9f };
        int lastWord = -1;
        const auto& src = script.source();
        for (const auto& h : heardWords)
        {
            if (h.word == lastWord || h.word < 0 || h.word >= script.wordCount()) continue;
            lastWord = h.word;
            double t = h.sec - tw::Punch().heardLagSec;
            if (viewSpans != nullptr)
            {
                if (tw::EditedView::hidden (t, *viewSpans)) continue;
                t = tw::EditedView::toView (t, viewNow, *viewSpans);
            }
            if (t < t0 || t > t1) continue;
            const auto& w = script.all()[(size_t) h.word];
            const auto text = juce::String::fromUTF8 (src.substr (w.from, w.to - w.from).c_str());
            const float x = xOf (t), wdt = (float) wordFont.getStringWidth (text) + 8.0f;
            const int row = x >= rowRight[0] ? 0 : rows > 1 && x >= rowRight[1] ? 1 : -1;
            if (row < 0) continue;
            g.setColour (juce::Colour (0xffb9c4d0));
            g.drawText (text, juce::Rectangle<float> (x, labels.getY() + row * wordRow, wdt, wordRow), juce::Justification::centredLeft);
            rowRight[row] = x + wdt;
        }

        }

        // stan i podpowiedz: zawsze widac, czy sie nagrywa
        g.setFont (juce::Font (zs::type::small));
        juce::String text;
        if (! recorder.isRecording() && ! previewLive)
        {
            g.setColour (Ink::textDim());
            text = PL ("Tu zobaczysz falę nagrania  ·  Nagraj albo spacja — zacznij");
            g.drawText (text, hint.toNearestInt(), juce::Justification::centredLeft);
        }
        else if (paused)
        {
            g.setColour (Ink::yellow());
            g.fillRect (hint.getX(), hint.getCentreY() - 5.0f, 3.0f, 10.0f);
            g.fillRect (hint.getX() + 5.0f, hint.getCentreY() - 5.0f, 3.0f, 10.0f);
            // pelne zdanie, a gdy sie nie miesci (male okno) — krotsze, bez wielokropka
            const auto area = hint.withTrimmedLeft (14.0f).toNearestInt();
            const auto pick = [&] (const juce::String& full, const juce::String& brief)
            { return juce::Font (zs::type::small).getStringWidth (full) <= area.getWidth() ? full : brief; };
            text = pz.ahead    ? pick (PL ("PAUZA  ·  tego wersu jeszcze nie czytałeś — kliknij ten, przy którym jesteś, albo wcześniejszy"),
                                       PL ("PAUZA  ·  ten wers jeszcze przed Tobą — kliknij wcześniejszy"))
                 : pz.tooOld   ? pick (PL ("PAUZA  ·  ten wers jest dalej niż 5 minut wstecz — tak daleko nie nagrasz od nowa; popraw go w Montażu"),
                                       PL ("PAUZA  ·  za daleko wstecz — popraw w Montażu"))
                 : pz.resuming ? PL ("PAUZA  ·  za chwilę nagrywanie od czerwonej linii")
                 : pz.selected ? PL ("PAUZA  ·  Wznowisz od czerwonej linii")
                 : noText()    ? pick (PL ("PAUZA  ·  kliknij miejsce na fali, od którego chcesz nagrać od nowa  ·  kółko — przewiń"),
                                       PL ("PAUZA  ·  kliknij miejsce na fali"))
                               : pick (PL ("PAUZA  ·  kliknij wers albo miejsce na fali, od którego chcesz nagrać  ·  kółko — przewiń"),
                                       PL ("PAUZA  ·  kliknij wers albo miejsce na fali"));
            g.drawText (text, area, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (Ink::red());
            g.fillEllipse (hint.getX(), hint.getCentreY() - 4.0f, 8.0f, 8.0f);
            g.setColour (Ink::textDim());
            g.drawText (PL ("NAGRYWANIE  ·  ostatnie 10 s"), hint.withTrimmedLeft (14.0f).toNearestInt(), juce::Justification::centredLeft);
        }
    }

    /** Po R: w sluchawkach 2 s tego, co w czystym pliku bedzie przed tym
        wersem, i odliczanie do czytania (DSP/PreRoll.h). Gdy kreator zmierzyl,
        ze sluchawki uciekaja do mikrofonu — samo odliczanie. */
    void startPreRoll()
    {
        if (! recorder.isRecording()) return;

        tw::PreRollPlan plan;
        preRollHistory.clear();
        punchMovedByHand = false;
        if (! settings.roomCheck().headphonesLeak())
        {
            preRollHistory = recorder.recentAudio (30.0, preRollStartSec);
            plan = tw::PreRoll().plan (lineLog.marks(), preRollHistory, recorder.currentSampleRate(), preRollStartSec, &heardWords);
        }

        // koniec pre-rollu = miejsce ciecia, ktore czlowiek wlasnie uslyszy
        if (! plan.audio.empty()) lineLog.setLastCut (plan.toSec, false);
        playPreRollPlan (std::move (plan));
    }

    void playPreRollPlan (tw::PreRollPlan plan)
    {
        const double sec = (double) plan.audio.size() / juce::jmax (1.0, recorder.currentSampleRate());
        recorder.playPreRoll (plan.audio);
        preRoll = std::move (plan);
        countdownStart = juce::Time::getMillisecondCounterHiRes();
        countdownTotal = countdown.totalFor (sec);
    }

    /** Strzalka w czasie odliczania: miejsce ciecia o pauze wczesniej / pozniej.
        Pre-roll gra od nowa i konczy sie w nowym miejscu; czlowiek je wskazal,
        wiec ciecie jest pewne (Punch::review). */
    bool movePunchPoint (int by)
    {
        if (countdownStart < 0.0 || preRoll.candidates.empty() || preRoll.chosen < 0 || preRollHistory.empty())
            return false;

        const int next = juce::jlimit (0, (int) preRoll.candidates.size() - 1, preRoll.chosen + by);
        if (next == preRoll.chosen) return true;

        auto p = preRoll;
        if (! tw::PreRoll().render (p, preRollHistory, recorder.currentSampleRate(), preRollStartSec, p.candidates[(size_t) next]))
            return true;
        p.chosen = next;
        lineLog.setLastCut (p.toSec, true);
        punchMovedByHand = true;
        playPreRollPlan (std::move (p));
        repaint();
        return true;
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
        preRoll.candidates = { 1.0, 2.0 };        // podpowiedz strzalek na zrzucie
        preRoll.chosen = 1;
        repaint();
    }

    /** Tylko zrzut ekranu (--preview-ui prompter-wskazanie): pasek
        wskazywania miejsca ciecia na sztucznym przebiegu. */
    void previewAimScreen()
    {
        previewMarks();
        countdownStart = -1.0;
        const double sr = 48000.0;
        preRollRate = sr;
        preRollStartSec = 0.0;
        preRollHistory.assign ((size_t) (12.0 * sr), 0.0f);
        const double words[][2] = { { 2.2, 2.6 }, { 2.7, 3.3 }, { 3.4, 3.8 }, { 4.6, 5.0 }, { 5.1, 5.7 },
                                    { 5.8, 6.4 }, { 7.3, 7.7 }, { 7.8, 8.5 }, { 9.6, 10.0 }, { 10.1, 10.6 } };
        // jak mowa: szum z obwiednia sylab, nie gladkie sinusy (wyglada jak fala z DAW)
        uint32_t seed = 12345u;
        auto noise = [&seed] { seed = seed * 1664525u + 1013904223u; return (float) ((int) (seed >> 9) - (1 << 22)) / (float) (1 << 22); };
        for (auto& v : preRollHistory) v = 0.0006f * noise();
        for (auto& w : words)
            for (auto i = (size_t) (w[0] * sr); i < (size_t) (w[1] * sr); ++i)
            {
                const double t = (double) i / sr - w[0], len = w[1] - w[0];
                const double env = std::pow (std::sin (3.14159 * t / len), 0.6) * (0.55 + 0.45 * std::sin (t * 37.0));
                preRollHistory[i] = (float) (0.06 * env * env) * noise();
            }
        heardWords.clear();
        const int first = lines.size() > 3 ? lines[2].firstWord : 0;
        for (int k = 0; k < 10; ++k)
            heardWords.push_back ({ words[k][1] + tw::Punch().heardLagSec, first + k });
        // pauza z zaznaczona linia (podglad nowego transportu)
        pz = {};
        pz.on = true; pz.atSec = 11.0; pz.rate = sr; pz.historyStart = 0.0;
        pz.history = preRollHistory; pz.viewEnd = 12.0;
        pz.cuts = { 3.95, 6.6, 9.0 };
        pz.selected = true; pz.cutSec = 6.6; pz.line = 2;
        updateTransport();
        repaint();
    }

    /** Tylko zrzut ekranu (--preview-ui prompter-pasek): pasek na zywo. */
    void previewLiveScreen() { previewAimScreen(); pz = {}; previewLive = true; removedSpans = { { 6.45, 7.25 } }; updateTransport(); repaint(); }

    /** Spacja: start i stop nagrania (MainComponent::startStop). */
    std::function<void()> onStartStop;

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
        preRoll = {};
        preRollHistory.clear();
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
    /* Dwie karty: tekst u gory, fala na dole — osobne okienko, widoczne od
       wejscia na ekran (Adrian 13.10, uklad z projektu ChatGPT). */
    static constexpr int stripGap = 12;
    static constexpr int transportH = 44, transportGap = 10;
    static constexpr int noTextCardH = 88;
    /** Bez tekstu: maly naglowek u gory, a okno z fala zajmuje reszte. */
    bool noText() const noexcept { return script.wordCount() == 0; }
    int stripCardHeight() const
    {
        if (noText()) return juce::jmax (110, getHeight() - noTextCardH - stripGap - transportGap - transportH);
        return juce::jlimit (getHeight() < 440 ? 110 : 132, 250, (int) (getHeight() * 0.30));
    }
    juce::Rectangle<int> textCard() const  { auto r = getLocalBounds(); r.removeFromBottom (stripCardHeight() + stripGap + transportGap + transportH); return r; }
    juce::Rectangle<int> stripCard() const
    {
        auto r = getLocalBounds(); r.removeFromBottom (transportH + transportGap);
        return r.removeFromBottom (stripCardHeight());
    }
    /** Transport pod oknem z fala, nie w nim (Adrian 25.10). */
    juce::Rectangle<int> transportRow() const { return getLocalBounds().removeFromBottom (transportH); }

    void resized() override
    {
        layoutDirty = true;
        stripTip.setBounds (stripCard());
        layoutTransport();
        auto bar = textCard().removeFromBottom (barHeight).reduced (12, 8);
        const juce::Font f (zs::type::body);
        const auto fit = [&] (juce::Button& b) { return f.getStringWidth (b.getButtonText()) + 24; };
        backButton.setBounds (bar.removeFromLeft (fit (backButton))); bar.removeFromLeft (6);
        smallerButton.setBounds (bar.removeFromLeft (juce::jmax (40, fit (smallerButton)))); bar.removeFromLeft (6);
        largerButton.setBounds (bar.removeFromLeft (juce::jmax (40, fit (largerButton)))); bar.removeFromLeft (6);
        mirrorButton.setBounds (bar.removeFromRight (fit (mirrorButton))); bar.removeFromRight (6);
        const int w = (bar.getWidth() - 6) / 2;
        // "← Cofnij tekst" w waskim oknie: sama strzalka (tak jak przyciski nagrywania)
        const bool compact = w < juce::jmax (fit (previousButton), fit (nextButton));
        zs::Look::setCompact (previousButton, compact);
        zs::Look::setCompact (nextButton, compact);
        previousButton.setBounds (bar.removeFromLeft (w)); bar.removeFromLeft (6);
        nextButton.setBounds (bar);
    }

    void paint (juce::Graphics& g) override
    {
        if (traceFrames > 0) zs::trace ("prompter: rysuje klatke");
        if (layoutDirty) rebuildLines (g);
        g.fillAll (Ink::bg());
        g.setColour (Ink::panel());            // plaskie tlo: wygaszenie przy krawedziach ma ten sam kolor
        g.fillRoundedRectangle (textCard().toFloat(), 10.0f);
        drawAim (g, stripCard());              // fala: bez lustra, zawsze czytelna
        drawCountdown (g, stripCard());
        auto r = textCard();
        // Tekst nie wchodzi na ramke: przyciety 12 px od gory i dolu karty,
        // a obrys rysowany jeszcze raz na koncu (Adrian 25.10: "na gorze tekst
        // zanika, powinien byc obrys").
        juce::Graphics::ScopedSaveState clipText (g);
        g.reduceClipRegion (textCard().reduced (1, noText() ? 1 : 12));
        if (!recorder.isRecording() && ! noText()) r.removeFromBottom (barHeight);
        auto bottom = r.removeFromBottom (follower.isLost() ? bottomHeight() : 0);
        // Lustro dotyczy czytanego tekstu; przyciski i lampka sa stale.
        juce::Graphics::ScopedSaveState keep (g);
        if (mirror)
            g.addTransform (juce::AffineTransform::scale (-1.0f, 1.0f)
                                                 .translated ((float) getWidth(), 0.0f));
        drawScript (g, r);
        // wersy wychodzace poza karte gasna lagodnie, zamiast urywac sie na krawedzi
        if (! noText())
        {
            const auto card = textCard().toFloat();
            const float fade = 44.0f;
            g.setGradientFill (juce::ColourGradient (Ink::panel(), 0.0f, card.getY() + 24.0f,
                                                     Ink::panel().withAlpha (0.0f), 0.0f, card.getY() + 24.0f + fade, false));
            g.fillRect (card.withHeight (24.0f + fade));
            const float bottomY = (float) r.getBottom();
            g.setGradientFill (juce::ColourGradient (Ink::panel().withAlpha (0.0f), 0.0f, bottomY - fade,
                                                     Ink::panel(), 0.0f, bottomY, false));
            g.fillRect (card.withTop (bottomY - fade).withBottom (bottomY));
        }
        if (follower.isLost()) drawLostBanner (g, bottom);
        drawNoCutNotice (g, r);
        if (traceFrames > 0) { --traceFrames; zs::trace ("prompter: klatka gotowa"); }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        // obrys karty tekstu na wierzchu: przewijany tekst nie zaciera krawedzi
        g.setColour (Ink::edge());
        g.drawRoundedRectangle (textCard().toFloat().reduced (0.5f), 10.0f, 1.0f);
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
        // model mowy wczytal sie po tekscie: zamienniki liczymy teraz, w tle
        if (subs == nullptr && ! recorder.isRecording()) prepareSubstitutes();
        if (! running) return;

        const bool rec = recorder.isRecording();
        refreshControls();
        if (! rec && pz.on) pz = {};            // zakonczone z pauzy
        updateTransport();

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
                removedSpans.clear(); pz = {};
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
        writeJournal();

        // Na pauzie prompter stoi tam, gdzie czlowiek wskazal. Bez tego timer
        // dopisywal znacznik wybranego wersu z czasem pauzy (drugi klik w ten sam
        // wers stawial linie przy pauzie), a spoznione slowa z rozpoznawania
        // przestawialy prompter z wybranego wersu (przeglad 14.10, nr 2 i 12).
        // Zapis rusza w chwili "Czytaj" — tu, nie w paint(): napis wisi 0,7 s
        // i pierwsze slowa ginely, a przy zaslonietym oknie paint nie przychodzi.
        if (pz.on)
        {
            clock.takeNewSyllables();
            if (listening && speech != nullptr)
            {
                VoskEngine::Hypothesis h;
                while (speech->nextHypothesis (h)) {}
                words.endUtterance();
            }
            if (pz.resuming && recorder.hasBacking())
            {
                if (recorder.takeCueFinished()) finishResume();
            }
            else if (pz.resuming && countdownStart >= 0.0
                && (juce::Time::getMillisecondCounterHiRes() - countdownStart) / 1000.0 >= countdownTotal)
                finishResume();
            repaint();
            return;
        }

        // Z rozpoznawaniem mowy sylaby jada tylko zaraz po trafionych slowach —
        // muzyka albo rozmowa w pokoju nie przesuwa wersow (WordTracker::syllablesTrusted).
        const int newSyllables = clock.takeNewSyllables();
        follower.advance (now, ! listening || words.syllablesTrusted (now) ? newSyllables : 0, clock.pauseLength());

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
        // (na "Nic dwa razy": 7 slow, z tego 6 potem uslyszanych). Liczone w
        // tle zaraz po wczytaniu tekstu (prepareSubstitutes) — tu liczone na
        // miejscu zamrazaly okno po Nagraj na kilka sekund (Adrian 25.10;
        // 244 tys. slow modelu, 92 slowa: 6,7 s). Gdy jeszcze nie gotowe,
        // to nagranie idzie bez zamiennikow.
        if (subs != nullptr && subs->done.load() && subs->vocabulary == words.vocabulary())
            for (auto& sb : subs->pairs)
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

    /** Odliczanie po R siedzi w karcie fali, poza czytanym tekstem. */
    void drawCountdown (juce::Graphics& g, juce::Rectangle<int> r)
    {
        if (countdownStart < 0.0) return;
        const double elapsed = (juce::Time::getMillisecondCounterHiRes() - countdownStart) / 1000.0;
        const auto label = countdown.label (elapsed, countdownTotal);
        if (label.empty())
        {
            countdownStart = -1.0;              // zapis ruszyl juz na "Czytaj" (timerCallback)
            return;
        }

        const bool go = label == "Czytaj";
        const bool canMove = ! preRoll.candidates.empty() && preRoll.chosen >= 0;
        auto box = r.toFloat().reduced (12.0f, 8.0f).removeFromTop (56.0f).removeFromRight (juce::jmin (360.0f, (float) r.getWidth() - 24.0f));
        g.setColour (Ink::bar().withAlpha (0.96f));
        g.fillRoundedRectangle (box, 8.0f);
        g.setColour (go ? Ink::green() : Ink::edge());
        g.drawRoundedRectangle (box.reduced (0.5f), 8.0f, 1.0f);

        auto number = box.removeFromRight (go ? 150.0f : 70.0f);   // "Czytaj" szersze niz cyfra
        g.setColour (go ? Ink::green() : Ink::textNow());
        g.setFont (juce::Font (40.0f, juce::Font::bold));
        g.drawText (juce::String (label), number, juce::Justification::centred);

        if (canMove && ! go && ! pz.on)
        {
            g.setColour (Ink::textDim());
            g.setFont (juce::Font (zs::type::small));
            g.drawFittedText (PL (punchMovedByHand ? "Miejsce cięcia: Twoje · ← →" : "← → przesuń miejsce cięcia"),
                              box.reduced (10.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 2);
        }
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
    /** Dziennik nagrania co ok. 2 s, gdy cos sie zmienilo (TakeJournal.h):
        po awarii z niego wracaja R i miejsce w tekscie. Zapis przez plik
        tymczasowy (replaceWithText), wiec urwany zapis nie psuje poprzedniego. */
    void writeJournal()
    {
        const auto nowMs = juce::Time::getMillisecondCounter();
        if (journalAtMs != 0 && nowMs - journalAtMs < 2000) return;
        journalAtMs = juce::jmax (1u, nowMs);
        const auto wav = recorder.currentFile();
        if (wav == juce::File() || lineLog.marks().empty()) return;

        tw::TakeJournal j;
        j.marks = lineLog.marks();
        j.heard = heardWords;
        j.script = script.source();
        auto text = j.toText();
        if (text == journalText) return;
        if (zs::journalFile (wav).replaceWithText (juce::String::fromUTF8 (text.c_str()), false, false, "\n"))
            journalText = std::move (text);
    }

    zs::PunchResult finishTake (const juce::File& wav)
    {
        zs::PunchResult res;
        // zwykle zatrzymanie: dziennik na wypadek awarii juz niepotrzebny
        zs::journalFile (wav).deleteFile();
        journalText.clear(); journalAtMs = 0;
        regrade();                // znaczki z pelnego nagrania, zanim zapis wersow sie wyczysci
        if (wav == juce::File()) return res;

        // Z podkladem: wycinamy dokladnie to, od czego liczyl sie rozbieg muzyki
        // (removedSpans), zawsze i bez listy do decyzji — inaczej glos po cieciu
        // wypadlby obok bitu (BackingEdit.h).
        // Bez tekstu (Adrian 25.10: "musze moc cofnac nagranie tam, gdzie chce")
        // nie ma wersow, wiec nie ma czego szukac po slowach: wycinamy dokladnie
        // to, co wskazano na fali (linia stoi juz w ciszy — PauseCut::nearest).
        if (recorder.hasBacking() || noText())
        {
            auto marks = lineLog.marks();
            res = zs::applyBackingCuts (wav, removedSpans, &marks);
            if (! marks.empty())
            {
                tw::LineLog out;
                out.begin();
                for (const auto& m : marks) out.restart (m, m.source);
                juce::String text = juce::String::fromUTF8 (out.toText().c_str());
                if (res.applied)
                    text = "# nagranie z podkladem: wyciete dokladnie miejsca wznowien (" + juce::String (res.cuts)
                         + ", razem " + juce::String (res.removedSec, 1) + " s); surowe: " + res.rawCopy.getFileName()
                         + " w folderze surowe\n" + text;
                wav.getSiblingFile (wav.getFileNameWithoutExtension() + ".wersy.txt")
                   .replaceWithText (text, false, false, "\n");
            }
            lineLog.begin();
            return res;
        }
        if (lineLog.marks().empty()) return res;

        auto marks = lineLog.marks();
        res = zs::applyPunch (wav, marks, heardWords, settings.punchAutoCut(),
                              tw::similarlyStartingLines (script, tw::lineSpans (script)));

        // lista ciec dla ekranu Odsluch (DSP/CutList.h): pewne + do wskazania
        if (! res.applied && ! res.proposals.empty())
            zs::writeCutList (wav, res.proposals);

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
                 + (res.audibleSeams > 0 ? "; polaczenia z inna cisza (posluchaj): " + juce::String (res.audibleSeams) : juce::String())
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
        if (lines.empty() && noText())
        {
            auto t = r.reduced (24, 10);
            g.setColour (Ink::textNow());
            g.setFont (juce::Font (zs::type::heading, juce::Font::bold));
            g.drawText (PL ("Nagranie bez tekstu — mów swobodnie"), t.removeFromTop (t.getHeight() / 2), juce::Justification::bottomLeft);
            g.setColour (Ink::textNext());
            g.setFont (juce::Font (zs::type::small));
            g.drawText (PL ("Pomyłka? Pauza (spacja), kliknij falę tam, skąd chcesz nagrać od nowa, i Nagraj od linii."),
                        t, juce::Justification::topLeft);
            return;
        }
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

        const juce::Colour guideCol = follower.isLost() ? Ink::lostBlue() : Ink::accent();

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

        // Wersy przy gornej i dolnej krawedzi wygaszone — zamiast polowy
        // liter ucietej ramka (tak robia promptery i listy z przewijaniem).
        {
            const float fade = juce::jmin (step * 0.6f, r.getHeight() * 0.2f);
            const auto top = r.toFloat().removeFromTop (fade);
            const auto bottom = r.toFloat().removeFromBottom (fade);
            g.setGradientFill (juce::ColourGradient (Ink::panel(), 0.0f, top.getY(),
                                                     Ink::panel().withAlpha (0.0f), 0.0f, top.getBottom(), false));
            g.fillRect (top);
            g.setGradientFill (juce::ColourGradient (Ink::panel().withAlpha (0.0f), 0.0f, bottom.getY(),
                                                     Ink::panel(), 0.0f, bottom.getBottom(), false));
            g.fillRect (bottom);
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

        g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));

        // Na gornej krawedzi celownika, jak etykieta na ramce — w srodku
        // zaslaniala koniec dlugiego wersu (przeglad wygladu 20.10).
        const int w = g.getCurrentFont().getStringWidth (label) + 18;
        auto box = juce::Rectangle<int> (guide.getRight() - w - 12, guide.getY() - 9, w, 18);

        g.setColour (Ink::panel());
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
        g.setFont (juce::Font (zs::type::small));
        g.drawText (PL ("Mów dalej — albo popraw miejsce strzałkami."),
                    box.reduced (12, 0).removeFromTop (30), juce::Justification::centredLeft);

        g.setColour (Ink::lostText());
        g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
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
    int traceFrames = 0;                ///< ile klatek jeszcze zapisac do sladu (Trace.h)

    /** Zamienniki slow nieznanych modelowi, liczone w tle dla danego tekstu. */
    struct Substitutes
    {
        std::vector<std::string> vocabulary;
        std::vector<std::pair<std::string, std::string>> pairs;
        std::atomic<bool> done { false };
    };
    std::shared_ptr<Substitutes> subs;
    juce::ThreadPool subsPool { 1 };

    void prepareSubstitutes()
    {
        if (speech == nullptr || speech->state() != VoskEngine::State::Ready || script.wordCount() == 0) return;
        auto job = std::make_shared<Substitutes>();
        job->vocabulary = words.vocabulary();
        if (subs != nullptr && subs->vocabulary == job->vocabulary) return;   // ten sam tekst
        subs = job;
        subsPool.addJob ([job, engine = speech]
        {
            job->pairs = engine->substitutesFor (job->vocabulary);
            job->done.store (true);
        });
    }
    bool listening = false;

    std::vector<Line> lines;
    std::vector<float> backingEnvelope;           ///< 100 szczytow na sekunde, poza audio callbackiem
    double previewBackingPositionSec = -1.0;      ///< tylko podglad ekranu
    std::vector<tw::LineSpan> spans;     ///< te same wersy, same numery slow
    tw::LineLog lineLog;                 ///< kiedy zaczal sie ktory wers (zapis obok nagrania)
    std::string journalText;             ///< ostatnio zapisany dziennik (TakeJournal.h)
    juce::uint32 journalAtMs = 0;
    std::vector<tw::Event> liveEvents;                 ///< zdarzenia lampki tego nagrania
    std::vector<tw::HeardWord> heardWords;             ///< co uslyszalo rozpoznawanie (straznik wyciecia)
    int lastHeard = -1;
    std::map<int, tw::LineGrade> grades;               ///< firstWord -> znaczek
    tw::Countdown countdown;             ///< po R, razem z pre-rollem
    double countdownStart = -1.0, countdownTotal = 0.0;
    double noCutNoticeAt = -1.0;                       ///< ms; R bez modelu mowy
    tw::PreRollPlan preRoll;                           ///< ostatni pre-roll (kandydaci do strzalek)
    std::vector<float> preRollHistory;                 ///< pamiec nagrania z chwili R
    double preRollRate = 48000.0;                      ///< probkowanie pamieci z chwili R
    tw::EditedView::Spans removedSpans;                ///< wycinki tego nagrania (widok fali)
    const tw::EditedView::Spans* viewSpans = nullptr;  ///< w czasie rysowania paska na zywo
    double viewNow = 0.0;
    double preRollStartSec = 0.0;
    bool punchMovedByHand = false;   ///< ms; -1 = nie odliczamy
    bool layoutDirty = true, running = false;

    /* Gdzie obraz JEST i gdzie ma dojechac, w numerach wersow. */
    float scrollNow = 0.0f, scrollTarget = 0.0f;
    mutable int shownLine = 0;

    double elapsed = 0.0;
    bool   wasRecording = false;

    Settings&    settings;
    StripTooltip stripTip;
    float        fontScale = 1.0f;
    bool         mirror = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrompterComponent)
};
