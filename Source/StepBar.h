/* Pasek krokow ze strzalkami i ptaszkami z prototypu. */

#pragma once

#include "Look.h"

namespace zs
{

class StepBar : public juce::Component
{
public:
    std::function<void (int)> onStepClick;
    std::function<bool (int)> canClickStep;

    StepBar() { setMouseClickGrabsKeyboardFocus (false); }
    void setSteps (const juce::StringArray& names) { steps = names; repaint(); }
    void setTileMode (bool enabled = true) { tileMode = enabled; repaint(); }
    void setCurrent (int index, bool everythingDone = false)
    {
        if (index == current && everythingDone == allDone) return;
        current = index;
        allDone = everythingDone;
        repaint();
    }

    int currentStep() const noexcept { return current; }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int step = hitStep (e.position);
        setMouseCursor (isClickable (step) ? juce::MouseCursor::PointingHandCursor
                                            : juce::MouseCursor::NormalCursor);
        if (hovered != step) { hovered = step; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        if (hovered >= 0) { hovered = -1; repaint(); }
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int step = hitStep (e.position);
        if (e.mouseWasClicked() && isClickable (step) && onStepClick) onStepClick (step);
    }

    static constexpr int preferredHeight = 46;

    void paint (juce::Graphics& g) override
    {
        if (steps.isEmpty()) return;
        if (getHeight() > getWidth()) { paintVertical (g); return; }

        if (tileMode) paintTiles (g); else paintTabs (g);
    }

    /** Workflow principal: equal-width destinations, instead of numbered
        circles. The six labels stay readable because the tiles use their own
        full-width row below the product title. */
    void paintTiles (juce::Graphics& g)
    {
        const int n = steps.size();
        if (n <= 0) return;
        constexpr int gap = 8;
        const int width = juce::jmax (0, getWidth() - gap * (n - 1)) / n;
        const auto font = juce::Font (type::small, juce::Font::bold);
        const float h = (float) getHeight();
        stepBounds.clearQuick();

        for (int i = 0; i < n; ++i)
        {
            const auto state = ui::stepState (i, current, allDone);
            const bool active = state == ui::StepState::Current;
            const bool done = state == ui::StepState::Done;
            auto tile = juce::Rectangle<float> ((float) (i * (width + gap)), 0.0f,
                                                (float) (i == n - 1 ? getWidth() - i * (width + gap) : width), h);
            const bool hot = hovered == i && isClickable (i);

            g.setColour (active ? Ink::accent().withAlpha (0.13f)
                                : hot ? Ink::raised().brighter (0.12f) : Ink::raised().withAlpha (0.48f));
            g.fillRoundedRectangle (tile, 6.0f);
            g.setColour (active ? Ink::accent().withAlpha (0.82f)
                                : hot ? Ink::edge().brighter (0.32f) : Ink::edge().withAlpha (0.78f));
            g.drawRoundedRectangle (tile.reduced (0.5f), 6.0f, active ? 1.4f : 1.0f);

            const auto icon = juce::Rectangle<float> (tile.getX() + 12.0f, tile.getCentreY() - 9.0f, 18.0f, 18.0f);
            const auto ink = active ? Ink::accent() : done ? Ink::textNext() : Ink::textDim();
            drawTileIcon (g, i, icon, ink);

            const auto label = tile.withTrimmedLeft (38.0f).withTrimmedRight (8.0f).toNearestInt();
            g.setColour (active ? Ink::textNow() : done ? Ink::textNext() : Ink::textDim());
            g.setFont (font);
            g.drawFittedText (steps[i], label, juce::Justification::centredLeft, 2, 0.92f);

            if (active)
            {
                g.setColour (Ink::accent());
                g.fillRoundedRectangle (tile.withWidth (2.0f).reduced (0.0f, 8.0f), 1.0f);
            }
            stepBounds.add (tile);
        }
    }

