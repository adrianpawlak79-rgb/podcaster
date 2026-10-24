/*
  ==============================================================================

    StartComponent.h — ekran startowy (24.10).

    Pierwsze, co widac po uruchomieniu: po lewej nowy projekt (nazwa i rodzaj
    nagrania, potem Start), po prawej ostatnie nagrania z folderu nagran —
    klikniecie otwiera odsluch. Dopiero Start prowadzi do Przygotowania
    (mikrofon i tekst). Wczesniej program zaczynal od formularza, ktory nie
    wygladal na poczatek pracy.

    Nazwa projektu idzie do nazwy pliku nagrania (Recorder::start), wiec
    lista ostatnich nagran mowi ludzkimi nazwami, a nie samymi datami.

    "Glos z podkladem" jest na razie zaznaczony jako "wkrotce": odtwarzanie
    muzyki podczas nagrania i w miksie to osobny krok.

  ==============================================================================
*/

#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Recorder.h"
#include "Look.h"
#include "ScreenFrame.h"

class StartComponent : public juce::Component
{
public:
    std::function<void (const juce::String& name)> onStart;
    std::function<void (const juce::File&)> onOpen;
    std::function<void()> onOpenOther;

    StartComponent()
    {
        setOpaque (true);
        nameInput.setFont (juce::Font (zs::type::heading));
        nameInput.setIndents (14, 12);
        nameInput.setTextToShowWhenEmpty (PL ("np. Odcinek 12 — Nic dwa razy"), zs::Ink::textDim());
        nameInput.setColour (juce::TextEditor::backgroundColourId, zs::Ink::bg());
        nameInput.setColour (juce::TextEditor::textColourId, zs::Ink::textNow());
        nameInput.setColour (juce::TextEditor::outlineColourId, zs::Ink::edge());
        nameInput.setColour (juce::TextEditor::focusedOutlineColourId, zs::Ink::accent());
        nameInput.onReturnKey = [this] { start(); };
        addAndMakeVisible (nameInput);

        startButton.setButtonText (PL ("Start"));
        zs::Look::setKind (startButton, zs::ui::ButtonKind::Primary);
        zs::Look::setIcon (startButton, zs::Look::Icon::Play);
        startButton.onClick = [this] { start(); };
        addAndMakeVisible (startButton);

        otherButton.setButtonText (PL ("Otwórz inny plik…"));
        otherButton.onClick = [this] { if (onOpenOther) onOpenOther(); };
        addAndMakeVisible (otherButton);
    }

    /** Nazwa w polu, gdy ekran wraca (np. po skonczonym odcinku: pusta). */
    void setName (const juce::String& n) { nameInput.setText (n, juce::dontSendNotification); }

