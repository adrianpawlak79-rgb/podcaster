/*
  ==============================================================================

    MixChainView.h — tor miksu w "Szczegolach miksu" (24.10).

    Moduly lancucha w kolejnosci, w jakiej przechodzi przez nie dzwiek. Kazdy
    kafel: nazwa, ile modul robi w tym miksie, wlacznik i jeden suwak
    "mniej <-> wiecej" (0 = jak automat; dwuklik wraca do 0). Logika poprawek
    jest w DSP/Mix/Tweaks.h — tu tylko kafle. Modul, ktorego automat w tym
    brzmieniu nie uzyl, jest wyszarzony, ale da sie go wlaczyc.

    Zmiana idzie do onChange po puszczeniu suwaka (nie w trakcie: kazda
    zmiana to miks od nowa w tle).

  ==============================================================================
*/

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Recorder.h"
#include "Look.h"
#include "DSP/Mix/Tweaks.h"

class MixChainView : public juce::Component
{
public:
    std::function<void (const mix::Tweaks&)> onChange;

    MixChainView()
    {
        for (int i = 0; i < mix::stageCount; ++i)
        {
            auto& t = tiles[(size_t) i];
            const auto s = (mix::Stage) i;
            t.power.setClickingTogglesState (true);
            t.power.setTooltip (PL ("Włącz / wyłącz: ") + PL (mix::stageName (s)));
            t.power.onClick = [this, i]
            {
                const bool state = tiles[(size_t) i].power.getToggleState();
                if (used (i)) tweaks.off[(size_t) i] = ! state;
                else          tweaks.forced[(size_t) i] = state;    // wlaczenie recznie: od ustawien studyjnego
                send();
            };
            addAndMakeVisible (t.power);

            t.amount.setSliderStyle (juce::Slider::LinearHorizontal);
            t.amount.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            t.amount.setRange (-1.0, 1.0, 0.25);
            t.amount.setValue (0.0, juce::dontSendNotification);
            t.amount.setDoubleClickReturnValue (true, 0.0);
            t.amount.setTooltip (PL (mix::stageHelp (s)) + PL ("\nDwuklik: jak automat."));
            t.amount.setColour (juce::Slider::trackColourId, zs::Ink::accent());
            t.amount.setColour (juce::Slider::backgroundColourId, zs::Ink::edge());
            t.amount.setColour (juce::Slider::thumbColourId, zs::Ink::textNow());
            t.amount.onDragEnd = [this, i] { take (i); };
            t.amount.onValueChange = [this, i] { if (! tiles[(size_t) i].amount.isMouseButtonDown()) take (i); };
            addAndMakeVisible (t.amount);
        }
        resetButton.setButtonText (PL ("Jak automat"));
        resetButton.setTooltip (PL ("Wszystkie moduły tak, jak dobrał je miks"));
        resetButton.onClick = [this] { tweaks = {}; refreshControls(); send(); };
        addAndMakeVisible (resetButton);
    }

    /** decided: ustawienia automatu (ktore moduly sa uzyte), mixed: z poprawkami
        (liczby na kaflach), t: poprawki, z ktorymi miks policzono. */
    void setState (const mix::SpeechChain::Params& decided, const mix::SpeechChain::Params& mixed, const mix::Tweaks& t)
    {
        decidedParams = decided; mixedParams = mixed; tweaks = t; known = true;
        refreshControls();
        repaint();
    }

    void setBusy (bool b) { busy = b; refreshControls(); repaint(); }

    /** Wysokosc potrzebna przy danej szerokosci. */
    int heightFor (int width) const
    {
        const int cols = columnsFor (width);
        const int rows = (mix::stageCount + cols - 1) / cols;
        return header + rows * tileH + (rows - 1) * gap;
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto head = r.removeFromTop (header);
        resetButton.setBounds (head.removeFromRight (124).withSizeKeepingCentre (124, 28));
        for (int i = 0; i < mix::stageCount; ++i)
        {
            const auto tile = tileBounds (i);
            auto in = tile.reduced (10, 8);
            auto top = in.removeFromTop (22);
            tiles[(size_t) i].power.setBounds (top.removeFromRight (36).withSizeKeepingCentre (34, 18));
            in.removeFromTop (18);
            tiles[(size_t) i].amount.setBounds (in.removeFromTop (22).reduced (14, 0));
        }
    }

