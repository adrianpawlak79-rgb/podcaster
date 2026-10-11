/*
    restore-test.cpp — moduly z drugiej tury: odszumianie widmowe, plozywy,
    mlaski, jazda glosnoscia, duckowanie, nasycenie, dither, przeprobkowanie.

    Sygnal testowy pochodzi z DWOCH zrodel. Czesc jest wygenerowana tutaj, ale
    mowa — z syntezatora espeak-ng, czyli z programu, ktorego nie pisalem. To
    nie jest drobiazg: kiedy i sygnal testowy, i kod pochodza z jednej glowy,
    ten sam blad myslowy siedzi po obu stronach i test go nie zobaczy. Mowa z
    espeaka ma prawdziwe formanty, prawdziwe zwarcia i prawdziwe sybilanty,
    ktorych moj stos harmonicznych nie ma.

        g++ -O2 -std=c++17 -I../Source restore-test.cpp -o restore-test
*/

#include "../Source/DSP/Mix/Restore.h"
#include "../Source/DSP/Mix/Level.h"
#include "../Source/DSP/Mix/Colour.h"
#include "../Source/Wav.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static constexpr double kSR = 48000.0;
static int failures = 0;

static void check (bool ok, const std::string& what, const std::string& detail = {})
{
    std::printf ("      %s  %-50s %s\n", ok ? "OK  " : "BLAD", what.c_str(), detail.c_str());
    if (! ok) ++failures;
}

static std::string num (double v, int d = 2)
{
    char b[64]; std::snprintf (b, sizeof (b), "%.*f", d, v); return b;
}

static double rmsDb (const std::vector<float>& x, size_t a, size_t b)
{
    double s = 0.0; size_t n = 0;
    for (size_t i = a; i < b && i < x.size(); ++i) { s += (double) x[i] * x[i]; ++n; }
    return n && s > 0.0 ? 10.0 * std::log10 (s / n) : -180.0;
}

static double bandRmsDb (const std::vector<float>& x, double hz, double q, size_t a, size_t b)
{
    mix::Biquad f;
    f.setCoeffs (mix::design::bandpass (hz, kSR, q));
    const size_t warm = a > 9600 ? a - 9600 : 0;
    double s = 0.0; size_t n = 0;

    for (size_t i = warm; i < b && i < x.size(); ++i)
    {
        const float y = f.process (x[i]);
        if (i >= a) { s += (double) y * y; ++n; }
    }

    return n && s > 0.0 ? 10.0 * std::log10 (s / n) : -180.0;
}

/** Poziom POJEDYNCZEJ czestotliwosci, w dBFS.

    Filtr pasmowy sie do tego nie nadaje i to nie jest drobiazg. Filtr drugiego
    rzedu o dobroci 30 przepuszcza ton lezacy oktawe nizej na poziomie -33 dB —
    dokladnie tyle, ile pierwsza wersja tego testu wzięła za "znieksztalcenie
    wnoszone przez nasycenie". Modul byl w porzadku, mierzylem zle.

    Goertzel z oknem Hanna liczy jeden prazek widma i przy dlugim oknie
    rozroznia sasiednie czestotliwosci z zapasem ponad 90 dB. */
static double toneDb (const std::vector<float>& x, double hz, size_t from, size_t to)
{
    const size_t len = std::min (to, x.size()) - from;
    if (len < 4096) return -180.0;

    const int block = 32768;
    const double w = 2.0 * M_PI * hz / kSR;
    const double c = 2.0 * std::cos (w);

    double total = 0.0;
    int count = 0;

    for (size_t start = from; start + (size_t) block <= std::min (to, x.size());
         start += (size_t) block)
    {
        double s1 = 0.0, s2 = 0.0;

        for (int i = 0; i < block; ++i)
        {
            const double win = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (block - 1));
            const double s0 = x[start + (size_t) i] * win + c * s1 - s2;
            s2 = s1; s1 = s0;
        }

        // okno Hanna zabiera polowe amplitudy — stad mnoznik
        total += (s1 * s1 + s2 * s2 - c * s1 * s2) * 4.0 / ((double) block * block);
        ++count;
    }

    return count && total > 1.0e-24 ? 10.0 * std::log10 (total / count) : -180.0;
}

