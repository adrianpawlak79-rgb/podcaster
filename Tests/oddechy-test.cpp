/*
    oddechy-test.cpp — sciszanie oddechow (Source/DSP/Mix/Breath.h).

        oddechy-test mowa.wav

    Mowa z espeak pocieta na zdania, miedzy nimi przerwy z tlem pokoju.
    W przerwach: "oddechy" (szum pasmowy, 26 dB pod mowa) — maja byc
    sciszone; glosne "s" (12 dB pod mowa) — nie; cichy szum wcisniety
    miedzy dwie samogloski bez ciszy (jak "s" w srodku slowa) — nie.
    Mowa ma zostac nietknieta.
*/

#include "../Source/DSP/Mix/Breath.h"
#include "../Source/DSP/Mix/Biquad.h"
#include "../Source/Wav.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-4s  %-60s %s\n", ok ? "OK" : "BLAD", what, got.c_str());
    if (! ok) ++fails;
}

static std::string num (double v, const char* unit = " dB")
{
    char b[48]; std::snprintf (b, sizeof (b), "%.1f%s", v, unit); return b;
}

static double rmsDb (const std::vector<float>& x, double sr, double a, double b)
{
    const size_t i0 = (size_t) (a * sr), i1 = std::min (x.size(), (size_t) (b * sr));
    double e = 0.0; for (size_t i = i0; i < i1; ++i) e += (double) x[i] * x[i];
    return 10.0 * std::log10 (e / std::max<size_t> (1, i1 - i0) + 1.0e-20);
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("uzycie: oddechy-test mowa.wav\n"); return 1; }
    wavio::Audio in;
    if (! wavio::read (argv[1], in).empty()) { std::printf ("brak mowy\n"); return 1; }
    const auto src = in.toMono();
    const double sr = (double) in.sampleRate;

    std::printf ("\nSciszanie oddechow\n==================\n\n");

    // zdania z espeak: kawalki po 1,4 s z miejsc, gdzie jest glos
    std::mt19937 rng (3);
    std::normal_distribution<float> n (0.0f, 1.0f);
    const size_t chunk = (size_t) (1.4 * sr), gap = (size_t) (0.9 * sr);

    std::vector<float> x;
    std::vector<std::pair<double, double>> speech, breaths, loud, between;

    auto addSpeech = [&] (size_t from) {
        const double t = x.size() / sr;
        for (size_t i = 0; i < chunk; ++i) x.push_back (src[(from + i) % src.size()]);
        speech.push_back ({ t, x.size() / sr });
    };
    auto noiseBurst = [&] (size_t len, float amp, double lo, double hi) {
        mix::Biquad a, b;
        a.setCoeffs (mix::design::highpass (lo, sr, 0.7071)); b.setCoeffs (mix::design::lowpass (hi, sr, 0.7071));
        std::vector<float> o (len);
        for (size_t i = 0; i < len; ++i)
        {
            const float env = (float) std::sin (3.14159265 * (double) i / (double) len);
            o[i] = amp * env * b.process (a.process (n (rng)));
        }
        return o;
    };
    auto addGap = [&] (const std::vector<float>& burst, std::vector<std::pair<double, double>>* mark) {
        const size_t start = x.size();
        x.resize (start + gap, 0.0f);
        const size_t at = start + (gap - burst.size()) / 2;
        for (size_t i = 0; i < burst.size(); ++i) x[at + i] += burst[i];
        if (mark) mark->push_back ({ at / sr, (at + burst.size()) / sr });
    };

    // poziom glosnej mowy z samego zrodla (95. percentyl ramek 30 ms)
    std::vector<double> lv;
    for (size_t i = 0; i + (size_t) (0.03 * sr) < src.size(); i += (size_t) (0.01 * sr))
        lv.push_back (rmsDb (src, sr, i / sr, i / sr + 0.03));
    std::sort (lv.begin(), lv.end());
    const double loudDb = lv[lv.size() * 95 / 100];
    const auto amp = [&] (double belowDb) { return (float) std::pow (10.0, (loudDb - belowDb) / 20.0) * 1.6f; };

    size_t pos = src.size() / 10;
    for (int k = 0; k < 4; ++k)
    {
        addSpeech (pos); pos += chunk;
        addGap (noiseBurst ((size_t) (0.30 * sr), amp (26.0), 300.0, 3000.0), &breaths);
    }
    addSpeech (pos); pos += chunk;
    addGap (noiseBurst ((size_t) (0.20 * sr), amp (12.0), 4000.0, 9000.0), &loud);    // glosne "s"
    addSpeech (pos); pos += chunk;

    // cichy szum wcisniety miedzy samogloski, bez ciszy
    {
        const auto burst = noiseBurst ((size_t) (0.15 * sr), amp (26.0), 300.0, 3000.0);
        const double t = x.size() / sr;
        for (auto v : burst) x.push_back (v);
        between.push_back ({ t, x.size() / sr });
    }
    addSpeech (pos);
    x.resize (x.size() + gap, 0.0f);

    for (auto& v : x) v += 3.0e-4f * n (rng);          // tlo pokoju ok. -70 dBFS

    mix::BreathControl bc;
    std::vector<float> y;
    bc.render (x, y, sr);

    std::printf ("  znalezione: %d (wlozone oddechy: %zu)\n\n", bc.found, breaths.size());

    double worstBreath = -1.0e9;
    for (auto& b : breaths) worstBreath = std::max (worstBreath, rmsDb (y, sr, b.first, b.second) - rmsDb (x, sr, b.first, b.second));
    check (worstBreath < -8.0, "oddechy w przerwach sciszone o co najmniej 8 dB", num (worstBreath));
    check (worstBreath > -14.0, "ale nie do zera", num (worstBreath));

    const double sLoud = rmsDb (y, sr, loud[0].first, loud[0].second) - rmsDb (x, sr, loud[0].first, loud[0].second);
    check (sLoud > -1.0, "glosne \"s\" w przerwie nietkniete", num (sLoud));

    const double sBetween = rmsDb (y, sr, between[0].first, between[0].second) - rmsDb (x, sr, between[0].first, between[0].second);
    check (sBetween > -1.0, "cichy szum miedzy samogloskami (jak glosk w slowie) nietkniety", num (sBetween));

    double worstSpeech = 0.0;
    for (auto& s : speech) worstSpeech = std::min (worstSpeech, rmsDb (y, sr, s.first, s.second) - rmsDb (x, sr, s.first, s.second));
    check (worstSpeech > -0.5, "mowa nietknieta", num (worstSpeech));

    // bez przerw z tlem (sama mowa): nic do roboty
    mix::BreathControl none;
    std::vector<float> z;
    none.render (src, z, sr);
    double diff = 0.0; for (size_t i = 0; i < z.size(); ++i) diff = std::max (diff, (double) std::fabs (z[i] - src[i]));
    check (none.found == 0 || diff < 0.05, "sama mowa z espeak: nic albo prawie nic", num ((double) none.found, ""));

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
