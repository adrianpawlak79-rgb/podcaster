/*
    Biquad.h — filtry drugiego rzedu. Fundament calej reszty miksu.

    Wszystko ponizej liczy sie w double, mimo ze sygnal jest w float. Powod jest
    konkretny: filtry o niskiej czestotliwosci odciecia przy 48 kHz maja bieguny
    tak blisko okregu jednostkowego, ze w pojedynczej precyzji szum kwantyzacji
    wspolczynnikow slychac jako podbicie basu. Filtr gornoprzepustowy na 80 Hz to
    dokladnie ten przypadek, a on stoi na poczatku kazdego lancucha mowy.

    Wzory: Robert Bristow-Johnson, Audio EQ Cookbook. Postac Direct Form I —
    gorzej znosi zmiany wspolczynnikow w locie niz TDF-II, ale my ich nie
    zmieniamy w locie nigdzie poza de-esserem, gdzie i tak liczy sie osobno.
*/

#pragma once

#include <algorithm>
#include <cmath>

namespace mix
{

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

struct Coeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
};

class Biquad
{
public:
    void setCoeffs (const Coeffs& c) noexcept { co = c; }
    const Coeffs& coeffs() const noexcept     { return co; }

    void reset() noexcept { x1 = x2 = y1 = y2 = 0.0; }

    inline float process (float in) noexcept
    {
        const double x = (double) in;
        double y = co.b0 * x + co.b1 * x1 + co.b2 * x2 - co.a1 * y1 - co.a2 * y2;

        // gasi denormale bez wlaczania trybu FTZ na calym procesie
        if (! (std::fabs (y) > 1.0e-30)) y = 0.0;

        x2 = x1; x1 = x;
        y2 = y1; y1 = y;

        return (float) y;
    }

    /** Odpowiedz amplitudowa w danej czestotliwosci — uzywana przez testy. */
    double magnitudeAt (double hz, double sr) const noexcept
    {
        const double w = 2.0 * M_PI * hz / sr;
        const double cw = std::cos (w), sw = std::sin (w);
        const double cw2 = std::cos (2.0 * w), sw2 = std::sin (2.0 * w);

        const double nr = co.b0 + co.b1 * cw + co.b2 * cw2;
        const double ni = -(co.b1 * sw + co.b2 * sw2);
        const double dr = 1.0 + co.a1 * cw + co.a2 * cw2;
        const double di = -(co.a1 * sw + co.a2 * sw2);

        return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
    }

private:
    Coeffs co;
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
};

//==============================================================================
namespace design
{
    /** Czestotliwosc filtra najwyzej 0,45 probkowania. Powyzej Nyquista sin(w) < 0,
        alpha < 0 i bieguny wychodza poza okrag: polka "powietrza" 9 kHz przy pliku
        16 kHz dawala inf/NaN (przeglad 14.10, nr 1). */
    inline double belowNyquist (double hz, double sr) noexcept { return std::min (hz, 0.45 * sr); }

    inline Coeffs normalise (double b0, double b1, double b2,
                             double a0, double a1, double a2) noexcept
    {
        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    inline Coeffs highpass (double hz, double sr, double q = 0.7071) noexcept
    {
        const double w = 2.0 * M_PI * belowNyquist (hz, sr) / sr;
        const double cw = std::cos (w), sw = std::sin (w);
        const double alpha = sw / (2.0 * q);

        return normalise ((1.0 + cw) * 0.5, -(1.0 + cw), (1.0 + cw) * 0.5,
                          1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }

    inline Coeffs lowpass (double hz, double sr, double q = 0.7071) noexcept
    {
        const double w = 2.0 * M_PI * belowNyquist (hz, sr) / sr;
        const double cw = std::cos (w), sw = std::sin (w);
        const double alpha = sw / (2.0 * q);

        return normalise ((1.0 - cw) * 0.5, 1.0 - cw, (1.0 - cw) * 0.5,
                          1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }

    inline Coeffs bandpass (double hz, double sr, double q) noexcept
    {
        const double w = 2.0 * M_PI * belowNyquist (hz, sr) / sr;
        const double cw = std::cos (w), sw = std::sin (w);
        const double alpha = sw / (2.0 * q);

        return normalise (alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }

    inline Coeffs peaking (double hz, double sr, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * M_PI * belowNyquist (hz, sr) / sr;
        const double cw = std::cos (w), sw = std::sin (w);
        const double alpha = sw / (2.0 * q);

        return normalise (1.0 + alpha * A, -2.0 * cw, 1.0 - alpha * A,
                          1.0 + alpha / A, -2.0 * cw, 1.0 - alpha / A);
    }

    inline Coeffs lowShelf (double hz, double sr, double gainDb, double s = 1.0) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * M_PI * belowNyquist (hz, sr) / sr;
        const double cw = std::cos (w), sw = std::sin (w);
        const double alpha = sw * 0.5 * std::sqrt ((A + 1.0 / A) * (1.0 / s - 1.0) + 2.0);
        const double t = 2.0 * std::sqrt (A) * alpha;

        return normalise (A * ((A + 1.0) - (A - 1.0) * cw + t),
                          2.0 * A * ((A - 1.0) - (A + 1.0) * cw),
                          A * ((A + 1.0) - (A - 1.0) * cw - t),
                          (A + 1.0) + (A - 1.0) * cw + t,
                          -2.0 * ((A - 1.0) + (A + 1.0) * cw),
                          (A + 1.0) + (A - 1.0) * cw - t);
    }

    inline Coeffs highShelf (double hz, double sr, double gainDb, double s = 1.0) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * M_PI * belowNyquist (hz, sr) / sr;
        const double cw = std::cos (w), sw = std::sin (w);
        const double alpha = sw * 0.5 * std::sqrt ((A + 1.0 / A) * (1.0 / s - 1.0) + 2.0);
        const double t = 2.0 * std::sqrt (A) * alpha;

        return normalise (A * ((A + 1.0) + (A - 1.0) * cw + t),
                          -2.0 * A * ((A - 1.0) + (A + 1.0) * cw),
                          A * ((A + 1.0) + (A - 1.0) * cw - t),
                          (A + 1.0) - (A - 1.0) * cw + t,
                          2.0 * ((A - 1.0) - (A + 1.0) * cw),
                          (A + 1.0) - (A - 1.0) * cw - t);
    }

    /** Waskie wyciecie — do przydzwieku sieciowego 50 Hz i jego harmonicznych. */
    inline Coeffs notch (double hz, double sr, double q = 30.0) noexcept
    {
        const double w = 2.0 * M_PI * belowNyquist (hz, sr) / sr;
        const double cw = std::cos (w), sw = std::sin (w);
        const double alpha = sw / (2.0 * q);

        return normalise (1.0, -2.0 * cw, 1.0, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }
}

} // namespace mix
