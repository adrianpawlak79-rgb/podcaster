/* Wspolny szkielet z prototypu: naglowek, kroki, tresc i stopka. */
#pragma once
#include "Look.h"

namespace zs
{
struct ScreenFrame
{
    static juce::Rectangle<int> content (juce::Rectangle<int> bounds)
    {
        bounds.removeFromTop (ui::frame::header + ui::frame::steps);
        bounds.removeFromBottom (ui::frame::footer);
        bounds.removeFromLeft (ui::frame::navWidth (bounds.getWidth()));
        return bounds;
    }
    /** Lewa kolumna z krokami. */
    static juce::Rectangle<int> nav (juce::Rectangle<int> bounds)
    {
        const int w = ui::frame::navWidth (bounds.getWidth());
        bounds.removeFromTop (ui::frame::header);
        bounds.removeFromBottom (ui::frame::footer);
        return bounds.removeFromLeft (w);
    }
    /** Gdzie stoi pasek krokow (StepBar rysuje sie pionowo, gdy jest wyzszy niz szerszy). */
    static juce::Rectangle<int> stepsArea (juce::Rectangle<int> bounds)
    {
        return nav (bounds).reduced (12, 20).withTrimmedRight (2);
    }

    static juce::Rectangle<int> sidebar (juce::Rectangle<int> bounds)
    {
        return content (bounds).removeFromRight (ui::frame::sideWidth (bounds.getWidth()));
    }

    static juce::Rectangle<int> mainArea (juce::Rectangle<int> bounds)
    {
        auto r = content (bounds);
        r.removeFromRight (ui::frame::sideWidth (bounds.getWidth()));
        return r.reduced (ui::frame::margin, 24);
    }

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

    static void paint (juce::Graphics& g, juce::Rectangle<int> bounds,
                       const juce::String& name, const juce::String& hint)
    {
        const auto navCol = nav (bounds);
        auto header = bounds.removeFromTop (ui::frame::header);
        g.setColour (Ink::bar());
        g.fillRect (header);
        g.fillRect (bounds.removeFromTop (ui::frame::steps));
        {
            g.setColour (Ink::bar());
            g.fillRect (navCol);
            g.setColour (Ink::edge());
            g.drawVerticalLine (navCol.getRight() - 1, (float) navCol.getY(), (float) navCol.getBottom());
        }
        auto footer = bounds.removeFromBottom (ui::frame::footer);
        g.setColour (Ink::bar());
        g.fillRect (footer);
        g.setColour (Ink::edge());
        g.drawHorizontalLine (header.getBottom() - 1, 0.0f, (float) header.getWidth());
        g.drawHorizontalLine (bounds.getY(), 0.0f, (float) bounds.getWidth());
        g.drawHorizontalLine (footer.getY(), 0.0f, (float) footer.getWidth());

        // Mikrofon jest rysowany, wiec nie zalezy od znakow w kroju systemowym. Zielony: to znak programu.
        g.setColour (Ink::green());
        g.drawRoundedRectangle (28.0f, 21.0f, 8.0f, 16.0f, 4.0f, 2.0f);
        juce::Path mic;
        mic.startNewSubPath (24.0f, 30.0f); mic.lineTo (24.0f, 34.0f);
        mic.quadraticTo (24.0f, 42.0f, 32.0f, 42.0f);
        mic.quadraticTo (40.0f, 42.0f, 40.0f, 34.0f); mic.lineTo (40.0f, 30.0f);
        g.strokePath (mic, juce::PathStrokeType (2.0f));
        g.drawLine (32.0f, 42.0f, 32.0f, 47.0f, 2.0f);
        g.drawLine (27.0f, 47.0f, 37.0f, 47.0f, 2.0f);
        g.setColour (Ink::textNow()); g.setFont (juce::Font (18.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 ("Zielone światło"), 54, 0, 230, header.getHeight(),
                    juce::Justification::centredLeft);
        g.setFont (juce::Font (type::body, juce::Font::bold));
        g.drawText (name, 300, 0, juce::jmax (0, header.getWidth() - 520), header.getHeight(),
                    juce::Justification::centredLeft);
        g.setFont (juce::Font (type::small)); g.setColour (Ink::textDim());
        auto foot = footer.reduced (22, 0);
        // Nazwa tyle, ile potrzebuje (najwyzej 1/3), reszta dla podpowiedzi —
        // w waskim oknie stala polowa ucinala "Esc — wroc" (19.10, 1000 px).
        const int nameW = juce::jmin (foot.getWidth() / 3, juce::Font (type::small).getStringWidth (name) + 24);
        g.drawText (name, foot.removeFromLeft (nameW), juce::Justification::centredLeft);
        g.setColour (Ink::textNext());
        g.drawFittedText (hint, foot, juce::Justification::centredRight, 1);
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
