/*
    ai-tlo-test.cpp — usuwanie tla (AI, RNNoise) jako pierwszy krok miksu.

    Material jak w tlo-test: glos z dlugimi pauzami (argv[1], 48 kHz, espeak
    pl+klatt — patrz minGainDb),
    szum pokoju 50 dB pod mowa i "telewizor" (argv[2]) 25 dB pod glosem.

    Sprawdza:
      - automat wlacza AI przy telewizorze (Studyjny, Mocny), nie wlacza przy
        samym cichym pokoju i w Delikatnym,
      - tlo w pauzach po miksie z AI jest nizej niz po dotychczasowym torze
        (to samo nagranie, AI wylaczone na torze) o co najmniej minGainDb,
      - koncowki slow i glosnosc czystej mowy zostaja,
      - dlugosc bez zmian i zero przesuniecia (48 kHz i 44,1 kHz),
      - czas: probka 15 s.

    Kompilacja z siecia: g++ ... ai-tlo-test.cpp $(bash rnnoise-lib.sh /tmp/zs-rnnoise)
    Wywolanie: ai-tlo-test glos-48k.wav telewizor-48k.wav
*/

#include "../Source/DSP/TakeReview.h"
#include "../Source/Wav.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static int fails = 0;
static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-62s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}
static std::string f1 (double v) { char b[32]; std::snprintf (b, sizeof b, "%.1f", v); return b; }

static std::vector<double> blocksDb (const std::vector<float>& x, size_t n)
{
    std::vector<double> e;
    for (size_t i = 0; i + n <= x.size(); i += n)
    {
        double s = 0; for (size_t k = 0; k < n; ++k) s += (double) x[i + k] * x[i + k];
        e.push_back (10.0 * std::log10 (s / (double) n + 1e-20));
    }
    return e;
}

/** Przesuniecie y wzgledem x (probki), szukane w +-maxLag. */
static int lagOf (const std::vector<float>& x, const std::vector<float>& y, int maxLag)
{
    int best = 0; double bv = -1e300;
    for (int l = -maxLag; l <= maxLag; ++l)
    {
        double s = 0;
        for (size_t i = (size_t) maxLag; i + (size_t) maxLag < x.size(); i += 2) s += (double) x[i] * y[(size_t) ((long) i + l)];
        if (s > bv) { bv = s; best = l; }
    }
    return best;
}

/** Ile tla (pauzy) musi zejsc wzgledem dotychczasowego toru (Studyjny,
    Mocny). Zmierzone (28.11): glos espeak pl+klatt (ci.sh) +2,9 dB; prawdziwe
    czytanie z badan (wl/konopnicka-pszczoly) +6,3 dB, (wl/kwiaty-zla-olbrzymka)
    ponad +10 dB. Telewizor to tez mowa, wiec siec zostawia jego czesc. Prog
    z zapasem dla materialu z CI. Zwykly glos espeak (bez klatt) siec bierze
    za szum (-7 dB mowy), dlatego test potrzebuje pl+klatt. */
static constexpr double minGainDb = 2.0;
/** Koncowki slow: siec sama na glosie pl+klatt -2,9 dB, na prawdziwym -0,4 dB;
    domieszka oryginalu -14 dB oddaje z tego tylko 0,3 dB, wiec jej nie ma. */
