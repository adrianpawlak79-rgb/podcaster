/*
    Colour.h — nasycenie, dither i przeprobkowanie. Trzy rzeczy z zupelnie
    roznych polek, ktore laczy jedno: wszystkie dotykaja probek na najnizszym
    poziomie i wszystkie latwo zrobic zle.
*/

#pragma once

#include "Biquad.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mix
{

//==============================================================================
/** Nasycenie — ta roznica, przez ktora nagranie brzmi jak nagranie, a nie jak
    komunikator.

    Miekkie zaokraglenie szczytow dokłada harmoniczne, ktorych w cyfrowej
    ciszy nie ma. Ucho czyta je jako "cieplo" i "gestosc", a technicznie jest to
    po prostu znieksztalcenie — tyle ze przewidywalne i sterowane.

    NADPROBKOWANIE JEST TU OBOWIAZKOWE, nie opcjonalne. Nieliniowosc rodzi
    harmoniczne powyzej polowy czestotliwosci probkowania, a te nie maja dokad
    pojsc: odbijaja sie od sufitu i wracaja w dol widma jako skladowe
    NIEHARMONICZNE — czyli takie, ktore z niczym w dzwieku nie wspolgraja.
    Slychac to jako metaliczny posmak przy glosnych sylabach. Dlatego liczymy
    na podwojnej czestotliwosci i wracamy przez filtr.

    Asymetria dodaje drugiej harmonicznej. Pierwsza harmoniczna parzysta brzmi
    "lampowo", nieparzyste — "tranzystorowo". Do mowy chcemy tej pierwszej.
*/
/** Nadprobkowanie dwukrotne filtrem o skonczonej odpowiedzi.

    Pierwsza wersja uzywala do tego dwoch biquadow — filtru czwartego rzedu o
    czestotliwosci granicznej tuz pod polowa czestotliwosci probkowania. Wydawalo
    sie, ze wystarczy. Pomiar pokazal, ze nie: przy tonie 1 kHz i WYLACZONYM
    nasyceniu, czyli gdy modul nie ma prawa nic zmieniac, na 2 kHz siedzialo
    -33 dB smieci. Braly sie z odbicia obrazu widmowego, ktorego filtr
    czwartego rzedu nie zdazyl stlumic: obraz lezy okolo 47 kHz, granica na
    21,6 kHz, a to raptem nieco ponad oktawa — dwadziescia kilka decybeli.

    Filtr o skonczonej odpowiedzi z oknem Kaisera daje w tym samym miejscu
    ponad 90 dB i ma liniowa faze, wiec nie rozmazuje zwarc.
*/
class Halfband
{
public:
    void prepare (int tapsPerPhase = 32)
    {
        perPhase = tapsPerPhase;
        const int total = perPhase * 2;

        h.assign ((size_t) total, 0.0f);

        for (int i = 0; i < total; ++i)
        {
            const double x = (double) i - (total - 1) / 2.0;
            const double s = std::fabs (x) < 1.0e-9 ? 0.5
                           : std::sin (M_PI * 0.5 * x) / (M_PI * x);

            const double r = 2.0 * i / (double) (total - 1) - 1.0;
            h[(size_t) i] = (float) (s * kaiser (r, 9.0) * 2.0);
        }

        // kazda faza osobno przepuszcza stala skladowa 1:1
        for (int p = 0; p < 2; ++p)
        {
            double sum = 0.0;
            for (int k = 0; k < perPhase; ++k) sum += h[(size_t) (2 * k + p)];

            if (std::fabs (sum) > 1.0e-9)
                for (int k = 0; k < perPhase; ++k) h[(size_t) (2 * k + p)] /= (float) sum;
        }

        upHist.assign ((size_t) perPhase, 0.0f);
        downHist.assign ((size_t) (perPhase * 2), 0.0f);
        upPos = downPos = 0;
    }

    void reset()
    {
        std::fill (upHist.begin(), upHist.end(), 0.0f);
        std::fill (downHist.begin(), downHist.end(), 0.0f);
        upPos = downPos = 0;
    }

    /** Jedna probka wchodzi, dwie wychodza. */
    inline void up (float x, float& y0, float& y1) noexcept
    {
        upHist[(size_t) upPos] = x;
        upPos = (upPos + 1) % perPhase;

        float a = 0.0f, b = 0.0f;

        for (int k = 0; k < perPhase; ++k)
        {
            const int idx = (upPos + perPhase - 1 - k) % perPhase;
            a += h[(size_t) (2 * k)]     * upHist[(size_t) idx];
            b += h[(size_t) (2 * k + 1)] * upHist[(size_t) idx];
        }

        y0 = a;
        y1 = b;
    }

    /** Dwie probki wchodza, jedna wychodzi. */
    inline float down (float y0, float y1) noexcept
    {
        const int n = perPhase * 2;

        downHist[(size_t) downPos] = y0; downPos = (downPos + 1) % n;
        downHist[(size_t) downPos] = y1; downPos = (downPos + 1) % n;

        float acc = 0.0f;

        for (int k = 0; k < n; ++k)
        {
            const int idx = (downPos + n - 1 - k) % n;
            acc += h[(size_t) k] * downHist[(size_t) idx];
        }

        return acc * 0.5f;
    }

    int latencySamples() const noexcept { return perPhase - 1; }

private:
    static double bessel0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int i = 1; i < 40; ++i) { term *= (x / 2.0) / i; sum += term * term; }
        return sum;
    }

    static double kaiser (double r, double beta)
    {
        if (r <= -1.0 || r >= 1.0) return 0.0;
        return bessel0 (beta * std::sqrt (1.0 - r * r)) / bessel0 (beta);
    }

    int perPhase = 32, upPos = 0, downPos = 0;
    std::vector<float> h, upHist, downHist;
};