    /** Folder nagran: zbiera najnowsze nagrania (bez miksow i kopii). */
    void setFolder (const juce::File& folder)
    {
        recent.clear();
        if (folder.isDirectory())
        {
            auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav");
            files.removeIf ([] (const juce::File& f)
            {
                const auto n = f.getFileNameWithoutExtension();
                return n.contains ("__miks") || n.endsWith (".tmp") || n.contains (".czysty");
            });
            std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
                       { return a.getLastModificationTime() > b.getLastModificationTime(); });
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            for (const auto& f : files)
            {
                if ((int) recent.size() >= maxRecent) break;
                Item it;
                it.file = f;
                it.when = f.getLastModificationTime();
                if (std::unique_ptr<juce::AudioFormatReader> r { formats.createReaderFor (f) })
                    if (r->sampleRate > 0) it.seconds = (double) r->lengthInSamples / r->sampleRate;
                it.mixed = f.getSiblingFile (f.getFileNameWithoutExtension() + "__miks.wav").existsAsFile();
                recent.push_back (it);
            }
        }
        resized();
        repaint();
    }

    /** Linijka o sprzecie na dole lewego panelu ("Mikrofon: ... · sprawdzony ..."). */
    void setEquipment (const juce::String& line, bool checked)
    {
        equipment = line; equipmentChecked = checked; repaint();
    }

    void focusName() { nameInput.grabKeyboardFocus(); nameInput.selectAll(); }

    void paint (juce::Graphics& g) override
    {
        using I = zs::Ink;
        g.fillAll (I::bg());
        zs::ScreenFrame::paint (g, getLocalBounds(), PL ("Nagrywanie głosu"),
                                PL ("Enter — start  ·  klik w nagranie — odsłuch"));
        const auto L = layout();

        // ---- nowy projekt --------------------------------------------------
        zs::ScreenFrame::paintPanel (g, L.left);
        auto a = L.left.reduced (pad, pad);
        caption (g, PL ("NOWY PROJEKT"), a.removeFromTop (18));
        g.setColour (I::textNow()); g.setFont (juce::Font (zs::type::title, juce::Font::bold));
        g.drawText (PL ("Co dziś nagrywamy?"), a.removeFromTop (44), juce::Justification::centredLeft);
        g.setColour (I::textNext()); g.setFont (juce::Font (zs::type::small));
        g.drawText (PL ("Nazwa"), L.nameLabel, juce::Justification::bottomLeft);
        g.drawText (PL ("Rodzaj nagrania"), L.kindLabel, juce::Justification::bottomLeft);

        paintCard (g, L.voiceCard, true, true, PL ("Sam głos"),
                   PL ("Podcast, lektor, audiobook. Prompter i lampka prowadzą przez nagranie."), false);
        paintCard (g, L.musicCard, false, false, PL ("Głos z podkładem"),
                   PL ("Muzyka pod głosem, ściszana, gdy mówisz."), true);

        g.setColour (I::textDim()); g.setFont (juce::Font (zs::type::small));
        g.drawText (PL ("Dalej: mikrofon i tekst do promptera."),
                    L.startHint, juce::Justification::centredLeft);

        if (equipment.isNotEmpty())
        {
            g.setColour (I::edge());
            g.drawHorizontalLine (L.equipment.getY() - 6, (float) L.equipment.getX(), (float) L.equipment.getRight());
            auto e = L.equipment;
            g.setColour (equipmentChecked ? I::green() : I::yellow());
            g.fillEllipse (e.removeFromLeft (10).withSizeKeepingCentre (8, 8).toFloat());
            e.removeFromLeft (8);
            g.setColour (I::textNext()); g.setFont (juce::Font (zs::type::small));
            g.drawText (equipment, e, juce::Justification::centredLeft, true);
        }

        // ---- ostatnie nagrania -------------------------------------------
        zs::ScreenFrame::paintPanel (g, L.right);
        auto b = L.right.reduced (pad, pad);
        caption (g, PL ("OSTATNIE NAGRANIA"), b.removeFromTop (18));
        if (recent.empty())
        {
            g.setColour (I::textDim()); g.setFont (juce::Font (zs::type::body));
            g.drawFittedText (PL ("Tu pojawią się Twoje nagrania."), L.list.withHeight (60),
                              juce::Justification::centredLeft, 2);
        }
        for (size_t i = 0; i < recent.size(); ++i)
        {
            const auto row = rowBounds (L, (int) i);
            if (row.getBottom() > L.list.getBottom()) break;
            paintRow (g, row, recent[i], (int) i == hoverRow);
        }
    }

    void resized() override
    {
        const auto L = layout();
        nameInput.setBounds (L.name);
        startButton.setBounds (L.start);
        otherButton.setBounds (L.other);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int r = rowAt (e.getPosition());
        if (r != hoverRow) { hoverRow = r; repaint(); }
        setMouseCursor (r >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { if (hoverRow >= 0) { hoverRow = -1; repaint(); } }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int r = rowAt (e.getPosition());
        if (r >= 0 && onOpen) onOpen (recent[(size_t) r].file);
    }

    int recentCount() const noexcept { return (int) recent.size(); }

private:
    struct Item { juce::File file; juce::Time when; double seconds = 0.0; bool mixed = false; };
    struct Layout
    {
        juce::Rectangle<int> left, right, nameLabel, name, kindLabel, voiceCard, musicCard,
                             start, startHint, list, other, equipment;
    };

    static constexpr int pad = 24, maxRecent = 8, rowH = 62;

    juce::TextEditor nameInput;
    juce::TextButton startButton, otherButton;
    std::vector<Item> recent;
    int hoverRow = -1;
    juce::String equipment;
    bool equipmentChecked = false;

    Layout layout() const
    {
        Layout L;
        auto c = getLocalBounds();
        c.removeFromTop (zs::ui::frame::header);
        c.removeFromBottom (zs::ui::frame::footer);
        c = c.reduced (zs::ui::frame::pad + 4, zs::ui::frame::pad + 2);
        const int rightW = juce::jlimit (300, 420, c.getWidth() * 36 / 100);
        L.right = c.removeFromRight (rightW);
        c.removeFromRight (zs::ui::frame::gap + 4);
        L.left = c;

        auto a = L.left.reduced (pad, pad);
        a.removeFromTop (18 + 44 + 10);
        L.nameLabel = a.removeFromTop (24);  a.removeFromTop (6);
        L.name = a.removeFromTop (52);       a.removeFromTop (18);
        L.kindLabel = a.removeFromTop (24);  a.removeFromTop (6);
        L.equipment = a.removeFromBottom (24);
        a.removeFromBottom (12);
        auto cards = a.removeFromTop (juce::jlimit (124, 170, a.getHeight() - 52 - 28));
        L.voiceCard = cards.removeFromLeft ((cards.getWidth() - 14) / 2);
        cards.removeFromLeft (14);
        L.musicCard = cards;
        a.removeFromTop (juce::jmin (28, juce::jmax (12, a.getHeight() - 52)));
        auto bottom = a.removeFromTop (52);
        L.start = bottom.removeFromLeft (220);
        bottom.removeFromLeft (16);
        L.startHint = bottom;

        auto b = L.right.reduced (pad, pad);
        b.removeFromTop (18 + 12);
        L.other = b.removeFromBottom (40);
        b.removeFromBottom (12);
        L.list = b;
        return L;
    }

    juce::Rectangle<int> rowBounds (const Layout& L, int i) const
    {
        return L.list.withY (L.list.getY() + i * (rowH + 6)).withHeight (rowH);
    }

    int rowAt (juce::Point<int> p) const
    {
        const auto L = layout();
        for (int i = 0; i < (int) recent.size(); ++i)
        {
            const auto r = rowBounds (L, i);
            if (r.getBottom() > L.list.getBottom()) break;
            if (r.contains (p)) return i;
        }
        return -1;
    }

    void start()
    {
        if (onStart) onStart (nameInput.getText().trim());
    }

    static void caption (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r)
    {
        g.setColour (zs::Ink::textDim()); g.setFont (juce::Font (zs::type::tiny, juce::Font::bold));
        g.drawText (text, r, juce::Justification::centredLeft);
    }

    static void paintCard (juce::Graphics& g, juce::Rectangle<int> r, bool selected, bool enabled,
                           const juce::String& title, const juce::String& text, bool music)
    {
        using I = zs::Ink;
        const auto rf = r.toFloat();
        g.setColour (selected ? I::accent().withAlpha (0.12f) : I::bg());
        g.fillRoundedRectangle (rf, 8.0f);
        g.setColour (selected ? I::accent() : I::edge());
        g.drawRoundedRectangle (rf.reduced (selected ? 1.0f : 0.5f), 8.0f, selected ? 2.0f : 1.0f);

        auto in = r.reduced (16, 14);
        auto top = in.removeFromTop (34);
        const auto icon = top.removeFromLeft (34).toFloat();
        const auto ink = ! enabled ? I::textDim() : selected ? I::select() : I::textNext();
        g.setColour (ink.withAlpha (0.16f));
        g.fillEllipse (icon);
        g.setColour (ink);
        const float cx = icon.getCentreX(), cy = icon.getCentreY();
        if (music)
        {
            // nuta: dwie glowki i belka
            g.fillEllipse (cx - 9.0f, cy + 2.0f, 7.0f, 6.0f);
            g.fillEllipse (cx + 2.0f, cy, 7.0f, 6.0f);
            g.fillRect (cx - 3.5f, cy - 9.0f, 1.8f, 14.0f);
            g.fillRect (cx + 7.5f, cy - 11.0f, 1.8f, 14.0f);
            juce::Path beam;
            beam.addQuadrilateral (cx - 3.5f, cy - 9.0f, cx + 9.3f, cy - 11.0f, cx + 9.3f, cy - 8.0f, cx - 3.5f, cy - 6.0f);
            g.fillPath (beam);
        }
        else
        {
            g.drawRoundedRectangle (cx - 3.5f, cy - 10.0f, 7.0f, 12.0f, 3.5f, 1.8f);
            juce::Path mic;
            mic.startNewSubPath (cx - 7.0f, cy - 3.0f); mic.lineTo (cx - 7.0f, cy - 1.0f);
            mic.quadraticTo (cx - 7.0f, cy + 6.0f, cx, cy + 6.0f);
            mic.quadraticTo (cx + 7.0f, cy + 6.0f, cx + 7.0f, cy - 1.0f); mic.lineTo (cx + 7.0f, cy - 3.0f);
            g.strokePath (mic, juce::PathStrokeType (1.8f));
            g.drawLine (cx, cy + 6.0f, cx, cy + 10.0f, 1.8f);
        }
        top.removeFromLeft (12);
        if (! enabled)
        {
            const juce::Font bf (zs::type::tiny, juce::Font::bold);
            const int bw = bf.getStringWidth (PL ("WKRÓTCE")) + 16;
            const auto badge = in.removeFromBottom (22).removeFromLeft (bw).toFloat();
            g.setColour (I::yellow());
            g.drawRoundedRectangle (badge.reduced (0.5f), 5.0f, 1.0f);
            g.setFont (bf);
            g.drawText (PL ("WKRÓTCE"), badge.toNearestInt(), juce::Justification::centred);
        }
        g.setColour (enabled ? I::textNow() : I::textDim());
        g.setFont (juce::Font (zs::type::heading, juce::Font::bold));
        g.drawText (title, top, juce::Justification::centredLeft);
        in.removeFromTop (8);
        g.setColour (enabled ? I::textNext() : I::textDim());
        g.setFont (juce::Font (zs::type::small));
        g.drawFittedText (text, in, juce::Justification::topLeft, 3, 1.0f);
    }

    static juce::String duration (double s)
    {
        const int t = (int) std::lround (s);
        return t >= 3600 ? juce::String::formatted ("%d:%02d:%02d", t / 3600, t / 60 % 60, t % 60)
                         : juce::String::formatted ("%d:%02d", t / 60, t % 60);
    }

    /** "Odcinek 12_2026-10-24_09-30-00" -> "Odcinek 12": data stoi wierszem nizej. */
    static juce::String displayName (const juce::File& f)
    {
        auto n = f.getFileNameWithoutExtension();
        const int k = n.length() - 20;                      // "_RRRR-MM-DD_GG-MM-SS"
        if (k > 0 && n[k] == '_' && n.substring (k + 1).containsOnly ("0123456789-_"))
            n = n.substring (0, k);
        return n == "sesja" ? PL ("Nagranie bez nazwy") : n;
    }

    static void paintRow (juce::Graphics& g, juce::Rectangle<int> r, const Item& it, bool hover)
    {
        using I = zs::Ink;
        const auto rf = r.toFloat();
        g.setColour (hover ? I::raised() : I::bg());
        g.fillRoundedRectangle (rf, 7.0f);
        g.setColour (hover ? I::accent() : I::edge());
        g.drawRoundedRectangle (rf.reduced (0.5f), 7.0f, 1.0f);

        auto in = r.reduced (14, 9);
        auto top = in.removeFromTop (22);
        const juce::String tag = it.mixed ? PL ("miks gotowy") : PL ("bez miksu");
        const juce::Font tf (zs::type::tiny, juce::Font::bold);
        const int tw = tf.getStringWidth (tag) + 16;
        auto tagArea = top.removeFromRight (tw).toFloat();
        g.setColour (it.mixed ? I::green() : I::textDim());
        g.drawRoundedRectangle (tagArea.reduced (0.5f), 5.0f, 1.0f);
        g.setFont (tf);
        g.drawText (tag, tagArea.toNearestInt(), juce::Justification::centred);
        top.removeFromRight (10);

        g.setColour (I::textNow()); g.setFont (juce::Font (zs::type::body, juce::Font::bold));
        g.drawText (displayName (it.file), top, juce::Justification::centredLeft, true);
        g.setColour (I::textDim()); g.setFont (juce::Font (zs::type::small));
        g.drawText (it.when.formatted ("%d.%m.%Y  %H:%M") + "   " + duration (it.seconds), in,
                    juce::Justification::centredLeft, true);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StartComponent)
};
