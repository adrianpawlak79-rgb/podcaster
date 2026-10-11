/*
    RoomCheck.h — to, co kreator zmierzyl w pokoju, w postaci, ktora rozumie miks.

    Osobny plik, bo czytaja go dwie strony: Settings.h (zapis miedzy
    uruchomieniami, JUCE) i Decide.h (decyzje miksu, bez JUCE).
*/

#pragma once

namespace mix
{

//==============================================================================
/** Co kreator zmierzyl w pokoju. known = false: kreatora nie przechodzono. */
struct RoomCheck
{
    bool   known            = false;
    double noiseFloorDb     = -140.0;  ///< tlo pokoju w ciszy, dBFS
    double humHz            = 0.0;     ///< 0 = przydzwieku nie bylo
    double humExcessDb      = 0.0;
    double proximityDb      = 0.0;     ///< nadmiar dolu w kroku "Poziom"
    double proximityLimitDb = -5.0;    ///< powyzej tego kreator mowil "za blisko"
    int    daysAgo          = -1;

    bool tooClose() const noexcept { return known && proximityDb > proximityLimitDb; }
};

} // namespace mix
