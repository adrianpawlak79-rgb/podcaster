/*
    Fonts.h — Inter, wszyty w program.

    ---------------------------------------------------------------------------
    DLACZEGO WLASNY KROJ

    Domyslny kroj JUCE na Windowsie to to, co system akurat poda pod nazwa
    "sans-serif" — zalezy od systemu, a pod Wine i na Linuksie przy
    budowaniu jest jeszcze inny. Prompter lamie wersy wedlug szerokosci
    liter, wiec rozny kroj to rozne lamanie: zrzut ekranu przy budowaniu nie musial
    pokazywac tego samego, co widzi czlowiek. Z krojem wszytym w program jest
    jeden i ten sam wszedzie.

    Inter ma polskie znaki, jest czytelny w duzych stopniach i ma licencje
    SIL OFL (Resources/Fonts/OFL.txt), ktora pozwala go wszyc w program.
    Pliki sa przyciete do lacinki z polskimi literami, strzalek i myslnikow —
    dwa kroje razem ok. 140 KB zamiast 830 KB.

    Program nadal jest jednym plikiem .exe: kroje siedza w BinaryData, nie
    obok.

    ---------------------------------------------------------------------------
    JAK TO DZIALA

    Nikt nie musi pamietac o Inter. Look::getTypefaceForFont() podmienia
    domyslny kroj bezszeryfowy na Inter — kazde juce::Font (15.0f) w programie
    jest wiec juz Interem. Pogrubienie daje SemiBold: pelny Bold w duzym
    stopniu na ciemnym tle "rozlewa sie" i litery sie zlepiaja.
    Kroj o stalej szerokosci (zegar nagrania) zostaje systemowy.
*/

#pragma once

#include <juce_graphics/juce_graphics.h>
#include "BinaryData.h"

namespace zs
{

struct Fonts
{
    Fonts()
        : regular  (juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf,
                                                             (size_t) BinaryData::InterRegular_ttfSize)),
          semibold (juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf,
                                                             (size_t) BinaryData::InterSemiBold_ttfSize))
    {}

    /** Dla Look: wlasciwy kroj albo nullptr, gdy to nie jest kroj domyslny. */
    juce::Typeface::Ptr forFont (const juce::Font& f) const
    {
        if (f.getTypefaceName() != juce::Font::getDefaultSansSerifFontName())
            return nullptr;

        return f.isBold() ? semibold : regular;
    }

    juce::Typeface::Ptr regular, semibold;
};

/** Stopnie pisma. Piec i ani jednego wiecej — inaczej kazdy ekran ma swoje
    15,5 i 16,5 i nic nie wyglada na jedna rodzine. */
namespace type
{
    constexpr float title   = 26.0f;   ///< pytanie albo werdykt na gorze ekranu
    constexpr float heading = 18.0f;
    constexpr float body    = 15.0f;
    constexpr float small   = 13.0f;
    constexpr float tiny    = 11.0f;   ///< etykiety nad liczbami
}

} // namespace zs
