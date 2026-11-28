/*
    HelpComponent.h — okno "Jak to dziala" i komunikat powitalny.

    ---------------------------------------------------------------------------
    PO CO

    Program jest dla kogos, kto nigdy nie nagrywal podcastu. Pomoc ma byc
    jedna strona, ktora da sie przeczytac w minute: piec krokow programu,
    skroty klawiszy i co zrobic, gdy cos nie dziala. Bez osobnego pliku, bez
    internetu.

    Okno jest nakladka na ekran (jak okno dialogowe, ale w stylu programu):
    zamyka je Esc, klikniecie poza oknem i przycisk. Otwiera sie na kroku,
    w ktorym czlowiek wlasnie jest. Przycisk "Pokaz mi na ekranie" uruchamia
    przewodnik z dymkami (TourOverlay.h).

    Ten sam komponent pokazuje komunikat powitalny przy pierwszym
    uruchomieniu (Settings::welcomeShown).

    Teksty: nazwy przyciskow i folderow sa takie, jak w programie
    (sprawdzone w kodzie: "Nagraj od nowa", "Nagraj od linii", "Powtorz
    wers", "Folder nagran", folder "surowe (oryginaly)").
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "Look.h"
#include "ScreenFrame.h"
#include "TourOverlay.h"

namespace zs
{

namespace help
{
    inline juce::String T (const char* s) { return juce::String::fromUTF8 (s); }

    struct StepText { const char* title; const char* text; };

    /** Pieć krokow — te same nazwy, co zakladki na gorze ekranu. */
    inline const StepText* steps()
    {
        static const StepText s[5] = {
            { "Przygotowanie",
              "Na ekranie startowym wpisz nazwę projektu, wybierz „Sam głos” albo „Głos z podkładem” i kliknij Start. "
              "W przygotowaniu wskaż mikrofon, wczytaj tekst do promptera (plik TXT albo wpisany) i ewentualnie muzykę. "
              "Przy pierwszym razie program sam sprawdzi sprzęt — to chwila, a od tego zależy dobry dźwięk. Potem „Przejdź do nagrania →”." },
            { "Nagranie",
              "Naciśnij Nagraj albo spację i czytaj tekst z promptera. Zielona lampka mówi, że jest dobrze. "
              "Pomyłka? Naciśnij R albo „Powtórz wers” i przeczytaj wers jeszcze raz — nagranie leci dalej. "
              "Pauza to spacja; kliknij wers albo falę i wybierz „Nagraj od linii”. Koniec: „Zakończ” albo Esc." },
            { "Odsłuch i montaż",
              "Po nagraniu słuchasz surowego nagrania, jeszcze bez miksu. Kliknij falę, żeby posłuchać od tego miejsca; "
              "przeciągnij po fali, żeby zaznaczyć fragment, i kliknij „Wytnij zaznaczenie”. "
              "Pomyłka w środku? „Popraw fragment” pozwala nagrać go jeszcze raz (poprawka). Gdy jest dobrze: „Dalej: miks →”." },
            { "Miks",
              "Program sam wyrównuje głośność i poprawia brzmienie głosu. Wybierz jedno z trzech brzmień "
              "i przełączaj „przed” i „po” (klawisz Tab), żeby usłyszeć różnicę. "
              "Gdy brzmi dobrze, kliknij „Zapisz miks”. Szczegóły są w „Zaawansowane ustawienia” — zwykle nie trzeba ich ruszać." },
            { "Gotowe",
              "Plik WAV i MP3 leżą w folderze nagrań — „Pokaż w folderze” otworzy go. "
              "Oryginalne nagranie i poprzednie wersje zostają w podfolderze „surowe (oryginały)”. "
              "Chcesz zacząć odcinek od początku? „Nagraj od nowa”. Kolejny odcinek: „Nowy projekt”." } };
        return s;
    }

    struct KeyRow { const char* keys; const char* what; };   // keys == nullptr: nagłówek grupy

    inline const std::vector<KeyRow>& keyRows()
    {
        static const std::vector<KeyRow> r = {
            { nullptr, "NAGRANIE" },
            { "Spacja",          "Zacznij nagrywać; w trakcie — pauza i wznowienie" },
            { "R",               "Powtórz wers: tekst wraca na początek wersu, nagranie leci dalej" },
            { "Esc",             "Zakończ nagranie i przejdź do odsłuchu (przed nagraniem: wyjście z promptera)" },
            { "↑ ↓ ← →   Home",  "Przewiń tekst w prompterze; Home — na początek" },
            { "+  /  −",         "Większy i mniejszy tekst w prompterze" },
            { "L",               "Lustro — odbicie tekstu do szybki promptera" },
            { nullptr, "ODSŁUCH I MONTAŻ" },
            { "Spacja",          "Odtwórz i zatrzymaj" },
            { "← →   Home",      "Przewiń o 2 sekundy; Home — na początek" },
            { "+  −  0",         "Przybliż falę, oddal ją, pokaż całość" },
            { "N",               "Następne miejsce do sprawdzenia" },
            { "Delete",          "Wytnij zaznaczony fragment" },
            { "Enter  Backspace","Na liście cięć: zaznacz do wycięcia / zostaw" },
            { "Esc",             "Wróć o krok (z miksu do montażu, z montażu do nagrywania)" },
            { nullptr, "MIKS" },
            { "Tab",             "Przełącz „przed” i „po” miksie" },
            { "Spacja",          "Odtwórz i zatrzymaj" },
            { "Esc",             "Wróć do montażu" } };
        return r;
    }

    struct Trouble { const char* problem; const char* fix; };

    inline const Trouble* trouble()
    {
        static const Trouble t[5] = {
            { "Nie słychać mikrofonu albo lampka nie reaguje",
              "Wejdź w Ustawienia i kliknij „Sprawdź sprzęt”. Upewnij się, że mikrofon jest podłączony, wybrany na liście "
              "i niewyciszony w Windows." },
            { "MP3 nie powstał",
              "Windows w wersji N nie ma kodera MP3. Doinstaluj od Microsoftu „Media Feature Pack” i zapisz miks jeszcze raz. "
              "Plik WAV jest gotowy i można go używać." },
            { "Program się zawiesił",
              "Zamknij go i uruchom jeszcze raz — nagranie jest zapisane na dysku. Jeśli to wraca, wyślij plik slad.txt "
              "z folderu %APPDATA%\\Zielone swiatlo (wklej tę ścieżkę w pasek adresu Eksploratora)." },
            { "Niebieskie okno Windows przy instalacji",
              "Windows pyta o zaufanie, bo instalator nie ma podpisu cyfrowego. Kliknij „Więcej informacji”, a potem „Uruchom mimo to”." },
            { "Nie wiem, gdzie są nagrania",
              "W Ustawieniach kliknij „Folder nagrań…”. Wersje sprzed poprawek leżą obok, w folderze „surowe (oryginały)”." } };
        return t;
    }
}

