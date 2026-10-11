/*
    LampLogic.h — decyzja, co ma pokazywac lampka. Bez JUCE, bez interfejsu.

    Powod wydzielenia: przez trzy wersje z rzedu bledy siedzialy nie w analizie
    dzwieku, tylko WLASNIE TUTAJ — w tym, jak wynik analizy zamienia sie na
    kolor. "Za cicho" zapalalo sie przy poprawnej pracy, bo patrzylo na chwilowy
    poziom zamiast na szczyt z ostatnich kilkunastu sekund. Kazde takie sprawdzenie
    kosztowalo czlowieka nagranie testowe i mowienie do mikrofonu.

    Kod bez zaleznosci od JUCE daje sie skompilowac na kazdym pudle i przepuscic
    przez wygenerowany sygnal: cicha mowa, krzyk w przesterze, wyciszony mikrofon,
    odejscie od mikrofonu w polowie zdania. Testy siedza w Tests/lamp-test.cpp.

    Zegar jest wewnetrzny i posuwany przez dt — dzieki temu test przerabia
    dziesiec minut sesji w ulamku sekundy.
*/

#pragma once

#include "DSP/TakeWatchCore.h"

namespace zs
{

struct LampView
{
    int         state = 0;      ///< 0 spokoj, 1 powtorz, 2 popraw, 3 stop
    const char* msg   = "";     ///< UTF-8
    bool        dim   = false;  ///< sygnalu chwilowo nie ma (pauza)
};

/** Progi w jednym miejscu, zeby dalo sie o nich rozmawiac liczbami. */
struct LampRules
{
    float  clipDb        = -0.2f;   ///< szczyt tutaj albo wyzej to sciete probki
    float  hotDb         = -1.5f;   ///< blisko sufitu
    float  quietDb       = -30.0f;  ///< szczyt ostatnich sekund ponizej = za cicho
    float  deadDb        = -75.0f;  ///< ponizej nie ma nawet tla pokoju: brak sygnalu
    float  pauseDb       = -34.0f;  ///< chwilowa cisza miedzy frazami
    float  activityDb    = 12.0f;   ///< o tyle szczyt musi przewyzszac tlo, zeby uznac
                                    ///< ze cokolwiek do mikrofonu wpada
    double graceSec      = 8.0;     ///< tyle nie oceniamy wzmocnienia po starcie
    double windowSec     = 15.0;    ///< dlugosc kubelka szczytu (dwa kubelki)
    double pauseWaitSec  = 6.0;     ///< po tylu sekundach ciszy zmiana napisu
    double holdRedoSec   = 3.5;
    double holdSetupSec  = 6.0;
    double holdClipSec   = 2.5;
};

class LampLogic
{
public:
    LampRules rules;

    void reset() noexcept
    {
        clock = 0.0;
        holdUntil = -1.0;
        heldState = 0;
        heldMsg = "";
        quietFor = 0.0;
        bucket = bucketPrev = 0.0f;
        floorCur = floorPrev = 1.0f;
        bucketAge = 0.0;
        sinceActivity = 0.0;
        everActive = false;
        sessionPeak = 0.0f;
        clippedInSession = false;
    }

    /** Po R: to, co zapalilo lampke, wlasnie zostalo wyciete (punch and roll)
        — lampka nie trzyma juz koloru za pomylke. Profil glosu, poziomy i to,
        ze w sesji byl przester (rada o wzmocnieniu), zostaja. */
    void afterCut() noexcept
    {
        holdUntil = -1.0;
        heldState = 0;
        heldMsg   = "";
    }

    /** Zdarzenie z TakeWatcha. @p advice ma przezyc wywolanie (staly napis). */
    void event (tw::Kind kind, int severity, const char* advice) noexcept
    {
        if (severity < 1)
            return;

        heldState = severity;
        heldMsg   = advice;
        holdUntil = clock + (severity >= 2 ? rules.holdSetupSec : rules.holdRedoSec);
    }

