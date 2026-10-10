/*
    dynamics-test.cpp — czy brama, kompresor, de-esser i limiter robia to, co
    obiecuja. Wszystko na wygenerowanym sygnale, bez mikrofonu i bez sluchania.

    Testy sa liczbowe, nie "brzmi dobrze". Kompresor 3:1 ma dawac 3:1 z
    dokladnoscia do polowy decybela; atak 10 ms ma trwac 10 ms, a nie 40;
    limiter z sufitem -1 dBTP ma nie przepuscic -0,4 dBTP. Roznica miedzy
    wtyczka dobra a byle jaka siedzi wlasnie w tych liczbach.

        g++ -O2 -std=c++17 -I../Source dynamics-test.cpp -o dynamics-test
*/

#include "../Source/DSP/Mix/Dynamics.h"
#include "../Source/DSP/Mix/Limiter.h"
#include "../Source/DSP/Mix/Loudness.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static constexpr double kSR = 48000.0;
static int failures = 0;

static void check (bool ok, const std::string& what, const std::string& detail = {})
{
    std::printf ("      %s  %-52s %s\n", ok ? "OK  " : "BLAD", what.c_str(), detail.c_str());
    if (! ok) ++failures;
}

static std::string num (double v, int dec = 2)
{
    char b[64];
    std::snprintf (b, sizeof (b), "%.*f", dec, v);
    return b;
}

//==============================================================================
/** Poziom skuteczny w pasmie, w dBFS. */
static double bandRmsDb (const std::vector<float>& x, double hz, double q,
                         size_t from, size_t to)
{
    mix::Biquad f;
    f.setCoeffs (mix::design::bandpass (hz, kSR, q));

    // filtr musi sie ustabilizowac przed pomiarem
    const size_t warm = from > 4800 ? from - 4800 : 0;
    double sum = 0.0;
    size_t n = 0;

    for (size_t i = warm; i < to && i < x.size(); ++i)
    {
        const float y = f.process (x[i]);

        if (i >= from) { sum += (double) y * y; ++n; }
    }

    return n > 0 && sum > 0.0 ? 10.0 * std::log10 (sum / n) : -180.0;
}

static double rmsDb (const std::vector<float>& x, size_t from, size_t to)
{
    double sum = 0.0;
    size_t n = 0;

    for (size_t i = from; i < to && i < x.size(); ++i) { sum += (double) x[i] * x[i]; ++n; }

    return n > 0 && sum > 0.0 ? 10.0 * std::log10 (sum / n) : -180.0;
}

static std::vector<float> sine (double hz, double dbfs, double seconds)
{
    const size_t n = (size_t) (seconds * kSR);
    const double a = std::pow (10.0, dbfs / 20.0);
    std::vector<float> v (n);

    for (size_t i = 0; i < n; ++i)
        v[i] = (float) (a * std::sin (2.0 * M_PI * hz * (double) i / kSR));

    return v;
}

//==============================================================================
static void testKrzywaKompresji()
{
    std::printf ("\n  1. kompresor — krzywa statyczna (prog -20 dBFS, 3:1, bez kolana)\n");

    const double levels[] = { -40, -30, -25, -20, -15, -10, -5 };

    for (double in : levels)
    {
        mix::Compressor comp;
        comp.params.thresholdDb = -20.0f;
        comp.params.ratio = 3.0f;
        comp.params.kneeDb = 0.0f;
        comp.params.attackMs = 1.0;
        comp.params.releaseMs = 50.0;
        comp.prepare (kSR);

        auto x = sine (1000.0, in, 1.0);
        std::vector<float> y (x.size());

        for (size_t i = 0; i < x.size(); ++i)
            y[i] = comp.process (x[i]);

        // pomiar po ustabilizowaniu, na ostatnich 200 ms
        const double outRms = rmsDb (y, y.size() - 9600, y.size());
        const double inRms  = rmsDb (x, x.size() - 9600, x.size());

        // oczekiwana zmiana wobec wejscia
        const double overPeak = in - (-20.0);
        const double expected = overPeak <= 0.0 ? inRms
                                                : inRms - overPeak * (1.0 - 1.0 / 3.0);

        check (std::fabs (outRms - expected) < 0.5,
               "wejscie " + num (in, 0) + " dBFS",
               "wyszlo " + num (outRms) + ", oczekiwane " + num (expected));
    }
}