/** Mowa z espeaka, jesli lezy na dysku. Bez niej czesc testow sie pomija. */
static bool loadSpeech (std::vector<float>& out)
{
    wavio::Audio a;
    if (! wavio::read ("/tmp/mowa/mowa-czysta.wav", a).empty()) return false;
    out = a.toMono();
    return out.size() > (size_t) (5 * kSR);
}

//==============================================================================
static void testFft()
{
    std::printf ("\n  1. przeksztalcenie Fouriera tam i z powrotem\n");

    mix::Fft f;
    f.prepare (2048);

    std::mt19937 rng { 3 };
    std::uniform_real_distribution<double> u { -1.0, 1.0 };

    std::vector<double> re (2048), im (2048), orig (2048);
    for (int i = 0; i < 2048; ++i) { orig[(size_t) i] = u (rng); re[(size_t) i] = orig[(size_t) i]; im[(size_t) i] = 0.0; }

    f.run (re, im, false);
    f.run (re, im, true);

    double err = 0.0;
    for (int i = 0; i < 2048; ++i)
        err = std::max (err, std::fabs (re[(size_t) i] - orig[(size_t) i]));

    const double errDb = 20.0 * std::log10 (std::max (1.0e-18, err));
    check (errDb < -130.0, "sygnal wraca bez zmian", num (errDb, 1) + " dB bledu");
}

static void testDenoise (const std::vector<float>& speech, bool haveSpeech)
{
    std::printf ("\n  2. odszumianie widmowe (mowa z espeaka + szum)\n");

    if (! haveSpeech) { std::printf ("      (pominiete — brak mowy)\n"); return; }

    std::mt19937 rng { 21 };
    std::normal_distribution<float> g { 0.0f, 1.0f };

    // Mowa z espeaka jest GESTA — pauzy miedzy zdaniami trwaja ulamek sekundy
    // i syntezator nie milknie na dluzej. Pierwsza wersja tego testu szukala w
    // niej "najcichszego fragmentu" i mierzyla tam odszumianie; wychodzilo 4,5
    // dB i wygladalo na blad modulu. Nie bylo: w tym fragmencie nadal byla
    // mowa, a odszumianie poprawnie jej nie ruszalo.
    //
    // Zeby zmierzyc, ile schodzi z SAMEGO szumu, trzeba miec kawalek, w ktorym
    // jest sam szum. Doklejamy go na koncu.
    const size_t speechLen = std::min (speech.size(), (size_t) (18 * kSR));
    const size_t silenceLen = (size_t) (4 * kSR);
    const size_t n = speechLen + silenceLen;

    std::vector<float> x (n);

    float pk = 0.0f;
    for (size_t i = 0; i < speechLen; ++i) pk = std::max (pk, std::fabs (speech[i]));
    const float sg = 0.1f / std::max (1.0e-6f, pk);

    for (size_t i = 0; i < n; ++i)
    {
        const float noise = g (rng) * 0.005f;          // ~ -46 dBFS
        x[i] = (i < speechLen ? speech[i] * sg : 0.0f) + noise;
    }

    mix::SpectralDenoise dn;
    dn.prepare (kSR);

    std::vector<float> y;
    dn.render (x, y);

    const size_t q0 = speechLen + (size_t) (1.5 * kSR);
    const size_t q1 = speechLen + (size_t) (3.5 * kSR);
    const double before = rmsDb (x, q0, q1);
    const double after  = rmsDb (y, q0, q1);

    check (before - after > 8.0, "szum w ciszy scieniony o >8 dB",
           "o " + num (before - after, 1) + " dB");

    // czy mowa przezyla: porownanie w pasmie mowy na najglosniejszym fragmencie
    size_t loudAt = 0; double loudest = -1.0e9;

    for (size_t s = 0; s + (size_t) (0.3 * kSR) < speechLen; s += (size_t) (0.1 * kSR))
    {
        const double lvl = rmsDb (x, s, s + (size_t) (0.3 * kSR));
        if (lvl > loudest) { loudest = lvl; loudAt = s; }
    }

    const double sIn  = bandRmsDb (x, 1000.0, 0.7, loudAt, loudAt + (size_t) (0.3 * kSR));
    const double sOut = bandRmsDb (y, 1000.0, 0.7, loudAt, loudAt + (size_t) (0.3 * kSR));

    check (std::fabs (sIn - sOut) < 2.0, "mowa przechodzi bez ubytku (<2 dB)",
           "roznica " + num (sIn - sOut, 2) + " dB");

    // szum muzyczny: jak bardzo poziom tla skacze z ramki na ramke
    auto wobble = [&] (const std::vector<float>& v)
    {
        std::vector<double> lv;
        for (size_t s = q0; s + 1024 < q1; s += 1024)
            lv.push_back (rmsDb (v, s, s + 1024));

        if (lv.size() < 3) return 0.0;
        double m = 0.0; for (double a : lv) m += a; m /= lv.size();
        double var = 0.0; for (double a : lv) var += (a - m) * (a - m);
        return std::sqrt (var / lv.size());
    };

    const double wIn = wobble (x), wOut = wobble (y);

    check (wOut < wIn + 3.0, "tlo nie zaczyna pobrzekiwac",
           "rozrzut " + num (wIn, 1) + " -> " + num (wOut, 1) + " dB");
}

