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

    static juce::Colour bg()        { return c (ui::pal().bg); }
    static juce::Colour bar()       { return c (ui::pal().bar); }
    static juce::Colour panel()     { return c (ui::pal().panel); }
    static juce::Colour raised()    { return c (ui::pal().raised); }
    static juce::Colour edge()      { return c (ui::pal().edge); }

    static juce::Colour textNow()   { return c (ui::pal().textNow); }
    static juce::Colour textNext()  { return c (ui::pal().textNext); }
    static juce::Colour textPast()  { return c (ui::pal().textPast); }
    static juce::Colour textDim()   { return c (ui::pal().textDim); }

    static juce::Colour accent()    { return c (ui::pal().accent); }
    static juce::Colour onAccent()  { return c (ui::pal().onAccent); }
    static juce::Colour select()    { return c (ui::pal().select); }

    static juce::Colour green()     { return c (ui::pal().green); }
    static juce::Colour yellow()    { return c (ui::pal().yellow); }
    static juce::Colour red()       { return c (ui::pal().red); }
    static juce::Colour clip()      { return c (ui::pal().red); }
    static juce::Colour lostBlue()  { return c (ui::pal().lostBlue); }
    static juce::Colour lostText()  { return c (ui::pal().lostText); }
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
        // Napis, ktory sie nie miesci, zawija sie albo dostaje wielokropek —
        // nigdy nie jest sciskany w poziomie (domyslne JUCE: do 70 %).
        // Sciesniony kroj czyta sie gorzej i wyglada jak inny krój.
        juce::Font::setDefaultMinimumHorizontalScaleFactor (1.0f);
        applyPalette();
    }

    /** Kolory komponentow JUCE z biezacej palety (ui::setPalette). */
    void applyPalette()
    {
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

    /** Ikony transportu, rysowane sciezkami jak w DAW-ach (kierunek D, 24.10).
        Znaki Unicode (▶ ❚❚ ■ ●) wygladaly w kazdym kroju inaczej, a "❚❚"
        w Segoe UI bywal szerszy niz caly przycisk. */
    enum class Icon { None = 0, Play, Pause, Stop, Record, Loop, ToStart };

    static void setIcon (juce::Button& b, Icon icon)
    {
        if ((int) b.getProperties().getWithDefault ("zsIcon", 0) == (int) icon) return;
        b.getProperties().set ("zsIcon", (int) icon);
        b.repaint();
    }

    static Icon iconOf (const juce::Button& b) { return (Icon) (int) b.getProperties().getWithDefault ("zsIcon", 0); }

    /** Szerokosc ikony z odstepem do napisu — do liczenia szerokosci przyciskow. */
    static constexpr int iconSpace = 16 + 8;

    static void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour c)
    {
        const auto s = r.withSizeKeepingCentre (14.0f, 14.0f);
        g.setColour (c);
        juce::Path p;
        switch (icon)
        {
            case Icon::Play:
                p.addTriangle (s.getX() + 1.5f, s.getY(), s.getX() + 1.5f, s.getBottom(), s.getRight(), s.getCentreY());
                g.fillPath (p); break;
            case Icon::Pause:
                g.fillRoundedRectangle (s.getX() + 1.0f, s.getY(), 4.5f, s.getHeight(), 1.0f);
                g.fillRoundedRectangle (s.getRight() - 5.5f, s.getY(), 4.5f, s.getHeight(), 1.0f);
                break;
            case Icon::Stop:
                g.fillRoundedRectangle (s.reduced (1.0f), 1.5f); break;
            case Icon::Record:
                g.setColour (Ink::red());
                g.fillEllipse (s); break;
            case Icon::ToStart:
                g.fillRect (s.getX(), s.getY(), 2.5f, s.getHeight());
                p.addTriangle (s.getRight(), s.getY(), s.getRight(), s.getBottom(), s.getX() + 3.0f, s.getCentreY());
                g.fillPath (p); break;
            case Icon::Loop:
            {
                const auto o = s.expanded (1.0f);
                p.addCentredArc (o.getCentreX(), o.getCentreY(), o.getWidth() / 2 - 1.0f, o.getHeight() / 2 - 1.0f,
                                 0.0f, 0.4f, 5.6f, true);
                g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                juce::Path head;
                const float ax = o.getCentreX() + std::sin (5.6f) * (o.getWidth() / 2 - 1.0f);
                const float ay = o.getCentreY() - std::cos (5.6f) * (o.getHeight() / 2 - 1.0f);
                head.addTriangle (ax - 3.5f, ay - 3.0f, ax + 3.0f, ay - 1.0f, ax - 1.0f, ay + 3.5f);
                g.fillPath (head);
                break;
            }
            case Icon::None: default: break;
        }
    }

    /** Waski przycisk: z ikona — sama ikona; bez ikony — znak sprzed pierwszej spacji. */
    static void setCompact (juce::Button& b, bool compact)
    {
        if ((bool) b.getProperties().getWithDefault ("zsCompact", false) == compact) return;
        b.getProperties().set ("zsCompact", compact);
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
            // Projekt z ChatGPT (Docs/projekt/): ciemne kolo z obrysem w kolorze
            // akcentu, cienki pierscien 86 %, czerwona kropka 54 %, wcisniety 96 %.
            auto c = recordCircle (b);
            if (down && b.isEnabled()) c = c.withSizeKeepingCentre (c.getWidth() * 0.96f, c.getHeight() * 0.96f);
            const float D = c.getWidth();
            const bool on = b.isEnabled();

            if (b.hasKeyboardFocus (true) && on)
            {
                g.setColour (Ink::select());
                g.drawEllipse (c.expanded (D * 0.035f), juce::jmax (2.0f, D * 0.028f));
            }
            g.setColour (on ? (hover ? Ink::raised() : Ink::panel()) : Ink::bar());
            g.fillEllipse (c);
            g.setColour (on ? (hover ? Ink::accent().brighter (0.25f) : Ink::accent()) : Ink::edge());
            g.drawEllipse (c.reduced (D * 0.007f), juce::jmax (1.5f, D * 0.014f));
            g.setColour (Ink::textNow().withAlpha (on ? 0.9f : 0.45f));
            g.drawEllipse (c.withSizeKeepingCentre (D * 0.86f, D * 0.86f), juce::jmax (1.2f, D * 0.014f));
            auto dot = Ink::red();
            if (! on) dot = dot.withSaturation (0.35f).withAlpha (0.65f);
            else if (down) dot = dot.darker (0.25f);
            else if (hover) dot = dot.brighter (0.15f);
            g.setColour (dot);
            g.fillEllipse (c.withSizeKeepingCentre (D * 0.54f, D * 0.54f));
            return;
        }
        // Wylaczony: jeden wyglad dla kazdego rodzaju — plaskie tlo, czytelny
        // przygaszony napis. Przezroczysty akcent (40 %) dawal szare bloto,
        // a napis 50 % ginal (przeglad wygladu 20.10).
        if (! b.isEnabled()) g.setColour (Ink::panel());
        // Nagrywanie (ikona ●): ciemna czerwien z czerwona ramka, jak w DAW-ach —
        // czerwony znaczy w programie tylko nagrywanie i przester.
        const bool recordKey = b.isEnabled() && iconOf (b) == Icon::Record;
        if (recordKey) g.setColour (Ink::red().interpolatedWith (Ink::bg(), 0.78f).withMultipliedBrightness (down ? 0.85f : hover ? 1.2f : 1.0f));
        g.fillRoundedRectangle (r, radius);

        const bool focus = b.hasKeyboardFocus (true) && b.isEnabled();
        g.setColour (focus ? Ink::accent() : recordKey ? Ink::red() : b.isEnabled() ? juce::Colour (p.outline) : Ink::edge());
        g.drawRoundedRectangle (focus ? r.reduced (1.0f) : r, radius, focus ? 2.0f : 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto p = ui::paintFor (kindOf (b));
        const auto text = ! b.isEnabled() ? Ink::textDim()
                        : iconOf (b) == Icon::Record ? Ink::textNow() : juce::Colour (p.text);

        if (kindOf (b) == ui::ButtonKind::Record)
        {
            // napis pod kolem, nie na kropce
            const auto c = recordCircle (b);
            auto r = b.getLocalBounds().toFloat().withTop (c.getBottom() + 4.0f);
            g.setColour (Ink::textNow().withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
            g.setFont (juce::Font (juce::jlimit (15.0f, 22.0f, c.getWidth() * 0.12f), juce::Font::bold));
            g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centredTop);
            return;
        }
        // margines 12 px, ale krotki napis w waskim przycisku ("A+") dostaje
        // tyle, ile zostaje — inaczej JUCE dawal w nim wielokropek
        const int textW = getTextButtonFont (b, b.getHeight()).getStringWidth (b.getButtonText());
        auto r = b.getLocalBounds().reduced (juce::jlimit (2, 12, (b.getWidth() - textW) / 2 - 1), 0);
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

        auto label = b.getButtonText();
        const bool compact = (bool) b.getProperties().getWithDefault ("zsCompact", false);
        const auto icon = iconOf (b);
        if (icon != Icon::None)
        {
            // ikona i napis razem, wysrodkowane; w waskim przycisku sama ikona
            const auto f = getTextButtonFont (b, b.getHeight());
            const bool withText = ! compact && label.isNotEmpty();
            const float w = 16.0f + (withText ? 8.0f + (float) f.getStringWidth (label) : 0.0f);
            auto box = b.getLocalBounds().toFloat().withSizeKeepingCentre (juce::jmin (w, (float) b.getWidth() - 4.0f), (float) b.getHeight());
            drawIcon (g, icon, box.removeFromLeft (16.0f), b.isEnabled() ? text : text.withMultipliedAlpha (0.8f));
            if (withText)
            {
                box.removeFromLeft (8.0f);
                g.setColour (text);
                g.setFont (f);
                g.drawText (label, box, juce::Justification::centredLeft, false);
            }
            return;
        }
        if (compact)
            label = label.upToFirstOccurrenceOf (" ", false, false);
        g.setColour (text);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (label, r, juce::Justification::centred, 1);
    }

    //==========================================================================
    // Wybor urzadzenia z JUCE (ekran "Mikrofon i sprzet") w stylu programu.
    // Zostaje komponent JUCE, bo umie ASIO, panel karty i czestotliwosc —
    // zmienia sie tylko wyglad: pismo, miernik i "<< brak >>" bez nawiasow.

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (type::body); }
    juce::Font getPopupMenuFont() override                { return juce::Font (type::body); }

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        auto text = label.getText();
        if (! (text.startsWith ("<< ") && text.endsWith (" >>"))) { LookAndFeel_V4::drawLabel (g, label); return; }

        // pusty wybor JUCE ("<< brak urządzenia >>"): przygaszony, bez nawiasow
        text = text.substring (3, text.length() - 3);
        g.fillAll (label.findColour (juce::Label::backgroundColourId));
        if (label.isBeingEdited()) return;
        g.setColour (Ink::textDim());
        g.setFont (getLabelFont (label));
        g.drawFittedText (text, getLabelBorderSize (label).subtractedFrom (label.getLocalBounds()),
                          label.getJustificationType(), 1);
    }

    void drawLevelMeter (juce::Graphics& g, int width, int height, float level) override
    {
        // jak miernik na ekranie przygotowania: plaski pasek, zielony sygnal
        const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.0f, height * 0.3f);
        g.setColour (Ink::edge());
        g.fillRoundedRectangle (r, 2.0f);
        const float shown = juce::jlimit (0.0f, 1.0f, std::sqrt (level));
        g.setColour (shown > 0.95f ? Ink::red() : Ink::green());
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * shown), 2.0f);
    }

    static constexpr float radius = 8.0f;

private:
    /** Kolo przycisku Nagraj: u gory pola, pod nim miejsce na napis i na
        obrys fokusu. */
    static juce::Rectangle<float> recordCircle (const juce::Button& b)
    {
        const auto r = b.getLocalBounds().toFloat();
        const float label = juce::jmax (24.0f, r.getHeight() * 0.18f);
        const float D = juce::jmin (r.getWidth(), r.getHeight() - label) * 0.90f;
        return { r.getCentreX() - D * 0.5f, r.getY() + D * 0.06f, D, D };
    }

    static juce::Colour focusColour (const juce::Button& b, uint32_t outline, float alpha)
    {
        return b.hasKeyboardFocus (true) && b.isEnabled() ? Ink::textNow()
                                                         : juce::Colour (outline).withAlpha (alpha);
    }
    juce::String family = juce::Font::getDefaultSansSerifFontName();
};

} // namespace zs
