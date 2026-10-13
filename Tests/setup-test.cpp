/*
    setup-test.cpp — czy kreator wykrywa to, co ma wykrywać, i NIE wykrywa tego,
    czego nie ma.

    Drugi warunek jest trudniejszy i wazniejszy. Kreator, ktory przy dobrym
    sprzecie znajduje problem, uczy uzytkownika klikac "dalej" bez czytania — a
    wtedy przestaje istniec. Dlatego kazda usterka jest tu sprawdzana w dwie
    strony: raz na materiale, w ktorym jest, raz na materiale, w ktorym jej nie
    ma.

    Dwie rzeczy, ktorych nauczyla mnie pierwsza wersja tego pliku:

    KOLEJNOSC ARGUMENTOW. W C++ argumenty funkcji moga byc obliczane w dowolnej
    kolejnosci, wiec check(run(...), "...", rep.cos) potrafi przeczytac rep
    ZANIM run() go wypelni. Wyniki wychodzily przesuniete o jeden test i przez
    chwile wygladalo to na blad kreatora.

    WIDMO SYGNALU TESTOWEGO. Moj stos harmonicznych 1/h^2 opada o 12 dB na
    oktawe, wiec ma ogromna przewage dolu nad srodkiem — niezaleznie od
    odleglosci od mikrofonu. Pomiar efektu zblizeniowego na takim sygnale
    ZAWSZE melduje "za blisko". Tam, gdzie liczy sie ksztalt widma, material
    musi pochodzic z mowy, nie z mojego generatora.

        g++ -O2 -std=c++17 -I../Source setup-test.cpp -o setup-test
*/

#include "../Source/DSP/Setup.h"
#include "../Source/DSP/Mix/Biquad.h"
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
    std::printf ("      %s  %-44s %s\n", ok ? "OK  " : "BLAD", what.c_str(), detail.c_str());
    if (! ok) ++failures;
}

static std::string i2s (double v) { return std::to_string ((int) std::lround (v)); }

//==============================================================================
struct Room
{
    double noiseDb    = -62.0;
    double humDb      = -140.0;   ///< poziom skladowej 50 Hz
    double speechPeak = -12.0;
    bool   speech     = true;
    bool   suppression = false;   ///< tlo zapada sie po chwili
    bool   agc         = false;   ///< tlo rosnie
};

/** Material syntetyczny — do krokow, w ktorych liczy sie POZIOM, nie barwa. */
static std::vector<float> make (const Room& r, double seconds, unsigned seed = 7)
{
    const size_t n = (size_t) (seconds * kSR);
    std::vector<float> x (n, 0.0f);

    std::mt19937 rng { seed };
    std::normal_distribution<float> g { 0.0f, 1.0f };

    const double noiseAmp = std::pow (10.0, r.noiseDb / 20.0);
    const double humAmp   = std::pow (10.0, r.humDb / 20.0);
    const double peakAmp  = std::pow (10.0, r.speechPeak / 20.0);

    double phase = 0.0;

    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / kSR;
        double scale = 1.0;

        if (r.suppression) scale = t < 0.4 ? 1.0 : 0.02;
        if (r.agc)         scale = std::pow (10.0, (t / seconds) * 12.0 / 20.0);

        double v = noiseAmp * g (rng) * 0.4 * scale;

        v += humAmp * (std::sin (2.0 * M_PI * 50.0 * t)
                     + 0.4 * std::sin (2.0 * M_PI * 100.0 * t));

        if (r.speech && std::fmod (t, 3.0) < 2.2)
        {
            phase += 2.0 * M_PI * 118.0 / kSR;
            double s = 0.0;

            for (int h = 1; h <= 16; ++h)
                s += std::sin (phase * h) / (h * h);

            const double in = std::fmod (t, 3.0);
            const double env = std::min (1.0, std::min (in / 0.08, (2.2 - in) / 0.08));
            v += s * 1.55 * peakAmp * std::max (0.0, env);
        }

        x[i] = (float) std::max (-1.0, std::min (1.0, v));
    }

    return x;
}

/** Mowa z espeaka — do krokow, w ktorych liczy sie ksztalt widma. */
static bool realSpeech (std::vector<float>& out)
{
    wavio::Audio a;
    if (! wavio::read ("/tmp/mowa/mowa-czysta.wav", a).empty()) return false;
    out = a.toMono();
    return out.size() > (size_t) (8 * kSR);
}

