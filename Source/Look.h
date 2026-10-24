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
#if ZS_FONTS
 #include "BinaryData.h"
#endif

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
/* Glebia i poswiata (kierunek E, specyfikacja Docs/projekt/ciemny-sprzet/).
   Bez DropShadow z JUCE: ten rozmywa obrazek przy kazdym rysowaniu, a ekran
   nagrywania odswieza sie 30 razy na sekunde. Kilka koncentrycznych, coraz
   slabszych ksztaltow daje ten sam miekki brzeg za ulamek kosztu. */
struct Depth
{
    /** Miekki cien pod plyta: przesuniety w dol, rozmyty na `blur` px. */
    static void shadow (juce::Graphics& g, juce::Rectangle<float> r, float corner, float blur = 12.0f,
                        float alpha = 0.42f, float dy = 4.0f)
    {
        const int steps = 7;
        for (int i = steps; i >= 1; --i)
        {
            const float k = (float) i / steps;
            g.setColour (juce::Colours::black.withAlpha (alpha * (1.0f - k) * (1.0f - k) * 0.55f));
            g.fillRoundedRectangle (r.translated (0.0f, dy).expanded (blur * k * 0.6f), corner + blur * k * 0.6f);
        }
    }

    /** Poswiata wokol ksztaltu: kilka obrysow w kolorze, coraz szerszych i slabszych. */
    static void glow (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour c,
                      float radius = 10.0f, float alpha = 0.24f)
    {
        const int steps = 6;
        for (int i = 1; i <= steps; ++i)
        {
            const float k = (float) i / steps;
            g.setColour (c.withAlpha (alpha * 1.8f * (1.0f - k) * (1.0f - k)));
            g.drawRoundedRectangle (r.expanded (radius * k), corner + radius * k, radius / steps * 1.6f);
        }
    }

    /** Poswiata do srodka — dla komponentow, ktore nie moga rysowac poza
        swoimi granicami (przyciski: JUCE przycina rysowanie do komponentu). */
    static void innerGlow (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour c,
                           float radius = 8.0f, float alpha = 0.30f)
    {
        const int steps = 6;
        for (int i = 0; i < steps; ++i)
        {
            const float k = (float) i / steps;
            g.setColour (c.withAlpha (alpha * (1.0f - k) * (1.0f - k)));
            g.drawRoundedRectangle (r.reduced (radius * k + 0.5f), juce::jmax (1.0f, corner - radius * k), radius / steps * 1.6f);
        }
    }

