/*
    zs-glosy — miks na wielu glosach: czy czegos nie psuje.

        g++ -O2 -std=c++17 -ISource Tools/zs-glosy.cpp -o zs-glosy
        zs-glosy raport.md nagranie1.wav [nagranie2.wav ...]

    Audyt 14.10: "Miks oceniony na glosie autora nie potwierdza jakosci na
    roznych glosach i mikrofonach. Stale wyciecia pasm, dodawanie powietrza
    i sciszanie oddechow moga pogorszyc inne nagrania."

    Dla kazdego nagrania dwie wersje wejscia:
      - "studyjne": tak, jak przyszlo (np. audiobook Wolnych Lektur —
        juz obrobiony, wiec miks nie powinien go zepsuc);
      - "laptop": 14 dB ciszej, szum wentylatora (rozowy, -62 dBFS) i
        przydzwiek 50 Hz (-66 dBFS) — jak tani mikrofon w zwyklym pokoju.
    Kazda przez trzy brzmienia (Delikatny, Studyjny, Mocny), te same klasy co
    w programie (zs::TakeReview).

    Mierzone przed i po (przed: po wyrownaniu do tej samej glosnosci):
      - glosnosc (LUFS) i szczyt (dBTP) — czy cel trafiony;
      - tlo: 10. percentyl RMS 50 ms wzgledem mowy (95. percentyl) — czy
        pauzy sa czystsze, a nie "pompuja";
      - rozpietosc: 95.-10. percentyl RMS 400 ms w mowie — ile wyrownano;
      - barwa: pasma 80-200, 200-500, 2-5 k, 5-10 k Hz wzgledem 0,5-2 kHz —
        o ile miks je zmienil. Flaga, gdy pasmo zmienia sie o wiecej niz
        6 dB albo gora (5-10 k) rosnie o wiecej niz 4 dB (syk, "s").

    To nie jest odsluch. Mowi, gdzie posluchac, a nie, ze jest dobrze.
*/

#include "DSP/TakeReview.h"
#include "DSP/Mix/Fft.h"
#include "Wav.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace
{
struct Measure
{
    double lufs = 0, peak = 0, floorRel = 0, spread = 0;
    double band[5] = {};   // 80-200, 200-500, 2-5k, 5-10k wzgledem 0,5-2 k (dB); [4] = 0,5-2 k bezwzgl.
};

double pct (std::vector<double> v, double p)
{
    if (v.empty()) return -140.0;
    std::sort (v.begin(), v.end());
    return v[(size_t) std::clamp (p * (double) (v.size() - 1), 0.0, (double) v.size() - 1)];
}

std::vector<double> rmsDb (const std::vector<float>& x, double sr, double winSec)
{
    std::vector<double> out;
    const size_t w = (size_t) (sr * winSec);
    for (size_t i = 0; i + w <= x.size(); i += w)
    {
        double s = 0;
        for (size_t j = i; j < i + w; ++j) s += (double) x[j] * x[j];
        out.push_back (10.0 * std::log10 (s / (double) w + 1e-14));
    }
    return out;
}

Measure measure (const std::vector<float>& x, double sr)
{
    Measure m;
    mix::LoudnessMeter lm; lm.prepare (sr, 1);
    const float* p[1] = { x.data() };
    lm.process (p, 1, (int) x.size());
    m.lufs = lm.integrated();
    mix::TruePeak tp; tp.prepare (sr, 1); tp.process (p, 1, (int) x.size()); tp.finish();
    m.peak = tp.dbTP();

    const auto r50 = rmsDb (x, sr, 0.05);
    const double speech = pct (r50, 0.95), floor = pct (r50, 0.10);
    m.floorRel = floor - speech;

    std::vector<double> r400, active;
    r400 = rmsDb (x, sr, 0.4);
    for (double v : r400) if (v > speech - 30.0) active.push_back (v);
    m.spread = pct (active, 0.95) - pct (active, 0.10);

    // srednie widmo
    constexpr int N = 4096;
    mix::Fft f; f.prepare (N);
    std::vector<double> win; mix::hann (win, N);
    std::vector<double> acc (N / 2, 0.0), re (N), im (N);
    for (size_t i = 0; i + N <= x.size(); i += N / 2)
    {
        for (int k = 0; k < N; ++k) { re[(size_t) k] = x[i + (size_t) k] * win[(size_t) k]; im[(size_t) k] = 0.0; }
        f.run (re, im, false);
        for (int k = 0; k < N / 2; ++k) acc[(size_t) k] += re[(size_t) k] * re[(size_t) k] + im[(size_t) k] * im[(size_t) k];
    }
    auto bandDb = [&] (double lo, double hi) {
        double s = 0;
        for (int k = 0; k < N / 2; ++k) { const double hz = k * sr / N; if (hz >= lo && hz < hi) s += acc[(size_t) k]; }
        return 10.0 * std::log10 (s + 1e-20);
    };
    const double mid = bandDb (500, 2000);
    m.band[0] = bandDb (80, 200) - mid;
    m.band[1] = bandDb (200, 500) - mid;
    m.band[2] = bandDb (2000, 5000) - mid;
    m.band[3] = bandDb (5000, 10000) - mid;
    m.band[4] = mid;
    return m;
}

std::vector<float> laptop (const std::vector<float>& x, double sr)
{
    std::vector<float> y (x.size());
    std::mt19937 rng (11);
    std::normal_distribution<double> nd (0.0, 1.0);
    // rozowy szum (Paul Kellet), poziom ok. -62 dBFS RMS
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    const double gain = std::pow (10.0, -14.0 / 20.0), noise = std::pow (10.0, -62.0 / 20.0) * 0.18,
                 hum = std::pow (10.0, -66.0 / 20.0) * std::sqrt (2.0);
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double w = nd (rng);
        b0 = 0.99886 * b0 + w * 0.0555179; b1 = 0.99332 * b1 + w * 0.0750759; b2 = 0.96900 * b2 + w * 0.1538520;
        b3 = 0.86650 * b3 + w * 0.3104856; b4 = 0.55000 * b4 + w * 0.5329522; b5 = -0.7616 * b5 - w * 0.0168980;
        const double pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362; b6 = w * 0.115926;
        y[i] = (float) (x[i] * gain + pink * noise + hum * std::sin (2.0 * M_PI * 50.0 * (double) i / sr));
    }
    return y;
}

