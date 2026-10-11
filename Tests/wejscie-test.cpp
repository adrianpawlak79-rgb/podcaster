/*
    wejscie-test.cpp — wybor wejscia karty, sprawdzany bez karty.

    Co tu sie moze zepsuc po cichu:
    - zapamietane wejscie 3, a wpieta karta z dwoma — program nie moze wtedy
      nagrywac z niczego, ma wrocic do trybu automatycznego,
    - Recorder liczy tylko wlaczone wejscia, wiec "kanal 0" przy wybranym
      wejsciu 3 to wejscie 3, a nie 1 — inaczej program mowi "slysze
      wejscie 1", gdy mikrofon jest w trzecim.
*/

#include "../Source/InputChoice.h"

#include <cstdio>
#include <string>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-52s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

int main()
{
    using namespace zs::input;

    std::printf ("\nWybor wejscia\n=============\n\n");

    // ---- 1. maska ----------------------------------------------------------------
    std::printf ("  1. ktore wejscia wlaczyc\n");
    check (channelMask (0, 4) == 0b0011, "automatycznie: dwa pierwsze");
    check (channelMask (0, 1) == 0b0001, "automatycznie przy jednym wejsciu: to jedno");
    check (channelMask (3, 4) == 0b0100, "wejscie 3: tylko trzecie");
    check (channelMask (3, 2) == 0b0011, "zapamietane 3, karta ma 2: automatycznie");
    check (channelMask (1, 0) == 0, "brak karty: nic");
    check (channelMask (18, 18) == (uint64_t (1) << 17), "duza karta, wejscie 18");

    // ---- 2. z maski z powrotem na liste --------------------------------------------
    std::printf ("\n  2. co pokazac na liscie\n");
    bool roundTrip = true;
    for (int k = 0; k <= 8; ++k)
        if (choiceFromMask (channelMask (k, 8)) != k) roundTrip = false;
    check (roundTrip, "wybor -> maska -> wybor, wejscia 0..8");
    check (choiceFromMask (0b0101) == 0, "kilka wejsc z ustawien systemu: automatycznie");
    check (choiceFromMask (0) == 0, "zadnego: automatycznie");

    // ---- 3. numer dla czlowieka ------------------------------------------------------
    std::printf ("\n  3. 'slysze wejscie N'\n");
    check (physicalInput (channelMask (3, 4), 0) == 3, "wybrane 3: kanal 0 Recordera to wejscie 3");
    check (physicalInput (channelMask (0, 4), 1) == 2, "automatycznie: kanal 1 to wejscie 2");
    check (physicalInput (0b0001, 1) == 0, "kanal, ktorego nie ma: 0");

    // ---- 4. lista ------------------------------------------------------------------
    std::printf ("\n  4. pozycje listy\n");
    const auto l = labels ({ "Input 1", "Analogue 2", "3" });
    check (l.size() == 4, "automatycznie + trzy wejscia", std::to_string (l.size()));
    check (l[0].find ("1 lub 2") != std::string::npos, "automatycznie mowi, ktorych slucha", l[0]);
    check (l[1] == "Wejście 1", "sama liczba ze sterownika nie jest powtarzana", l[1]);
    check (l[2] == "Wejście 2 — Analogue 2", "nazwa ze sterownika, gdy cos mowi", l[2]);
    check (l[3] == "Wejście 3", "nazwa '3' to tez sama liczba", l[3]);
    check (labels ({ "Mic" })[0] == "Automatycznie", "jedno wejscie: bez '1 lub 2'");

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
