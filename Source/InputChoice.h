/*
    InputChoice.h — ktore wejscie karty jest mikrofonem.

    ---------------------------------------------------------------------------
    PO CO

    Program sam wybieral wejscie: wlaczal dwa pierwsze wejscia karty i
    sluchal glosniejszego. Na karcie z czterema wejsciami mikrofon wpiety w
    trzecie byl dla programu niewidoczny — i nie bylo jak mu tego powiedziec.
    Teraz czlowiek moze wskazac wejscie sam; "Automatycznie" zostaje jako
    domyslne i dziala jak dotad.

    Bez JUCE, zeby test (Tests/wejscie-test.cpp) skladal sie jednym g++.
    MainComponent.h zamienia to na AudioDeviceSetup::inputChannels.

    ---------------------------------------------------------------------------
    NUMERACJA

    choice 0      = automatycznie: dwa pierwsze wejscia, glosniejsze wygrywa
                    (Recorder wybiera kanal sam, jak dotad),
    choice k >= 1 = tylko wejscie k (liczone od 1, jak na obudowie karty).
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace zs
{
namespace input
{

constexpr int autoChannels = 2;   ///< tyle wejsc slucha tryb automatyczny
constexpr int maxChannels  = 64;  ///< wiecej nie zmiesci maska

/** Maska wejsc do wlaczenia. Wybor spoza karty (np. zapamietane wejscie 3,
    a teraz wpieta karta z dwoma) wraca do trybu automatycznego. */
inline uint64_t channelMask (int choice, int available)
{
    if (available <= 0) return 0;
    if (available > maxChannels) available = maxChannels;

    if (choice >= 1 && choice <= available)
        return uint64_t (1) << (choice - 1);

    uint64_t m = 0;
    for (int i = 0; i < autoChannels && i < available; ++i) m |= uint64_t (1) << i;
    return m;
}

/** Odwrotnie: co pokazac na liscie, skoro karta ma wlaczone takie wejscia.
    Jedno wejscie = wybrane recznie; cokolwiek innego = automatycznie. */
inline int choiceFromMask (uint64_t mask)
{
    if (mask == 0 || (mask & (mask - 1)) != 0) return 0;

    int k = 1;
    while ((mask & 1) == 0) { mask >>= 1; ++k; }
    return k;
}

/** Recorder numeruje tylko wlaczone wejscia (0, 1, ...). Ktore to wejscie
    karty? Liczone od 1, do pokazania czlowiekowi. 0 = nie wiadomo. */
inline int physicalInput (uint64_t mask, int activeIndex)
{
    for (int bit = 0, seen = 0; bit < maxChannels; ++bit)
        if (mask & (uint64_t (1) << bit))
        {
            if (seen == activeIndex) return bit + 1;
            ++seen;
        }
    return 0;
}

/** Pozycje listy (UTF-8): najpierw "Automatycznie", potem wejscia karty.
    Nazwa ze sterownika tylko wtedy, gdy mowi cos wiecej niz numer. */
inline std::vector<std::string> labels (const std::vector<std::string>& driverNames)
{
    std::vector<std::string> out;
    out.push_back (driverNames.size() >= 2 ? "Automatycznie (1 lub 2)"
                                           : "Automatycznie");

    for (size_t i = 0; i < driverNames.size() && i < (size_t) maxChannels; ++i)
    {
        const std::string number = std::to_string (i + 1);
        std::string s = "Wejście " + number;

        const auto& n = driverNames[i];
        const bool justNumber = n.empty() || n == number
                             || n == "Input " + number || n == "Input" + number
                             || n == "In " + number;
        if (! justNumber) s += " — " + n;

        out.push_back (s);
    }
    return out;
}

} // namespace input
} // namespace zs