//==============================================================================
/** Tresc okna pomocy: rysuje wybrana zakladke w podanej szerokosci. */
class HelpContent : public juce::Component
{
public:
    enum Tab { Steps = 0, Keys = 1, Trouble = 2 };

    void setTab (Tab t) { tab = t; setSize (getWidth(), run (nullptr, getWidth())); repaint(); }
    void setCurrentStep (int s) { current = s; }
    void setWidthAndLayout (int w) { setSize (w, run (nullptr, w)); }

    /** Pionowe polozenie kroku (do przewiniecia na biezacy). */
    int stepTop (int i) const { return i >= 0 && i < 5 ? stepY[i] : 0; }

    void paint (juce::Graphics& g) override { run (&g, getWidth()); }

private:
    Tab tab = Steps;
    int current = -1;
    int stepY[5] = {};

    static juce::Font body() { return juce::Font (15.5f); }

    /** Uklada (g == nullptr) albo rysuje tresc; zwraca wysokosc. */
    int run (juce::Graphics* g, int width)
    {
        using I = zs::Ink;
        const int pad = 4;
        int y = pad;

        if (tab == Steps)
        {
            const auto* s = help::steps();
            for (int i = 0; i < 5; ++i)
            {
                const int textX = 56, textW = width - textX - 14;
                const auto title = help::T (s[i].title), text = help::T (s[i].text);
                const int th = zs::ScreenFrame::wrappedHeight (text, textW, body());
                const int h = 12 + 24 + 4 + th + 12;
                stepY[i] = y;
                if (g != nullptr)
                {
                    const juce::Rectangle<int> row (0, y, width, h);
                    if (i == current)
                    {
                        g->setColour (I::raised()); g->fillRoundedRectangle (row.toFloat(), 8.0f);
                        g->setColour (I::accent()); g->fillRoundedRectangle (juce::Rectangle<float> (0.0f, (float) y + 8.0f, 4.0f, (float) h - 16.0f), 2.0f);
                    }
                    const juce::Rectangle<float> badge (14.0f, (float) y + 12.0f, 28.0f, 28.0f);
                    g->setColour (i == current ? I::accent() : I::edge());
                    g->fillEllipse (badge);
                    g->setColour (i == current ? I::onAccent() : I::textNow());
                    g->setFont (juce::Font (15.0f, juce::Font::bold));
                    g->drawText (juce::String (i + 1), badge.toNearestInt(), juce::Justification::centred);
                    g->setColour (I::textNow()); g->setFont (juce::Font (18.0f, juce::Font::bold));
                    g->drawText (title, textX, y + 12, textW - (i == current ? 120 : 0), 24, juce::Justification::centredLeft);
                    if (i == current)
                    {
                        g->setColour (I::accent()); g->setFont (juce::Font (zs::type::tiny, juce::Font::bold));
                        g->drawText (help::T ("TU JESTEŚ"), width - 14 - 110, y + 12, 110, 24, juce::Justification::centredRight);
                    }
                    zs::ScreenFrame::wrapped (*g, text, { textX, y + 12 + 24 + 4, textW, th }, body(), I::textNext());
                }
                y += h + 6;
            }
            return y + pad;
        }

        if (tab == Keys)
        {
            const int keyW = juce::jmin (190, width / 3), gapX = 14, textX = keyW + gapX, textW = width - textX - 6;
            for (const auto& r : help::keyRows())
            {
                if (r.keys == nullptr)
                {
                    y += 10;
                    if (g != nullptr)
                    {
                        g->setColour (I::textDim()); g->setFont (juce::Font (zs::type::tiny, juce::Font::bold));
                        g->drawText (help::T (r.what), 0, y, width, 18, juce::Justification::centredLeft);
                    }
                    y += 22;
                    continue;
                }
                const auto what = help::T (r.what);
                const int th = zs::ScreenFrame::wrappedHeight (what, textW, body());
                const int h = juce::jmax (30, th + 6);
                if (g != nullptr)
                {
                    const auto keys = help::T (r.keys);
                    const juce::Font kf (14.5f, juce::Font::bold);
                    const int cw = juce::jmin (keyW, (int) std::ceil (kf.getStringWidthFloat (keys)) + 20);
                    const juce::Rectangle<float> chip (0.0f, (float) y + 1.0f, (float) cw, 26.0f);
                    g->setColour (I::raised()); g->fillRoundedRectangle (chip, 6.0f);
                    g->setColour (I::edge());   g->drawRoundedRectangle (chip.reduced (0.5f), 6.0f, 1.0f);
                    g->setColour (I::textNow()); g->setFont (kf);
                    g->drawText (keys, chip.toNearestInt(), juce::Justification::centred);
                    zs::ScreenFrame::wrapped (*g, what, { textX, y + 3, textW, th }, body(), I::textNext());
                }
                y += h;
            }
            return y + pad;
        }

        // Gdy cos nie dziala
        const auto* t = help::trouble();
        for (int i = 0; i < 5; ++i)
        {
            const int w = width - 6;
            const auto fix = help::T (t[i].fix);
            const int th = zs::ScreenFrame::wrappedHeight (fix, w - 28, body());
            const int h = 12 + 24 + 2 + th + 12;
            if (g != nullptr)
            {
                const juce::Rectangle<float> card (0.0f, (float) y, (float) w, (float) h);
                g->setColour (I::raised()); g->fillRoundedRectangle (card, 8.0f);
                g->setColour (I::yellow()); g->fillRoundedRectangle (juce::Rectangle<float> (0.0f, (float) y + 8.0f, 4.0f, (float) h - 16.0f), 2.0f);
                g->setColour (I::textNow()); g->setFont (juce::Font (17.0f, juce::Font::bold));
                g->drawText (help::T (t[i].problem), 18, y + 12, w - 28, 24, juce::Justification::centredLeft);
                zs::ScreenFrame::wrapped (*g, fix, { 18, y + 12 + 24 + 2, w - 28, th }, body(), I::textNext());
            }
            y += h + 8;
        }
        return y + pad;
    }
};