static void testDePlosive (const std::vector<float>& speech, bool haveSpeech)
{
    std::printf ("\n  3. tlumik plozyw\n");

    if (! haveSpeech) { std::printf ("      (pominiete — brak mowy)\n"); return; }

    const size_t n = std::min (speech.size(), (size_t) (10 * kSR));
    std::vector<float> x (speech.begin(), speech.begin() + (long) n);

    float pk = 0.0f;
    for (float v : x) pk = std::max (pk, std::fabs (v));
    for (auto& v : x) v *= 0.15f / std::max (1.0e-6f, pk);

    // grzmot 70 Hz w 3. sekundzie
    const size_t at = (size_t) (3.0 * kSR);
    const size_t len = (size_t) (0.06 * kSR);

    for (size_t i = 0; i < len; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos (2.0f * (float) M_PI * i / len);
        x[at + i] += w * 0.35f * std::sin (2.0f * (float) M_PI * 70.0f * i / (float) kSR);
    }

    mix::DePlosive dp;
    dp.prepare (kSR);

    std::vector<float> y (n);
    for (size_t i = 0; i < n; ++i) y[i] = dp.process (x[i]);

    const double thumpIn  = bandRmsDb (x, 70.0, 2.0, at, at + len);
    const double thumpOut = bandRmsDb (y, 70.0, 2.0, at, at + len);

    check (thumpIn - thumpOut > 8.0, "grzmot stlumiony o >8 dB",
           "o " + num (thumpIn - thumpOut, 1) + " dB");

    const size_t far = (size_t) (6.0 * kSR);
    const double voiceIn  = bandRmsDb (x, 900.0, 0.7, far, far + (size_t) (0.5 * kSR));
    const double voiceOut = bandRmsDb (y, 900.0, 0.7, far, far + (size_t) (0.5 * kSR));

    check (std::fabs (voiceIn - voiceOut) < 0.5, "glos poza plozywa nietkniety",
           "roznica " + num (voiceIn - voiceOut, 3) + " dB");
}