const char* styleName (int s) { return s == 0 ? "Delikatny" : s == 1 ? "Studyjny" : "Mocny"; }
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::fprintf (stderr, "zs-glosy raport.md nagranie.wav ...\n"); return 2; }
    FILE* out = std::fopen (argv[1], "w");
    if (! out) return 2;

    std::fprintf (out, "# Miks na wielu głosach\n\n"
                       "Wejście „studyjne” = nagranie tak, jak przyszło; „laptop” = 14 dB ciszej, szum wentylatora −62 dBFS i przydźwięk 50 Hz −66 dBFS.\n"
                       "Tło i rozpiętość w dB; barwa = zmiana pasma względem 0,5–2 kHz (po − przed). ⚠ = pasmo zmienione o > 6 dB albo góra 5–10 k w górę o > 4 dB, głośność poza −19 ±1 LUFS, szczyt nad −1 dBTP.\n\n"
                       "| nagranie | wejście | brzmienie | LUFS | szczyt | tło przed→po | rozpiętość przed→po | Δ 80–200 | Δ 200–500 | Δ 2–5 k | Δ 5–10 k | |\n"
                       "|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    int flags = 0, rows = 0;

    for (int a = 2; a < argc; ++a)
    {
        wavio::Audio in;
        if (! wavio::read (argv[a], in).empty() || in.channels.empty()) { std::fprintf (stderr, "nie czytam %s\n", argv[a]); continue; }
        const double sr = in.sampleRate;
        std::string name = argv[a];
        name = name.substr (name.find_last_of ('/') + 1);
        if (name.size() > 28) name = name.substr (0, 26) + "…";

        for (int v = 0; v < 2; ++v)
        {
            const auto x = v == 0 ? in.channels[0] : laptop (in.channels[0], sr);
            const auto before = measure (x, sr);

            for (int s = 0; s < 3; ++s)
            {
                auto wish = zs::TakeReview::podcastWish();
                wish.style = (mix::Decide::Style) s;
                zs::TakeReview r;
                r.setTake (x, sr, {});
                r.run (wish);
                const auto after = measure (r.mixedAudio(), sr);

                double d[4];
                for (int b = 0; b < 4; ++b) d[b] = after.band[b] - before.band[b];
                const bool bad = std::abs (after.lufs + 19.0) > 1.0 || after.peak > -0.9
                              || std::abs (d[0]) > 6 || std::abs (d[1]) > 6 || std::abs (d[2]) > 6 || std::abs (d[3]) > 6 || d[3] > 4;
                flags += bad; ++rows;
                std::fprintf (out, "| %s | %s | %s | %.1f | %.1f | %.0f→%.0f | %.1f→%.1f | %+.1f | %+.1f | %+.1f | %+.1f | %s |\n",
                              name.c_str(), v == 0 ? "studyjne" : "laptop", styleName (s), after.lufs, after.peak,
                              before.floorRel, after.floorRel, before.spread, after.spread, d[0], d[1], d[2], d[3], bad ? "⚠" : "");
                std::fflush (out);
                std::fprintf (stderr, "%s %s %s%s\n", name.c_str(), v == 0 ? "studyjne" : "laptop", styleName (s), bad ? "  ⚠" : "");
            }
        }
    }
    std::fprintf (out, "\n%d z %d przebiegów z flagą ⚠.\n", flags, rows);
    std::fclose (out);
    return 0;
}
