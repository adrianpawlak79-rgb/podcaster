/*
    tlo-test.cpp — tlo (telewizor 25 dB pod mowa) nie moze rosnac po miksie
    wzgledem glosu (raport audio 27.11, B1).

    Glos: polska mowa z syntezatora (argv[1], 48 kHz), rozciagnieta: w kazdej
    przerwie miedzy slowami >= 150 ms wstawiamy 1,2 s ciszy (tak wygladaja
    pauzy przy czytaniu). Pod to: szum pokoju 50 dB pod mowa i "telewizor" —
    angielska mowa z syntezatora (argv[2]) w petli, 25 dB RMS pod glosem.

    Miara (jak w raporcie): stosunek mocy w ramkach glosnej mowy do mocy w
    ramkach pauzy (>= 300 ms od slowa), na wejsciu i po miksie. Maski z
    czystego glosu, wiec tlo i mowa nie mieszaja sie w pomiarze. Tlo nie moze
    urosnac o wiecej niz 1 dB — w zadnym z trzech brzmien.

    Wywolanie: tlo-test glos-48k.wav telewizor-48k.wav
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
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
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

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("uzycie: tlo-test glos.wav telewizor.wav\n"); return 1; }
    wavio::Audio va, ta;
    if (! wavio::read (argv[1], va).empty() || ! wavio::read (argv[2], ta).empty()) { std::printf ("  BLAD  brak plikow\n"); return 1; }
    const double sr = va.sampleRate;
    if (ta.sampleRate != va.sampleRate) { std::printf ("  BLAD  rozne czestotliwosci\n"); return 1; }
    const auto v0 = va.toMono(), tv0 = ta.toMono();
    const size_t blk = (size_t) (0.05 * sr);

    // ---- glos z dlugimi pauzami ---------------------------------------------
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

    // ---- maski z czystego glosu -----------------------------------------------
    const auto E = blocksDb (voice, blk);
    double mx = -200; for (double x : E) mx = std::max (mx, x);
    std::vector<char> speech (E.size()), pause (E.size());
    for (size_t b = 0; b < E.size(); ++b) speech[b] = E[b] > mx - 20.0;
    for (size_t b = 0; b < E.size(); ++b)
    {
        bool far = true;
        for (long k = (long) b - 6; k <= (long) b + 6; ++k)
            if (k >= 0 && k < (long) E.size() && E[(size_t) k] > mx - 45.0) far = false;
        pause[b] = far;
    }
    const auto ratio = [&] (const std::vector<float>& y)
    {
        const auto e = blocksDb (y, blk);
        double s = 0, p = 0; size_t ns = 0, np = 0;
        for (size_t b = 0; b < E.size() && b < e.size(); ++b)
        {
            if (speech[b]) { s += std::pow (10.0, e[b] / 10.0); ++ns; }
            if (pause[b])  { p += std::pow (10.0, e[b] / 10.0); ++np; }
        }
        return 10.0 * std::log10 (s / (double) std::max<size_t> (1, ns)) - 10.0 * std::log10 (p / (double) std::max<size_t> (1, np) + 1e-30);
    };
    size_t np = 0; for (char c : pause) np += c;
    // ciche ramki mowy (koncowki slow, wejscia): 20-32 dB pod najglosniejsza
    std::vector<char> tail (E.size());
    for (size_t b = 0; b < E.size(); ++b) tail[b] = E[b] <= mx - 20.0 && E[b] > mx - 32.0;
    const auto tailDrop = [&] (const std::vector<float>& y)
    {
        const auto e = blocksDb (y, blk);
        double s = 0, t = 0; size_t ns = 0, nt = 0;
        for (size_t b = 0; b < E.size() && b < e.size(); ++b)
        {
            if (speech[b]) { s += std::pow (10.0, e[b] / 10.0); ++ns; }
            if (tail[b])   { t += std::pow (10.0, e[b] / 10.0); ++nt; }
        }
        return 10.0 * std::log10 (t / (double) std::max<size_t> (1, nt) + 1e-30) - 10.0 * std::log10 (s / (double) std::max<size_t> (1, ns));
    };

    // ---- tlo -------------------------------------------------------------------
    double sp = 0; size_t ns = 0;
    for (size_t b = 0; b < E.size(); ++b) if (speech[b]) { sp += std::pow (10.0, E[b] / 10.0); ++ns; }
    const double speechRms = std::sqrt (sp / (double) ns);
    double tvp = 0; for (float x : tv0) tvp += (double) x * x;
    const float gTv = (float) (speechRms * std::pow (10.0, -25.0 / 20.0) / std::sqrt (tvp / (double) tv0.size()));
    std::mt19937 rng (7); std::normal_distribution<float> nd (0.0f, 1.0f);
    const float gRoom = (float) (speechRms * std::pow (10.0, -50.0 / 20.0));
    std::vector<float> take (voice.size());
    for (size_t i = 0; i < take.size(); ++i) take[i] = voice[i] + gTv * tv0[i % tv0.size()] + gRoom * nd (rng);

    const double inRatio = ratio (take);
    const double inTail = tailDrop (take);
    std::printf ("Glos %.1f s, pauz (ramek 50 ms) %zu, mowa/tlo na wejsciu %.1f dB\n",
                 (double) take.size() / sr, np, inRatio);
    check (np > 100, "material ma dlugie pauzy", std::to_string (np));

    const mix::Decide::Style styles[] = { mix::Decide::Style::Delikatny, mix::Decide::Style::Studyjny, mix::Decide::Style::Mocny };
    for (auto st : styles)
    {
        zs::TakeReview r;
        r.setTake (take, sr, {});
        auto w = zs::TakeReview::podcastWish();
        w.style = st;
        const auto t0 = std::chrono::steady_clock::now();
        r.run (w, {}, 0.0);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const auto& out = r.mixedAudio();
        const double grow = inRatio - ratio (out);
        std::printf ("%s (%.2f s liczenia):\n", mix::Decide::styleName (st), secs);
        for (const auto& l : r.whatIDecided().because)
            if (l.find ("szum") != std::string::npos || l.find ("kspander") != std::string::npos || l.find ("tło") != std::string::npos)
                std::printf ("        %s\n", l.c_str());
        check (out.size() == take.size(), "dlugosc bez zmian");
        const std::string what = std::string ("tlo rosnie najwyzej o 1 dB: ") + mix::Decide::styleName (st);
        check (grow <= 1.0, what.c_str(), (grow >= 0 ? "+" : "") + f1 (grow) + " dB (mowa/tlo " + f1 (ratio (out)) + " dB)");
        // ekspander nie moze zjadac koncowek slow
        const double td = tailDrop (out) - inTail;
        check (td > -3.0, "koncowki slow wzgledem glosnej mowy: najwyzej -3 dB", f1 (td) + " dB");
    }

    std::printf (fails ? "\nBLEDY: %d\n" : "\nWszystko OK\n", fails);
    return fails ? 1 : 0;
}
