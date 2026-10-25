/*
    MusicBed.h — glos z podkladem muzycznym w jeden plik (25.10).

    Adrian nagrywa wiersze do bitu. Muzyka jest nieruszalna: gra od probki 0
    nagrania (Recorder), nie tniemy jej i nie przesuwamy, a glos po montazu
    lezy tam, gdzie wypadl w czasie nagrania (BackingEdit.h). Tu tylko
    skladamy:

    1. Dlugosc = dluzsze z dwojga. Muzyka gra do konca, nawet gdy glos
       skonczyl wczesniej; glos po koncu muzyki tez zostaje.
    2. Poziom muzyki wzgledem glosu w LUFS (oba mierzone jako stereo, glos
       jako dual-mono w srodku). Domyslnie muzyka 11 dB pod glosem — jak
       podklad pod czytany tekst; "glosniej" to 6 dB, "ciszej" 16 dB.
    3. Lekkie sciszanie pod glosem (ducking): gdy glos mowi, muzyka schodzi
       o kilka dB (domyslnie 4), powoli (atak 80 ms, powrot 400 ms), zeby
       nie pompowalo. Przerwy w mowie krotsze niz 250 ms nie wracaja muzyki.
    4. Calosc na -16 LUFS (stereo; to ta sama glosnosc co -19 LUFS samego
       glosu w mono) i limiter na -1 dBTP.

    Bez JUCE — test: Tests/podklad-miks-test.cpp.
*/

#pragma once

