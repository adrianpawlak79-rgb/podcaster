/*  Tor miksu (Tweaks.h): reczne poprawki na wierzchu automatu.  */
#include "../Source/DSP/Mix/Tweaks.h"
#include <cstdio>
#include <random>

static int fails = 0;
static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str()); if (! ok) ++fails;
}

/** Ustawienia jak w "Studyjnym" (Decide::studio), bez nagrania. */
static mix::SpeechChain::Params studio()
{
    mix::SpeechChain::Params p;
    using Bell = mix::SpeechChain::Params::Bell;
    p.highpassHz = 75.0; p.steepHighpass = true;
    p.preEq = { Bell { 200.0, 1.2, -2.5f }, Bell { 380.0, 1.4, -3.0f }, Bell { 800.0, 2.0, -1.5f } };
    p.denoiseDb = -10.0f; p.useBreath = true; p.useGate = true; p.gateFloorDb = -18.0f;
    p.useRider = false; p.levelerRatio = 2.5f; p.peakRatio = 4.0f;
    p.notchEq = { Bell { 3059.0, 6.0, -3.0f }, Bell { 5262.0, 6.0, -2.0f } };
    p.useTame = false; p.useDeEsser = true; p.essMaxCutDb = -3.0f;
    p.lowDb = 0.0f; p.mudDb = 0.0f; p.presenceDb = 1.5f; p.airDb = 4.0f;
    return p;
}

/** Jak "Delikatny" (Decide::gentle): wiekszosc modulow wylaczona. */
static mix::SpeechChain::Params gentle()
{
    auto p = mix::SpeechChain::Params {};
    p.highpassHz = 70.0; p.preEq.clear(); p.notchEq.clear(); p.useRider = false;
    p.levelerRatio = 2.0f; p.peakRatio = 1.0f; p.denoiseDb = -6.0f;
    p.useGate = p.usePlosive = p.useDeClick = p.useBreath = p.useTame = p.useDeEsser = false;
    p.lowDb = p.mudDb = p.presenceDb = p.airDb = 0.0f;
    return p;
}

