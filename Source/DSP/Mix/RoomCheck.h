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
    double bleedPercent     = 0.0;     ///< ile sluchawek wraca do mikrofonu
    double bleedLimit       = 4.0;     ///< powyzej tego kreator mowil "uciekaja"
    int    daysAgo          = -1;

    bool tooClose() const noexcept { return known && proximityDb > proximityLimitDb; }

    /** Sluchawki uciekaja do mikrofonu (albo to glosniki): nic nie grac w
        trakcie nagrania, bo wejdzie do nagrania (pre-roll po R, PreRoll.h). */
    bool headphonesLeak() const noexcept { return known && bleedPercent > bleedLimit; }
};

} // namespace mix
