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

        paintTabs (g);
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
};

} // namespace zs
