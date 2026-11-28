/*
    TourOverlay.h — przewodnik po ekranie: dymki ze strzalka, ktore wskazuja
    prawdziwe przyciski i miejsca.

    ---------------------------------------------------------------------------
    JAK TO DZIALA

    Przewodnik to lista krokow (TourStep): "gdzie" + "co powiedziec".
    "Gdzie" jest funkcja, ktora w chwili pytania oddaje prostokat w ukladzie
    nakladki — liczony z prawdziwych komponentow (getLocalArea), nigdy ze
    stalych wspolrzednych. Dzieki temu przewodnik dziala w kazdym rozmiarze
    okna i nie psuje sie, gdy ktos przesunie przycisk.

    Krok bez elementu (pusty prostokat: komponent nie istnieje albo jest
    ukryty) jest pomijany przy starcie przewodnika.

    Nowy przewodnik = nowa lista krokow na ekranie, ktory go zna:

        steps.push_back (zs::tour::of (okButton, "Tu zatwierdzasz."));
        steps.push_back (zs::tour::rectIn (*this, [this] { return waveArea(); }, "Tu klikasz fale."));

    Dymek ma przyciemnione tlo z "wycieciem" wokol elementu, strzalke do
    elementu, licznik "2 z 5" i przyciski Wstecz / Dalej / Zakoncz. Esc konczy.
    Dymek nie wychodzi poza okno: jest nad, pod albo obok elementu, a gdy
    element jest wielki (np. caly prompter) — wewnatrz niego, bez strzalki.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include "Look.h"
#include "ScreenFrame.h"

namespace zs
{

/** Jeden krok przewodnika. area dostaje nakladke i oddaje prostokat w jej
    ukladzie (pusty = brak elementu, krok pominiety). */
struct TourStep
{
    std::function<juce::Rectangle<int> (const juce::Component& overlay)> area;
    juce::String text;
};

namespace tour
{
    inline juce::String T (const char* s) { return juce::String::fromUTF8 (s); }

    /** Czy komponent i wszyscy jego rodzice sa widoczni (isShowing() wymaga
        okna systemowego, a przy zrzutach ekranu go nie ma). */
    inline bool visibleChain (const juce::Component* c)
    {
        // okno najwyzszego poziomu (bez rodzica) pomijamy: w podgladach nie ma okna systemowego
        for (; c != nullptr && c->getParentComponent() != nullptr; c = c->getParentComponent())
            if (! c->isVisible()) return false;
        return true;
    }

    /** Krok wskazujacy komponent. */
    inline TourStep of (juce::Component& c, const juce::String& text)
    {
        juce::Component::SafePointer<juce::Component> p (&c);
        return { [p] (const juce::Component& overlay) -> juce::Rectangle<int>
                 {
                     if (p == nullptr || ! visibleChain (p) || p->getWidth() <= 0 || p->getHeight() <= 0) return {};
                     return overlay.getLocalArea (p, p->getLocalBounds());
                 }, text };
    }

    /** Krok wskazujacy prostokat we wspolrzednych komponentu owner
        (np. fala albo karta, ktore nie sa osobnymi komponentami). */
    inline TourStep rectIn (juce::Component& owner, std::function<juce::Rectangle<int>()> rect, const juce::String& text)
    {
        juce::Component::SafePointer<juce::Component> p (&owner);
        return { [p, rect] (const juce::Component& overlay) -> juce::Rectangle<int>
                 {
                     if (p == nullptr || ! visibleChain (p)) return {};
                     const auto r = rect();
                     return r.isEmpty() ? juce::Rectangle<int>() : overlay.getLocalArea (p, r);
                 }, text };
    }

    /** Dwa elementy jako jedno wskazanie (np. dwie karty obok siebie). */
    inline TourStep both (juce::Component& a, juce::Component& b, const juce::String& text)
    {
        auto sa = of (a, {}), sb = of (b, {});
        return { [fa = sa.area, fb = sb.area] (const juce::Component& o)
                 {
                     const auto x = fa (o), y = fb (o);
                     return x.isEmpty() ? y : y.isEmpty() ? x : x.getUnion (y);
                 }, text };
    }
}

//==============================================================================
class TourOverlay : public juce::Component
{
public:
    /** completed = przeszedl do ostatniego kroku i kliknal Gotowe. */
    std::function<void (bool completed)> onFinish;

