/* Wspolny szkielet z prototypu: naglowek, kroki, tresc i stopka. */
#pragma once
#include "Look.h"

namespace zs
{
struct ScreenFrame
{
    // Uklad kierunku D (24.10): naglowek z zakladkami krokow, pod nim tresc
    // i prawy panel jako osobna, zaokraglona plyta (jak panele wtyczek).

    static juce::Rectangle<int> content (juce::Rectangle<int> bounds)
    {
        bounds.removeFromTop (ui::frame::header + ui::frame::steps);
        bounds.removeFromBottom (ui::frame::footer);
        return bounds.reduced (ui::frame::pad, ui::frame::pad - 2);
    }
    /** Dawna lewa kolumna z krokami — kroki sa teraz w naglowku. */
    static juce::Rectangle<int> nav (juce::Rectangle<int>) { return {}; }

    /** Pelny wiersz kafelkow pod nazwa programu. Osobny pas daje kazdemu
        etapowi rowna szerokosc i nie sciska polskich etykiet przy 960 px. */
    static juce::Rectangle<int> stepsArea (juce::Rectangle<int> bounds, int = 210)
    {
        bounds.removeFromTop (ui::frame::topBar);
        return bounds.removeFromTop (ui::frame::stepsRow).reduced (16, 4);
    }

    static juce::Rectangle<int> sidebar (juce::Rectangle<int> bounds)
    {
        return content (bounds).removeFromRight (ui::frame::sideWidth (bounds.getWidth()));
    }

    static juce::Rectangle<int> mainArea (juce::Rectangle<int> bounds)
    {
        auto r = content (bounds);
        r.removeFromRight (ui::frame::sideWidth (bounds.getWidth()) + ui::frame::gap);
        return r.reduced (ui::frame::margin - ui::frame::pad + 4, 14);
    }

    /** Plyta panelu (prawy panel, karty): gradient, cien, jasna krawedz u gory
        — glebia zamiast plaskiej ramki (kierunek E, Depth w Look.h). */
    static void paintPanel (juce::Graphics& g, juce::Rectangle<int> r)
    {
        Depth::plate (g, r.toFloat(), 10.0f, Ink::panel());
    }

    static constexpr int leftBlock = 270;   ///< znak + nazwa programu + nazwa nagrania

    //==========================================================================
    /** Tekst zawijany po slowach, w pelnym rozmiarze: bez sciskania liter i bez
        zmniejszania pisma (tak robi drawFittedText, gdy brakuje miejsca).
        Zwraca wysokosc, jakiej tekst potrzebuje; rysuje tylko w obrebie area. */
    static int wrapped (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                        const juce::Font& font, juce::Colour colour,
                        juce::Justification j = juce::Justification::topLeft)
    {
        const auto layout = makeLayout (text, area.getWidth(), font, colour, j);
        juce::Graphics::ScopedSaveState keep (g);
        g.reduceClipRegion (area);
        auto box = area.toFloat();
        if (j.testFlags (juce::Justification::verticallyCentred))
            box = box.withSizeKeepingCentre (box.getWidth(), layout.getHeight());
        layout.draw (g, box);
        return (int) std::ceil (layout.getHeight());
    }

    static int wrappedHeight (const juce::String& text, int width, const juce::Font& font)
    {
        return (int) std::ceil (makeLayout (text, width, font, {}, juce::Justification::topLeft).getHeight());
    }

    /** Podpowiedz klawiszy "a — x  ·  b — y  ·  c — z": gdy sie nie miesci,
        odpadaja ostatnie pozycje (najwazniejsze stoja na poczatku) — zamiast
        wielokropka w polowie slowa. */
    static juce::String fitHints (const juce::String& text, const juce::Font& font, int width)
    {
        auto items = juce::StringArray::fromTokens (text, juce::String::fromUTF8 ("·"), {});
        items.trim();
        items.removeEmptyStrings();
        const auto sep = juce::String::fromUTF8 ("   ·   ");
        while (items.size() > 1 && font.getStringWidthFloat (items.joinIntoString (sep)) > (float) width)
            items.remove (items.size() - 1);
        return items.joinIntoString (sep);
    }

    /** Naglowek tresci: etykieta kroku (18), tytul (40) i podtytul — jeden
        wiersz, a gdy sie nie miesci, dwa. Uklad (resized) i rysowanie (paint)
        pytaja o te sama liczbe, wiec przebieg nie wjedzie na podtytul. */
    static int headHeight (juce::Rectangle<int> bounds, const juce::String& subtitle)
    {
        const int lineH = (int) std::ceil (juce::Font (type::body).getHeight() * 1.25f);
        const bool two = juce::Font (type::body).getStringWidthFloat (subtitle) > (float) mainArea (bounds).getWidth();
        return 18 + 40 + (two ? 2 : 1) * lineH + 6;
    }