    static void drawTileIcon (juce::Graphics& g, int index, juce::Rectangle<float> r, juce::Colour colour)
    {
        g.setColour (colour);
        juce::Path p;
        const auto c = r.getCentre();
        switch (index)
        {
            case 0: // start / home
                p.startNewSubPath (r.getX() + 1.0f, c.y - 1.0f); p.lineTo (c.x, r.getY() + 1.0f);
                p.lineTo (r.getRight() - 1.0f, c.y - 1.0f);
                p.startNewSubPath (r.getX() + 4.0f, c.y - 2.0f); p.lineTo (r.getX() + 4.0f, r.getBottom() - 1.0f);
                p.lineTo (r.getRight() - 4.0f, r.getBottom() - 1.0f); p.lineTo (r.getRight() - 4.0f, c.y - 2.0f);
                g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.drawRoundedRectangle (c.x - 2.0f, c.y + 2.0f, 4.0f, 6.0f, 1.0f, 1.4f);
                break;
            case 1: // preparation / checklist
                g.drawRoundedRectangle (r.reduced (2.0f), 2.0f, 1.4f);
                for (int k = 0; k < 3; ++k)
                {
                    const float y = r.getY() + 5.0f + k * 5.0f;
                    g.drawLine (r.getX() + 5.0f, y, r.getX() + 7.0f, y + 1.5f, 1.2f);
                    g.drawLine (r.getX() + 7.0f, y + 1.5f, r.getX() + 9.0f, y - 1.0f, 1.2f);
                    g.drawLine (r.getX() + 11.0f, y, r.getRight() - 4.0f, y, 1.2f);
                }
                break;
            case 2: // recording / waveform
                for (int k = 0; k < 5; ++k)
                {
                    const float x = r.getX() + 3.0f + k * 3.0f;
                    const float half = (k == 2 ? 7.0f : k == 1 || k == 3 ? 5.0f : 3.0f);
                    g.drawLine (x, c.y - half, x, c.y + half, 1.7f);
                }
                break;
            case 3: // review / headphones
                p.addCentredArc (c.x, c.y + 1.0f, 6.0f, 7.0f, 0.0f, 3.14f, 6.28f, true);
                g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.drawRoundedRectangle (r.getX() + 1.0f, c.y - 1.0f, 4.0f, 7.0f, 1.5f, 1.5f);
                g.drawRoundedRectangle (r.getRight() - 5.0f, c.y - 1.0f, 4.0f, 7.0f, 1.5f, 1.5f);
                break;
            case 4: // mix / three faders
                for (int k = 0; k < 3; ++k)
                {
                    const float x = r.getX() + 4.0f + k * 5.0f;
                    g.drawLine (x, r.getY() + 2.0f, x, r.getBottom() - 2.0f, 1.3f);
                    g.setColour (Ink::panel()); g.fillRoundedRectangle (x - 2.0f, r.getY() + 4.0f + (k % 2) * 5.0f, 4.0f, 4.0f, 1.0f);
                    g.setColour (colour); g.drawRoundedRectangle (x - 2.0f, r.getY() + 4.0f + (k % 2) * 5.0f, 4.0f, 4.0f, 1.0f, 1.1f);
                }
                break;
            default: // ready / check
                p.startNewSubPath (r.getX() + 3.0f, c.y);
                p.lineTo (c.x - 1.0f, r.getBottom() - 3.0f);
                p.lineTo (r.getRight() - 2.0f, r.getY() + 3.0f);
                g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                break;
        }
    }

    /** Zakladki (kierunek D, 24.10): plytka z ramka, w niej kroki — biezacy
        wypelniony akcentem, zrobione z ptaszkiem. Kazda zakladka tak szeroka
        jak jej napis; gdy wszystkie sie nie mieszcza, inne niz biezaca
        zostaja samym numerem, a gdy i to za malo — wszystkie. */
    void paintTabs (juce::Graphics& g)
    {
        // Kierunek E (makieta 24.10): kolka z numerem polaczone linia, bez
        // ramki dookola. Biezace kolko swieci, przebyta czesc linii jest
        // niebieska. Gdy brakuje miejsca: napis tylko przy biezacym, potem same kolka.
        const juce::Font font (type::small, juce::Font::bold);
        const int n = steps.size();
        const float d = 28.0f, inner = 8.0f, lineMin = 14.0f, lineMax = 48.0f, lineGap = 8.0f;
        auto labelW = [&] (int i) { return (float) font.getStringWidth (steps[i]); };

        int mode = 0;                                   // 0 pelne, 1 tylko biezaca z napisem, 2 same kolka
        auto itemsW = [&] (int m)
        {
            float t = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                t += d;
                if (m == 0 || (m == 1 && i == current && ! allDone)) t += inner + labelW (i);
            }
            return t;
        };
        auto need = [&] (int m) { return itemsW (m) + (n - 1) * (lineMin + 2 * lineGap); };
        while (mode < 2 && need (mode) > (float) getWidth()) ++mode;
        const float line = n > 1 ? juce::jlimit (lineMin, lineMax, ((float) getWidth() - itemsW (mode)) / (n - 1) - 2 * lineGap)
                                 : 0.0f;

