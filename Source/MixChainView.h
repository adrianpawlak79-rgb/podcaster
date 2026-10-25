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
            t.amount.setTooltip (PL (mix::stageHelp (s)) + PL ("\nDwuklik: bez zmian."));
            t.amount.setColour (juce::Slider::trackColourId, zs::Ink::accent());
            t.amount.setColour (juce::Slider::backgroundColourId, zs::Ink::edge());
            t.amount.setColour (juce::Slider::thumbColourId, zs::Ink::textNow());
            t.amount.onDragEnd = [this, i] { take (i); };
            t.amount.onValueChange = [this, i] { if (! tiles[(size_t) i].amount.isMouseButtonDown()) take (i); };
            addAndMakeVisible (t.amount);
        }
        resetButton.setButtonText (PL ("Przywróć domyślne ustawienia"));
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
        resetButton.setBounds (head.removeFromRight (230).withSizeKeepingCentre (230, 26));
        for (int i = 0; i < mix::stageCount; ++i)
        {
            const auto tile = tileBounds (i);
            auto in = tile.reduced (10, 8);
            auto top = in.removeFromTop (22);
            tiles[(size_t) i].power.setBounds (top.removeFromRight (36).withSizeKeepingCentre (34, 18));
            in.removeFromTop (20 + 4 + helpHeight);
            tiles[(size_t) i].amount.setBounds (in.removeFromTop (22).reduced (14, 0));
        }
    }

    void paint (juce::Graphics& g) override
    {
        using I = zs::Ink;
        auto head = getLocalBounds().removeFromTop (header);
        g.setColour (I::textDim()); g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
        g.drawText (PL ("ZAAWANSOWANE USTAWIENIA — dźwięk idzie od lewej do prawej"), head.withTrimmedRight (176),
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

            auto valueRow = in.removeFromTop (20);
            g.setFont (juce::Font (zs::type::tiny));
            g.setColour (on ? I::textNext() : I::textDim());
            g.drawText (! known ? juce::String() : on ? PL (mix::stageValue (mixedParams, s).c_str())
                                 : used (i) ? PL ("wyłączone") : PL ("nie w tym brzmieniu"),
                        valueRow, juce::Justification::centredLeft, true);

            auto help = in.removeFromTop (helpHeight);
            g.setFont (juce::Font (zs::type::tiny));
            g.setColour (I::textNext());
            g.drawFittedText (PL (mix::stageHelp (s)), help, juce::Justification::topLeft, 2);

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
            // specyfikacja E: tor 34 x 18, wylaczony #252F3B / obrys #465261 /
            // galka #AEB9C6, wlaczony #1476B7 / obrys #32B9FF / galka #E7EEF5
            const auto r = getLocalBounds().toFloat().withSizeKeepingCentre (34.0f, 18.0f);
            const bool on = getToggleState() && isEnabled();
            auto track = juce::Colour (on ? 0xff1476b7 : 0xff252f3b);
            if (over && isEnabled()) track = track.brighter (0.08f);
            zs::Depth::well (g, r, 9.0f);
            if (on) zs::Depth::glow (g, r, 9.0f, I::accent(), 5.0f, 0.24f);
            g.setColour (track.withMultipliedAlpha (isEnabled() ? 1.0f : 0.42f));
            g.fillRoundedRectangle (r.reduced (1.0f), 8.0f);
            g.setColour (juce::Colour (on ? 0xff32b9ff : 0xff465261));
            g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);
            const float d = 12.0f;
            const auto knob = juce::Rectangle<float> (d, d).withPosition (
                on ? r.getRight() - d - 3.0f : r.getX() + 3.0f, r.getCentreY() - d / 2);
            zs::Depth::shadow (g, knob, d * 0.5f, 3.0f, 0.38f, 1.5f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (on ? 0xfff4f7fa : 0xffd2d8df),
                knob.getX() + 3.0f, knob.getY() + 1.0f,
                juce::Colour (on ? 0xff8e9aa8 : 0xff7d8792), knob.getCentreX(), knob.getBottom(), true));
            g.fillEllipse (knob);
            g.setColour (juce::Colour (0xff5a6572)); g.drawEllipse (knob.reduced (0.5f), 1.0f);
        }
    };
    struct Tile { Power power; juce::Slider amount; };

    static constexpr int header = 28, tileH = 112, gap = 6, minTileW = 150, helpHeight = 30;

    std::array<Tile, mix::stageCount> tiles;
    juce::TextButton resetButton;
    mix::SpeechChain::Params decidedParams, mixedParams;
    mix::Tweaks tweaks;
    bool known = false, busy = false;

    /** Dwa wiersze (5 + 4), bez przewijania w bok (Adrian 25.10); gdy kafel
        bylby wezszy niz minTileW — trzy wiersze. */
    static int columnsFor (int width)
    {
        const int two = (mix::stageCount + 1) / 2;
        return width >= two * minTileW + (two - 1) * gap ? two : (mix::stageCount + 2) / 3;
    }

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
