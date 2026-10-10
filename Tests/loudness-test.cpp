/*
    loudness-test.cpp — generuje przebiegi kontrolne, mierzy je naszym miernikiem
    i zapisuje na dysk, zeby niezalezna implementacja mogla zmierzyc TE SAME
    pliki. Porownanie robi loudness-check.py.

        g++ -O2 -std=c++17 -I../Source loudness-test.cpp -o loudness-test
        ./loudness-test /tmp/lufs && python3 loudness-check.py /tmp/lufs
*/

#include "../Source/DSP/Mix/Loudness.h"
#include "wav.h"

#include <cmath>
#include <filesystem>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static constexpr double kSR = 48000.0;

struct Signal
{
    std::string name;
    int channels;
    std::vector<float> data;    // przeplecione
};

//==============================================================================
static Signal sine (const std::string& name, double hz, double dbfs,
                    double seconds, int channels)
{
    Signal s { name, channels, {} };
    const int n = (int) (seconds * kSR);
    const double amp = std::pow (10.0, dbfs / 20.0);

    s.data.resize ((size_t) (n * channels));

    for (int i = 0; i < n; ++i)
    {
        const float v = (float) (amp * std::sin (2.0 * M_PI * hz * i / kSR));
        for (int c = 0; c < channels; ++c)
            s.data[(size_t) (i * channels + c)] = v;
    }

    return s;
}

static Signal noise (const std::string& name, double dbfs, double seconds, int channels)
{
    Signal s { name, channels, {} };
    const int n = (int) (seconds * kSR);
    const double amp = std::pow (10.0, dbfs / 20.0);

    std::mt19937 rng { 777 };
    std::normal_distribution<float> g { 0.0f, 1.0f };

    s.data.resize ((size_t) (n * channels));

    for (int i = 0; i < n; ++i)
        for (int c = 0; c < channels; ++c)
            s.data[(size_t) (i * channels + c)] = (float) (amp * g (rng) * 0.3);

    return s;
}

/** Mowa z pauzami — najwazniejszy przypadek, bo tu wchodza bramy standardu.
    Bez nich cisza miedzy zdaniami zanizylaby wynik o kilka LU i program
    podkrecilby material za wysoko. */
static Signal speechLike (const std::string& name, double dbfs, double seconds, int channels)
{
    Signal s { name, channels, {} };
    const int n = (int) (seconds * kSR);
    const double amp = std::pow (10.0, dbfs / 20.0);

    std::mt19937 rng { 4242 };
    std::normal_distribution<float> g { 0.0f, 1.0f };

    s.data.resize ((size_t) (n * channels));
    double phase = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double t = i / kSR;
        const bool on = std::fmod (t, 4.2) < 3.0;

        double v = 0.0;

        if (on)
        {
            phase += 2.0 * M_PI * 115.0 / kSR;
            if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;

            for (int h = 1; h <= 14; ++h)
                v += std::sin (phase * h) / (h * h);

            v = v * 1.55 + 0.04 * g (rng);
            v *= amp;

            // obwiednia frazy, zeby nie bylo skokow na stykach
            const double inPhrase = std::fmod (t, 4.2);
            const double env = std::min (1.0, std::min (inPhrase / 0.05, (3.0 - inPhrase) / 0.05));
            v *= std::max (0.0, env);
        }

        for (int c = 0; c < channels; ++c)
            s.data[(size_t) (i * channels + c)] = (float) v;
    }

    return s;
}

//==============================================================================
int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : "/tmp/lufs";
    std::filesystem::create_directories(dir);

    std::vector<Signal> all;
    all.push_back (sine       ("sinus-1k-20db-mono",   1000.0, -20.0, 10.0, 1));
    all.push_back (sine       ("sinus-1k-20db-stereo", 1000.0, -20.0, 10.0, 2));
    all.push_back (sine       ("sinus-100hz-6db",       100.0,  -6.0, 10.0, 1));
    all.push_back (sine       ("sinus-8k-20db",        8000.0, -20.0, 10.0, 1));
    all.push_back (noise      ("szum-mono",             -12.0, 12.0, 1));
    all.push_back (speechLike ("mowa-z-pauzami",        -12.0, 30.0, 1));
    all.push_back (speechLike ("mowa-cicha",            -30.0, 30.0, 1));

    std::printf ("%-24s %10s %10s\n", "przebieg", "LUFS", "dBTP");

    for (auto& s : all)
    {
        mix::LoudnessMeter meter;
        meter.prepare (kSR, s.channels);

        mix::TruePeak tp;
        tp.prepare (kSR, s.channels);

        // rozplot na kanaly, blokami po 1024 probki
        const int n = (int) (s.data.size() / (size_t) s.channels);
        std::vector<std::vector<float>> ch ((size_t) s.channels,
                                            std::vector<float> (1024));
        std::vector<const float*> ptr ((size_t) s.channels);

        for (int i = 0; i < n; i += 1024)
        {
            const int len = std::min (1024, n - i);

            for (int c = 0; c < s.channels; ++c)
            {
                for (int k = 0; k < len; ++k)
                    ch[(size_t) c][(size_t) k] = s.data[(size_t) ((i + k) * s.channels + c)];

                ptr[(size_t) c] = ch[(size_t) c].data();
            }

            meter.process (ptr.data(), s.channels, len);
            tp.process (ptr.data(), s.channels, len);
        }

        std::printf ("%-24s %10.2f %10.2f\n", s.name.c_str(), meter.integrated(), tp.dbTP());

        wav::writeFloat (dir + "/" + s.name + ".wav", s.data, s.channels, (int) kSR);

        FILE* f = std::fopen ((dir + "/" + s.name + ".nasz").c_str(), "w");
        if (f) { std::fprintf (f, "%.4f\n", meter.integrated()); std::fclose (f); }
    }

    std::printf ("\npliki w %s\n", dir.c_str());
    return 0;
}