    TourOverlay()
    {
        setWantsKeyboardFocus (true);
        setInterceptsMouseClicks (true, true);

        zs::Look::setKind (backButton, zs::ui::ButtonKind::Secondary);
        zs::Look::setKind (nextButton, zs::ui::ButtonKind::Primary);
        zs::Look::setKind (endButton,  zs::ui::ButtonKind::Secondary);
        backButton.setButtonText (tour::T ("Wstecz"));
        nextButton.setButtonText (tour::T ("Dalej"));
        endButton.setButtonText  (tour::T ("Zakończ"));
        backButton.onClick = [this] { go (-1); };
        nextButton.onClick = [this] { go (+1); };
        endButton.onClick  = [this] { finish (false); };
        for (auto* b : { &backButton, &nextButton, &endButton })
        {
            b->setWantsKeyboardFocus (false);
            addAndMakeVisible (b);
        }
    }

    /** Uruchamia przewodnik. Nakladka musi juz miec rodzica i rozmiar okna.
        Zwraca false, gdy zaden krok nie ma elementu na ekranie. */
    bool begin (std::vector<TourStep> all, int firstStep = 0)
    {
        steps.clear();
        for (auto& s : all)
            if (s.area && ! s.area (*this).isEmpty()) steps.push_back (std::move (s));

        if (steps.empty()) return false;
        index = juce::jlimit (0, (int) steps.size() - 1, firstStep);
        setVisible (true);
        toFront (true);
        grabKeyboardFocus();
        layoutBubble();
        repaint();
        return true;
    }

    int stepCount() const { return (int) steps.size(); }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        layoutBubble();
        if (steps.empty()) return;

        using I = zs::Ink;
        // przyciemnione tlo z wycieciem
        juce::Path scrim;
        scrim.addRectangle (getLocalBounds().toFloat());
        if (! hole.isEmpty()) scrim.addRoundedRectangle (hole.toFloat(), 8.0f);
        scrim.setUsingNonZeroWinding (false);
        g.setColour (juce::Colours::black.withAlpha (0.72f));
        g.fillPath (scrim);

        if (! hole.isEmpty())
        {
            g.setColour (I::accent().withAlpha (0.28f));
            g.drawRoundedRectangle (hole.toFloat().expanded (3.0f), 10.0f, 3.0f);
            g.setColour (I::accent());
            g.drawRoundedRectangle (hole.toFloat(), 8.0f, 2.0f);
        }

        if (hasArrow) drawArrow (g);

        // dymek
        zs::Depth::shadow (g, bubble.toFloat(), 12.0f, 18.0f, 0.55f);
        g.setColour (I::panel());
        g.fillRoundedRectangle (bubble.toFloat(), 12.0f);
        g.setColour (I::accent());
        g.drawRoundedRectangle (bubble.toFloat().reduced (0.5f), 12.0f, 1.5f);

        auto inner = bubble.reduced (pad, pad);
        g.setColour (I::accent()); g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
        g.drawText (juce::String (index + 1) + tour::T (" z ") + juce::String ((int) steps.size()),
                    inner.removeFromTop (counterH), juce::Justification::centredLeft);
        inner.removeFromTop (6);
        zs::ScreenFrame::wrapped (g, steps[(size_t) index].text, inner.removeFromTop (textH),
                                  juce::Font (textFont), I::textNow());
    }

    void resized() override { layoutBubble(); }

    void mouseDown (const juce::MouseEvent&) override {}   // tlo pod nakladka nie reaguje

    bool keyPressed (const juce::KeyPress& k) override
    {
        const int c = k.getKeyCode();
        if (c == juce::KeyPress::escapeKey) { finish (false); return true; }
        if (c == juce::KeyPress::rightKey || c == juce::KeyPress::returnKey || c == juce::KeyPress::spaceKey) { go (+1); return true; }
        if (c == juce::KeyPress::leftKey) { go (-1); return true; }
        return true;   // reszta klawiszy nie przechodzi do ekranu pod spodem
    }