/** Kawalek mowy o zadanym szczycie, z opcjonalnym podbiciem dolu. */
static std::vector<float> speechAt (const std::vector<float>& src, double seconds,
                                    double peakDb, double lowBoostDb = 0.0)
{
    const size_t n = std::min (src.size(), (size_t) (seconds * kSR));
    std::vector<float> x (src.begin(), src.begin() + (long) n);

    if (lowBoostDb != 0.0)
    {
        mix::Biquad shelf;
        shelf.setCoeffs (mix::design::lowShelf (160.0, kSR, lowBoostDb));
        for (auto& v : x) v = shelf.process (v);
    }

    float pk = 0.0f;
    for (float v : x) pk = std::max (pk, std::fabs (v));

    const float g = (float) std::pow (10.0, peakDb / 20.0) / std::max (1.0e-6f, pk);

    std::mt19937 rng { 31 };
    std::normal_distribution<float> noise { 0.0f, 1.0f };
    const float na = (float) std::pow (10.0, -62.0 / 20.0) * 0.4f;

    for (auto& v : x) v = v * g + noise (rng) * na;

    return x;
}

//==============================================================================
/** Przewija kreator do zadanego kroku i przepuszcza przez niego material. */
static tw::SetupFinding run (tw::SetupStep want, const std::vector<float>& x,
                             tw::SetupReport* out = nullptr,
                             const std::vector<float>* ref = nullptr,
                             double roomNoiseDb = -64.0)
{
    tw::SetupWizard w;
    w.begin (kSR);

    while (w.step() != want && w.step() != tw::SetupStep::Done)
    {
        // Krok "cisza" trzeba przewinac CISZA, a nie mowa — inaczej kreator
        // slusznie uzna, ze w pokoju jest halas, i caly przebieg nie zaliczy.
        Room fill;
        fill.speech = (w.step() != tw::SetupStep::Room);
        fill.noiseDb = roomNoiseDb;

        const auto filler = make (fill, tw::SetupWizard::seconds (w.step()) + 0.2, 3);
        w.feed (filler.data(), (int) filler.size());
        w.evaluate();
        w.advance();
    }

    w.feed (x.data(), (int) x.size());
    if (ref) w.feedReference (ref->data(), (int) ref->size());

    const auto f = w.evaluate();
    if (out) *out = w.result();
    return f;
}