static void testDeClick (const std::vector<float>& speech, bool haveSpeech)
{
    std::printf ("\n  4. usuwanie mlaskow\n");

    if (! haveSpeech) { std::printf ("      (pominiete — brak mowy)\n"); return; }

    const size_t n = std::min (speech.size(), (size_t) (10 * kSR));
    std::vector<float> clean (speech.begin(), speech.begin() + (long) n);

    float pk = 0.0f;
    for (float v : clean) pk = std::max (pk, std::fabs (v));
    for (auto& v : clean) v *= 0.2f / std::max (1.0e-6f, pk);

    // czysty material nie moze niczego wywolac
    mix::DeClick dc0;
    std::vector<float> untouched;
    dc0.render (clean, untouched);

    check (dc0.repaired <= 2, "na czystej mowie prawie nic nie znajduje",
           std::to_string (dc0.repaired) + " zdarzen");

    // dorzucamy dwanascie trzaskow
    std::vector<float> dirty = clean;
    std::mt19937 rng { 5 };
    std::uniform_int_distribution<size_t> where { (size_t) (1 * kSR), n - (size_t) (1 * kSR) };

    std::vector<size_t> spots;

    for (int k = 0; k < 12; ++k)
    {
        const size_t p = where (rng);
        spots.push_back (p);
        for (int i = 0; i < 6; ++i)
            dirty[p + (size_t) i] += (i % 2 ? -0.35f : 0.35f);
    }

    mix::DeClick dc;
    std::vector<float> fixed;
    dc.render (dirty, fixed);

    check (dc.repaired >= 8, "znajduje wstawione trzaski",
           std::to_string (dc.repaired) + " z 12");

    // Liczymy, ILE miejsc zostalo naprawionych, a nie najgorszy przypadek w
    // calym pliku. Najgorszy przypadek mowi tylko o jednym trzasku, ktorego
    // detektor nie zlapal — a to juz wiemy z licznika wyzej i jest to swiadomy
    // kompromis: prog dosc wysoki, zeby nie ruszac mowy, przepusci czasem
    // trzask cichszy od otoczenia.
    int better = 0;

    for (size_t p : spots)
    {
        double b = 0.0, a2 = 0.0;

        for (int i = 0; i < 12; ++i)
        {
            const size_t j = p + (size_t) i;
            if (j >= n) break;
            b  = std::max (b,  (double) std::fabs (dirty[j] - clean[j]));
            a2 = std::max (a2, (double) std::fabs (fixed[j] - clean[j]));
        }

        if (a2 < b * 0.5) ++better;
    }

    check (better >= 10, "naprawione miejsca sa blizej oryginalu",
           std::to_string (better) + " z 12 poprawionych");

    // ---- trudniej: cichsze mlaski, zanikajacy ksztalt, cala probka -----------
    // Prawdziwy mlask jest zwykle cichszy niz trzask wyzej (0,35 przy mowie
    // o szczycie 0,2) i nie jest rowna pila, tylko szybko gasnie. Pomiar przy
    // naprawie (pazdziernik): stara wersja 24 z 40, nowa 36 z 40.
    std::vector<float> all = speech;
    pk = 0.0f;
    for (float v : all) pk = std::max (pk, std::fabs (v));
    for (auto& v : all) v *= 0.2f / std::max (1.0e-6f, pk);

    mix::DeClick dcAll;
    std::vector<float> allOut;
    dcAll.render (all, allOut);
    check (dcAll.repaired <= 1, "cala probka mowy: nic albo prawie nic",
           std::to_string (dcAll.repaired) + " zdarzen w " + num ((double) all.size() / kSR, 1) + " s");

    std::vector<float> soft = all;
    std::mt19937 rng2 { 9 };
    std::uniform_int_distribution<size_t> where2 { (size_t) kSR, all.size() - (size_t) kSR };
    std::vector<size_t> spots2;
    for (int k = 0; k < 40; ++k)
    {
        const size_t p = where2 (rng2);
        spots2.push_back (p);
        for (int i = 0; i < 12; ++i)
            soft[p + (size_t) i] += 0.1f * std::exp (-i / 2.5f) * (i % 3 == 0 ? 1.0f : -0.6f);
    }
    mix::DeClick dcSoft;
    std::vector<float> softOut;
    dcSoft.render (soft, softOut);

    int softBetter = 0;
    for (size_t p : spots2)
    {
        double b = 0.0, a2 = 0.0;
        for (int i = 0; i < 14; ++i)
        {
            const size_t j = p + (size_t) i;
            b  = std::max (b,  (double) std::fabs (soft[j] - all[j]));
            a2 = std::max (a2, (double) std::fabs (softOut[j] - all[j]));
        }
        if (a2 < b * 0.5) ++softBetter;
    }
    check (softBetter >= 32, "cichsze, gasnace mlaski (0,1)", std::to_string (softBetter) + " z 40");
}