    /** Plyta: #202A36 (0 %) -> #171F29 (18 %) -> #111720 (100 %), obrys #303B49,
        cien 0 4 12 alpha .42, swiatlo u gory 1 px biale .07. */
    static void plate (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour base,
                       bool withShadow = true)
    {
        if (withShadow) shadow (g, r, corner);
        juce::ColourGradient grad (base.interpolatedWith (Ink::edge(), 0.55f), 0.0f, r.getY(),
                                   base, 0.0f, r.getBottom(), false);
        grad.addColour (0.18, base.interpolatedWith (Ink::edge(), 0.2f));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, corner);
        g.setColour (Ink::edge());
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + corner, r.getRight() - corner);
    }

    /** Okienko z liczba / wglebienie: #090F15, obrys #303B49, linia u gory biale .05. */
    static void well (juce::Graphics& g, juce::Rectangle<float> r, float corner = 6.0f)
    {
        g.setColour (juce::Colour (0xff090f15));
        g.fillRoundedRectangle (r, corner);
        g.setColour (Ink::edge());
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + corner, r.getRight() - corner);
    }
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
       #if ZS_FONTS
        // Kierunek E: Inter do napisow, JetBrains Mono do liczb (wkompilowane,
        // wiec wygladaja tak samo na kazdym Windowsie).
        sans     = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
        sansBold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
        mono     = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoSemiBold_ttf, BinaryData::JetBrainsMonoSemiBold_ttfSize);
       #endif
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

    /** Wybrany z kilku (brzmienie): ciemny niebieski z ramka, kreska akcentu
        z lewej i poswiata — zamiast pelnego niebieskiego wypelnienia (spec. E). */
    static void setSelected (juce::Button& b, bool on)
    {
        if ((bool) b.getProperties().getWithDefault ("zsSelected", false) == on) return;
        b.getProperties().set ("zsSelected", on);
        b.repaint();
    }
    static bool isSelected (const juce::Button& b) { return (bool) b.getProperties().getWithDefault ("zsSelected", false); }

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
                // na czerwonym przycisku (spec. E) kropka biala, inaczej czerwona
                g.setColour (c.getPerceivedBrightness() > 0.9f ? c : Ink::red());
                g.fillEllipse (s.reduced (1.5f)); break;
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

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    {
       #if ZS_FONTS
        if (mono != nullptr && f.getTypefaceName() == juce::Font::getDefaultMonospacedFontName()) return mono;
        if (sans != nullptr && f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
            return f.isBold() ? sansBold : sans;
       #endif
        return LookAndFeel_V4::getTypefaceForFont (f);
    }

    //==========================================================================
    juce::Font getTextButtonFont (juce::TextButton& b, int) override
    {
        // makiety E: wszystkie napisy na przyciskach polgrube (Inter SemiBold)
        juce::ignoreUnused (b);
        return juce::Font (type::body, juce::Font::bold);
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
            // specyfikacja E: tarcza #151D27 z obrysem #3A4654 (najechany #687585),
            // pierscien 86 % biale .18, srodek 54 % czerwony gradient
            g.setColour (on ? Ink::raised() : Ink::bar());
            g.fillEllipse (c);
            g.setColour (on ? juce::Colour (hover ? 0xff687585 : 0xff3a4654) : Ink::edge());
            g.drawEllipse (c.reduced (1.0f), 2.0f);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.18f : 0.08f));
            g.drawEllipse (c.withSizeKeepingCentre (D * 0.86f, D * 0.86f), juce::jmax (1.2f, D * 0.014f));
            auto t = juce::Colour (hover ? 0xffff6970 : 0xffff555e), bt = juce::Colour (hover ? 0xffd63846 : 0xffc92d3c);
            if (down) { t = juce::Colour (0xffd63846); bt = juce::Colour (0xffa92332); }
            if (! on) { t = t.withSaturation (0.35f).withAlpha (0.42f); bt = bt.withSaturation (0.35f).withAlpha (0.42f); }
            const auto dc = c.withSizeKeepingCentre (D * 0.54f, D * 0.54f);
            g.setGradientFill (juce::ColourGradient (t, 0.0f, dc.getY(), bt, 0.0f, dc.getBottom(), false));
            g.fillEllipse (dc);
            return;
        }
        // Kierunek E — liczby ze specyfikacji (Docs/projekt/ciemny-sprzet/).
        // Wylaczony: jeden wyglad dla kazdego rodzaju, plaska plyta i
        // przygaszony napis (przeglad wygladu 20.10: przezroczysty akcent
        // dawal szare bloto).
        const bool on = b.isEnabled();
        // Nagrywanie (ikona ●): czerwony gradient — czerwony znaczy w programie
        // tylko nagrywanie i przester.
        const bool recordKey = on && iconOf (b) == Icon::Record;
        const bool selected = on && isSelected (b);
        const bool primary = on && kindOf (b) == ui::ButtonKind::Primary && ! selected;
        const auto base = juce::Colour (p.fill);

        juce::Colour top, bottom, border;
        if (! on)              { top = bottom = Ink::panel(); border = Ink::edge().darker (0.25f); }
        else if (recordKey)    { top = juce::Colour (0xffff555e); bottom = juce::Colour (0xffc92d3c); border = juce::Colour (0xffff7a81); }
        else if (primary)      { top = base.interpolatedWith (Ink::accent(), 0.25f); bottom = base.darker (0.30f); border = Ink::accent().withAlpha (0.85f); }
        else if (selected)     { top = bottom = Ink::accent().interpolatedWith (Ink::bg(), 0.86f); border = Ink::accent().darker (0.15f); }
        else                   { top = Ink::raised().interpolatedWith (Ink::edge(), 0.18f); bottom = Ink::raised().darker (0.22f);
                                 border = Ink::edge().brighter (0.08f); }
        if (on && hover && ! down)
        {
            top = top.brighter (0.10f); bottom = bottom.brighter (0.10f);
            if (! primary && ! selected && ! recordKey) border = border.brighter (0.35f);
        }
        if (on && down) { top = bottom = bottom.darker (0.25f); r = r.translated (0.0f, 1.0f); }

        g.setGradientFill (juce::ColourGradient (top, 0.0f, r.getY(), bottom, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        if (! down && on)
        {
            g.setColour (juce::Colours::white.withAlpha (primary || recordKey ? 0.16f : 0.06f));
            g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
        }
        // wybrany / glowny po najechaniu swieca do srodka (przycisk nie moze
        // rysowac poza soba); wybrany ma jeszcze kreske akcentu z lewej
        if (selected || (primary && hover) || recordKey)
            Depth::innerGlow (g, r, radius, recordKey ? Ink::red() : Ink::accent(),
                              juce::jmin (10.0f, r.getHeight() * 0.3f), selected ? (hover ? 0.42f : 0.30f) : 0.22f);
        if (selected)
        {
            g.setColour (Ink::accent());
            g.fillRoundedRectangle (r.getX() + 1.0f, r.getY() + 5.0f, 2.5f, r.getHeight() - 10.0f, 1.2f);
        }

        const bool focus = b.hasKeyboardFocus (true) && on;
        g.setColour (focus ? Ink::accent() : border);
        g.drawRoundedRectangle (focus || selected ? r.reduced (0.5f) : r, radius, focus ? 2.0f : selected ? 1.5f : 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const auto p = ui::paintFor (kindOf (b));
        const auto text = ! b.isEnabled() ? Ink::textPast()
                        : iconOf (b) == Icon::Record ? juce::Colours::white
                        : isSelected (b) ? Ink::textNow() : juce::Colour (p.text);

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

    // ---- pola tekstowe i listy (aneks specyfikacji E, pkt 1-2) -------------
    void fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e) override
    {
        if (dynamic_cast<juce::AlertWindow*> (e.getParentComponent()) != nullptr) { LookAndFeel_V4::fillTextEditorBackground (g, w, h, e); return; }
        const auto bgc = e.findColour (juce::TextEditor::backgroundColourId);
        if (bgc.isTransparent()) return;                                      // pola "tylko do czytania" na tle
        g.setColour (e.isEnabled() ? juce::Colour (0xff111a25) : Ink::panel());
        g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h), e.isMultiLine() ? 6.0f : 5.0f);
    }
    void drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e) override
    {
        if (dynamic_cast<juce::AlertWindow*> (e.getParentComponent()) != nullptr) { LookAndFeel_V4::drawTextEditorOutline (g, w, h, e); return; }
        if (e.findColour (juce::TextEditor::outlineColourId).isTransparent()) return;
        const bool focus = e.hasKeyboardFocus (true) && ! e.isReadOnly();
        const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (focus ? 1.0f : 0.5f);
        g.setColour (focus ? Ink::accent() : e.isMouseOver (true) ? juce::Colour (0xff526273) : juce::Colour (0xff334151));
        g.drawRoundedRectangle (r, e.isMultiLine() ? 6.0f : 5.0f, focus ? 2.0f : 1.0f);
    }
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
        const bool focus = box.hasKeyboardFocus (true) || box.isPopupActive();
        g.setColour (box.isEnabled() ? juce::Colour (0xff111a25) : Ink::panel());
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (focus ? Ink::accent() : box.isMouseOver (true) ? juce::Colour (0xff657486) : juce::Colour (0xff3a4858));
        g.drawRoundedRectangle (r.reduced (focus ? 1.0f : 0.5f), 6.0f, focus ? 2.0f : 1.0f);
        // chevron 12 x 7, kreska 1,5 px
        const float cx = (float) w - 20.0f, cy = h * 0.5f;
        juce::Path chev;
        chev.startNewSubPath (cx - 6.0f, cy - 3.0f); chev.lineTo (cx, cy + 3.5f); chev.lineTo (cx + 6.0f, cy - 3.0f);
        g.setColour (juce::Colour (0xff9ba9b8).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.48f));
        g.strokePath (chev, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 36, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (Ink::raised());
        g.setColour (Ink::edge());
        g.drawRect (0, 0, w, h, 1);
    }

    // ---- suwak poziomy (specyfikacja E: tor 6 px, kciuk 16 px) ----------------
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar)
        { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s); return; }
        juce::ignoreUnused (minPos, maxPos);
        const bool on = s.isEnabled();
        const float cy = y + h * 0.5f;
        const auto track = juce::Rectangle<float> ((float) x, cy - 3.0f, (float) w, 6.0f);
        g.setColour (on ? juce::Colour (0xff0a1017) : Ink::panel());
        g.fillRoundedRectangle (track, 3.0f);
        g.setColour (juce::Colour (0xff344252));
        g.drawRoundedRectangle (track.reduced (0.5f), 3.0f, 1.0f);
        // wypelnienie: od lewej, a dla suwaka -1..+1 od srodka (tor miksu)
        const float zeroX = s.getMinimum() < 0.0 && s.getMaximum() > 0.0
                          ? (float) s.getPositionOfValue (0.0) : (float) x;
        const auto filled = juce::Rectangle<float> (juce::jmin (zeroX, pos), cy - 3.0f, std::abs (pos - zeroX), 6.0f);
        if (on && filled.getWidth() > 0.5f)
        {
            g.setGradientFill (juce::ColourGradient (Ink::accent(), filled.getX(), 0.0f, juce::Colour (0xff087dd7), filled.getRight(), 0.0f, false));
            g.fillRoundedRectangle (filled, 3.0f);
        }
        const bool over = on && s.isMouseOverOrDragging();
        const float d = over && ! s.isMouseButtonDown() ? 18.0f : 16.0f;
        const auto thumb = juce::Rectangle<float> (d, d).withCentre ({ pos, cy });
        g.setColour (on ? juce::Colour (s.isMouseButtonDown() ? 0xffb6c9d9 : 0xffdce8f3) : juce::Colour (0xff687585));
        g.fillEllipse (thumb);
        if (on)
        {
            g.setColour (juce::Colour (over ? 0xff69ccff : 0xff35b7ff));
            g.drawEllipse (thumb.reduced (1.0f), 2.0f);
        }
    }
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
    juce::Typeface::Ptr sans, sansBold, mono;
};

} // namespace zs
