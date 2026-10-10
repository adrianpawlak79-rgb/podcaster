/*
    Fft.h — przeksztalcenie Fouriera w obie strony i analiza ramkowa.

    W Primitives.h jest juz FFT w przod, ale liczy wspolczynniki obrotu
    rekurencyjnie: kazdy nastepny powstaje z poprzedniego przez mnozenie. Blad
    sie w tym kumuluje i przy 2048 punktach schodzi do okolic -70 dB. Do
    wykrywania zdarzen to nie ma znaczenia, bo tam patrzymy na energie w
    pasmach. Do ODSZUMIANIA ma znaczenie decydujace: rozkladamy sygnal, mnozymy
    prazki i skladamy z powrotem, wiec kazdy blad rozkladu slychac w wyniku
    jako szelest. Dlatego tutaj wspolczynniki liczy sie wprost, w podwojnej
    precyzji, raz na rozmiar okna.
*/

#pragma once

#include <cmath>
#include <vector>

namespace mix
{

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

class Fft
{
public:
    void prepare (int size)
    {
        n = size;
        cosT.resize ((size_t) n / 2);
        sinT.resize ((size_t) n / 2);

        for (int k = 0; k < n / 2; ++k)
        {
            cosT[(size_t) k] = std::cos (-2.0 * M_PI * k / n);
            sinT[(size_t) k] = std::sin (-2.0 * M_PI * k / n);
        }
    }

    int size() const noexcept { return n; }

    /** @param inverse  true = z widma na sygnal, ze skalowaniem 1/n */
    void run (std::vector<double>& re, std::vector<double>& im, bool inverse) const
    {
        if (inverse)
            for (int i = 0; i < n; ++i) im[(size_t) i] = -im[(size_t) i];

        for (int i = 1, j = 0; i < n; ++i)
        {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;

            if (i < j)
            {
                std::swap (re[(size_t) i], re[(size_t) j]);
                std::swap (im[(size_t) i], im[(size_t) j]);
            }
        }

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2;
            const int step = n / len;

            for (int i = 0; i < n; i += len)
            {
                for (int k = 0; k < half; ++k)
                {
                    const double wr = cosT[(size_t) (k * step)];
                    const double wi = sinT[(size_t) (k * step)];

                    const int a = i + k, b = i + k + half;
                    const double xr = re[(size_t) b] * wr - im[(size_t) b] * wi;
                    const double xi = re[(size_t) b] * wi + im[(size_t) b] * wr;

                    re[(size_t) b] = re[(size_t) a] - xr;
                    im[(size_t) b] = im[(size_t) a] - xi;
                    re[(size_t) a] += xr;
                    im[(size_t) a] += xi;
                }
            }
        }

        if (inverse)
        {
            const double s = 1.0 / n;

            for (int i = 0; i < n; ++i)
            {
                re[(size_t) i] *= s;
                im[(size_t) i] *= -s;
            }
        }
    }

private:
    int n = 0;
    std::vector<double> cosT, sinT;
};

/** Okno Hanna nalozone przy analizie I przy syntezie.

    Przy skoku o cwierc okna suma kwadratow okien wychodzi stala i rowna 1,5 —
    stad dzielnik. Nakladanie z oknem po obu stronach jest tu konieczne, a nie
    ozdobne: modyfikujemy widmo miedzy rozkladem a skladaniem, wiec ramki
    przestaja do siebie pasowac na stykach. Okno przy syntezie wygasza te
    niezgodnosc; bez niego kazde 512 probek slychac jako pyknięcie. */
inline void hann (std::vector<double>& w, int n)
{
    w.resize ((size_t) n);

    for (int i = 0; i < n; ++i)
        w[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / n);
}

} // namespace mix