#include "Limiter.h"
#include "Loudness.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace mix
{

struct MusicBed
{
    struct Params
    {
        float musicDb    = -11.0f;   ///< glosnosc muzyki wzgledem glosu (LUFS)
        bool  duck       = true;     ///< sciszanie pod glosem
        float duckDb     = 4.0f;
        float targetLufs = -16.0f;   ///< calosc, stereo
        float ceilingDb  = -1.0f;    ///< dBTP
    };

    struct Result
    {
        std::vector<float> left, right;
        double lufs = -140.0, peakDbtp = -140.0;
        double musicGainDb = 0.0;    ///< ile podnieslismy/sciszylismy muzyke przed sumowaniem
        bool   musicSilent = false;
    };

    static double lufsOf (const std::vector<float>& l, const std::vector<float>& r, double sr)
    {
        if (l.empty()) return -140.0;
        LoudnessMeter m;
        m.prepare (sr, 2);
        const float* p[] = { l.data(), (r.empty() ? l : r).data() };
        m.process (p, 2, (int) l.size());
        return m.integrated();
    }

    /** Wzmocnienie muzyki w kazdej probce: 1 bez mowy, -duckDb pod mowa.
        Mowa = RMS z 50 ms nie nizej niz 20 dB pod glosnoscia calego glosu. */
    static std::vector<float> duckCurve (const std::vector<float>& voice, size_t len, double sr, double voiceLufs, const Params& p)
    {
        std::vector<float> g (len, 1.0f);
        if (! p.duck || voice.empty() || voiceLufs < -100.0) return g;
        const size_t win = std::max<size_t> (1, (size_t) (0.05 * sr));
        const float thr = (float) std::pow (10.0, (voiceLufs - 20.0) / 20.0);
        const size_t hold = (size_t) (0.25 * sr);
        const float low = (float) std::pow (10.0, -p.duckDb / 20.0);
        const float att = (float) (1.0 - std::exp (-1.0 / (0.080 * sr)));
        const float rel = (float) (1.0 - std::exp (-1.0 / (0.400 * sr)));

        // aktywnosc mowy z wyprzedzeniem pol okna (srodek okna RMS)
        std::vector<char> talk (len, 0);
        double sum = 0.0;
        for (size_t i = 0; i < voice.size(); ++i)
        {
            sum += (double) voice[i] * voice[i];
            if (i >= win) sum -= (double) voice[i - win] * voice[i - win];
            const size_t c = i >= win / 2 ? i - win / 2 : 0;
            if (c < len && std::sqrt (std::max (0.0, sum) / (double) win) > thr) talk[c] = 1;
        }
        // przytrzymanie: krotka przerwa w mowie nie podnosi muzyki
        size_t last = 0; bool any = false;
        for (size_t i = 0; i < len; ++i)
        {
            if (talk[i]) { last = i; any = true; }
            else if (any && i - last < hold) talk[i] = 2;
        }
        // atak liczymy wstecz, zeby zejscie zaczelo sie PRZED pierwsza gloska
        float v = 1.0f;
        for (size_t i = len; i-- > 0;)
        {
            const float target = talk[i] ? low : 1.0f;
            v += (target - v) * att;   // wstecz: zejscie przed mowa, w tym samym tempie co atak
            g[i] = v;
        }
        float u = 1.0f;
        for (size_t i = 0; i < len; ++i)
        {
            const float target = talk[i] ? low : 1.0f;
            u += (target - u) * (target < u ? att : rel);
            g[i] = std::min (g[i], u);
        }
        return g;
    }

    /** voice: zmiksowany glos (mono). musicL/musicR: muzyka w tej samej
        czestotliwosci, od probki 0 nagrania (musicR puste = mono). */
    /** measure = false: bez koncowego pomiaru glosnosci i szczytu (wersja tylko
        do odsluchu). musicLufsKnown: glosnosc muzyki policzona wczesniej (NaN = licz). */
    static Result render (const std::vector<float>& voice, const std::vector<float>& musicL,
                          const std::vector<float>& musicR, double sr, const Params& p,
                          bool measure = true, double musicLufsKnown = std::numeric_limits<double>::quiet_NaN())
    {
        Result res;
        const auto& mR = musicR.empty() ? musicL : musicR;
        const size_t len = std::max (voice.size(), musicL.size());
        if (len == 0 || sr <= 0.0) return res;

        const double voiceLufs = lufsOf (voice, {}, sr);
        const double musicLufs = std::isnan (musicLufsKnown) ? lufsOf (musicL, mR, sr) : musicLufsKnown;
        res.musicSilent = musicLufs < -70.0;
        // Glos prawie cichy (sciszony mikrofon, nikt nie mowil — Adrian 25.10:
        // calosc wyszla -25 LUFS): muzyka nie idzie za takim glosem w dol, tylko
        // zostaje jak jest, a calosc i tak dostaje cel glosnosci nizej.
        const bool voiceSilent = voiceLufs < -55.0;
        const double gDb = res.musicSilent || voiceSilent ? 0.0 : voiceLufs + p.musicDb - musicLufs;
        res.musicGainDb = gDb;
        const float gm = (float) std::pow (10.0, gDb / 20.0);
        auto pd = p; if (voiceSilent) pd.duck = false;
        const auto duck = duckCurve (voice, len, sr, voiceLufs, pd);

        res.left.assign (len, 0.0f);
        res.right.assign (len, 0.0f);
        for (size_t i = 0; i < len; ++i)
        {
            const float v = i < voice.size() ? voice[i] : 0.0f;
            const float k = gm * duck[i];
            res.left[i]  = v + (i < musicL.size() ? musicL[i] * k : 0.0f);
            res.right[i] = v + (i < mR.size()     ? mR[i] * k     : 0.0f);
        }

        // Cel glosnosci liczony tam, gdzie mowi glos. Dawniej z calosci: 45 s
        // glosu i 6 min samej muzyki (cichszej o musicDb) zanizalo pomiar,
        // wiec wszystko szlo w gore o kilka dB - glos -11 LUFS, a z nim
        // telewizor w tle (Adrian 25.10). Ogon samej muzyki zostaje nizej.
        const bool voicePart = ! voiceSilent && voice.size() > (size_t) (3.0 * sr) && voice.size() < len;
        const double now = voicePart
            ? lufsOf (std::vector<float> (res.left.begin(), res.left.begin() + (long) voice.size()),
                      std::vector<float> (res.right.begin(), res.right.begin() + (long) voice.size()), sr)
            : lufsOf (res.left, res.right, sr);
        if (now > -100.0)
        {
            const float g = (float) std::pow (10.0, (p.targetLufs - now) / 20.0);
            for (size_t i = 0; i < len; ++i) { res.left[i] *= g; res.right[i] *= g; }
        }

        // limiter z wyprzedzeniem; opoznienie oddajemy, wiec nic sie nie przesuwa
        Limiter lim;
        lim.params.ceilingDb = p.ceilingDb;
        lim.prepare (sr, 2);
        const size_t look = (size_t) lim.latencySamples();
        res.left.resize (len + look, 0.0f);
        res.right.resize (len + look, 0.0f);
        float* io[] = { res.left.data(), res.right.data() };
        lim.process (io, 2, (int) (len + look));
        res.left.erase (res.left.begin(), res.left.begin() + (long) look);
        res.right.erase (res.right.begin(), res.right.begin() + (long) look);

        if (measure) measureResult (res, sr);
        return res;
    }

    /** Glosnosc i szczyt gotowej calosci (TruePeak to polowa czasu skladania —
        program liczy to dopiero przy zapisie). */
    static void measureResult (Result& res, double sr)
    {
        if (res.left.empty()) return;
        res.lufs = lufsOf (res.left, res.right, sr);
        TruePeak tp;
        tp.prepare (sr, 2);
        const float* in[] = { res.left.data(), res.right.data() };
        tp.process (in, 2, (int) res.left.size());
        res.peakDbtp = tp.dbTP();
    }
};

} // namespace mix