static constexpr double maxTailDb = -4.0;

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("uzycie: ai-tlo-test glos.wav telewizor.wav\n"); return 1; }
    if (! mix::ai::available()) { std::printf ("  BLAD  test zbudowany bez RNNoise (-DZS_RNNOISE=1 librnnoise.a)\n"); return 1; }
    wavio::Audio va, ta;
    if (! wavio::read (argv[1], va).empty() || ! wavio::read (argv[2], ta).empty()) { std::printf ("  BLAD  brak plikow\n"); return 1; }
    const double sr = va.sampleRate;
    if (ta.sampleRate != va.sampleRate || (int) sr != 48000) { std::printf ("  BLAD  pliki maja miec 48 kHz\n"); return 1; }
    const auto v0 = va.toMono(), tv0 = ta.toMono();
    const size_t blk = (size_t) (0.05 * sr);

    // ---- glos z dlugimi pauzami (jak tlo-test) ---------------------------------
    std::vector<float> voice;
    {
        const auto e = blocksDb (v0, blk);
        double mx = -200; for (double x : e) mx = std::max (mx, x);
        size_t quietRun = 0;
        for (size_t b = 0; b < e.size(); ++b)
        {
            voice.insert (voice.end(), v0.begin() + (long) (b * blk), v0.begin() + (long) ((b + 1) * blk));
            if (e[b] < mx - 35.0) ++quietRun; else quietRun = 0;
            if (quietRun == 3) voice.insert (voice.end(), (size_t) (1.2 * sr), 0.0f);
        }
        voice.insert (voice.end(), (size_t) (1.0 * sr), 0.0f);
    }

    const auto E = blocksDb (voice, blk);
    double mx = -200; for (double x : E) mx = std::max (mx, x);
    std::vector<char> speech (E.size()), pause (E.size()), tail (E.size());
    for (size_t b = 0; b < E.size(); ++b) speech[b] = E[b] > mx - 20.0;
    for (size_t b = 0; b < E.size(); ++b)
    {
        bool far = true;
        for (long k = (long) b - 6; k <= (long) b + 6; ++k)
            if (k >= 0 && k < (long) E.size() && E[(size_t) k] > mx - 45.0) far = false;
        pause[b] = far;
        tail[b] = E[b] <= mx - 20.0 && E[b] > mx - 32.0;
    }
    const auto meanDb = [&] (const std::vector<float>& y, const std::vector<char>& m)
    {
        const auto e = blocksDb (y, blk);
        double s = 0; size_t n = 0;
        for (size_t b = 0; b < m.size() && b < e.size(); ++b) if (m[b]) { s += std::pow (10.0, e[b] / 10.0); ++n; }
        return 10.0 * std::log10 (s / (double) std::max<size_t> (1, n) + 1e-30);
    };
    const auto ratio    = [&] (const std::vector<float>& y) { return meanDb (y, speech) - meanDb (y, pause); };
    const auto tailDrop = [&] (const std::vector<float>& y) { return meanDb (y, tail) - meanDb (y, speech); };

    double sp = 0; size_t ns = 0;
    for (size_t b = 0; b < E.size(); ++b) if (speech[b]) { sp += std::pow (10.0, E[b] / 10.0); ++ns; }
    const double speechRms = std::sqrt (sp / (double) ns);
    double tvp = 0; for (float x : tv0) tvp += (double) x * x;
    const float gTv = (float) (speechRms * std::pow (10.0, -25.0 / 20.0) / std::sqrt (tvp / (double) tv0.size()));
    std::mt19937 rng (7); std::normal_distribution<float> nd (0.0f, 1.0f);
    const float gRoom = (float) (speechRms * std::pow (10.0, -50.0 / 20.0));
    std::vector<float> take (voice.size()), quiet (voice.size());
    for (size_t i = 0; i < take.size(); ++i)
    {
        const float room = gRoom * nd (rng);
        take[i]  = voice[i] + gTv * tv0[i % tv0.size()] + room;
        quiet[i] = voice[i] + room;
    }
    const double inTail = tailDrop (take);
    std::printf ("Glos %.1f s, mowa/tlo na wejsciu %.1f dB\n", (double) take.size() / sr, ratio (take));

    // ---- 1. kiedy wlaczone --------------------------------------------------
    {
        using St = mix::Decide::Style;
        for (auto st : { St::Delikatny, St::Studyjny, St::Mocny })
        {
            auto w = zs::TakeReview::podcastWish(); w.style = st;
            const auto dTv = mix::Decide::listen (take, sr, w), dQ = mix::Decide::listen (quiet, sr, w);
            const bool want = st != St::Delikatny;
            std::string name = std::string ("telewizor 25 dB pod mowa: AI ") + (want ? "wlaczone" : "wylaczone") + ", " + mix::Decide::styleName (st);
            check (dTv.params.useAiDenoise == want, name.c_str(),
                   "pauzy " + f1 (dTv.speechDb - dTv.pauseDb) + " dB pod mowa");
            name = std::string ("cichy pokoj (50 dB): AI wylaczone, ") + mix::Decide::styleName (st);
            check (! dQ.params.useAiDenoise, name.c_str(), "pauzy " + f1 (dQ.speechDb - dQ.pauseDb) + " dB pod mowa");
        }
    }

    // ---- 2. tlo nizej niz dotychczasowy tor, koncowki zostaja ---------------------
    for (auto st : { mix::Decide::Style::Studyjny, mix::Decide::Style::Mocny })
    {
        auto w = zs::TakeReview::podcastWish(); w.style = st;
        mix::Tweaks noAi; noAi.off[(size_t) mix::Stage::TloAI] = true;

        zs::TakeReview a, b;
        a.setTake (take, sr, {}); b.setTake (take, sr, {});
        const auto t0 = std::chrono::steady_clock::now();
        a.run (w, {}, 0.0);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        b.run (w, noAi, 0.0);
        const auto& ya = a.mixedAudio(); const auto& yb = b.mixedAudio();
        std::printf ("%s (%.2f s liczenia z AI, %.1f s nagrania):\n", mix::Decide::styleName (st), secs, take.size() / sr);
        check (a.whatIDid().aiDenoised && ! b.whatIDid().aiDenoised, "AI zadzialalo tylko w torze bez poprawki");
        check (ya.size() == take.size(), "dlugosc bez zmian");
        const double gain = ratio (ya) - ratio (yb);
        check (gain >= minGainDb, ("tlo w pauzach nizej niz dotychczasowy tor o >= " + f1 (minGainDb) + " dB").c_str(),
               f1 (gain) + " dB (mowa/tlo " + f1 (ratio (yb)) + " -> " + f1 (ratio (ya)) + " dB)");
        const double td = tailDrop (ya) - inTail;
        check (td > maxTailDb, "koncowki slow wzgledem glosnej mowy: najwyzej -4 dB", f1 (td) + " dB");
        check (std::abs (a.whatIDid().lufsAfter - b.whatIDid().lufsAfter) < 0.5, "glosnosc po miksie ta sama",
               f1 (a.whatIDid().lufsAfter) + " / " + f1 (b.whatIDid().lufsAfter) + " LUFS");
        bool said = false;
        for (const auto& l : a.plainSays()) if (l.find ("hałas w tle") != std::string::npos) said = true;
        check (said, "opis miksu mowi o usunieciu tla");
        check (! a.whatIDecided().because.empty() && a.whatIDecided().because.front().rfind ("Usuwanie tła (AI)", 0) == 0,
               "szczegoly: zdanie z pomiarem pauz");
    }

    // ---- 3. czysta mowa: glosnosc i koncowki, wyrownanie ------------------------
    {
        auto y = quiet;
        check (mix::ai::denoise (y, sr), "siec dziala na 48 kHz");
        check (y.size() == quiet.size(), "dlugosc bez zmian (48 kHz)");
        const int lag = lagOf (quiet, y, 960);
        check (lag == 0, "bez przesuniecia (48 kHz)", std::to_string (lag) + " probek");
        const double dl = meanDb (y, speech) - meanDb (quiet, speech);
        check (std::abs (dl) < 1.0, "glosna mowa: najwyzej 1 dB roznicy", f1 (dl) + " dB");
        const double dt = tailDrop (y) - tailDrop (quiet);
        check (dt > maxTailDb, "koncowki i oddechy czystej mowy: najwyzej -4 dB", f1 (dt) + " dB");

        // 44,1 kHz: przez resampling i z powrotem
        mix::Resampler rs; rs.prepare (48000, 44100);
        std::vector<float> q44; rs.render (quiet, q44);
        auto y44 = q44;
        check (mix::ai::denoise (y44, 44100.0), "siec dziala na 44,1 kHz (resampling)");
        check (y44.size() == q44.size(), "dlugosc bez zmian (44,1 kHz)");
        const int lag44 = lagOf (q44, y44, 960);
        check (lag44 == 0, "bez przesuniecia (44,1 kHz)", std::to_string (lag44) + " probek");

        // domieszka oryginalu
        auto yd = quiet;
        mix::ai::denoise (yd, sr, -20.0f);
        check (lagOf (quiet, yd, 960) == 0 && yd.size() == quiet.size(), "domieszka oryginalu bez przesuniecia");
    }

    // ---- 4. czas: probka 15 s ---------------------------------------------------
    {
        std::vector<float> s15 (take.begin(), take.begin() + (long) std::min (take.size(), (size_t) (15 * sr)));
        const auto t0 = std::chrono::steady_clock::now();
        mix::ai::denoise (s15, sr);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        check (secs < 3.0, "15 s nagrania: siec ponizej 3 s", f1 (secs) + " s");
    }

    std::printf (fails ? "\nBLEDY: %d\n" : "\nWszystko OK\n", fails);
    return fails ? 1 : 0;
}
