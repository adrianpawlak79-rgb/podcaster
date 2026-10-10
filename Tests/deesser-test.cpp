/*
    deesser-test.cpp — czy de-esser robi to, co ma w ustawieniach.

    ---------------------------------------------------------------------------
    SKAD SIE WZIAL TEN TEST

    De-esser dzialal przez caly czas, mial sensowny detektor i sensowne
    parametry, i przechodzil kazde spojrzenie na kod. Czlowiek sluchal kolejnych
    wersji i przy kazdej mowil to samo: "przytlumione", "zamulone", "gorsze". Ja
    trzy razy zmienialem liczbe w ustawieniach zamiast sprawdzic, CO TA LICZBA
    ROBI z widmem.

    Gdy w koncu sprawdzilem — puszczajac szum z detektorem wymuszonym na stale —
    okazalo sie, ze ustawienie "tlum najwyzej 6 dB" robi dziure 11,4 dB przy
    5 kHz i zabiera 3 dB obecnosci przy 2,5 kHz. Przyczyna: podzial na pasma
    przez ODEJMOWANIE. Filtr obraca faze, wiec "sygnal minus dol" nie jest gora,
    i po zlozeniu polowki sie czesciowo kasuja.

    Ten test istnieje, zeby taka rozbieznosc miedzy ustawieniem a skutkiem nie
    mogla juz przejsc niezauwazona. Nie sprawdza, czy de-esser "brzmi dobrze" —
    sprawdza, czy robi to, co obiecuje, i tylko tam, gdzie obiecuje.
*/

#include "../Source/DSP/Mix/Dynamics.h"
#include "../Source/DSP/Mix/Fft.h"

#include <cstdio>
#include <random>
#include <vector>

static int fails = 0;

static void check (bool ok, const char* what, const char* got)
{
    std::printf ("  %-5s %-46s %s\n", ok ? "OK" : "ZLE", what, got);
    if (! ok) ++fails;
}

/** Odpowiedz de-essera przy tlumieniu wymuszonym na stale, w danym pasmie. */
static double responseDb (float maxCut, double lo, double hi)
{
    const double sr = 48000.0;

    mix::DeEsser d;
    d.params.thresholdDb = -60.0f;      // zawsze tnie do maksimum
    d.params.maxCutDb    = maxCut;
    d.params.attackMs    = 0.1;
    d.params.releaseMs   = 1.0;
    d.prepare (sr);

    std::mt19937 rng (7);
    std::normal_distribution<float> g (0.0f, 0.05f);

    const size_t N = 1 << 16;
    const size_t skip = 48000;          // sekunda na ustalenie sie detektora

    std::vector<float> x (N + skip), y (N + skip);
    for (auto& v : x) v = g (rng);
    for (size_t i = 0; i < x.size(); ++i) y[i] = d.process (x[i]);

    std::vector<double> re1 (N), im1 (N), re2 (N), im2 (N), w;
    mix::hann (w, (int) N);

    for (size_t i = 0; i < N; ++i)
    {
        re1[i] = x[i + skip] * w[i]; im1[i] = 0.0;
        re2[i] = y[i + skip] * w[i]; im2[i] = 0.0;
    }

    mix::Fft f;
    f.prepare ((int) N);
    f.run (re1, im1, false);
    f.run (re2, im2, false);

    double a = 0.0, b = 0.0;

    for (size_t i = 0; i < N / 2; ++i)
    {
        const double hz = (double) i * sr / (double) N;
        if (hz < lo || hz >= hi) continue;
        a += re1[i] * re1[i] + im1[i] * im1[i];
        b += re2[i] * re2[i] + im2[i] * im2[i];
    }

    return 10.0 * std::log10 (b / (a + 1e-30) + 1e-30);
}

//==============================================================================
/** Stary de-esser, odtworzony tu i tylko tu.

    Test, ktory nigdy niczego nie zlapal, nie jest testem — jest ozdoba. Wiec
    trzymamy obok wersje z bledem i sprawdzamy, ze kryteria powyzej ja
    ODRZUCAJA. Gdyby ktos kiedys je poluzowal "bo sie nie zgadzaja o wlos", ten
    punkt zapali sie pierwszy.
*/
static double oldResponseDb (float maxCut, double lo, double hi)
{
    const double sr = 48000.0;

    mix::Biquad lp1, lp2;
    lp1.setCoeffs (mix::design::lowpass (5000.0, sr));
    lp2.setCoeffs (mix::design::lowpass (5000.0, sr));

    const float g = std::pow (10.0f, maxCut / 20.0f);

    std::mt19937 rng (7);
    std::normal_distribution<float> gen (0.0f, 0.05f);

    const size_t N = 1 << 16, skip = 48000;
    std::vector<float> x (N + skip), y (N + skip);
    for (auto& v : x) v = gen (rng);

    for (size_t i = 0; i < x.size(); ++i)
    {
        const float low  = lp2.process (lp1.process (x[i]));
        const float high = x[i] - low;              // <- tu byl blad
        y[i] = low + high * g;
    }

    std::vector<double> re1 (N), im1 (N), re2 (N), im2 (N), w;
    mix::hann (w, (int) N);

    for (size_t i = 0; i < N; ++i)
    {
        re1[i] = x[i + skip] * w[i]; im1[i] = 0.0;
        re2[i] = y[i + skip] * w[i]; im2[i] = 0.0;
    }

    mix::Fft f;
    f.prepare ((int) N);
    f.run (re1, im1, false);
    f.run (re2, im2, false);

    double a = 0.0, b = 0.0;

    for (size_t i = 0; i < N / 2; ++i)
    {
        const double hz = (double) i * sr / (double) N;
        if (hz < lo || hz >= hi) continue;
        a += re1[i] * re1[i] + im1[i] * im1[i];
        b += re2[i] * re2[i] + im2[i] * im2[i];
    }

    return 10.0 * std::log10 (b / (a + 1e-30) + 1e-30);
}