    void paint (juce::Graphics& g) override
    {
        using I = zs::Ink;
        auto head = getLocalBounds().removeFromTop (header);
        g.setColour (I::textDim()); g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
        g.drawText (PL ("TOR MIKSU — dźwięk idzie od lewej do prawej"), head.withTrimmedRight (130),
                    juce::Justification::centredLeft, true);

        for (int i = 0; i < mix::stageCount; ++i)
        {
            const auto s = (mix::Stage) i;
            const auto tile = tileBounds (i);
            const bool on = isOn (i);
            const auto rf = tile.toFloat();
            g.setColour (on ? I::panel().brighter (0.04f) : I::bg());
            g.fillRoundedRectangle (rf, 7.0f);
            g.setColour (on && std::abs (tweaks.amount[(size_t) i]) > 1e-4f ? I::accent() : I::edge());
            g.drawRoundedRectangle (rf.reduced (0.5f), 7.0f, 1.0f);

            auto in = tile.reduced (10, 8);
            auto top = in.removeFromTop (22);
            top.removeFromRight (40);
            // lampka modulu: zielona = pracuje
            g.setColour (on ? I::green() : I::textDim().withAlpha (0.5f));
            g.fillEllipse (top.removeFromLeft (8).withSizeKeepingCentre (7, 7).toFloat());
            top.removeFromLeft (7);
            g.setColour (on || used (i) ? I::textNow() : I::textNext());
            g.setFont (juce::Font (zs::type::small, juce::Font::bold));
            g.drawText (PL (mix::stageName (s)), top, juce::Justification::centredLeft, true);

            auto valueRow = in.removeFromTop (18);
            g.setFont (juce::Font (zs::type::tiny));
            g.setColour (on ? I::textNext() : I::textDim());
            g.drawText (! known ? juce::String() : on ? PL (mix::stageValue (mixedParams, s).c_str())
                                 : used (i) ? PL ("wyłączone") : PL ("nie w tym brzmieniu"),
                        valueRow, juce::Justification::centredLeft, true);

            if (on)
            {
                auto slider = in.removeFromTop (22);
                g.setColour (I::textDim());
                g.drawText (PL ("−"), slider.removeFromLeft (12), juce::Justification::centred);
                g.drawText (PL ("+"), slider.removeFromRight (12), juce::Justification::centred);
            }
        }
    }

private:
    struct Power : juce::ToggleButton
    {
        void paintButton (juce::Graphics& g, bool over, bool) override
        {
            using I = zs::Ink;
            const auto r = getLocalBounds().toFloat().reduced (1.0f);
            const bool on = getToggleState() && isEnabled();
            g.setColour (on ? I::green().withAlpha (0.85f) : I::edge());
            g.fillRoundedRectangle (r, r.getHeight() / 2);
            if (over && isEnabled()) { g.setColour (I::textNow().withAlpha (0.15f)); g.fillRoundedRectangle (r, r.getHeight() / 2); }
            const float d = r.getHeight() - 4.0f;
            g.setColour (isEnabled() ? I::textNow() : I::textDim());
            g.fillEllipse (on ? r.getRight() - d - 2.0f : r.getX() + 2.0f, r.getY() + 2.0f, d, d);
        }
    };
    struct Tile { Power power; juce::Slider amount; };

    static constexpr int header = 34, tileH = 84, gap = 8, minTileW = 168;

    std::array<Tile, mix::stageCount> tiles;
    juce::TextButton resetButton;
    mix::SpeechChain::Params decidedParams, mixedParams;
    mix::Tweaks tweaks;
    bool known = false, busy = false;

    static int columnsFor (int width) { return juce::jlimit (1, mix::stageCount, (width + gap) / (minTileW + gap)); }

    juce::Rectangle<int> tileBounds (int i) const
    {
        const int cols = columnsFor (getWidth());
        const int w = (getWidth() - (cols - 1) * gap) / cols;
        const int c = i % cols, row = i / cols;
        return { c * (w + gap), header + row * (tileH + gap), w, tileH };
    }

    /** Automat uzyl modulu (przed poprawkami). */
    bool used (int i) const { return known && mix::stageUsed (decidedParams, (mix::Stage) i); }
    /** Modul pracuje w tym miksie: uzyty i nie wylaczony albo wlaczony recznie. */
    bool isOn (int i) const { return used (i) ? ! tweaks.off[(size_t) i] : known && tweaks.forced[(size_t) i]; }

    void refreshControls()
    {
        for (int i = 0; i < mix::stageCount; ++i)
        {
            auto& t = tiles[(size_t) i];
            t.power.setVisible (known);
            t.amount.setVisible (known && isOn (i));
            t.power.setToggleState (isOn (i), juce::dontSendNotification);
            t.power.setTooltip (used (i) ? PL ("Włącz / wyłącz: ") + PL (mix::stageName ((mix::Stage) i))
                                         : PL ("Włącz — od ustawień brzmienia Studyjnego"));
            t.amount.setValue (tweaks.amount[(size_t) i], juce::dontSendNotification);
            t.power.setEnabled (! busy);
            t.amount.setEnabled (! busy);
        }
        resetButton.setEnabled (! busy && ! tweaks.isDefault());
    }

    void take (int i)
    {
        const float v = (float) tiles[(size_t) i].amount.getValue();
        if (std::abs (v - tweaks.amount[(size_t) i]) < 1e-4f) return;
        tweaks.amount[(size_t) i] = v;
        send();
    }

    void send()
    {
        refreshControls();
        repaint();
        if (onChange) onChange (tweaks);
    }
};