//==============================================================================
class HelpComponent : public juce::Component
{
public:
    std::function<void()> onClose;
    std::function<void()> onShowTour;        ///< "Pokaz mi na ekranie"
    std::function<void()> onWelcomeStart;    ///< "Zaczynam"
    std::function<void()> onWelcomeTour;     ///< "Pokaz, jak to dziala"

    HelpComponent()
    {
        setWantsKeyboardFocus (true);
        setInterceptsMouseClicks (true, true);

        closeButton.setButtonText (help::T ("Zamknij"));
        closeButton.onClick = [this] { close(); };
        zs::Look::setKind (closeButton, zs::ui::ButtonKind::Secondary);

        const char* names[3] = { "Pięć kroków", "Skróty klawiszy", "Gdy coś nie działa" };
        for (int i = 0; i < 3; ++i)
        {
            tabs[i].setButtonText (help::T (names[i]));
            tabs[i].onClick = [this, i] { selectTab (i); };
            zs::Look::setKind (tabs[i], zs::ui::ButtonKind::Secondary);
            addChildComponent (tabs[i]);
        }

        tourButton.setButtonText (help::T ("Pokaż mi na ekranie"));
        tourButton.onClick = [this] { if (onShowTour) onShowTour(); };
        zs::Look::setKind (tourButton, zs::ui::ButtonKind::Primary);

        welcomeTour.setButtonText (help::T ("Pokaż, jak to działa"));
        welcomeTour.onClick = [this] { if (onWelcomeTour) onWelcomeTour(); };
        zs::Look::setKind (welcomeTour, zs::ui::ButtonKind::Primary);
        welcomeStart.setButtonText (help::T ("Zaczynam"));
        welcomeStart.onClick = [this] { if (onWelcomeStart) onWelcomeStart(); };
        zs::Look::setKind (welcomeStart, zs::ui::ButtonKind::Secondary);

        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (10);
        for (auto* c : { (juce::Component*) &closeButton, (juce::Component*) &tourButton, (juce::Component*) &welcomeTour,
                         (juce::Component*) &welcomeStart, (juce::Component*) &viewport })
        {
            c->setWantsKeyboardFocus (false);
            addChildComponent (c);
        }
        setVisible (false);
    }