static void testGainRider()
{
    std::printf ("\n  5. jazda glosnoscia\n");

    // dwa zdania, drugie o 10 dB cichsze
    const size_t n = (size_t) (24.0 * kSR);
    std::vector<float> x (n, 0.0f);
    double phase = 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / kSR;
        const bool on = std::fmod (t, 4.0) < 3.0;
        if (! on) continue;

        phase += 2.0 * M_PI * 130.0 / kSR;
        double v = 0.0;
        for (int h = 1; h <= 12; ++h) v += std::sin (phase * h) / (h * h);

        const double lvl = t < 12.0 ? std::pow (10.0, -14.0 / 20.0)
                                    : std::pow (10.0, -24.0 / 20.0);
        x[i] = (float) (v * 1.55 * lvl);
    }

    mix::GainRider r;
    r.params.maxSlopeDbS = 4.0f;
    r.prepare (kSR);

    std::vector<float> y (n);
    std::vector<float> gains (n);

    for (size_t i = 0; i < n; ++i) { y[i] = r.process (x[i]); gains[i] = r.currentGainDb(); }

    const double a = rmsDb (y, (size_t) (9.0 * kSR), (size_t) (11.0 * kSR));
    const double b = rmsDb (y, (size_t) (21.0 * kSR), (size_t) (23.0 * kSR));

    check (std::fabs (a - b) < 2.5, "oba zdania wyrownane do siebie",
           "roznica " + num (a - b, 2) + " dB (na wejsciu 10 dB)");

    // predkosc nigdy nie przekracza zadanej
    double maxSlope = 0.0;
    const size_t step = (size_t) (kSR / 100);

    for (size_t i = step; i < n; i += step)
        maxSlope = std::max (maxSlope, std::fabs ((double) (gains[i] - gains[i - step])) * 100.0);

    check (maxSlope < 4.6, "suwak nie jedzie szybciej niz 4 dB/s",
           num (maxSlope, 2) + " dB/s");
}

static void testDucker()
{
    std::printf ("\n  6. duckowanie podkladu\n");

    const size_t n = (size_t) (12.0 * kSR);
    std::vector<float> music (n), voice (n, 0.0f);
    double mp = 0.0, vp = 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / kSR;
        mp += 2.0 * M_PI * 220.0 / kSR;
        music[i] = (float) (0.2 * std::sin (mp));

        if (t >= 4.0 && t < 8.0)
        {
            vp += 2.0 * M_PI * 140.0 / kSR;
            double v = 0.0;
            for (int h = 1; h <= 10; ++h) v += std::sin (vp * h) / (h * h);
            voice[i] = (float) (v * 1.55 * 0.15);
        }
    }

    mix::Ducker d;
    d.params.duckDb = -12.0f;
    d.prepare (kSR);

    std::vector<float> out (n);
    for (size_t i = 0; i < n; ++i) out[i] = d.process (music[i], voice[i]);

    const size_t lat = (size_t) d.latencySamples();

    const double quietMusic = rmsDb (out, (size_t) (1.0 * kSR), (size_t) (3.0 * kSR));
    const double duckedMusic = rmsDb (out, (size_t) (6.0 * kSR) + lat, (size_t) (7.5 * kSR) + lat);
    const double backAgain  = rmsDb (out, (size_t) (10.5 * kSR), (size_t) (11.5 * kSR));

    check (std::fabs ((quietMusic - duckedMusic) - 12.0) < 1.5,
           "podklad schodzi o zadane 12 dB",
           "o " + num (quietMusic - duckedMusic, 1) + " dB");

    check (std::fabs (quietMusic - backAgain) < 0.6, "po zdaniu wraca na swoje",
           "roznica " + num (quietMusic - backAgain, 2) + " dB");

    // Wyprzedzenie mierzymy w chwili, w ktorej SLUCHACZ slyszy pierwsze slowo.
    // Glos idzie do niego opozniony o tyle samo, co podklad — inaczej oba
    // rozjezdzaja sie o dziewiecdziesiat milisekund. Pierwsza wersja tego
    // testu mierzyla w chwili wejscia glosu na WEJSCIE modulu, czyli o cale
    // wyprzedzenie za wczesnie, i pokazywala 0,2 dB.
    const double atOnset = rmsDb (out, (size_t) (4.0 * kSR) + lat,
                                       (size_t) (4.04 * kSR) + lat);
    check (atOnset < quietMusic - 1.0, "zaczyna schodzic przed pierwszym slowem",
           num (quietMusic - atOnset, 1) + " dB juz zdjete");
}