//==============================================================================
class Saturator
{
public:
    struct Params
    {
        float driveDb   = 3.0f;
        float amount    = 0.5f;    ///< 0 = czysto, 1 = pelne nasycenie
        float asymmetry = 0.2f;    ///< 0 = same nieparzyste, wyzej = wiecej drugiej
    };

    Params params;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        os.prepare (32);
        dcBlock.setCoeffs (design::highpass (12.0, sr, 0.7071));
        reset();
    }

    void reset()
    {
        os.reset();
        dcBlock.reset();
    }

    inline float process (float x) noexcept
    {
        const float drive = dbToGainL (params.driveDb);

        float a = 0.0f, b = 0.0f;
        os.up (x * drive, a, b);

        const float y = os.down (shape (a), shape (b));

        // Nasycenie asymetryczne przesuwa srednia — bez odciecia skladowej
        // stalej ta zmiana sumuje sie w calym lancuchu i zjada zapas do sufitu.
        return dcBlock.process (y / drive);
    }

    int latencySamples() const noexcept { return os.latencySamples(); }

private:
    inline float shape (float v) const noexcept
    {
        if (params.amount <= 0.0f) return v;

        const float asym = v > 0.0f ? 1.0f : (1.0f - params.asymmetry);
        const float t = std::tanh (v * asym);
        return v + params.amount * (t - v);
    }

    static float dbToGainL (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

    double sr = 48000.0;
    Halfband os;
    Biquad dcBlock;
};

//==============================================================================
/** Dither z ksztaltowaniem szumu.

    Przy skracaniu z 32 bitow do 16 blad zaokraglenia jest SKORELOWANY z
    sygnalem — a skorelowany blad ucho slyszy jako znieksztalcenie, nie jako
    szum. Slychac to na wybrzmieniach: cichy ogon zaczyna chrobotac.

    Dither to dodanie odrobiny szumu PRZED zaokragleniem. Blad przestaje byc
    zwiazany z sygnalem i zamienia sie w rowny, cichy syk, ktory ucho odrzuca.

    Rozklad trojkatny (suma dwoch rownomiernych) jest tu wlasciwy: przy
    rozkladzie rownomiernym glosnosc szumu nadal zalezy od sygnalu.

    Ksztaltowanie przesuwa ten szum tam, gdzie ucho jest najmniej czule —
    powyzej 10 kHz. Sumaryczna moc szumu rosnie, a slyszalna spada.
*/
class Dither
{
public:
    void reset() { e1 = e2 = 0.0f; state = 22222; }

