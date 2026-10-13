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
        g.drawText (name, foot.removeFromLeft (foot.getWidth() / 2),
                    juce::Justification::centredLeft);
        g.setColour (Ink::textNext());
        g.drawText (hint, foot, juce::Justification::centredRight);
    }
};
}
