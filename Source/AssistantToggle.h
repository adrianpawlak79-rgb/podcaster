/*
    AssistantToggle.h — przelacznik "Asystent" (wl./wyl.) w prawym gornym rogu.

    Maly przycisk-toggle z lampka: zapalona (kolor akcentu) = asystent wlaczony
    i sam pokazuje przewodnik przy wejsciu na ekran; zgaszona = nic sie samo nie
    pokazuje (Pomoc → "Pokaz mi na ekranie" dziala zawsze).

    Nie bierze klawiatury: Esc, spacja i reszta klawiszy zostaja dla ekranu.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Look.h"

namespace zs
{

class AssistantToggle : public juce::Button
{
public:
    static constexpr int width = 116, height = 36;

    AssistantToggle() : juce::Button ("Asystent")
    {
        setClickingTogglesState (true);
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        updateTip();
    }

    void setOn (bool on) { setToggleState (on, juce::dontSendNotification); updateTip(); repaint(); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        using I = zs::Ink;
        const bool on = getToggleState();
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? I::raised() : over ? I::raised().interpolatedWith (I::panel(), 0.5f) : I::panel());
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (on ? I::accent().withAlpha (0.7f) : I::edge());
        g.drawRoundedRectangle (r, 8.0f, 1.0f);

        // lampka-suwak: pastylka z kropka po lewej (wyl.) albo prawej (wl.)
        auto pill = juce::Rectangle<float> (30.0f, 16.0f).withCentre ({ r.getRight() - 10.0f - 15.0f, r.getCentreY() });
        g.setColour (on ? I::accent().withAlpha (0.28f) : I::bg());
        g.fillRoundedRectangle (pill, 8.0f);
        g.setColour (on ? I::accent() : I::edge());
        g.drawRoundedRectangle (pill, 8.0f, 1.0f);
        const float d = 10.0f;
        const float cx = on ? pill.getRight() - 4.0f - d / 2 : pill.getX() + 4.0f + d / 2;
        const auto dot = juce::Rectangle<float> (d, d).withCentre ({ cx, pill.getCentreY() });
        g.setColour (on ? I::accent() : I::textDim());
        g.fillEllipse (dot);

        g.setColour (on ? I::textNow() : I::textNext());
        g.setFont (juce::Font (zs::type::small));
        g.drawText (juce::String::fromUTF8 ("Asystent"),
                    juce::Rectangle<float> (r.getX() + 10.0f, r.getY(), pill.getX() - r.getX() - 12.0f, r.getHeight()),
                    juce::Justification::centredLeft);
    }

private:
    void updateTip()
    {
        setTooltip (juce::String::fromUTF8 (getToggleState()
            ? "Asystent włączony: przy wejściu na ekran sam pokaże, co gdzie jest. Kliknij, by wyłączyć."
            : "Asystent wyłączony: nic nie pokazuje się samo. Kliknij, by włączyć."));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AssistantToggle)
};

} // namespace zs
