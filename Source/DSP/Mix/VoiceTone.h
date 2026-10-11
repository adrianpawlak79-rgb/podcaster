/*
    VoiceTone.h — barwa glosu zmierzona z nagrania, zamiast stalych liczb.

    ---------------------------------------------------------------------------
    PO CO

    Korektor barwy mial na sztywno -2 dB na 250 Hz, +1,5 dB na 3 kHz i +1 dB
    powyzej 10 kHz. Dla kazdego glosu, kazdego mikrofonu i kazdego pokoju to
    samo. Glos nagrany za blisko (dudniacy dol) dostawal tyle samo co glos
    nagrany z daleka, a ciemny mikrofon tyle samo co jasny.

    Teraz program mierzy, ile energii glos ma w czterech pasmach wzgledem
    pasma 1 kHz, porownuje to ze srednim widmem mowy i poprawia POLOWE
    roznicy, ktora wychodzi poza margines. Polowe, bo srednie widmo to
    srednia, a nie wzor: kazdy glos ma prawo sie od niej roznic. Poprawiamy
    tylko to, co odstaje wyraznie.

    ---------------------------------------------------------------------------
    PASMA (moc oktawy wzgledem oktawy 710-1410 Hz)

        dol        80-180 Hz      efekt zblizeniowy, dudnienie
        bloto      180-355 Hz     "pudelko", maly pokoj
        obecnosc   2,2-4,5 kHz    zrozumialosc, "glos z przodu"
        powietrze  7,1-14 kHz     oddech, szczegol

    Pasma pokrywaja sie z tym, gdzie dzialaja filtry w SpeechChain: polka
    150 Hz, dzwon 250 Hz, dzwon 3 kHz, polka 10 kHz.

    ---------------------------------------------------------------------------
    SKAD WZORZEC — I CZEGO O NIM NIE WIEMY

    Wzorzec to miedzynarodowe srednie widmo mowy (Byrne i in., 1994, JASA
    96(4), doi 10.1121/1.410152), meskie i zenskie razem, przeliczone
    z tercji na oktawy:

        dol +1,6   bloto +5,0   obecnosc -7,9   powietrze -11,9   (dB wzgl. 1 kHz)

    UWAGA: wartosci tercjowe wpisane z pamieci; przy pisaniu nie dalo sie
    pobrac tabeli z artykulu. Do sprawdzenia ze zrodlem. Blad wzorca o 2 dB
    zmienia korekte o 1 dB (poprawiamy polowe) — dlatego margines i polowa.

    Zmierzone przy pisaniu (narzedzie jak measure(), na calych plikach):

        nagranie czlowieka (mp3)   dol +12,4  bloto +6,2  obecnosc  0,0  powietrze  0,0
        espeak pl                  dol -10,8  bloto -2,5  obecnosc -6,6  powietrze -24,7
        espeak pl+f2               dol -14,0  bloto -3,5  obecnosc -7,6  powietrze -23,1
        espeak pl+m3               dol -10,9  bloto -2,6  obecnosc -5,3  powietrze -21,6

    Syntezator nie ma pasma powyzej 11 kHz (nagrywa w 22 kHz), wiec jego
    "powietrze" to brak pasma, nie ciemny glos. Takie nagranie rozpoznaje
    bandwidthHz() i powietrza wtedy nie ruszamy: podbicie pustego pasma
    podnosi tylko szum.
*/

#pragma once

