/*
    lamp-test.cpp — stanowisko testowe lampki. Buduje sie i chodzi wszedzie,
    bez JUCE, bez karty dzwiekowej, bez mowienia do mikrofonu.

    Po co to jest. Przez trzy wersje z rzedu bledy siedzialy nie w analizie
    dzwieku, tylko w tym, jak wynik analizy zamienia sie na kolor lampki:
    "za cicho" zapalalo sie w normalnej pauzie miedzy zdaniami, a przester przy
    krzyku do mikrofonu nie zapalal nic. Kazde takie sprawdzenie kosztowalo
    czlowieka osobne nagranie testowe i mowienie na glos.

    Tu sygnal jest generowany, a dziesiec minut sesji przelatuje w ulamku
    sekundy. Sytuacje, ktore trzeba bylo odgrywac przed mikrofonem — krzyk,
    szept, odejscie od mikrofonu w polowie zdania, wyciecie wejscia — sa
    zapisane jako liczby i chodza przy kazdej budowie.

        g++ -O2 -std=c++17 -I../Source lamp-test.cpp -o lamp-test && ./lamp-test
*/

#include "../Source/LampLogic.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <random>

static constexpr double kSR    = 48000.0;
static constexpr int    kBlock = 512;
static constexpr double kFps   = 30.0;

//==============================================================================
/** Prosty generator "glosu": stos harmonicznych z lekkim vibrato i szumem
    oddechu. Nie chodzi o to, zeby brzmialo — chodzi o to, zeby TakeWatch mial
    co uznac za fraze: energie w pasmie mowy, atak, wybrzmienie i przerwy. */
struct Voice
{
    double f0 = 110.0;
    double phase = 0.0;
    double t = 0.0;
    std::mt19937 rng { 12345 };
    std::normal_distribution<float> gauss { 0.0f, 1.0f };

    /** @param peak  docelowy szczyt frazy w skali liniowej
        @param on    czy w tej chwili trwa fraza */
    float sample (double dt, float peak, bool on, float roomTone)
    {
        t += dt;

        const double f = f0 * (1.0 + 0.008 * std::sin (2.0 * M_PI * 4.7 * t));
        phase += 2.0 * M_PI * f * dt;
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;

        float v = 0.0f;

        if (on)
        {
            // harmoniczne z opadajacym widmem + odrobina sykow wyzej
            for (int h = 1; h <= 14; ++h)
                v += (float) (std::sin (phase * h) / (double) (h * h));

            v *= 1.55f;                       // normalizacja do ~1.0
            v += 0.035f * gauss (rng);        // przydech
            v *= peak;
        }

        return v + roomTone * gauss (rng);
    }
};

//==============================================================================
struct Frame
{
    double sec;
    int    state;
    std::string msg;
    bool   dim;
};

struct Scenario
{
    std::string name;
    double      seconds;
    /** Zwraca szczyt frazy i to, czy fraza trwa, dla danej chwili. */
    void      (*plan) (double t, float& peak, bool& on, float& room);
};

static std::vector<Frame> run (const Scenario& sc, bool verbose)
{
    tw::TakeWatch watch;
    watch.prepare (kSR, kBlock);
    watch.reset();

    zs::LampLogic logic;
    logic.reset();

    Voice voice;
    std::vector<float> buf ((size_t) kBlock);

    float  level = 0.0f;
    double tickAcc = 0.0;
    double elapsed = 0.0;
    std::vector<Frame> frames;

    const double blockSec = kBlock / kSR;
    const double tickSec  = 1.0 / kFps;

    while (elapsed < sc.seconds)
    {
        float peak = 0.0f, room = 0.0f;
        bool  on = false;
        sc.plan (elapsed, peak, on, room);

        float blockPeak = 0.0f;

        for (int n = 0; n < kBlock; ++n)
        {
            float s = voice.sample (1.0 / kSR, peak, on, room);

            // twarde obciecie na sufcie — tak samo jak zrobilaby to karta
            if (s >  1.0f) s =  1.0f;
            if (s < -1.0f) s = -1.0f;

            buf[(size_t) n] = s;
            const float a = std::fabs (s);
            if (a > blockPeak) blockPeak = a;
        }

        // ten sam wzor opadania, co w Recorder.h
        level = blockPeak > level ? blockPeak : level * 0.82f + blockPeak * 0.18f;

        const float* chans[1] = { buf.data() };
        watch.process (chans, 1, kBlock, elapsed);

        elapsed += blockSec;
        tickAcc += blockSec;

        while (tickAcc >= tickSec)
        {
            tickAcc -= tickSec;

            tw::Event e;
            while (watch.nextEvent (e))
                if ((int) e.severity >= 1)
                    logic.event (e.kind, (int) e.severity, "zdarzenie");

            const auto v = logic.tick (tickSec, true, elapsed, level,
                                       watch.profileReady(), watch.profileProgress());

            frames.push_back ({ elapsed, v.state, v.msg, v.dim });
        }
    }

    if (verbose)
    {
        std::string last;
        for (const auto& f : frames)
            if (f.msg != last)
            {
                std::printf ("      %6.1f s  [%d]%s %s\n", f.sec, f.state,
                             f.dim ? " ." : "  ", f.msg.c_str());
                last = f.msg;
            }
    }

    return frames;
}

//==============================================================================
static int failures = 0;

static void check (bool ok, const std::string& what)
{
    std::printf ("      %s  %s\n", ok ? "OK  " : "BLAD", what.c_str());
    if (! ok) ++failures;
}

/** Czy w przedziale czasu pojawil sie napis zaczynajacy sie od @p prefix. */
static bool seen (const std::vector<Frame>& f, const char* prefix, double from, double to)
{
    for (const auto& x : f)
        if (x.sec >= from && x.sec <= to && x.msg.rfind (prefix, 0) == 0)
            return true;
    return false;
}

