/*
    Look.h — wspolny wyglad wszystkich ekranow.

    ---------------------------------------------------------------------------
    CO TU JEST

    - Ink: kolory znaczen ("green" znaczy "jest dobrze", a nie "zielony").
      Liczby stoja w UiRules.h, bo tam pilnuje ich test bez JUCE.
    - Look: LookAndFeel calego programu. Kroj Segoe UI, trzy rodzaje
      przyciskow, klawisz skrotu narysowany na przycisku.

    Ink stal kiedys osobno (Ink.h), a kazdy ekran sam kolorowal swoje
    przyciski. Skonczylo sie piecioma rodzajami przyciskow na trzech
    ekranach. Teraz ekran mowi tylko, JAKI to przycisk (setKind), a jak ma
    wygladac — wie wylacznie ten plik.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "UiRules.h"
#include "Fonts.h"

namespace zs
{

struct Ink
{
    static juce::Colour c (uint32_t argb) { return juce::Colour (argb); }

    static juce::Colour bg()        { return c (ui::colour::bg); }
    static juce::Colour bar()       { return c (ui::colour::bar); }
    static juce::Colour panel()     { return c (ui::colour::panel); }
    static juce::Colour raised()    { return c (ui::colour::raised); }
    static juce::Colour edge()      { return c (ui::colour::edge); }

    static juce::Colour textNow()   { return c (ui::colour::textNow); }
    static juce::Colour textNext()  { return c (ui::colour::textNext); }
    static juce::Colour textPast()  { return c (ui::colour::textPast); }
    static juce::Colour textDim()   { return c (ui::colour::textDim); }

    static juce::Colour accent()    { return c (ui::colour::accent); }
    static juce::Colour onAccent()  { return c (ui::colour::onAccent); }

    static juce::Colour green()     { return c (ui::colour::green); }
    static juce::Colour yellow()    { return c (ui::colour::yellow); }
    static juce::Colour red()       { return c (ui::colour::red); }
    static juce::Colour clip()      { return c (ui::colour::clip); }
    static juce::Colour lostBlue()  { return c (ui::colour::lostBlue); }
    static juce::Colour lostText()  { return c (ui::colour::lostText); }
};

//==============================================================================
class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        // Na Windowsie kroj z prototypu, na Linuksie systemowy zamiennik.
        const auto available = juce::Font::findAllTypefaceNames();
        for (const auto& name : { "Segoe UI Variable", "Segoe UI", "DejaVu Sans" })
            if (available.contains (name)) { family = name; break; }
        setDefaultSansSerifTypefaceName (family);
        setColour (juce::ResizableWindow::backgroundColourId, Ink::bg());
        setColour (juce::DocumentWindow::backgroundColourId, Ink::bg());
        setColour (juce::Label::textColourId, Ink::textNow());
        setColour (juce::ComboBox::backgroundColourId, Ink::raised());
        setColour (juce::ComboBox::textColourId, Ink::textNow());
        setColour (juce::ComboBox::outlineColourId, Ink::edge());
        setColour (juce::ComboBox::arrowColourId, Ink::textNext());
        setColour (juce::PopupMenu::backgroundColourId, Ink::panel());
        setColour (juce::PopupMenu::textColourId, Ink::textNow());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Ink::edge());
        setColour (juce::PopupMenu::highlightedTextColourId, Ink::textNow());
        setColour (juce::ListBox::backgroundColourId, Ink::panel());
        setColour (juce::ListBox::textColourId, Ink::textNow());
        setColour (juce::TextEditor::backgroundColourId, Ink::panel());
        setColour (juce::TextEditor::textColourId, Ink::textNow());
        setColour (juce::TextEditor::outlineColourId, Ink::edge());
        setColour (juce::TextEditor::focusedOutlineColourId, Ink::accent());
        setColour (juce::ToggleButton::textColourId, Ink::textNow());
        setColour (juce::ToggleButton::tickColourId, Ink::accent());
        setColour (juce::Slider::thumbColourId, Ink::accent());
        setColour (juce::Slider::trackColourId, Ink::edge());
        setColour (juce::ScrollBar::thumbColourId, Ink::edge());

        // przyciski rysuje drawButtonBackground/drawButtonText wedlug rodzaju,
        // ale komponenty JUCE (wybor urzadzenia) pytaja o te kolory same
        setColour (juce::TextButton::buttonColourId, Ink::raised());
        setColour (juce::TextButton::textColourOffId, Ink::textNow());
    }

    //==========================================================================
    /** Rodzaj przycisku. Domyslnie drugorzedny. */
    static void setKind (juce::Button& b, ui::ButtonKind k)
    {
        b.getProperties().set ("zsKind", (int) k);
        b.repaint();
    }

    static ui::ButtonKind kindOf (const juce::Button& b)
    {
        return static_cast<ui::ButtonKind> ((int) b.getProperties().getWithDefault (
            "zsKind", (int) ui::ButtonKind::Secondary));
    }

    /** Klawisz skrotu, rysowany na przycisku jako maly klawisz z prawej.
        Przycisk i skrot uczy sie wtedy razem, bez czytania instrukcji. */
    static void setKey (juce::Button& b, const juce::String& key)
    {
        b.getProperties().set ("zsKey", key);
        b.repaint();
    }

    //==========================================================================
    juce::Font getTextButtonFont (juce::TextButton& b, int) override
    {
        return juce::Font (type::body, kindOf (b) != ui::ButtonKind::Secondary ? juce::Font::bold
                                                                              : juce::Font::plain);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool hover, bool down) override
    {
        const auto p = ui::paintFor (kindOf (b));
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);

        const float alpha = b.isEnabled() ? 1.0f : 0.4f;
        auto fill = juce::Colour (p.fill);
        if (b.isEnabled()) fill = fill.withMultipliedBrightness (down ? 0.88f : hover ? 1.12f : 1.0f);

        g.setColour (fill.withMultipliedAlpha (alpha));
        if (kindOf (b) == ui::ButtonKind::Record)
        {
            r = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()),
                                         juce::jmin (r.getWidth(), r.getHeight()));
            g.fillEllipse (r);
            g.setColour (focusColour (b, p.outline, alpha));
            g.drawEllipse (r.reduced (1.0f), b.hasKeyboardFocus (true) ? 2.0f : 1.0f);
            return;
        }
        g.fillRoundedRectangle (r, radius);

        const bool focus = b.hasKeyboardFocus (true) && b.isEnabled();
        g.setColour (focus ? Ink::accent() : juce::Colour (p.outline).withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (focus ? r.reduced (1.0f) : r, radius, focus ? 2.0f : 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto p = ui::paintFor (kindOf (b));
        const auto text = juce::Colour (p.text).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f);

        if (kindOf (b) == ui::ButtonKind::Record)
        {
            auto r = b.getLocalBounds();
            const float d = (float) juce::jmin (r.getWidth(), r.getHeight());
            const float symbol = d * 0.18f;
            auto ring = juce::Rectangle<float> (symbol, symbol).withCentre (
                { r.getCentreX() * 1.0f, r.getCentreY() - d * 0.10f });
            g.setColour (text);
            g.drawEllipse (ring, d * 0.045f);
            g.setFont (juce::Font (juce::jlimit (19.0f, 25.0f, d * 0.12f), juce::Font::bold));
            g.drawText (b.getButtonText(), r.withY (r.getCentreY() + (int) (d * 0.05f))
                          .withHeight ((int) (d * 0.22f)), juce::Justification::centred);
            return;
        }
        auto r = b.getLocalBounds().reduced (12, 0);
        const juce::String key = b.getProperties().getWithDefault ("zsKey", {}).toString();

        if (key.isNotEmpty())
        {
            const juce::Font kf (type::tiny, juce::Font::bold);
            const int kw = juce::jmax (22, kf.getStringWidth (key) + 12);
            auto cap = r.removeFromRight (kw).withSizeKeepingCentre (kw, 20).toFloat();
            r.removeFromRight (6);

            g.setColour (text.withMultipliedAlpha (0.55f));
            g.drawRoundedRectangle (cap, 4.0f, 1.0f);
            g.setFont (kf);
            g.drawText (key, cap, juce::Justification::centred);
        }

        g.setColour (text);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (b.getButtonText(), r, juce::Justification::centred, 1);
    }

    static constexpr float radius = 8.0f;

private:
    static juce::Colour focusColour (const juce::Button& b, uint32_t outline, float alpha)
    {
        return b.hasKeyboardFocus (true) && b.isEnabled() ? Ink::textNow()
                                                         : juce::Colour (outline).withAlpha (alpha);
    }
    juce::String family = juce::Font::getDefaultSansSerifFontName();
};

} // namespace zs