static void testSaturator()
{
    std::printf ("\n  7. nasycenie\n");

    auto harmonics = [] (float driveDb, float amount, double hz)
    {
        mix::Saturator s;
        s.params.driveDb = driveDb;
        s.params.amount = amount;
        s.prepare (kSR);

        const size_t n = (size_t) (3.0 * kSR);
        std::vector<float> y (n);

        for (size_t i = 0; i < n; ++i)
            y[i] = s.process ((float) (0.3 * std::sin (2.0 * M_PI * hz * (double) i / kSR)));

        struct R { double f0, h2, h3, alias; } r;
        r.f0    = toneDb (y, hz,          n / 4, n);
        r.h2    = toneDb (y, hz * 2,      n / 4, n);
        r.h3    = toneDb (y, hz * 3,      n / 4, n);
        r.alias = toneDb (y, hz * 0.37,   n / 4, n);
        return r;
    };

    const auto clean = harmonics (0.0f, 0.0f, 1000.0);
    check (clean.h2 - clean.f0 < -80.0, "przy zerowym nasyceniu nic nie dokłada",
           "druga harmoniczna " + num (clean.h2 - clean.f0, 1) + " dB");

    const auto warm = harmonics (8.0f, 0.8f, 1000.0);
    check (warm.h2 - warm.f0 > -45.0, "przy nasyceniu pojawia sie druga harmoniczna",
           num (warm.h2 - warm.f0, 1) + " dB");

    // aliasing: ton 15 kHz, harmoniczne wypadaja poza pasmo i musza sie NIE odbic
    const auto high = harmonics (8.0f, 0.8f, 15000.0);
    check (high.alias - high.f0 < -55.0, "harmoniczne nie odbijaja sie w dol pasma",
           "odbicie " + num (high.alias - high.f0, 1) + " dB");
}

static void testDither()
{
    std::printf ("\n  8. dither przy skracaniu do 16 bitow\n");

    // Dwie poprzednie wersje tego testu mierzyly nie to, co trzeba: raz czy
    // ton "znika", raz poziom w pasmie wokol harmonicznej. Obie odpowiadaly
    // na pytania, ktore nie maja zwiazku z tym, po co jest dither.
    //
    // Wlasciwa wlasnosc jest jedna: blad zaokraglenia ma przestac byc ZWIAZANY
    // z sygnalem. Zwiazany blad ucho slyszy jako znieksztalcenie, niezwiazany —
    // jako rowny szum, ktory odrzuca. Mierzy sie to wprost, korelacja bledu z
    // sygnalem, i nie zalezy to od zadnego filtru ani okna.
    const size_t n = (size_t) (4.0 * kSR);
    std::vector<float> x (n), plain (n), dithered (n);

    for (size_t i = 0; i < n; ++i)
        x[i] = (float) (std::pow (10.0, -80.0 / 20.0)
                        * std::sin (2.0 * M_PI * 997.0 * (double) i / kSR));

    const float scale = 32767.0f;
    for (size_t i = 0; i < n; ++i) plain[i] = std::round (x[i] * scale) / scale;

    mix::Dither d;
    d.reset();
    for (size_t i = 0; i < n; ++i) dithered[i] = d.process (x[i], 16);

    auto correlation = [&] (const std::vector<float>& q)
    {
        double xe = 0.0, xx = 0.0, ee = 0.0;

        for (size_t i = 0; i < n; ++i)
        {
            const double e = (double) q[i] - x[i];
            xe += (double) x[i] * e;
            xx += (double) x[i] * x[i];
            ee += e * e;
        }

        return xx > 0.0 && ee > 0.0 ? std::fabs (xe) / std::sqrt (xx * ee) : 0.0;
    };

    const double cPlain = correlation (plain);
    const double cDith  = correlation (dithered);

    check (cPlain > 0.1, "bez dithera blad idzie za sygnalem",
           "korelacja " + num (cPlain, 3));

    check (cDith < cPlain * 0.25, "z ditherem blad przestaje byc zwiazany z sygnalem",
           "korelacja " + num (cDith, 3));

    check (toneDb (dithered, 997.0, 0, n) > -95.0, "sam ton przezywa",
           num (toneDb (dithered, 997.0, 0, n), 1) + " dBFS");
}

