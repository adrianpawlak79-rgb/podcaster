/*
    decide-test.cpp — czy program dobiera ustawienia z tego, co slyszy.

    ---------------------------------------------------------------------------
    CO TU SIE SPRAWDZA

    Nie to, czy ustawienia sa "dobre" — tego zaden test nie rozstrzygnie. Tylko
    to, czy decyzja wynika z materialu: czy nagranie, ktore odjezdza, dostaje
    jazde suwakiem, a nagranie, ktore tylko gestykuluje, jej nie dostaje.

    Kazdy material jest tu zbudowany tak, ze wiadomo z gory, co program ma
    powiedziec. Gdyby rozroznienie gest/wedrowka przestalo dzialac, wysypia sie
    punkty 2 i 3 — i to jest dokladnie ten blad, ktory przez cztery rundy
    odsluchu brzmial jako "przytlumione i za agresywne".
*/

#include "../Source/DSP/Mix/Decide.h"
#include "../Source/Wav.h"

#include <cstdio>
#include <random>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got)
{
    std::printf ("  %-5s %-44s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static std::string num (double v, const char* unit = " dB")
{
    char b[48];
    std::snprintf (b, sizeof (b), "%.1f%s", v, unit);
    return b;
}

/** Mowa z espeaka, powielona do zadanej dlugosci. */
static std::vector<float> speech (const std::vector<float>& src, double sr, double sec)
{
    std::vector<float> out ((size_t) (sr * sec));

    for (size_t i = 0; i < out.size(); ++i)
        out[i] = src[i % src.size()];

    return out;
}

static void addRoom (std::vector<float>& x, float amp, unsigned seed)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> n (0.0f, amp);
    for (auto& v : x) v += n (rng);
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("uzycie: decide-test mowa.wav\n"); return 1; }

    wavio::Audio in;
    if (! wavio::read (argv[1], in).empty()) { std::printf ("brak mowy\n"); return 1; }

    const auto base = in.toMono();
    const double sr = (double) in.sampleRate;

    std::printf ("\nDobor ustawien — czy decyzja wynika z materialu\n");
    std::printf ("===============================================\n\n");

    // ---- 1. nagranie rowne, cisza cyfrowa --------------------------------------
    {
        std::printf ("1. mowa rowna, bez tla\n");

        auto x = speech (base, sr, 70.0);
        const auto d = mix::Decide::listen (x, sr);

        check (! d.params.useRider, "bez jazdy suwakiem", num (d.driftDb));
        check (! d.params.useDenoise, "bez odszumiania", "cisza cyfrowa");
        check (d.params.levelerRatio <= 1.6f, "kompresja lekka",
               num (d.params.levelerRatio, ":1"));
    }

    // ---- 2. to samo, ale poziom odjezdza ---------------------------------------
    {
        std::printf ("\n2. to samo z odjazdem 10 dB przez cale nagranie\n");

        auto x = speech (base, sr, 70.0);

        for (size_t i = 0; i < x.size(); ++i)
        {
            const double t = (double) i / (double) x.size();
            x[i] *= (float) std::pow (10.0, (-10.0 * t) / 20.0);
        }

        const auto d = mix::Decide::listen (x, sr);

        check (d.params.useRider, "suwak wlaczony", num (d.driftDb));
        check (d.params.riderRangeDb > 5.0f, "zakres odpowiada odjazdowi",
               num (d.params.riderRangeDb));
        check (d.params.riderSlopeDbS < 1.0f, "suwak idzie wolno",
               num (d.params.riderSlopeDbS, " dB/s"));
    }

    // ---- 3. to samo, ale poziom skacze w obie strony ---------------------------
    //
    // TO JEST TEN TEST. Rozrzut poziomu jest tu wiekszy niz w punkcie 2, wiec
    // miara oparta na samym rozrzucie kazalaby sciskac jeszcze mocniej. A to
    // jest wykonanie i ma zostac.
    {
        std::printf ("\n3. to samo, ale glosniej i ciszej na przemian co 8 s\n");

        auto x = speech (base, sr, 70.0);

        for (size_t i = 0; i < x.size(); ++i)
        {
            const double t = (double) i / sr;
            const double db = 6.0 * (std::sin (2.0 * M_PI * t / 16.0) > 0.0 ? 1.0 : -1.0);
            x[i] *= (float) std::pow (10.0, db / 20.0);
        }

        const auto d = mix::Decide::listen (x, sr);

        check (! d.params.useRider || d.params.riderRangeDb < 3.0f,
               "suwak wylaczony albo ledwo rusza", num (d.driftDb));
        check (d.sectionDb > 4.0, "wahanie widziane jako budowa nagrania",
               num (d.sectionDb));
    }

    // ---- 4. cichy pokoj w tle --------------------------------------------------
    {
        std::printf ("\n4. to samo z tlem pokoju 30 dB pod mowa\n");

        auto x = speech (base, sr, 70.0);
        addRoom (x, 0.004f, 11);

        const auto d = mix::Decide::listen (x, sr);

        check (d.params.useDenoise, "odszumianie wlaczone",
               num (d.speechDb - d.noiseFloorDb) + " zapasu");
        check (d.params.useGate && ! d.params.autoGate,
               "prog bramy policzony przy doborze", num (d.params.gateThreshDb));
        check (d.params.gateThreshDb > d.noiseFloorDb + 2.0
                 && d.params.gateThreshDb < d.speechDb - 10.0,
               "prog lezy miedzy tlem a mowa",
               num (d.noiseFloorDb) + " < " + num (d.params.gateThreshDb)
                 + " < " + num (d.speechDb));
    }

    // ---- 5. cel glosnosci wynika z zapasu szczytowego ---------------------------
    {
        std::printf ("\n5. cel glosnosci\n");

        auto x = speech (base, sr, 70.0);
        const auto d = mix::Decide::listen (x, sr);

        const double expected = -1.0 - d.plr;

        check (std::fabs (d.freeTargetLufs - std::max (-23.0, expected)) < 0.3
                 || d.freeTargetLufs <= -22.9,
               "granica = sufit minus zapas", num (d.freeTargetLufs, " LUFS"));

        check (d.params.targetLufs >= -23.0 && d.params.targetLufs <= -14.0,
               "cel w rozsadnych granicach", num (d.params.targetLufs, " LUFS"));
    }

    // ---- 6. kazda decyzja ma zdanie --------------------------------------------
    {
        std::printf ("\n6. czy program tlumaczy sie z kazdej decyzji\n");

        auto x = speech (base, sr, 70.0);
        addRoom (x, 0.004f, 5);

        const auto d = mix::Decide::listen (x, sr);

        bool allHaveNumber = true;

        for (const auto& s : d.because)
            if (s.find_first_of ("0123456789") == std::string::npos)
                allHaveNumber = false;

        check (d.because.size() >= 5, "kazdy stopien ma uzasadnienie",
               std::to_string (d.because.size()) + " zdan");
        check (allHaveNumber, "w kazdym zdaniu jest liczba",
               allHaveNumber ? "tak" : "ktoras bez liczby");
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
