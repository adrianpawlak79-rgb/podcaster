/*
    Ink.h — wszystkie kolory ciemnych ekranow w jednym miejscu.

    Paleta stala najpierw w PrompterComponent.h, bo byl jedynym ciemnym ekranem.
    Z ekranem po nagraniu zrobily sie dwa — a dwie kopie tych samych liczb
    szesnastkowych to gwarancja, ze przy nastepnej zmianie wygladu jedna z nich
    zostanie po staremu. I nie bedzie to blad widoczny od razu, tylko jeden
    odcien zieleni nie taki jak reszta.

    Znaczenia, a nie nazwy kolorow: "green" znaczy "jest dobrze", a nie
    "zielony". Gdyby kiedys okazalo sie, ze zielen jest nieczytelna dla czesci
    ludzi, zmienia sie tutaj jedna liczbe i caly program wie, co ma robic.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace zs
{

struct Ink
{
    static juce::Colour bg()        { return juce::Colour (0xff0d1117); }
    static juce::Colour panel()     { return juce::Colour (0xff121820); }
    static juce::Colour edge()      { return juce::Colour (0xff212c3a); }

    static juce::Colour textNow()   { return juce::Colour (0xffeceff4); }
    static juce::Colour textNext()  { return juce::Colour (0xff8e9db0); }
    static juce::Colour textPast()  { return juce::Colour (0xff677689); }
    static juce::Colour textDim()   { return juce::Colour (0xff8997a8); }

    static juce::Colour green()     { return juce::Colour (0xff71d9ac); }
    static juce::Colour yellow()    { return juce::Colour (0xffffd600); }
    static juce::Colour red()       { return juce::Colour (0xffff3d00); }
    static juce::Colour clip()      { return juce::Colour (0xffff1744); }
    static juce::Colour lostBlue()  { return juce::Colour (0xff3d5afe); }
    static juce::Colour lostText()  { return juce::Colour (0xff82b1ff); }
};

class StudioLook : public juce::LookAndFeel_V4 {
public:
    StudioLook() {
        setColour(juce::ResizableWindow::backgroundColourId,Ink::bg());
        setColour(juce::Label::textColourId,Ink::textNow());
        setColour(juce::TextButton::buttonColourId,Ink::panel());
        setColour(juce::TextButton::textColourOffId,Ink::textNow());
        setColour(juce::ComboBox::backgroundColourId,Ink::panel());
        setColour(juce::ComboBox::textColourId,Ink::textNow());
        setColour(juce::ComboBox::outlineColourId,Ink::edge());
        setColour(juce::ListBox::backgroundColourId,Ink::panel());
        setColour(juce::ListBox::textColourId,Ink::textNow());
        setColour(juce::TextEditor::backgroundColourId,Ink::panel());
        setColour(juce::TextEditor::textColourId,Ink::textNow());
        setColour(juce::ToggleButton::textColourId,Ink::textNow());
    }
    juce::Font getTextButtonFont(juce::TextButton&,int) override { return juce::Font(15.0f); }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& c,bool hover,bool down) override {
        auto r=b.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(c.withMultipliedBrightness(down?0.85f:hover?1.18f:1.0f).withMultipliedAlpha(b.isEnabled()?1.0f:0.4f));
        g.fillRoundedRectangle(r,8.0f);
        g.setColour(b.hasKeyboardFocus(true)?Ink::green():Ink::edge());
        g.drawRoundedRectangle(r,8.0f,b.hasKeyboardFocus(true)?2.0f:1.0f);
    }
};
} // namespace zs