    /** @param bits  docelowa rozdzielczosc, zwykle 16 albo 24 */
    inline float process (float x, int bits) noexcept
    {
        const float scale = (float) ((1u << (bits - 1)) - 1);
        const float step = 1.0f / scale;

        // ksztaltowanie drugiego rzedu
        const float shaped = x + 2.0f * e1 - e2;

        const float tri = (rnd() + rnd() - 1.0f) * step;
        const float dithered = shaped + tri;

        const float out = std::round (dithered * scale) / scale;

        e2 = e1;
        e1 = out - dithered;

        return out;
    }

private:
    inline float rnd() noexcept
    {
        state = state * 1664525u + 1013904223u;
        return (float) (state >> 8) / 16777216.0f;
    }

    uint32_t state = 22222;
    float e1 = 0.0f, e2 = 0.0f;
};

//==============================================================================
/** Przeprobkowanie miedzy 44,1 a 48 kHz.

    Ludzie przynosza pomieszane pliki: podklad z Suno w 44,1, nagranie z
    interfejsu w 48. Bez tego program po prostu sie wyloży albo — gorzej —
    odtworzy jedno z nich za szybko i nikt nie zrozumie dlaczego.

    Filtr polifazowy z jadrem sinc w oknie Kaisera. Liczba odczepow na faze
    decyduje o stromosci: 32 daje tlumienie okolo 90 dB, czyli grubo ponizej
    szumu kazdego mikrofonu.
*/
class Resampler
{
public:
    /** @param taps  odczepy na faze; wiecej = ostrzejszy filtr i wolniej */
    void prepare (int fromRate, int toRate, int taps = 32)
    {
        const int g = gcd (fromRate, toRate);
        up = toRate / g;
        down = fromRate / g;
        perPhase = taps;

        const double cutoff = 0.5 / (double) std::max (up, down);
        const int total = perPhase * up;

        kernel.assign ((size_t) total, 0.0f);

        for (int i = 0; i < total; ++i)
        {
            const double x = (double) i - total / 2.0;
            const double s = std::fabs (x) < 1.0e-9 ? 2.0 * cutoff
                           : std::sin (2.0 * M_PI * cutoff * x) / (M_PI * x);

            const double r = 2.0 * i / (double) (total - 1) - 1.0;
            const double w = kaiser (r, 8.6);

            kernel[(size_t) i] = (float) (s * w * up);
        }

        // kazda faza z osobna ma przepuszczac stala skladowa 1:1
        for (int p = 0; p < up; ++p)
        {
            double sum = 0.0;
            for (int t = 0; t < perPhase; ++t)
            {
                const int idx = t * up + p;
                if (idx < total) sum += kernel[(size_t) idx];
            }

            if (std::fabs (sum) > 1.0e-9)
                for (int t = 0; t < perPhase; ++t)
                {
                    const int idx = t * up + p;
                    if (idx < total) kernel[(size_t) idx] /= (float) sum;
                }
        }
    }

    void render (const std::vector<float>& in, std::vector<float>& out) const
    {
        const size_t n = in.size();
        out.clear();
        out.reserve (n * (size_t) up / (size_t) down + 8);

        const int total = perPhase * up;

        const int64_t length=(int64_t)std::llround(double(n)*up/down);
        for (int64_t m = 0;m<length; ++m)
        {
            const int64_t idx = m * down;
            const int64_t base = idx / up;
            const int phase = (int) (idx % up);


            float acc = 0.0f;

            for (int t = 0; t < perPhase; ++t)
            {
                const int64_t s = base - t + perPhase / 2;
                const int k = t * up + phase;

                if (s >= 0 && s < (int64_t) n && k < total)
                    acc += kernel[(size_t) k] * in[(size_t) s];
            }

            out.push_back (acc);
        }
    }

    int upFactor()   const noexcept { return up; }
    int downFactor() const noexcept { return down; }

private:
    static int gcd (int a, int b) { while (b) { const int t = a % b; a = b; b = t; } return a; }

    static double bessel0 (double x)
    {
        double sum = 1.0, term = 1.0;

        for (int i = 1; i < 40; ++i)
        {
            term *= (x / 2.0) / i;
            sum += term * term;
        }

        return sum;
    }

    static double kaiser (double r, double beta)
    {
        if (r <= -1.0 || r >= 1.0) return 0.0;
        return bessel0 (beta * std::sqrt (1.0 - r * r)) / bessel0 (beta);
    }

    int up = 1, down = 1, perPhase = 32;
    std::vector<float> kernel;
};

} // namespace mix