#include "Biquad.h"
#include "RoomCheck.h"
#include "TiltMatch.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mix
{

//==============================================================================
class VoiceTone
{
public:
    enum Band { Low = 0, Mud, Presence, Air, kBands };

    struct Bands
    {
        double db[kBands] {};      ///< moc pasma wzgledem oktawy 1 kHz
        double bandwidthHz = 0.0;  ///< najwyzsza czestotliwosc z trescia
        bool   valid = false;

        bool hasAir() const noexcept { return bandwidthHz >= 12000.0; }
    };

    static constexpr double lo[kBands] = {   80.0,  180.0, 2240.0,  7100.0 };
    static constexpr double hi[kBands] = {  180.0,  355.0, 4470.0, 14100.0 };

    /** Srednie widmo mowy (Byrne 1994), moc oktawy wzgl. 1 kHz. Patrz naglowek. */
    static constexpr double reference[kBands] = { 1.6, 5.0, -7.9, -11.9 };

    //==========================================================================
    /** Barwa glosnych ramek nagrania. Na dlugim nagraniu wystarcza pierwsze
        dwie minuty — barwa glosu nie zmienia sie w trakcie czytania. */
    static Bands measure (const std::vector<float>& x, double sr)
    {
        Bands b;

        const size_t maxLen = (size_t) (sr * 120.0);
        const std::vector<float> part (x.begin(), x.begin() + (long) std::min (maxLen, x.size()));

        const auto sp = TiltMatch::analyse (part, sr);
        if (! sp.valid) return b;

        const double ref = bandPower (sp, 710.0, 1410.0);
        if (ref <= 1.0e-20) return b;

        for (int k = 0; k < kBands; ++k)
            b.db[k] = 10.0 * std::log10 ((bandPower (sp, lo[k], hi[k]) + 1.0e-30) / ref);

        b.bandwidthHz = bandwidth (sp);
        b.valid = true;
        return b;
    }

    /** Odchylenie od wzorca: dodatnie = za duzo w tym pasmie. */
    static double deviation (const Bands& b, int band) noexcept
    {
        return b.db[band] - reference[band];
    }

    /** Polowa tego, co wychodzi poza margines, z przeciwnym znakiem,
        w granicach [cutLimit, boostLimit]. W marginesie: zero. */
    static float correction (double dev, double margin, float cutLimit, float boostLimit) noexcept
    {
        double over = 0.0;
        if (dev >  margin) over = dev - margin;
        if (dev < -margin) over = dev + margin;
        if (over == 0.0) return 0.0f;   // bez "-0,0 dB" w zdaniach

        return (float) std::clamp (-0.5 * over, (double) cutLimit, (double) boostLimit);
    }

    //==========================================================================
    /** O ile ton hz wystaje ponad sasiednie czestotliwosci (+-12%), w dB.
        Ta sama miara co w kreatorze, zeby liczby dalo sie porownac. */
    static double humExcessDb (const std::vector<float>& x, double sr, double hz)
    {
        const double on  = toneDb (x, sr, hz);
        const double off = std::max (toneDb (x, sr, hz * 0.88), toneDb (x, sr, hz * 1.12));
        return on - off;
    }

private:
    static double bandPower (const TiltMatch::Spectrum& s, double from, double to)
    {
        double e = 0.0;
        for (size_t i = 0; i < s.bin.size(); ++i)
        {
            const double hz = (double) i * s.hzPerBin;
            if (hz >= from && hz < to) e += s.bin[i];
        }
        return e;
    }

    /** Najwyzsza czestotliwosc, przy ktorej gestosc widma jest nie wiecej niz
        60 dB pod gestoscia pasma 1 kHz. mp3 obcina na 16 kHz, syntezator na
        11 kHz, mikrofon nie obcina wcale. */
    static double bandwidth (const TiltMatch::Spectrum& s)
    {
        auto density = [&] (double from, double to)
        {
            double e = 0.0; int n = 0;
            for (size_t i = 0; i < s.bin.size(); ++i)
            {
                const double hz = (double) i * s.hzPerBin;
                if (hz >= from && hz < to) { e += s.bin[i]; ++n; }
            }
            return n ? e / n : 0.0;
        };

        const double ref = density (710.0, 1410.0);
        double top = 0.0;

        for (double f = 2000.0; f < s.hzPerBin * (double) s.bin.size(); f *= 1.0905)  // ~1/8 oktawy
            if (density (f, f * 1.0905) > ref * 1.0e-6) top = f * 1.0905;

        return top;
    }

    /** Goertzel z oknem Hanna, bloki 32768 probek, pierwsza minuta. */
    static double toneDb (const std::vector<float>& x, double sr, double hz)
    {
        const size_t len = std::min (x.size(), (size_t) (sr * 60.0));
        const int block = std::min ((int) len, 32768);
        if (block < 8192) return -140.0;

        const double c = 2.0 * std::cos (2.0 * M_PI * hz / sr);
        double total = 0.0;
        int count = 0;

        for (size_t start = 0; start + (size_t) block <= len; start += (size_t) block)
        {
            double s1 = 0.0, s2 = 0.0;

            for (int i = 0; i < block; ++i)
            {
                const double win = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / (block - 1));
                const double s0 = x[start + (size_t) i] * win + c * s1 - s2;
                s2 = s1; s1 = s0;
            }

            total += (s1 * s1 + s2 * s2 - c * s1 * s2) * 4.0 / ((double) block * block);
            ++count;
        }

        return count && total > 1.0e-24 ? 10.0 * std::log10 (total / count) : -140.0;
    }
};

} // namespace mix