static void testResampler()
{
    std::printf ("\n  9. przeprobkowanie 48 -> 44,1 -> 48 kHz\n");

    const size_t n = (size_t) (3.0 * kSR);
    std::vector<float> x (n);

    for (size_t i = 0; i < n; ++i)
        x[i] = (float) (0.5 * std::sin (2.0 * M_PI * 1000.0 * (double) i / kSR));

    mix::Resampler down, up;
    down.prepare (48000, 44100);
    up.prepare (44100, 48000);

    std::vector<float> mid, back;
    down.render (x, mid);
    up.render (mid, back);

    const double expected = (double) n * 44100.0 / 48000.0;
    check (std::fabs ((double) mid.size() - expected) < 64.0,
           "dlugosc po zmianie sie zgadza",
           std::to_string (mid.size()) + " probek, oczekiwane " + num (expected, 0));

    const size_t a = (size_t) (1.0 * kSR), b = (size_t) (2.0 * kSR);
    const double inLvl  = bandRmsDb (x, 1000.0, 20.0, a, b);
    const double outLvl = bandRmsDb (back, 1000.0, 20.0, a, b);

    check (std::fabs (inLvl - outLvl) < 0.5, "poziom tonu nie ucierpial",
           "roznica " + num (inLvl - outLvl, 2) + " dB");

    // Czestotliwosc liczymy z przejsc przez zero, nie z filtru pasmowego.
    // Filtr o dobroci 40 ma zbocza szerokie na kilkadziesiat hercow, wiec
    // przy porownaniu 1000 z 918 Hz mierzy w duzej mierze to samo — pierwsza
    // wersja tego testu odrzucila poprawny wynik z tego wlasnie powodu.
    int crossings = 0;

    for (size_t i = a + 1; i < b; ++i)
        if ((back[i - 1] <= 0.0f) != (back[i] <= 0.0f))
            ++crossings;

    const double measured = crossings * 0.5 * kSR / (double) (b - a);

    check (std::fabs (measured - 1000.0) < 2.0, "czestotliwosc sie nie przesunela",
           num (measured, 1) + " Hz zamiast 1000");
}

//==============================================================================
int main()
{
    std::printf ("\nOdtwarzanie i barwa — moduly z drugiej tury\n");
    std::printf ("==========================================\n");

    std::vector<float> speech;
    const bool haveSpeech = loadSpeech (speech);

    std::printf ("\n  mowa testowa: %s\n",
                 haveSpeech ? "espeak-ng, polski, 48 kHz" : "BRAK — czesc testow pominieta");

    testFft();
    testDenoise (speech, haveSpeech);
    testDePlosive (speech, haveSpeech);
    testDeClick (speech, haveSpeech);
    testGainRider();
    testDucker();
    testSaturator();
    testDither();
    testResampler();

    std::printf ("\n==========================================\n");
    std::printf ("%s\n\n", failures == 0 ? "wszystko przeszlo"
                                         : (std::to_string (failures) + " nieprzeszlych").c_str());
    return failures == 0 ? 0 : 1;
}