int main()
{
    char s[96];

    std::printf ("\nDe-esser — czy tnie tyle, ile obiecuje, i tam, gdzie obiecuje\n");
    std::printf ("=============================================================\n\n");

    // ---- 1. nie tnie wiecej, niz mu kazano -------------------------------------
    std::printf ("1. glebokosc tlumienia\n");

    for (float cut : { -3.0f, -6.0f, -12.0f })
    {
        double worst = 0.0;
        double worstHz = 0.0;

        for (double hz = 250.0; hz < 20000.0; hz *= std::pow (2.0, 1.0 / 3.0))
        {
            const double d = responseDb (cut, hz / 1.12, hz * 1.12);
            if (d < worst) { worst = d; worstHz = hz; }
        }

        // pol decybela zapasu na niedokladnosc pomiaru szumem
        std::snprintf (s, sizeof (s), "najglebiej %+.2f dB przy %.0f Hz", worst, worstHz);
        std::printf ("   ustawienie %.0f dB:\n", cut);
        check (worst > cut - 0.5f, "nigdzie nie tnie glebiej niz ustawienie", s);
    }

    // ---- 2. nie rusza obecnosci ------------------------------------------------
    //
    // Tu siedzial blad: przy -6 dB stara wersja zabierala 3,0 dB przy 2,5 kHz i
    // 10,0 dB przy 4 kHz. Glos traci wtedy zrozumialosc, a nie syczenie.
    std::printf ("\n2. obecnosc glosu (ponizej 4 kHz) ma zostac\n");

    {
        const double d1000 = responseDb (-6.0f, 900.0, 1120.0);
        const double d2500 = responseDb (-6.0f, 2240.0, 2820.0);
        const double d4000 = responseDb (-6.0f, 3560.0, 4480.0);

        std::snprintf (s, sizeof (s), "%+.2f dB", d1000);
        check (d1000 > -0.5, "1 kHz nietkniete", s);

        std::snprintf (s, sizeof (s), "%+.2f dB", d2500);
        check (d2500 > -1.5, "2,5 kHz prawie nietkniete", s);

        std::snprintf (s, sizeof (s), "%+.2f dB", d4000);
        check (d4000 > -3.0, "4 kHz najwyzej lekko przyciete", s);
    }

    // ---- 3. krzywa jest monotoniczna ------------------------------------------
    //
    // Dziura powstala z kasowania fazowego nie byla po prostu "za gleboka" — byla
    // LOKALNA: -11 dB przy 5 kHz i -6 dB przy 8 kHz. Polka tak nie potrafi.
    std::printf ("\n3. tlumienie ma rosnac z czestotliwoscia, bez dziur\n");

    {
        double prev = 1.0;
        double worstJump = 0.0;
        double atHz = 0.0;
        bool   monotonic = true;

        for (double hz = 500.0; hz < 18000.0; hz *= std::pow (2.0, 1.0 / 3.0))
        {
            const double d = responseDb (-9.0f, hz / 1.12, hz * 1.12);

            if (d > prev + 0.35)          // zrobilo sie CICHSZE nizej niz wyzej
            {
                monotonic = false;
                if (d - prev > worstJump) { worstJump = d - prev; atHz = hz; }
            }

            prev = d;
        }

        if (monotonic) std::snprintf (s, sizeof (s), "bez zawrotow");
        else           std::snprintf (s, sizeof (s), "zawrot %+.2f dB przy %.0f Hz", worstJump, atHz);

        check (monotonic, "krzywa opada i nie wraca", s);
    }

    // ---- 4. wylaczony znaczy wylaczony -----------------------------------------
    std::printf ("\n4. przy zerowym tlumieniu nie zmienia nic\n");

    {
        const double d = responseDb (0.0f, 4000.0, 12000.0);
        std::snprintf (s, sizeof (s), "%+.2f dB w 4-12 kHz", d);
        check (std::fabs (d) < 0.2, "zero to zero", s);
    }

    // ---- 5. czy te kryteria w ogole cokolwiek lapia ----------------------------
    std::printf ("\n5. kontrola: stara wersja ma TE testy oblac\n");

    {
        const double d2500 = oldResponseDb (-6.0f, 2240.0, 2820.0);
        const double d5000 = oldResponseDb (-6.0f, 4480.0, 5640.0);

        std::snprintf (s, sizeof (s), "stara: %+.2f dB (prog -1,5)", d2500);
        check (d2500 < -1.5, "punkt 2 odrzuca stara wersje", s);

        std::snprintf (s, sizeof (s), "stara: %+.2f dB przy ustawieniu -6", d5000);
        check (d5000 < -6.5, "punkt 1 odrzuca stara wersje", s);
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
