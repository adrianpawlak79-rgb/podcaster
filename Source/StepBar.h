/* Pasek krokow ze strzalkami i ptaszkami z prototypu. */

#pragma once

#include "Look.h"

namespace zs
{

class StepBar : public juce::Component
{
public:
    void setSteps (const juce::StringArray& names) { steps = names; repaint(); }
    void setCurrent (int index, bool everythingDone = false)
    {
        if (index == current && everythingDone == allDone) return;
        current = index;
        allDone = everythingDone;
        repaint();
    }

    int currentStep() const noexcept { return current; }

    static constexpr int preferredHeight = 46;

    void paint (juce::Graphics& g) override
    {
        if (steps.isEmpty()) return;

        const int gap = 6;
        const auto widths = ui::stepWidths (getWidth(), steps.size(), gap);
        int x = 0;
        for (int i = 0; i < steps.size(); ++i)
        {
            const auto cell = juce::Rectangle<int> (x, 0, widths[(size_t) i], getHeight());
            x += widths[(size_t) i] + gap;
            const auto state = ui::stepState (i, current, allDone);
            const bool active = state == ui::StepState::Current;
            const bool done = state == ui::StepState::Done;
            const float left = (float) cell.getX(), right = (float) cell.getRight();
            const float h = (float) cell.getHeight(), arrow = 14.0f;
            juce::Path shape;
            shape.startNewSubPath (left, 0.0f);
            shape.lineTo (right - (i + 1 < steps.size() ? arrow : 0.0f), 0.0f);
            if (i + 1 < steps.size()) shape.lineTo (right, h / 2);
            shape.lineTo (right - (i + 1 < steps.size() ? arrow : 0.0f), h);
            shape.lineTo (left, h);
            if (i > 0) shape.lineTo (left + arrow, h / 2);
            shape.closeSubPath();
            g.setColour (active ? Ink::accent() : done ? Ink::accent().interpolatedWith (Ink::bar(), 0.80f)
                                                        : Ink::panel());
            g.fillPath (shape);
            const juce::Font font (type::small, juce::Font::bold);
            const int labelWidth = font.getStringWidth (steps[i]);
            const int start = cell.getCentreX() - (22 + 9 + labelWidth) / 2;
            const auto badge = juce::Rectangle<float> ((float) start, h / 2 - 11, 22, 22);
            g.setColour (done ? Ink::accent() : Ink::bg().withAlpha (0.7f));
            g.fillEllipse (badge);
            g.setColour (done ? Ink::onAccent() : active ? Ink::textNow() : Ink::textDim());
            if (done)
            {
                juce::Path tick;
                tick.startNewSubPath (badge.getX() + 6, badge.getY() + 11);
                tick.lineTo (badge.getX() + 10, badge.getY() + 15);
                tick.lineTo (badge.getX() + 16, badge.getY() + 7);
                g.strokePath (tick, juce::PathStrokeType (1.8f));
            }
            else
            {
                g.setFont (juce::Font (type::tiny, juce::Font::bold));
                g.drawText (juce::String (i + 1), badge, juce::Justification::centred);
            }
            g.setColour (active ? Ink::onAccent() : done ? Ink::accent() : Ink::textDim());
            g.setFont (font);
            g.drawFittedText (steps[i], cell.withX (start + 31).withWidth (
                juce::jmax (0, cell.getRight() - 14 - start - 31)), juce::Justification::centredLeft, 1);
        }
    }

private:
    juce::StringArray steps;
    int current = 0;
    bool allDone = false;
};

} // namespace zs