static void testCzasAtaku()
{
    std::printf ("\n  2. kompresor — czasy ataku i zwolnienia\n");

    const double attackMs = 10.0, releaseMs = 200.0;

    mix::Compressor comp;
    comp.params.thresholdDb = -30.0f;
    comp.params.ratio = 4.0f;
    comp.params.kneeDb = 0.0f;
    comp.params.attackMs = attackMs;
    comp.params.releaseMs = releaseMs;
    comp.prepare (kSR);

    // skok z -40 na -6 dBFS w polowie, potem powrot
    auto quiet = sine (1000.0, -40.0, 0.5);
    auto loud  = sine (1000.0, -6.0, 0.5);
    auto back  = sine (1000.0, -40.0, 0.5);

    std::vector<float> x;
    x.insert (x.end(), quiet.begin(), quiet.end());
    x.insert (x.end(), loud.begin(), loud.end());
    x.insert (x.end(), back.begin(), back.end());

    std::vector<float> gr (x.size());

    for (size_t i = 0; i < x.size(); ++i)
    {
        comp.process (x[i]);
        gr[i] = comp.gainReductionDb();
    }

    const size_t stepAt = quiet.size();
    const float  finalGr = gr[stepAt + (size_t) (0.4 * kSR)];
    const float  target63 = finalGr * 0.63f;

    size_t reached = stepAt;
    while (reached < x.size() && gr[reached] > target63) ++reached;

    const double measuredAttack = (reached - stepAt) / kSR * 1000.0;

    check (measuredAttack > attackMs * 0.6 && measuredAttack < attackMs * 1.6,
           "atak " + num (attackMs, 0) + " ms",
           "zmierzone " + num (measuredAttack, 1) + " ms");

    // zwolnienie: od konca glosnego fragmentu do powrotu do 37% redukcji
    const size_t relAt = stepAt + loud.size();
    const float back37 = finalGr * 0.37f;
    size_t r = relAt;
    while (r < x.size() && gr[r] < back37) ++r;

    const double measuredRelease = (r - relAt) / kSR * 1000.0;

    check (measuredRelease > releaseMs * 0.6 && measuredRelease < releaseMs * 1.6,
           "zwolnienie " + num (releaseMs, 0) + " ms",
           "zmierzone " + num (measuredRelease, 1) + " ms");
}

