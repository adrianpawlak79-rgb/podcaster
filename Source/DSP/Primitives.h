/*
    Primitives.h — small, dependency-free building blocks for TakeWatch.

    Everything here is real-time safe: no allocation, no locks, no exceptions
    after prepare(). The analyser above it never touches the audio; it only
    measures. That is deliberate — a recording assistant that can damage the
    take is worse than no assistant.
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

/* M_PI nie jest czescia standardu C++ — glibc je daje, ale MinGW i MSVC juz
   nie, dopoki nie zdefiniuje sie _USE_MATH_DEFINES. Kompilacja skrosna pod
   Windowsa wykladala sie tu na siedmiu bledach naraz. */
#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

namespace tw
{

constexpr float kMinDb = -140.0f;

inline float toDb (float linear) noexcept
{
    return linear > 1.0e-7f ? 20.0f * std::log10 (linear) : kMinDb;
}

inline float toDbEnergy (float energy) noexcept
{
    return energy > 1.0e-14f ? 10.0f * std::log10 (energy) : kMinDb;
}

inline float fromDb (float db) noexcept
{
    return std::pow (10.0f, db / 20.0f);
}

//==============================================================================
/** Direct-form-II transposed biquad. */
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void reset() noexcept { z1 = z2 = 0.0; }

    inline float process (float x) noexcept
    {
        const double in  = (double) x;
        const double out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return (float) out;
    }

    void setLowpass (double sr, double f, double q) noexcept
    {
        const double w = 2.0 * M_PI * std::min (f, sr * 0.45) / sr;
        const double c = std::cos (w), s = std::sin (w);
        const double alpha = s / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = ((1.0 - c) * 0.5) / a0;
        b1 = (1.0 - c) / a0;
        b2 = b0;
        a1 = (-2.0 * c) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    void setHighpass (double sr, double f, double q) noexcept
    {
        const double w = 2.0 * M_PI * std::min (f, sr * 0.45) / sr;
        const double c = std::cos (w), s = std::sin (w);
        const double alpha = s / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = ((1.0 + c) * 0.5) / a0;
        b1 = -(1.0 + c) / a0;
        b2 = b0;
        a1 = (-2.0 * c) / a0;
        a2 = (1.0 - alpha) / a0;
    }
};

//==============================================================================
/** Two cascaded biquads forming a band, plus a level follower on its output.

    The follower is asymmetric on purpose: fast attack so a plosive is visible
    within a couple of milliseconds, slow release so the measurement does not
    collapse between syllables.
*/
struct BandEnvelope
{
    Biquad hp1, hp2, lp1, lp2;
    float  env = 0.0f;
    float  attackCoef = 0.0f, releaseCoef = 0.0f;
    bool   useHp = true, useLp = true;

    void setup (double sr, double fLow, double fHigh,
                double attackMs, double releaseMs) noexcept
    {
        useHp = fLow  > 1.0;
        useLp = fHigh < sr * 0.45;

        if (useHp) { hp1.setHighpass (sr, fLow,  0.7071); hp2.setHighpass (sr, fLow,  0.7071); }
        if (useLp) { lp1.setLowpass  (sr, fHigh, 0.7071); lp2.setLowpass  (sr, fHigh, 0.7071); }

        attackCoef  = (float) std::exp (-1.0 / (0.001 * attackMs  * sr));
        releaseCoef = (float) std::exp (-1.0 / (0.001 * releaseMs * sr));
        reset();
    }

    void reset() noexcept
    {
        hp1.reset(); hp2.reset(); lp1.reset(); lp2.reset();
        env = 0.0f;
    }

    inline float process (float x) noexcept
    {
        float y = x;
        if (useHp) y = hp2.process (hp1.process (y));
        if (useLp) y = lp2.process (lp1.process (y));

        const float rect = std::abs (y);
        const float c    = rect > env ? attackCoef : releaseCoef;
        env = rect + c * (env - rect);
        return env;
    }

    float db() const noexcept { return toDb (env); }
};

//==============================================================================
/** A one-pole follower with independent up/down rates, used for slow baselines. */
struct SlowFollower
{
    float value = 0.0f;
    float upCoef = 0.0f, downCoef = 0.0f;
    bool  primed = false;

    void setup (double sr, double upMs, double downMs) noexcept
    {
        upCoef   = (float) std::exp (-1.0 / (0.001 * upMs   * sr));
        downCoef = (float) std::exp (-1.0 / (0.001 * downMs * sr));
        value = 0.0f;
        primed = false;
    }