private:
    static constexpr int pad = 16, counterH = 16, btnH = 34, gap = 30, margin = 12;
    static constexpr float textFont = 16.0f;

    std::vector<TourStep> steps;
    int index = 0;
    juce::Rectangle<int> hole, bubble;
    int textH = 0;
    bool hasArrow = false;
    juce::Point<float> arrowFrom, arrowTo;
    juce::TextButton backButton, nextButton, endButton;

    void go (int d)
    {
        const int n = index + d;
        if (n >= (int) steps.size()) { finish (true); return; }
        if (n < 0) return;
        index = n;
        layoutBubble();
        repaint();
    }

    void finish (bool completed)
    {
        setVisible (false);
        if (onFinish) onFinish (completed);
    }

    /** Dymek i strzalka dla biezacego kroku. Liczone z aktualnych granic
        komponentow, wiec zmiana rozmiaru okna nic nie psuje. */
    void layoutBubble()
    {
        if (steps.empty() || getWidth() <= 0) return;
        const auto W = getLocalBounds();

        auto h = steps[(size_t) index].area ? steps[(size_t) index].area (*this) : juce::Rectangle<int>();
        hole = h.isEmpty() ? juce::Rectangle<int>() : h.expanded (6).getIntersection (W);

        const int bw = juce::jmin (380, W.getWidth() - 2 * margin);
        textH = zs::ScreenFrame::wrappedHeight (steps[(size_t) index].text, bw - 2 * pad, juce::Font (textFont));
        const int bh = pad + counterH + 6 + textH + 12 + btnH + pad;

        auto clampX = [&] (int x) { return juce::jlimit (margin, juce::jmax (margin, W.getRight() - margin - bw), x); };
        auto clampY = [&] (int y) { return juce::jlimit (margin, juce::jmax (margin, W.getBottom() - margin - bh), y); };
        const auto inside = W.reduced (margin);

        enum Side { Below, Above, Right, Left, None };
        Side side = None;
        juce::Rectangle<int> b;

        if (hole.isEmpty())
            b = juce::Rectangle<int> (bw, bh).withCentre (W.getCentre());
        else
        {
            const juce::Rectangle<int> cand[4] = {
                { clampX (hole.getCentreX() - bw / 2), hole.getBottom() + gap, bw, bh },
                { clampX (hole.getCentreX() - bw / 2), hole.getY() - gap - bh, bw, bh },
                { hole.getRight() + gap, clampY (hole.getCentreY() - bh / 2), bw, bh },
                { hole.getX() - gap - bw, clampY (hole.getCentreY() - bh / 2), bw, bh } };
            for (int i = 0; i < 4 && side == None; ++i)
                if (inside.contains (cand[i]) && ! cand[i].intersects (hole)) { side = (Side) i; b = cand[i]; }

            if (side == None)   // element zajmuje prawie cale okno: dymek wewnatrz, na dole
                b = juce::Rectangle<int> (bw, bh).withCentre (hole.getCentre())
                        .withBottomY (hole.getBottom() - 16);
        }
        b = juce::Rectangle<int> (clampX (b.getX()), clampY (b.getY()), bw, bh);
        bubble = b;

        hasArrow = side != None;
        if (hasArrow)
        {
            auto mid = [] (int a0, int a1, int b0, int b1) {   // srodek wspolnego odcinka albo najblizszy punkt
                const int lo = juce::jmax (a0, b0), hi = juce::jmin (a1, b1);
                return lo <= hi ? (lo + hi) / 2 : (a1 < b0 ? (a1 + b0) / 2 : (b1 + a0) / 2); };
            if (side == Below || side == Above)
            {
                const float x = (float) mid (b.getX() + 24, b.getRight() - 24, hole.getX() + 8, hole.getRight() - 8);
                arrowFrom = { x, (float) (side == Below ? b.getY() : b.getBottom()) };
                arrowTo   = { x, (float) (side == Below ? hole.getBottom() + 4 : hole.getY() - 4) };
            }
            else
            {
                const float y = (float) mid (b.getY() + 24, b.getBottom() - 24, hole.getY() + 8, hole.getBottom() - 8);
                arrowFrom = { (float) (side == Right ? b.getX() : b.getRight()), y };
                arrowTo   = { (float) (side == Right ? hole.getRight() + 4 : hole.getX() - 4), y };
            }
        }

        auto row = bubble.reduced (pad, pad).removeFromBottom (btnH);
        const bool first = index == 0, last = index == (int) steps.size() - 1;
        nextButton.setButtonText (last ? tour::T ("Gotowe") : tour::T ("Dalej"));
        backButton.setVisible (! first);
        nextButton.setBounds (row.removeFromRight (92)); row.removeFromRight (8);
        backButton.setBounds (row.removeFromRight (92));
        endButton.setBounds (row.removeFromLeft (92));
        for (auto* c : { &backButton, &nextButton, &endButton }) c->toFront (false);
    }

    void drawArrow (juce::Graphics& g) const
    {
        g.setColour (zs::Ink::accent());
        g.drawLine ({ arrowFrom, arrowTo }, 2.5f);
        // grot na koncu, skierowany w element
        const auto d = (arrowTo - arrowFrom);
        const float len = std::sqrt (d.x * d.x + d.y * d.y);
        if (len < 1.0f) return;
        const juce::Point<float> u (d.x / len, d.y / len), n (-u.y, u.x);
        juce::Path head;
        head.addTriangle (arrowTo.x, arrowTo.y,
                          arrowTo.x - u.x * 14.0f + n.x * 8.0f, arrowTo.y - u.y * 14.0f + n.y * 8.0f,
                          arrowTo.x - u.x * 14.0f - n.x * 8.0f, arrowTo.y - u.y * 14.0f - n.y * 8.0f);
        g.fillPath (head);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TourOverlay)
};

} // namespace zs
