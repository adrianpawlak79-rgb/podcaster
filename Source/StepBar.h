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
        if (getHeight() > getWidth()) { paintVertical (g); return; }

        paintTabs (g);
    }

    /** Zakladki (kierunek D, 24.10): plytka z ramka, w niej kroki — biezacy
        wypelniony akcentem, zrobione z ptaszkiem. Kazda zakladka tak szeroka
        jak jej napis; gdy wszystkie sie nie mieszcza, inne niz biezaca
        zostaja samym numerem, a gdy i to za malo — wszystkie. */
    void paintTabs (juce::Graphics& g)
    {
        const juce::Font font (type::small, juce::Font::bold);
        const int badge = 22, padX = 10, inner = 8, gap = 2, frame = 3;
        const int n = steps.size();
        auto full = [&] (int i) { return padX + badge + inner + font.getStringWidth (steps[i]) + padX + 4; };
        const int compactW = padX + badge + padX;

        int mode = 0;                                   // 0 pelne, 1 tylko biezaca z napisem, 2 same numery
        auto total = [&] (int m)
        {
            int t = 2 * frame + gap * (n - 1);
            for (int i = 0; i < n; ++i)
                t += (m == 0 || (m == 1 && i == current && ! allDone)) ? full (i) : compactW;
            return t;
        };
        while (mode < 2 && total (mode) > getWidth()) ++mode;

        auto box = getLocalBounds().toFloat().withWidth ((float) juce::jmin (getWidth(), total (mode)));
        g.setColour (Ink::bg());
        g.fillRoundedRectangle (box, 7.0f);
        g.setColour (Ink::edge().withAlpha (0.8f));
        g.drawRoundedRectangle (box.reduced (0.5f), 7.0f, 1.0f);

        float x = (float) frame;
        for (int i = 0; i < n; ++i)
        {
            const auto state = ui::stepState (i, current, allDone);
            const bool active = state == ui::StepState::Current;
            const bool done = state == ui::StepState::Done;
            const bool label = mode == 0 || (mode == 1 && active);
            const float w = (float) (label ? full (i) : compactW);
            const auto tab = juce::Rectangle<float> (x, (float) frame, w, (float) getHeight() - 2.0f * frame);
            x += w + (float) gap;

            if (active)
            {
                g.setColour (Ink::accent());
                g.fillRoundedRectangle (tab, 5.0f);
            }
            const auto b = juce::Rectangle<float> (tab.getX() + padX, tab.getCentreY() - badge / 2.0f, (float) badge, (float) badge);
            g.setColour (active ? Ink::onAccent().withAlpha (0.22f) : done ? Ink::raised().brighter (0.15f) : Ink::raised());
            g.fillEllipse (b);
            g.setColour (active ? Ink::onAccent() : done ? Ink::select() : Ink::textDim());
            if (done)
            {
                juce::Path tick;
                tick.startNewSubPath (b.getX() + 6.5f, b.getY() + 11.5f);
                tick.lineTo (b.getX() + 10.0f, b.getY() + 15.0f);
                tick.lineTo (b.getX() + 16.0f, b.getY() + 7.5f);
                g.strokePath (tick, juce::PathStrokeType (1.8f));
            }
            else
            {
                g.setFont (juce::Font (type::tiny, juce::Font::bold));
                g.drawText (juce::String (i + 1), b, juce::Justification::centred);
            }
            if (! label) continue;
            g.setColour (active ? Ink::onAccent() : done ? Ink::textNow() : Ink::textDim());
            g.setFont (font);
            g.drawText (steps[i], tab.withTrimmedLeft ((float) (padX + badge + inner)), juce::Justification::centredLeft);
        }
    }

private:
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
    int current = 0;
    bool allDone = false;
};

} // namespace zs