    /** Okno "Jak to dziala". step: 0..4 (Przygotowanie..Gotowe), -1 = bez wyrozniania.
        tourAvailable: czy biezacy ekran ma przewodnik; tourSeen: czy go juz obejrzano. */
    void showHelp (int step, bool tourAvailable, bool tourSeen)
    {
        mode = Mode::Help;
        current = step;
        seen = tourSeen;
        content.setCurrentStep (step);
        tourButton.setEnabled (tourAvailable);
        tourButton.setTooltip (tourAvailable ? juce::String() : help::T ("Na tym ekranie nie ma przewodnika."));
        open();
        selectTab (0, true);
    }

    /** Komunikat powitalny (pierwsze uruchomienie). */
    void showWelcome()
    {
        mode = Mode::Welcome;
        open();
    }

    bool isWelcome() const { return mode == Mode::Welcome; }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        using I = zs::Ink;
        g.setColour (juce::Colours::black.withAlpha (0.66f));
        g.fillAll();
        if (mode == Mode::Welcome) { paintWelcome (g); return; }

        zs::Depth::plate (g, panel.toFloat(), 14.0f, I::panel());
        auto head = panel.reduced (pad, 0).removeFromTop (headH);
        g.setColour (I::textNow()); g.setFont (juce::Font (zs::type::title, juce::Font::bold));
        g.drawText (help::T ("Jak to działa"), head.withTrimmedRight (120), juce::Justification::centredLeft);