static void testBrama()
{
    std::printf ("\n  3. brama — tlo miedzy frazami, nienaruszone poczatki slow\n");

    std::mt19937 rng { 99 };
    std::normal_distribution<float> g { 0.0f, 1.0f };

    // 4 s: 1 s tla, 1 s mowy, 1 s tla, 1 s mowy
    const size_t n = (size_t) (4.0 * kSR);
    std::vector<float> x (n);

    const double roomAmp = std::pow (10.0, -55.0 / 20.0);
    const double voiceAmp = std::pow (10.0, -14.0 / 20.0);
    double phase = 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / kSR;
        const bool on = (t >= 1.0 && t < 2.0) || (t >= 3.0);

        double v = roomAmp * g (rng) * 0.3;

        if (on)
        {
            phase += 2.0 * M_PI * 120.0 / kSR;
            double s = 0.0;
            for (int h = 1; h <= 12; ++h) s += std::sin (phase * h) / (h * h);
            v += voiceAmp * s * 1.55;
        }

        x[i] = (float) v;
    }

    mix::Gate gate;
    gate.params.thresholdDb = -40.0f;
    gate.params.floorDb = -18.0f;
    gate.prepare (kSR);

    std::vector<float> y (n);
    for (size_t i = 0; i < n; ++i) y[i] = gate.process (x[i]);

    const size_t lat = (size_t) gate.latencySamples();

    // tlo: 2,5–2,9 s (dobrze po zamknieciu bramy)
    const double bgIn  = rmsDb (x, (size_t) (2.5 * kSR), (size_t) (2.9 * kSR));
    const double bgOut = rmsDb (y, (size_t) (2.5 * kSR) + lat, (size_t) (2.9 * kSR) + lat);

    check (bgIn - bgOut > 10.0,
           "tlo miedzy frazami stlumione o >10 dB",
           "o " + num (bgIn - bgOut, 1) + " dB");

    // poczatek frazy: pierwsze 8 ms po 3,0 s — to jest test wyprzedzenia
    const size_t a0 = (size_t) (3.0 * kSR) + lat;
    const size_t a1 = a0 + (size_t) (0.008 * kSR);
    const double onsetIn  = rmsDb (x, (size_t) (3.0 * kSR), (size_t) (3.0 * kSR) + (size_t) (0.008 * kSR));
    const double onsetOut = rmsDb (y, a0, a1);

    check (onsetIn - onsetOut < 1.5,
           "poczatek frazy nietkniety (<1,5 dB)",
           "stracone " + num (onsetIn - onsetOut, 2) + " dB");

    // srodek frazy w ogole nie ruszony
    const double midIn  = rmsDb (x, (size_t) (3.4 * kSR), (size_t) (3.8 * kSR));
    const double midOut = rmsDb (y, (size_t) (3.4 * kSR) + lat, (size_t) (3.8 * kSR) + lat);

    check (std::fabs (midIn - midOut) < 0.3,
           "srodek frazy przechodzi 1:1",
           "roznica " + num (midIn - midOut, 3) + " dB");
}

static void testDeEsser()
{
    std::printf ("\n  4. de-esser — tlumi sybilant, nie rusza korpusu glosu\n");

    const size_t n = (size_t) (3.0 * kSR);
    std::vector<float> x (n);

    // caly czas ton 1 kHz (korpus), a w srodku dochodzi 7 kHz (sybilant)
    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / kSR;
        double v = std::pow (10.0, -18.0 / 20.0) * std::sin (2.0 * M_PI * 1000.0 * t);

        if (t >= 1.0 && t < 2.0)
            v += std::pow (10.0, -14.0 / 20.0) * std::sin (2.0 * M_PI * 7000.0 * t);

        x[i] = (float) v;
    }

    mix::DeEsser de;
    de.prepare (kSR);

    std::vector<float> y (n);
    for (size_t i = 0; i < n; ++i) y[i] = de.process (x[i]);

    const size_t s0 = (size_t) (1.5 * kSR), s1 = (size_t) (1.9 * kSR);

    const double sibIn  = bandRmsDb (x, 7000.0, 4.0, s0, s1);
    const double sibOut = bandRmsDb (y, 7000.0, 4.0, s0, s1);
    const double bodyIn  = bandRmsDb (x, 1000.0, 4.0, s0, s1);
    const double bodyOut = bandRmsDb (y, 1000.0, 4.0, s0, s1);

    check (sibIn - sibOut > 5.0,
           "sybilant stlumiony o >5 dB",
           "o " + num (sibIn - sibOut, 1) + " dB");

    check (std::fabs (bodyIn - bodyOut) < 0.7,
           "1 kHz nietkniete (<0,7 dB)",
           "roznica " + num (bodyIn - bodyOut, 2) + " dB");

    // bez sybilantu de-esser ma nie robic nic
    const double preIn  = bandRmsDb (x, 1000.0, 4.0, (size_t) (0.5 * kSR), (size_t) (0.9 * kSR));
    const double preOut = bandRmsDb (y, 1000.0, 4.0, (size_t) (0.5 * kSR), (size_t) (0.9 * kSR));

    check (std::fabs (preIn - preOut) < 0.2,
           "przy braku sybilantu sygnal przechodzi 1:1",
           "roznica " + num (preIn - preOut, 3) + " dB");
}