    /** Rysuje naglowek i zwraca tresc pod nim. */
    static juce::Rectangle<int> paintHead (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& step,
                                           const juce::String& title, const juce::String& subtitle)
    {
        auto main = mainArea (bounds);
        auto head = main.removeFromTop (headHeight (bounds, subtitle));
        g.setColour (Ink::textDim()); g.setFont (juce::Font (type::tiny, juce::Font::bold));
        g.drawText (step, head.removeFromTop (18), juce::Justification::centredLeft);
        g.setColour (Ink::textNow()); g.setFont (juce::Font (type::title, juce::Font::bold));
        g.drawText (title, head.removeFromTop (40), juce::Justification::centredLeft);
        wrapped (g, subtitle, head, juce::Font (type::body), Ink::textNext());
        return main;
    }

    /** hintsUnderMain: podpowiedz klawiszy po lewej, pod glowna kolumna (pod
        przyciskami, ktore uruchamia), bez nazwy w stopce — nazwa stoi wtedy w
        prawym panelu ekranu (Adrian 25.10, ekran nagrania). */
    static void paint (juce::Graphics& g, juce::Rectangle<int> bounds,
                       const juce::String& name, const juce::String& hint, bool hintsUnderMain = false)
    {
        const auto main = mainArea (bounds);
        auto header = bounds.removeFromTop (ui::frame::header);
        g.setColour (Ink::bar());
        g.fillRect (header);
        auto footer = bounds.removeFromBottom (ui::frame::footer);
        g.fillRect (footer);
        g.setColour (Ink::edge());
        g.drawHorizontalLine (header.getBottom() - 1, 0.0f, (float) header.getWidth());
        g.drawHorizontalLine (footer.getY(), 0.0f, (float) footer.getWidth());

        // Znak: mikrofon w zielonym pierscieniu (zielen = program, "dobrze").
        // Rysowany, wiec nie zalezy od znakow w kroju systemowym.
        {
            const juce::Rectangle<float> ring (16.0f, 14.0f, 36.0f, 36.0f);
            g.setColour (Ink::green().withAlpha (0.18f));
            g.fillEllipse (ring.expanded (3.0f));
            g.setColour (Ink::green());
            g.drawEllipse (ring, 2.0f);
            const float cx = ring.getCentreX(), cy = ring.getCentreY();
            g.drawRoundedRectangle (cx - 3.5f, cy - 10.0f, 7.0f, 12.0f, 3.5f, 1.8f);
            juce::Path mic;
            mic.startNewSubPath (cx - 7.0f, cy - 3.0f); mic.lineTo (cx - 7.0f, cy - 1.0f);
            mic.quadraticTo (cx - 7.0f, cy + 6.0f, cx, cy + 6.0f);
            mic.quadraticTo (cx + 7.0f, cy + 6.0f, cx + 7.0f, cy - 1.0f); mic.lineTo (cx + 7.0f, cy - 3.0f);
            g.strokePath (mic, juce::PathStrokeType (1.8f));
            g.drawLine (cx, cy + 6.0f, cx, cy + 10.0f, 1.8f);
        }
        // Nazwa programu, pod nia nazwa nagrania (bylo: osobno, na srodku naglowka)
        g.setColour (Ink::textNow()); g.setFont (juce::Font (18.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 ("Zielone światło"), 62, 10, leftBlock - 70, 24, juce::Justification::centredLeft);
        g.setColour (Ink::textDim()); g.setFont (juce::Font (type::small));
        g.drawText (name, 62, 33, leftBlock - 70, 20, juce::Justification::centredLeft);

        g.setFont (juce::Font (type::small)); g.setColour (Ink::textDim());
        auto foot = footer.reduced (22, 0);
        if (hintsUnderMain)
        {
            const auto under = foot.withX (main.getX()).withRight (main.getRight());
            g.setColour (Ink::textNext());
            g.drawText (fitHints (hint, juce::Font (type::small), under.getWidth()), under, juce::Justification::centredLeft);
            return;
        }
        // Nazwa tyle, ile potrzebuje (najwyzej 1/3), reszta dla podpowiedzi —
        // w waskim oknie stala polowa ucinala "Esc — wroc" (19.10, 1000 px).
        const int nameW = juce::jmin (foot.getWidth() / 3, juce::Font (type::small).getStringWidth (name) + 24);
        g.drawText (name, foot.removeFromLeft (nameW), juce::Justification::centredLeft);
        g.setColour (Ink::textNext());
        g.drawText (fitHints (hint, juce::Font (type::small), foot.getWidth()), foot, juce::Justification::centredRight);
    }

private:
    static juce::TextLayout makeLayout (const juce::String& text, int width, const juce::Font& font,
                                        juce::Colour colour, juce::Justification j)
    {
        juce::AttributedString s;
        s.setJustification (j.getOnlyHorizontalFlags() | juce::Justification::top);
        s.setWordWrap (juce::AttributedString::byWord);
        s.append (text, font, colour);
        juce::TextLayout layout;
        layout.createLayout (s, (float) juce::jmax (1, width));
        return layout;
    }
};
}