        g.setColour (I::edge());
        g.drawHorizontalLine (footer.getY() - 6, (float) panel.getX() + pad, (float) panel.getRight() - pad);
        g.setColour (I::textDim()); g.setFont (juce::Font (zs::type::small));
        const auto hintArea = footer.withTrimmedLeft (tourButton.getWidth() + 14);
        g.drawText (tourButton.isEnabled()
                        ? (seen ? help::T ("Przewodnik po tym ekranie: już obejrzany.") : help::T ("Dymki wskażą przyciski na tym ekranie."))
                        : juce::String(),
                    hintArea, juce::Justification::centredLeft);
        g.drawText (help::T ("Esc — zamknij"), hintArea, juce::Justification::centredRight);
    }

    void resized() override
    {
        const auto W = getLocalBounds();
        if (mode == Mode::Welcome)
        {
            const int w = juce::jmin (540, W.getWidth() - 32);
            const int textH = zs::ScreenFrame::wrappedHeight (welcomeText(), w - 2 * 28, juce::Font (zs::type::body));
            const int h = 28 + 38 + 8 + textH + 22 + 42 + 28;
            panel = juce::Rectangle<int> (w, h).withCentre (W.getCentre());
            auto row = panel.reduced (28, 28).removeFromBottom (42);
            welcomeStart.setBounds (row.removeFromRight (120)); row.removeFromRight (10);
            welcomeTour.setBounds (row.removeFromRight (210));
            return;
        }

        panel = juce::Rectangle<int> (juce::jmin (860, W.getWidth() - 32), juce::jmin (660, W.getHeight() - 32)).withCentre (W.getCentre());
        auto in = panel.reduced (pad, 0);
        auto head = in.removeFromTop (headH);
        closeButton.setBounds (head.removeFromRight (100).withSizeKeepingCentre (100, 36));
        auto tabRow = in.removeFromTop (38);
        int x = tabRow.getX();
        const int tw[3] = { 150, 170, 190 };
        for (int i = 0; i < 3; ++i) { tabs[i].setBounds (x, tabRow.getY(), tw[i], 36); x += tw[i] + 8; }
        footer = in.removeFromBottom (44);
        in.removeFromBottom (12);
        in.removeFromTop (12);
        tourButton.setBounds (footer.removeFromLeft (220).withSizeKeepingCentre (220, 38));
        viewport.setBounds (in);
        content.setWidthAndLayout (viewport.getMaximumVisibleWidth());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! panel.contains (e.getPosition())) close();     // klik poza oknem zamyka
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.getKeyCode() == juce::KeyPress::escapeKey) { close(); return true; }
        if (mode == Mode::Welcome && k.getKeyCode() == juce::KeyPress::returnKey) { welcomeStart.onClick(); return true; }
        return true;     // reszta klawiszy nie trafia do ekranu pod spodem
    }

    void close()
    {
        setVisible (false);
        if (onClose) onClose();
    }

    /** Przewija na ekran: do kroku biezacego. */
    void selectTab (int i, bool scrollToCurrent = false)
    {
        for (int t = 0; t < 3; ++t) zs::Look::setSelected (tabs[t], t == i);
        content.setTab ((HelpContent::Tab) i);
        content.setWidthAndLayout (viewport.getMaximumVisibleWidth());
        viewport.setViewPosition (0, (scrollToCurrent && i == 0 && current >= 0)
                                         ? juce::jmax (0, content.stepTop (current) - 4) : 0);
        repaint();
    }

private:
    enum class Mode { Help, Welcome };
    Mode mode = Mode::Help;
    static constexpr int pad = 24, headH = 64;

    juce::Rectangle<int> panel, footer;
    juce::TextButton closeButton, tourButton, welcomeTour, welcomeStart;
    juce::TextButton tabs[3];
    juce::Viewport viewport;
    HelpContent content;
    int current = -1;
    bool seen = false;

    static juce::String welcomeText()
    {
        return help::T ("Zielone światło poprowadzi Cię od mikrofonu do gotowego pliku MP3. "
                        "Chcesz zobaczyć w minutę, co gdzie jest? Pomoc znajdziesz potem pod przyciskiem „Pomoc”.");
    }

    void open()
    {
        const bool isHelp = mode == Mode::Help;
        closeButton.setVisible (isHelp); tourButton.setVisible (isHelp); viewport.setVisible (isHelp);
        for (auto& t : tabs) t.setVisible (isHelp);
        welcomeTour.setVisible (! isHelp); welcomeStart.setVisible (! isHelp);
        setVisible (true);
        toFront (true);
        resized();
        grabKeyboardFocus();
        repaint();
    }

    void paintWelcome (juce::Graphics& g)
    {
        using I = zs::Ink;
        zs::Depth::plate (g, panel.toFloat(), 14.0f, I::panel());
        auto a = panel.reduced (28, 28);
        g.setColour (I::green());
        g.fillEllipse (juce::Rectangle<float> (0.0f, 0.0f, 14.0f, 14.0f).withCentre ({ (float) a.getX() + 7.0f, (float) a.getY() + 19.0f }));
        g.setColour (I::textNow()); g.setFont (juce::Font (zs::type::title, juce::Font::bold));
        g.drawText (help::T ("Cześć!"), a.removeFromTop (38).withTrimmedLeft (26), juce::Justification::centredLeft);
        a.removeFromTop (8);
        zs::ScreenFrame::wrapped (g, welcomeText(), a, juce::Font (zs::type::body), I::textNext());
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpComponent)
};

} // namespace zs