//==============================================================================
int main()
{
    std::printf ("\nKreator sprawdzenia sprzetu\n");
    std::printf ("===========================\n");

    std::vector<float> speech;
    const bool haveSpeech = realSpeech (speech);

    std::printf ("\n  mowa testowa: %s\n",
                 haveSpeech ? "espeak-ng, polski" : "BRAK — czesc krokow pominieta");

    // ---- 1. urzadzenie ------------------------------------------------------
    {
        std::printf ("\n  1. wybor urzadzenia\n");

        Room dead; dead.speech = false; dead.noiseDb = -120.0;
        check (! run (tw::SetupStep::Device, make (dead, 3.2)).passed,
               "martwe wejscie nie przechodzi");

        check (run (tw::SetupStep::Device, make (Room{}, 3.2)).passed,
               "dzialajacy mikrofon przechodzi");

        Room faint; faint.speechPeak = -48.0;
        check (! run (tw::SetupStep::Device, make (faint, 3.2)).passed,
               "ledwo slyszalny nie przechodzi");
    }

    // ---- 2. pokoj -----------------------------------------------------------
    {
        std::printf ("\n  2. cisza w pokoju\n");

        tw::SetupReport rep;

        Room quiet; quiet.speech = false; quiet.noiseDb = -66.0;
        const auto q = run (tw::SetupStep::Room, make (quiet, 6.2), &rep);
        check (q.passed, "cichy pokoj przechodzi", "tlo " + i2s (rep.noiseFloorDb) + " dBFS");

        Room loud; loud.speech = false; loud.noiseDb = -34.0;
        const auto l = run (tw::SetupStep::Room, make (loud, 6.2), &rep);
        check (! l.passed, "glosny pokoj nie przechodzi", "tlo " + i2s (rep.noiseFloorDb) + " dBFS");

        Room hum; hum.speech = false; hum.noiseDb = -70.0; hum.humDb = -55.0;
        const auto h = run (tw::SetupStep::Room, make (hum, 6.2), &rep);
        check (rep.humHz == 50.0, "przydzwiek 50 Hz wykryty",
               i2s (rep.humExcessDb) + " dB ponad sasiedztwo");
        check (! h.passed, "slyszalne buczenie zatrzymuje",
               "tlo " + i2s (rep.noiseFloorDb) + " dBFS");

        // Adrian 13.10: tlo -76 dBFS, wentylatory przy otwartej obudowie.
        // Buczenie jest, ale niesłyszalne — nie zatrzymujemy, tylko mowimy.
        Room faint; faint.speech = false; faint.noiseDb = -84.0; faint.humDb = -72.0;
        const auto fh = run (tw::SetupStep::Room, make (faint, 6.2), &rep);
        check (rep.humHz == 50.0 && fh.passed, "ciche buczenie w cichym pokoju: przechodzi, ale mowi",
               "tlo " + i2s (rep.noiseFloorDb) + " dBFS, " + i2s (rep.humExcessDb) + " dB");

        run (tw::SetupStep::Room, make (quiet, 6.2), &rep);
        check (rep.humHz == 0.0, "bez przydzwieku nic nie zglasza");

        Room sup; sup.speech = false; sup.noiseDb = -58.0; sup.suppression = true;
        run (tw::SetupStep::Room, make (sup, 6.2), &rep);
        check (rep.suppressionSuspected, "systemowa redukcja szumu wykryta");

        run (tw::SetupStep::Room, make (quiet, 6.2), &rep);
        check (! rep.suppressionSuspected, "przy zwyklym tle nie krzyczy o redukcji");

        Room agc; agc.speech = false; agc.noiseDb = -66.0; agc.agc = true;
        run (tw::SetupStep::Room, make (agc, 6.2), &rep);
        check (rep.agcSuspected, "automatyczne wzmocnienie wykryte");

        run (tw::SetupStep::Room, make (quiet, 6.2), &rep);
        check (! rep.agcSuspected, "przy stalym tle nie krzyczy o wzmocnieniu");
    }

    // ---- 3. poziom i odleglosc ----------------------------------------------
    {
        std::printf ("\n  3. wzmocnienie i odleglosc (na mowie z espeaka)\n");

        if (! haveSpeech)
        {
            std::printf ("      (pominiete)\n");
        }
        else
        {
            tw::SetupReport rep;

            const auto good = run (tw::SetupStep::Level, speechAt (speech, 6.2, -12.0), &rep);
            check (good.passed, "poziom w sam raz przechodzi",
                   "szczyt " + i2s (rep.speechPeakDb) + " dBFS, dol "
                   + i2s (rep.proximityDb) + " dB");

            run (tw::SetupStep::Level, speechAt (speech, 6.2, -30.0), &rep);
            check (rep.gainAdviceDb > 12.0, "za cicho: radzi podkrecic",
                   "o " + i2s (rep.gainAdviceDb) + " dB");

            run (tw::SetupStep::Level, speechAt (speech, 6.2, -2.0), &rep);
            check (rep.gainAdviceDb < -6.0, "za glosno: radzi przykrecic",
                   "o " + i2s (-rep.gainAdviceDb) + " dB");

            run (tw::SetupStep::Level, speechAt (speech, 6.2, -12.0), &rep);

            // Sprawdzamy RELACJE, nie wartosc bezwzgledna: prog absolutny
            // wymaga prawdziwego mikrofonu i dwoch odleglosci, czego espeak
            // nie da. Tu ma wyjsc tylko tyle, ze blizej znaczy wiecej dolu.
            const double normalLow = rep.proximityDb;

            const auto close = run (tw::SetupStep::Level, speechAt (speech, 6.2, -12.0, 12.0), &rep);
            check (rep.proximityDb > normalLow + 3.0,
                   "blizej mikrofonu = wiecej dolu",
                   i2s (normalLow) + " -> " + i2s (rep.proximityDb) + " dB");
            check (close.passed, "blisko nie zatrzymuje (prog bez kalibracji, miks wyrowna)",
                   close.headline);

            // Adrian 13.10, SM7B na SSL2: szczyty -22 dBFS, tlo -76 dBFS.
            // Gain byl na koncu skali, a kreator kazal podkrecic.
            const auto lowQuiet = run (tw::SetupStep::Level, speechAt (speech, 6.2, -22.0), &rep, nullptr, -82.0);
            check (lowQuiet.passed, "szczyty -22 przy bardzo cichym tle: wystarczy",
                   "odstep " + i2s (rep.speechPeakDb - rep.noiseFloorDb) + " dB");

            const auto lowNoisy = run (tw::SetupStep::Level, speechAt (speech, 6.2, -22.0), &rep, nullptr, -50.0);
            check (! lowNoisy.passed, "szczyty -22 przy zwyklym tle: za cicho",
                   "odstep " + i2s (rep.speechPeakDb - rep.noiseFloorDb) + " dB");

            const auto tooLow = run (tw::SetupStep::Level, speechAt (speech, 6.2, -34.0), &rep, nullptr, -90.0);
            check (! tooLow.passed, "szczyty -34 zawsze za cicho, nawet w ciszy", tooLow.headline);
        }
    }

    // ---- 4. plozywy ---------------------------------------------------------
    {
        std::printf ("\n  4. plozywy\n");

        if (! haveSpeech)
        {
            std::printf ("      (pominiete)\n");
        }
        else
        {
            tw::SetupReport rep;
            const auto clean = speechAt (speech, 4.2, -14.0);

            const auto ok = run (tw::SetupStep::Plosive, clean, &rep);
            check (ok.passed, "mowa bez uderzen przechodzi",
                   i2s (rep.plosiveExcessDb) + " dB ponad tlo");

            auto thumped = clean;
            const size_t at = (size_t) (1.5 * kSR), len = (size_t) (0.05 * kSR);

            for (size_t i = 0; i < len; ++i)
            {
                const float w = 0.5f - 0.5f * std::cos (2.0f * (float) M_PI * i / len);
                thumped[at + i] += w * 0.45f
                                 * std::sin (2.0f * (float) M_PI * 65.0f * i / (float) kSR);
            }

            const auto bad = run (tw::SetupStep::Plosive, thumped, &rep);
            check (! bad.passed, "uderzenie w membrane wykryte",
                   i2s (rep.plosiveExcessDb) + " dB ponad tlo");
        }
    }

    // ---- 5. przesluch ze sluchawek -----------------------------------------
    {
        std::printf ("\n  5. przesluch ze sluchawek\n");

        tw::SetupReport rep;
        const size_t n = (size_t) (3.2 * kSR);

        std::vector<float> ref (n);
        for (size_t i = 0; i < n; ++i)
            ref[i] = (float) (0.3 * std::sin (2.0 * M_PI * 440.0 * (double) i / kSR)
                            + 0.1 * std::sin (2.0 * M_PI * 660.0 * (double) i / kSR));

        std::mt19937 rng { 21 };
        std::normal_distribution<float> g { 0.0f, 1.0f };

        std::vector<float> tight (n);
        for (size_t i = 0; i < n; ++i) tight[i] = g (rng) * 0.001f;

        run (tw::SetupStep::Bleed, tight, &rep, &ref);
        check (rep.bleedPercent < 4.0, "zamkniete sluchawki przechodza",
               i2s (rep.bleedPercent) + "%");

        std::vector<float> leaky (n);
        const size_t d = (size_t) (0.001 * kSR);

        for (size_t i = 0; i < n; ++i)
            leaky[i] = g (rng) * 0.001f + (i > d ? ref[i - d] * 0.1f : 0.0f);

        const auto lf = run (tw::SetupStep::Bleed, leaky, &rep, &ref);
        check (rep.bleedPercent > 4.0 && ! lf.passed, "uciekajace sluchawki wykryte",
               i2s (rep.bleedPercent) + "%, " + i2s (rep.bleedDb) + " dBFS");

        // Adrian 13.10: 4,3 % w bardzo cichym pokoju. Korelacja wysoka, bo
        // nie ma szumu, ktory by ja rozmyl — ale przesluchu nie da sie uslyszec.
        std::vector<float> faint (n);
        for (size_t i = 0; i < n; ++i)
            faint[i] = g (rng) * 0.00016f + (i > d ? ref[i - d] * 0.0001f : 0.0f);

        const auto ff = run (tw::SetupStep::Bleed, faint, &rep, &ref);
        check (rep.bleedPercent > 4.0 && ff.passed, "slad przesluchu ponizej slyszalnosci przechodzi",
               i2s (rep.bleedPercent) + "%, " + i2s (rep.bleedDb) + " dBFS");
    }

    // ---- 6. caly przebieg ---------------------------------------------------
    {
        std::printf ("\n  6. caly kreator od poczatku do konca\n");

        tw::SetupWizard w;
        w.begin (kSR);

        int steps = 0;

        while (w.step() != tw::SetupStep::Done && steps < 10)
        {
            const double len = tw::SetupWizard::seconds (w.step()) + 0.2;

            Room r;
            r.speech = (w.step() != tw::SetupStep::Room && w.step() != tw::SetupStep::Bleed);
            r.noiseDb = -64.0;

            const auto x = (haveSpeech && r.speech) ? speechAt (speech, len, -12.0)
                                                    : make (r, len, 5);
            w.feed (x.data(), (int) x.size());

            if (w.step() == tw::SetupStep::Bleed)
            {
                const std::vector<float> ref (x.size(), 0.0f);
                w.feedReference (ref.data(), (int) ref.size());
            }

            const auto f = w.evaluate();

            if (! f.passed)
                std::printf ("      ---   krok %d zglosil: %s\n", steps + 1, f.headline.c_str());

            w.advance();
            ++steps;
        }

        check (steps == 6, "kreator ma szesc krokow", i2s (steps) + " przeszlo");
        check (w.result().ready, "dobry sprzet dostaje zielone swiatlo");
    }

    std::printf ("\n===========================\n");
    std::printf ("%s\n\n", failures == 0 ? "wszystko przeszlo"
                                         : (std::to_string (failures) + " nieprzeszlych").c_str());
    return failures == 0 ? 0 : 1;
}