    inline float process (float x) noexcept
    {
        if (! primed) { value = x; primed = true; return value; }
        const float c = x > value ? upCoef : downCoef;
        value = x + c * (value - x);
        return value;
    }
};

//==============================================================================
/** Fixed-bin histogram over a dB range. Gives medians and percentiles in O(1)
    per sample and O(bins) to read back, which keeps the audio thread honest. */
template <int NumBins>
struct DbHistogram
{
    static constexpr float loDb = -120.0f;
    static constexpr float hiDb =   24.0f;

    uint32_t bins[NumBins] {};
    uint32_t count = 0;

    void clear() noexcept { std::memset (bins, 0, sizeof (bins)); count = 0; }

    inline void add (float db) noexcept
    {
        const float t = (db - loDb) / (hiDb - loDb);
        int i = (int) (t * (float) NumBins);
        i = std::clamp (i, 0, NumBins - 1);
        ++bins[i];
        ++count;
    }

    float percentile (float p) const noexcept
    {
        if (count == 0) return kMinDb;

        const uint32_t target = (uint32_t) std::max (1.0f, p * (float) count);
        uint32_t acc = 0;

        for (int i = 0; i < NumBins; ++i)
        {
            acc += bins[i];
            if (acc >= target)
                return loDb + (hiDb - loDb) * (((float) i + 0.5f) / (float) NumBins);
        }

        return hiDb;
    }

    float median() const noexcept { return percentile (0.5f); }
};

//==============================================================================
/** 4x true-peak estimator (ITU-R BS.1770 style, short windowed-sinc polyphase).

    Sample peak lies about a digital clip; a signal can sit at -0.2 dBFS on every
    sample and still overshoot the converter between them. This is the cheap
    version of the standard measurement: 12 taps per phase, flat to ~19 kHz.
*/
class TruePeak
{
public:
    void prepare() noexcept
    {
        // windowed sinc, cutoff 0.45 * fs/2 of the 4x grid
        for (int phase = 0; phase < 4; ++phase)
            for (int t = 0; t < numTaps; ++t)
            {
                const double n = (double) t - (double) (numTaps - 1) * 0.5
                                 - (double) phase * 0.25;
                double s;
                if (std::abs (n) < 1.0e-9) s = 1.0;
                else                       s = std::sin (M_PI * n) / (M_PI * n);

                const double w = 0.42 - 0.5 * std::cos (2.0 * M_PI * (double) t / (double) (numTaps - 1))
                                      + 0.08 * std::cos (4.0 * M_PI * (double) t / (double) (numTaps - 1));
                coef[phase][t] = (float) (s * w);
            }

        // normalise each phase to unity DC gain so a DC-ish peak is not inflated
        for (int phase = 0; phase < 4; ++phase)
        {
            double sum = 0.0;
            for (int t = 0; t < numTaps; ++t) sum += coef[phase][t];
            if (std::abs (sum) > 1.0e-9)
                for (int t = 0; t < numTaps; ++t) coef[phase][t] /= (float) sum;
        }

        reset();
    }

    void reset() noexcept
    {
        std::memset (hist, 0, sizeof (hist));
        pos = 0;
    }

    inline float process (float x) noexcept
    {
        hist[pos] = x;
        pos = (pos + 1) % numTaps;

        float peak = std::abs (x);

        for (int phase = 0; phase < 4; ++phase)
        {
            float acc = 0.0f;
            int idx = pos;
            for (int t = 0; t < numTaps; ++t)
            {
                acc += coef[phase][t] * hist[idx];
                idx = (idx + 1) % numTaps;
            }
            peak = std::max (peak, std::abs (acc));
        }

        return peak;
    }

private:
    static constexpr int numTaps = 12;
    float coef[4][numTaps] {};
    float hist[numTaps] {};
    int   pos = 0;
};

//==============================================================================
/** In-place radix-2 complex FFT. Size must be a power of two. */
inline void fft (float* re, float* im, int n) noexcept
{
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap (re[i], re[j]); std::swap (im[i], im[j]); }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * M_PI / (double) len;
        const float wr = (float) std::cos (ang);
        const float wi = (float) std::sin (ang);

        for (int i = 0; i < n; i += len)
        {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; ++k)
            {
                const int a = i + k, b = i + k + len / 2;
                const float xr = re[b] * cr - im[b] * ci;
                const float xi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - xr; im[b] = im[a] - xi;
                re[a] += xr;        im[a] += xi;
                const float nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
}

} // namespace tw