static void testLimiter()
{
    std::printf ("\n  5. limiter — nic ponad sufit, cisza nietknieta\n");

    const size_t n = (size_t) (3.0 * kSR);
    std::vector<float> x (n);

    // cichy material z ostrymi szczytami grubo ponad zero
    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / kSR;
        double v = std::pow (10.0, -20.0 / 20.0) * std::sin (2.0 * M_PI * 220.0 * t);

        // impulsy co 500 ms, +6 dBFS
        if (std::fmod (t, 0.5) < 0.002)
            v += 2.0 * std::sin (2.0 * M_PI * 900.0 * t);

        x[i] = (float) v;
    }

    std::vector<float> y = x;
    float* io[1] = { y.data() };

    mix::Limiter lim;
    lim.params.ceilingDb = -1.0f;
    lim.prepare (kSR, 1);
    lim.process (io, 1, (int) n);

    mix::TruePeak tp;
    tp.prepare (kSR, 1);
    const float* ptr[1] = { y.data() };
    tp.process (ptr, 1, (int) n);

    check (tp.dbTP() <= -1.0 + 0.05,
           "szczyt miedzyprobkowy nie przekracza -1 dBTP",
           "wyszlo " + num (tp.dbTP()) + " dBTP");

    check (tp.dbTP() > -3.0,
           "limiter nie tlumi na zapas",
           "wyszlo " + num (tp.dbTP()) + " dBTP");

    // Fragment dobrze po impulsie ma wrocic do wlasnego poziomu. Mierzone
    // 300–450 ms po szczycie, przy zwolnieniu 120 ms — czyli po dwoch i pol
    // stalych czasowych. Gdyby limiter tu nadal tlumil, znaczyloby to, ze
    // jeden trzask sciaga glosnosc calej wypowiedzi, i to slychac jako
    // "oddychanie" material.
    const size_t q0 = (size_t) (1.30 * kSR), q1 = (size_t) (1.45 * kSR);
    const double qi = rmsDb (x, q0, q1);
    const double qo = rmsDb (y, q0 + (size_t) lim.latencySamples(), q1 + (size_t) lim.latencySamples());

    check (std::fabs (qi - qo) < 0.5,
           "material miedzy szczytami nietkniety",
           "roznica " + num (qi - qo, 2) + " dB");
}

static void testNormalizacja()
{
    std::printf ("\n  6. wyrownanie do -16 LUFS (norma podcastowa)\n");

    std::mt19937 rng { 5150 };
    std::normal_distribution<float> g { 0.0f, 1.0f };

    const size_t n = (size_t) (20.0 * kSR);
    std::vector<float> x (n);
    double phase = 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / kSR;
        const bool on = std::fmod (t, 4.2) < 3.0;
        double v = 0.0;

        if (on)
        {
            phase += 2.0 * M_PI * 118.0 / kSR;
            for (int h = 1; h <= 14; ++h) v += std::sin (phase * h) / (h * h);
            v = (v * 1.55 + 0.04 * g (rng)) * std::pow (10.0, -26.0 / 20.0);
        }

        x[i] = (float) v;
    }

    auto measure = [] (const std::vector<float>& v)
    {
        mix::LoudnessMeter m;
        m.prepare (kSR, 1);
        const float* p[1] = { v.data() };
        m.process (p, 1, (int) v.size());
        return m.integrated();
    };

    const double before = measure (x);
    const double gainDb = -16.0 - before;

    std::vector<float> y (n);
    const float gain = mix::dbToGain ((float) gainDb);
    for (size_t i = 0; i < n; ++i) y[i] = x[i] * gain;

    const double after = measure (y);

    check (std::fabs (after - (-16.0)) < 0.1,
           "po korekcie material ma -16 LUFS",
           "przed " + num (before) + ", po " + num (after));
}

//==============================================================================
int main()
{
    std::printf ("\nDynamika — nasze wlasne moduly, bez wtyczek komercyjnych\n");
    std::printf ("=======================================================\n");

    testKrzywaKompresji();
    testCzasAtaku();
    testBrama();
    testDeEsser();
    testLimiter();
    testNormalizacja();

    std::printf ("\n=======================================================\n");
    std::printf ("%s\n\n", failures == 0 ? "wszystko przeszlo"
                                         : (std::to_string (failures) + " nieprzeszlych").c_str());
    return failures == 0 ? 0 : 1;
}