    /** @param dt        sekundy od poprzedniego wywolania
        @param peakNow   szczyt wejscia w tej klatce (0..1+)
        @param recSec    ile juz nagrane */
    LampView tick (double dt, bool recording, double recSec, float peakNow,
                   bool profileReady, float profileProgress) noexcept
    {
        clock += dt;

        if (! recording)
        {
            quietFor = 0.0;
            return { 0, "Zatrzymane.", true };
        }

        if (peakNow > sessionPeak) sessionPeak = peakNow;

        // ---- okno ostatnich kilkunastu sekund -------------------------------
        // Szczyt CALEJ sesji do niczego sie tu nie nadaje: raz zapamietany,
        // nigdy nie spada, wiec odejscie od mikrofonu w polowie nagrania
        // przeszloby niezauwazone. Dwa kubelki zamieniane na zmiane daja okno
        // 15–30 s. Obok szczytu chodzi MINIMUM — to jest tlo pokoju.
        bucketAge += dt;
        if (peakNow > bucket)   bucket   = peakNow;
        if (peakNow < floorCur) floorCur = peakNow;

        if (bucketAge > rules.windowSec)
        {
            bucketPrev = bucket;   bucket   = 0.0f;
            floorPrev  = floorCur; floorCur = 1.0f;
            bucketAge  = 0.0;
        }

        const float recent = bucket > bucketPrev ? bucket : bucketPrev;
        const float floorL = floorCur < floorPrev ? floorCur : floorPrev;

        const float recentDb = tw::toDb (recent);
        const float floorDb  = tw::toDb (floorL);
        const float nowDb    = tw::toDb (peakNow);

        // ---- czy cokolwiek do tego mikrofonu wpada --------------------------
        // To jest rozroznienie, ktorego brakowalo przez trzy wersje: CISZA i
        // ZA CICHO to nie jest to samo. Mowa siedzi kilkadziesiat decybeli nad
        // tlem pokoju. Jesli szczyt ostatnich sekund jest tuz nad tlem, to nikt
        // nie mowi — i wtedy zadna uwaga o wzmocnieniu nie ma sensu. Dopiero
        // gdy cos nad to tlo wychodzi, wolno oceniac, czy wychodzi dosc wysoko.
        const bool active = recentDb > rules.deadDb
                         && (recentDb - floorDb) > rules.activityDb;

        if (active) { everActive = true; sinceActivity = 0.0; }
        else        { sinceActivity += dt; }

        // ---- przester ma pierwszenstwo przed wszystkim ----------------------
        // Nie przechodzi przez zdarzenia TakeWatcha i nie da sie go wyciszyc.
        // To nie jest opinia programu o tym, jak cos brzmi — to zniszczone
        // probki, ktorych zaden miks nie odzyska.
        if (nowDb >= rules.clipDb)
        {
            clippedInSession = true;
            heldState = 3;
            heldMsg   = "Przester — przykręć wzmocnienie i powtórz frazę.";
            holdUntil = clock + rules.holdClipSec;
        }
        else if (nowDb >= rules.hotDb && clock > holdUntil)
        {
            heldState = 2;
            heldMsg   = "Za gorąco — przykręć wzmocnienie albo odsuń się.";
            holdUntil = clock + rules.holdClipSec;
        }

        if (clock <= holdUntil)
            return { heldState, heldMsg, false };

        // ---- stan spoczynkowy ------------------------------------------------
        if (recSec < rules.graceSec)
        {
            quietFor = 0.0;
            return { 0, "Sprawdzam wejście — powiedz coś.", true };
        }

        // Brak nawet tla pokoju. Zaden mikrofon nie oddaje ciszy cyfrowej —
        // to znaczy, ze wejscie jest nie to, wyciszone albo bez zasilania.
        if (recentDb < rules.deadDb)
        {
            quietFor = 0.0;
            return { 3, "Nie słyszę mikrofonu. Sprawdź wejście i zasilanie.", false };
        }

        if (active)
        {
            if (recentDb < rules.quietDb)
            {
                quietFor = 0.0;
                return { 2, "Za cicho. Podkręć wzmocnienie albo przysuń się — "
                            "celuj w -12 dBFS na szczytach.", false };
            }
        }
        else if (! everActive && sinceActivity > 20.0)
        {
            // nagrywa od dwudziestu sekund i nikt nic nie powiedzial
            quietFor = 0.0;
            return { 0, "Nic nie słychać. Powiedz coś, żebym sprawdził poziom.", true };
        }

        // CISZA MIEDZY FRAZAMI TO NIE JEST BLAD. O wzmocnieniu orzeka wylacznie
        // szczyt z okna wyzej; to, ze akurat teraz jest cicho, znaczy tylko
        // tyle, ze czlowiek nabiera powietrza.
        if (nowDb < rules.pauseDb)
        {
            quietFor += dt;
            return { 0, quietFor > rules.pauseWaitSec ? "Cisza — czekam." : "Pauza.", true };
        }

        quietFor = 0.0;

        if (! profileReady)
        {
            progressPercent = (int) (profileProgress * 100.0f);
            return { 0, "learning", false };   // napis sklada interfejs, z procentem
        }

        return { 0, "Mów dalej.", false };
    }

    bool  clipped()      const noexcept { return clippedInSession; }
    float peak()         const noexcept { return sessionPeak; }
    int   learnPercent() const noexcept { return progressPercent; }

private:
    double clock = 0.0;
    double holdUntil = -1.0;
    int    heldState = 0;
    const char* heldMsg = "";

    double quietFor = 0.0;
    float  bucket = 0.0f, bucketPrev = 0.0f;
    float  floorCur = 1.0f, floorPrev = 1.0f;
    double bucketAge = 0.0;
    double sinceActivity = 0.0;
    bool   everActive = false;

    float sessionPeak = 0.0f;
    bool  clippedInSession = false;
    int   progressPercent = 0;
};

} // namespace zs