int main()
{
    using mix::Stage;
    std::printf ("\nTor miksu — wlacznik i suwak mniej/wiecej na kazdym module\n");
    std::printf ("===========================================================\n\n");

    {
        auto p = studio();
        mix::applyTweaks (p, {});
        const auto q = studio();
        check (p.highpassHz == q.highpassHz && p.denoiseDb == q.denoiseDb && p.levelerRatio == q.levelerRatio
               && p.preEq[1].db == q.preEq[1].db && p.airDb == q.airDb, "bez poprawek ustawienia automatu bez zmian");
        check (mix::Tweaks {}.isDefault(), "puste poprawki to brak poprawek");
    }

    // wylaczenie kazdego modulu
    {
        bool all = true;
        for (int i = 0; i < mix::stageCount; ++i)
        {
            const auto s = (Stage) i;
            auto p = studio();
            if (! mix::stageUsed (p, s)) { all = false; continue; }
            mix::Tweaks t; t.off[(size_t) i] = true;
            mix::applyTweaks (p, t);
            const bool gone = s == Stage::Filtr ? p.highpassHz <= 30.0 : ! mix::stageUsed (p, s);
            if (! gone) { all = false; std::printf ("     nie wylaczyl: %s\n", mix::stageName (s)); }
        }
        check (all, "w studyjnym kazdy modul jest uzyty i daje sie wylaczyc");
    }

    // wiecej / mniej
    {
        mix::Tweaks t;
        t.amount[(size_t) Stage::Odszumianie] = 1.0f;
        t.amount[(size_t) Stage::Kompresja] = 1.0f;
        t.amount[(size_t) Stage::DeEsser] = -1.0f;
        t.amount[(size_t) Stage::Filtr] = 1.0f;
        auto p = studio();
        mix::applyTweaks (p, t);
        check (std::abs (p.denoiseDb + 20.0f) < 1e-4f, "odszumianie +1: dwa razy wiecej (10 -> 20 dB)", mix::stageValue (p, Stage::Odszumianie));
        check (std::abs (p.levelerRatio - 4.0f) < 1e-4f && std::abs (p.peakRatio - 7.0f) < 1e-4f,
               "kompresja +1: dwa razy wiecej ponad 1:1", mix::stageValue (p, Stage::Kompresja));
        check (std::abs (p.essMaxCutDb + 1.5f) < 1e-4f, "de-esser -1: polowa (3 -> 1,5 dB)", mix::stageValue (p, Stage::DeEsser));
        check (std::abs (p.highpassHz - 112.5) < 1e-6, "filtr +1: cięcie 1,5 raza wyzej", mix::stageValue (p, Stage::Filtr));

        t = {};
        t.amount[(size_t) Stage::Odszumianie] = 1.0f;
        auto q = studio(); q.denoiseDb = -18.0f;
        mix::applyTweaks (q, t);
        check (q.denoiseDb >= -24.0f, "odszumianie ma gorna granice 24 dB", mix::stageValue (q, Stage::Odszumianie));
    }

    // moduly nieuzyte w delikatnym zostaja nieuzyte
    {
        auto p = gentle();
        check (! mix::stageUsed (p, Stage::Korektor) && ! mix::stageUsed (p, Stage::Barwa)
               && ! mix::stageUsed (p, Stage::DeEsser) && mix::stageUsed (p, Stage::Kompresja),
               "delikatny: korektor, barwa, de-esser nieuzyte; kompresja tak");
        mix::Tweaks t;
        t.amount[(size_t) Stage::Barwa] = 1.0f; t.amount[(size_t) Stage::DeEsser] = 1.0f;
        mix::applyTweaks (p, t);
        check (! mix::stageUsed (p, Stage::Barwa) && ! p.useDeEsser, "suwak nieuzytego modulu niczego nie wlacza");
        check (mix::describeTweaks (gentle(), t).empty(), "poprawki nieuzytych modulow nie trafiaja do opisu");
    }

    // opis i liczby po polsku
    {
        mix::Tweaks t; t.off[(size_t) Stage::DeEsser] = true; t.amount[(size_t) Stage::Odszumianie] = -0.5f;
        const auto d = mix::describeTweaks (studio(), t);
        check (d == "Twoje zmiany w torze miksu: Odszumianie — mniej, De-esser — wyłączone.", "opis zmian", d);
        const auto p = studio();
        check (mix::stageValue (p, Stage::Korektor) == "3 pasma, do 3 dB", "korektor: liczba pasm", mix::stageValue (p, Stage::Korektor));
        check (mix::stageValue (p, Stage::Kompresja) == "2,5:1 i 4:1", "kompresja: przecinek", mix::stageValue (p, Stage::Kompresja));
        check (mix::stageValue (p, Stage::Rezonanse) == "2 wycięcia", "rezonanse: odmiana", mix::stageValue (p, Stage::Rezonanse));
    }

    // przez caly lancuch: kompresja wylaczona daje wiekszy rozrzut glosnosci
    {
        const double sr = 48000.0;
        std::mt19937 rng (5); std::normal_distribution<float> g (0.0f, 1.0f);
        std::vector<float> in ((size_t) (sr * 6));
        for (size_t i = 0; i < in.size(); ++i)
        {
            const double t = i / sr;
            const float env = (std::fmod (t, 1.0) < 0.5 ? 0.25f : 0.03f) * (0.6f + 0.4f * (float) std::sin (2 * M_PI * 3 * t));
            in[i] = env * ((float) std::sin (2 * M_PI * 180 * t) + 0.3f * (float) std::sin (2 * M_PI * 2400 * t) + 0.02f * g (rng));
        }
        auto spread = [&] (const mix::Tweaks& t)
        {
            mix::SpeechChain c; c.params = studio(); mix::applyTweaks (c.params, t);
            c.params.useBreath = false; c.params.useGate = false;
            c.params.levelerThreshDb = -36.0f;   // glosne fragmenty wyraznie ponad progiem
            c.setSampleRate (sr); c.prepare (sr);
            std::vector<float> out; c.render (in, out);
            double loud = 0, quiet = 0; size_t nl = 0, nq = 0;
            for (size_t i = (size_t) sr; i < out.size(); ++i)
                (std::fmod (i / sr, 1.0) < 0.5 ? (loud += out[i] * out[i], ++nl) : (quiet += out[i] * out[i], ++nq));
            return 10.0 * std::log10 ((loud / nl) / (quiet / nq + 1e-20));
        };
        mix::Tweaks off; off.off[(size_t) Stage::Kompresja] = true;
        mix::Tweaks more; more.amount[(size_t) Stage::Kompresja] = 1.0f;
        const double a = spread ({}), b = spread (off), c = spread (more);
        char s[120]; std::snprintf (s, sizeof (s), "rozrzut: auto %.1f dB, bez %.1f dB, wiecej %.1f dB", a, b, c);
        check (b > a + 2.0 && c < a - 1.0, "kompresja: wylaczona szerzej, wiecej wezej", s);
    }

    std::printf ("\n%s\n", fails ? "SA BLEDY" : "wszystko OK");
    return fails ? 1 : 0;
}