        const float cy = getHeight() * 0.5f;
        float x = 2.0f;
        stepBounds.clearQuick();
        for (int i = 0; i < n; ++i)
        {
            const auto state = ui::stepState (i, current, allDone);
            const bool active = state == ui::StepState::Current;
            const bool done = state == ui::StepState::Done;
            const bool label = mode == 0 || (mode == 1 && active);
            const auto b = juce::Rectangle<float> (x, cy - d / 2, d, d);
            const float itemX = x;

            if (active)
            {
                Depth::glow (g, b, d / 2, Ink::accent(), 8.0f, 0.55f);
                g.setGradientFill (juce::ColourGradient (Ink::accent().brighter (0.25f), 0.0f, b.getY(),
                                                         Ink::accent().darker (0.15f), 0.0f, b.getBottom(), false));
                g.fillEllipse (b);
            }
            else
            {
                if (done && hovered == i && isClickable (i))
                    Depth::glow (g, b, d / 2, Ink::accent(), 5.0f, 0.35f);
                g.setColour (Ink::bg().darker (0.3f));
                g.fillEllipse (b);
                g.setColour (done ? Ink::select().withAlpha (0.8f) : Ink::edge().brighter (0.15f));
                g.drawEllipse (b.reduced (0.75f), 1.5f);
            }
            g.setColour (active ? Ink::onAccent() : done ? Ink::select() : Ink::textDim());
            if (done)
            {
                juce::Path tick;
                tick.startNewSubPath (b.getX() + 9.0f, b.getY() + 14.5f);
                tick.lineTo (b.getX() + 12.5f, b.getY() + 18.0f);
                tick.lineTo (b.getX() + 19.5f, b.getY() + 10.0f);
                g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else
            {
                g.setFont (juce::Font (type::small, juce::Font::bold));
                g.drawText (juce::String (i + 1), b, juce::Justification::centred);
            }
            x += d;
            if (label)
            {
                g.setColour (active ? Ink::textNow() : done ? Ink::textNext() : Ink::textDim());
                if (done && hovered == i && isClickable (i)) g.setColour (Ink::textNow());
                g.setFont (font);
                g.drawText (steps[i], juce::Rectangle<float> (x + inner, 0.0f, labelW (i) + 2.0f, (float) getHeight()),
                            juce::Justification::centredLeft, false);
                x += inner + labelW (i);
            }
            stepBounds.add (juce::Rectangle<float> (itemX, 0.0f, x - itemX, (float) getHeight()));
            if (i < n - 1)
            {
                // przebyta czesc drogi (do biezacego kroku) swieci na niebiesko
                const bool passed = allDone || i < current;
                g.setColour (passed ? Ink::accent() : Ink::edge());
                g.fillRoundedRectangle (x + lineGap, cy - 1.0f, line, 2.0f, 1.0f);
                x += line + 2 * lineGap;
            }
        }
    }

private:
    int hitStep (juce::Point<float> point) const
    {
        for (int i = 0; i < stepBounds.size(); ++i)
            if (stepBounds.getReference (i).contains (point)) return i;
        return -1;
    }
    bool isClickable (int step) const
    {
        return step >= 0 && step < steps.size()
            && ui::stepState (step, current, allDone) == ui::StepState::Done
            && onStepClick != nullptr && (! canClickStep || canClickStep (step));
    }

    /** Lewa kolumna (uklad z projektu ChatGPT, 13.10): kroki jeden pod drugim,
        biezacy w kolorze akcentu, zrobione ze znaczkiem. */
    void paintVertical (juce::Graphics& g)
    {
        const int rowH = 46, gap = 8;
        for (int i = 0; i < steps.size(); ++i)
        {
            const auto row = juce::Rectangle<int> (0, i * (rowH + gap), getWidth(), rowH).toFloat();
            const auto state = ui::stepState (i, current, allDone);
            const bool active = state == ui::StepState::Current;
            const bool done = state == ui::StepState::Done;

            if (active)
            {
                g.setColour (Ink::accent());
                g.fillRoundedRectangle (row, 10.0f);
            }
            const auto badge = juce::Rectangle<float> (row.getX() + 12.0f, row.getCentreY() - 12.0f, 24.0f, 24.0f);
            g.setColour (active ? Ink::onAccent().withAlpha (0.25f) : done ? Ink::accent() : Ink::raised());
            g.fillEllipse (badge);
            g.setColour (active ? Ink::onAccent() : done ? Ink::onAccent() : Ink::textDim());
            if (done)
            {
                juce::Path tick;
                tick.startNewSubPath (badge.getX() + 7, badge.getY() + 12);
                tick.lineTo (badge.getX() + 11, badge.getY() + 16);
                tick.lineTo (badge.getX() + 17, badge.getY() + 8);
                g.strokePath (tick, juce::PathStrokeType (2.0f));
            }
            else
            {
                g.setFont (juce::Font (type::tiny, juce::Font::bold));
                g.drawText (juce::String (i + 1), badge, juce::Justification::centred);
            }
            g.setColour (active ? Ink::onAccent() : done ? Ink::textNow() : Ink::textDim());
            g.setFont (juce::Font (type::small, active ? juce::Font::bold : juce::Font::plain));
            g.drawFittedText (steps[i], row.withTrimmedLeft (46.0f).withTrimmedRight (6.0f).toNearestInt(),
                              juce::Justification::centredLeft, 1);
        }
    }

    juce::StringArray steps;
    juce::Array<juce::Rectangle<float>> stepBounds;
    int current = 0;
    int hovered = -1;
    bool allDone = false;
    bool tileMode = false;
};

} // namespace zs
