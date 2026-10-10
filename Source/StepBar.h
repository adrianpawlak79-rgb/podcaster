/*
    StepBar.h — pasek krokow.

    Kreator mial szesc kropek na dole ekranu, a ekran glowny napis
    "1 Przygotuj / 2 Nagraj / 3 Odsluchaj / 4 Zapisz" malym szarym drukiem.
    Ani jedno, ani drugie nie mowilo, gdzie czlowiek JEST: kropki mialy 9
    pikseli i nie mialy nazw, a napis nie zmienial sie wcale.

    Teraz jedno i to samo na obu ekranach: kreska nad kazdym krokiem i nazwa
    pod nia. Biezacy krok ma akcent i jasny napis, zrobione sa przygaszone,
    przyszle ledwo widoczne. Stany liczy ui::stepState (UiRules.h), wiec test
    widzi dokladnie to, co ekran.
*/

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

    static constexpr int preferredHeight = 34;

    void paint (juce::Graphics& g) override
    {
        if (steps.isEmpty()) return;

        const int gap = 8;
        const auto widths = ui::stepWidths (getWidth(), steps.size(), gap);

        int x = 0;
        for (int i = 0; i < steps.size(); ++i)
        {
            auto cell = juce::Rectangle<int> (x, 0, widths[(size_t) i], getHeight());
            x += widths[(size_t) i] + gap;

            const auto s = ui::stepState (i, current, allDone);

            const juce::Colour bar = s == ui::StepState::Current ? Ink::accent()
                                   : s == ui::StepState::Done    ? Ink::accent().withAlpha (0.45f)
                                                                 : Ink::edge();
            const juce::Colour txt = s == ui::StepState::Current ? Ink::textNow()
                                   : s == ui::StepState::Done    ? Ink::textNext()
                                                                 : Ink::textPast();

            g.setColour (bar);
            g.fillRoundedRectangle (cell.removeFromTop (3).toFloat(), 1.5f);
            cell.removeFromTop (8);

            g.setColour (txt);
            g.setFont (juce::Font (type::small, s == ui::StepState::Current ? juce::Font::bold
                                                                          : juce::Font::plain));
            g.drawFittedText (juce::String (i + 1) + "  " + steps[i], cell,
                              juce::Justification::topLeft, 1, 0.9f);
        }
    }

private:
    juce::StringArray steps;
    int current = 0;
    bool allDone = false;
};

} // namespace zs