static bool maxState (const std::vector<Frame>& f, double from, double to, int limit)
{
    for (const auto& x : f)
        if (x.sec >= from && x.sec <= to && x.state > limit)
            return false;
    return true;
}

//==============================================================================
// Plany sesji. Fraza trwa 3 s, pauza 1,2 s — tak mowi czlowiek.

static bool phraseOn (double t, double period = 4.2, double len = 3.0)
{
    return std::fmod (t, period) < len;
}

static void planDobry (double t, float& peak, bool& on, float& room)
{
    peak = 0.126f;            // ~ -18 dBFS
    room = 0.0004f;           // ~ -68 dBFS tla
    on   = phraseOn (t);
}

static void planZaCicho (double t, float& peak, bool& on, float& room)
{
    peak = 0.0056f;           // ~ -45 dBFS
    room = 0.0002f;
    on   = phraseOn (t);
}

static void planKrzyk (double t, float& peak, bool& on, float& room)
{
    peak = 1.9f;              // grubo ponad sufit — karta obetnie
    room = 0.0004f;
    on   = phraseOn (t);
}

static void planMartwyMikrofon (double t, float& peak, bool& on, float& room)
{
    (void) t; peak = 0.0f; room = 0.0f; on = false;
}

static void planDlugaCisza (double t, float& peak, bool& on, float& room)
{
    peak = 0.126f;
    room = 0.0004f;
    on   = t < 40.0 ? phraseOn (t) : false;    // po 40 s czlowiek milknie
}

static void planOdejscie (double t, float& peak, bool& on, float& room)
{
    peak = t < 30.0 ? 0.126f : 0.0079f;        // -18 dBFS, potem -42 dBFS
    room = 0.0004f;
    on   = phraseOn (t);
}

static void planWyciecie (double t, float& peak, bool& on, float& room)
{
    const bool dead = t > 35.0;                 // ktos wypial kabel
    peak = dead ? 0.0f : 0.126f;
    room = dead ? 0.0f : 0.0004f;
    on   = ! dead && phraseOn (t);
}

//==============================================================================
int main (int argc, char** argv)
{
    const bool verbose = argc > 1 && std::strcmp (argv[1], "-v") == 0;

    std::printf ("\nLampka — sesje wygenerowane, bez mikrofonu\n");
    std::printf ("==========================================\n");

    {
        std::printf ("\n  1. dobry poziom, pauzy miedzy zdaniami (90 s @ -18 dBFS)\n");
        auto f = run ({ "dobry", 90.0, planDobry }, verbose);
        check (maxState (f, 0, 90, 0),
               "lampka ani razu nie zolknie");
        check (seen (f, "Pauza.", 10, 90),
               "w pauzie pokazuje \"Pauza.\"");
        check (! seen (f, "Za cicho", 0, 90),
               "nigdy nie mowi \"za cicho\"");
    }

    {
        std::printf ("\n  2. za ciche wejscie (60 s @ -45 dBFS)\n");
        auto f = run ({ "cicho", 60.0, planZaCicho }, verbose);
        check (! seen (f, "Za cicho", 0, 8),
               "przez pierwsze 8 s nie ocenia");
        check (seen (f, "Za cicho", 8, 20),
               "miedzy 8 a 20 s zapala \"za cicho\"");
    }

    {
        std::printf ("\n  3. krzyk do mikrofonu (30 s, sygnal ponad sufitem)\n");
        auto f = run ({ "krzyk", 30.0, planKrzyk }, verbose);
        check (seen (f, "Przester", 0, 6),
               "przester widac w ciagu 6 s");
    }

    {
        std::printf ("\n  4. mikrofon nie gra w ogole (30 s ciszy cyfrowej)\n");
        auto f = run ({ "martwy", 30.0, planMartwyMikrofon }, verbose);
        check (seen (f, "Nie słyszę mikrofonu", 8, 30),
               "melduje brak sygnalu");
        check (! seen (f, "Za cicho", 0, 30),
               "nie doradza podkrecenia wzmocnienia");
    }

    {
        std::printf ("\n  5. czlowiek milknie na 50 s po dobrym poczatku\n");
        auto f = run ({ "cisza", 90.0, planDlugaCisza }, verbose);
        check (! seen (f, "Za cicho", 40, 90),
               "dluga cisza to NIE jest \"za cicho\"");
        check (! seen (f, "Nie słyszę mikrofonu", 40, 90),
               "dluga cisza to NIE jest awaria mikrofonu");
        check (seen (f, "Cisza — czekam.", 45, 90),
               "mowi \"cisza — czekam\"");
    }

    {
        std::printf ("\n  6. odejscie od mikrofonu w polowie (-18 -> -42 dBFS)\n");
        auto f = run ({ "odejscie", 90.0, planOdejscie }, verbose);
        check (! seen (f, "Za cicho", 0, 30),
               "dopoki jest dobrze, milczy");
        check (seen (f, "Za cicho", 30, 70),
               "po odejsciu ostrzega w ciagu ~35 s");
    }

    {
        std::printf ("\n  7. kabel wypada w 35 sekundzie\n");
        auto f = run ({ "wyciecie", 80.0, planWyciecie }, verbose);
        check (! seen (f, "Nie słyszę mikrofonu", 0, 35),
               "przed awaria cisza w eterze");
        check (seen (f, "Nie słyszę mikrofonu", 35, 80),
               "po awarii melduje brak sygnalu");
    }

    std::printf ("\n==========================================\n");
    std::printf ("%s\n\n", failures == 0 ? "wszystko przeszlo"
                                         : (std::to_string (failures) + " nieprzeszlych").c_str());
    return failures == 0 ? 0 : 1;
}
