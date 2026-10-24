/*
    Tweaks.h — tor miksu: reczne poprawki na wierzchu automatu.

    ---------------------------------------------------------------------------
    SKAD TO SIE WZIELO

    Adrian 24.10: "podczas miksu nie widze toru wtyczek i mozliwosci robienia
    zmian". Program ma zostac prosty, wiec nie ma tu pokretel z liczbami —
    kazdy modul lancucha ma tylko dwie rzeczy:

      - wlacznik (modul pracuje albo nie),
      - jeden suwak "mniej <-> wiecej" od -1 do +1; 0 to dokladnie to, co
        wybral automat (Decide.h) dla tego nagrania i brzmienia.

    Suwak skaluje to, co automat ustawil, zamiast podawac wlasne liczby:
    +1 to dwa razy tyle (dB, stopien kompresji ponad 1:1), -1 to polowa.
    Dzieki temu poprawka pasuje do kazdego nagrania i kazdego brzmienia.

    Modul, ktorego automat w tym brzmieniu nie uzyl (np. korektor w
    "Delikatnym"), mozna wlaczyc recznie (Adrian 24.10: "zostawilbym
    mozliwosc wlaczenia"). Startuje wtedy z ustawien "Studyjnego"
    (enableStage), a suwak dziala od nich. Rezonanse trzeba zmierzyc na
    nagraniu — TakeReview podaje je w TweakContext. Glosnosc i limiter nie sa
    na liscie — pilnuja normy, nie brzmienia.

    Czysta funkcja na SpeechChain::Params, bez stanu i bez JUCE: test
    Tests/tor-test.cpp sprawdza ja bez okna.
*/
#pragma once
#include "SpeechChain.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace mix
{

enum class Stage
{
    Filtr, Korektor, Odszumianie, Oddechy, Ekspander, Kompresja, Rezonanse, DeEsser, Barwa,
    count
};

constexpr int stageCount = (int) Stage::count;

struct Tweaks
{
    std::array<bool,  stageCount> off {};      ///< true: modul wylaczony recznie
    std::array<float, stageCount> amount {};   ///< -1 (mniej) .. +1 (wiecej), 0 = jak automat
    std::array<bool,  stageCount> forced {};   ///< true: wlaczony recznie, choc automat go nie uzyl

    bool isDefault() const noexcept
    {
        for (int i = 0; i < stageCount; ++i)
            if (off[(size_t) i] || forced[(size_t) i] || std::abs (amount[(size_t) i]) > 1e-4f) return false;
        return true;
    }
    bool anyForced() const noexcept
    {
        for (bool f : forced) if (f) return true;
        return false;
    }
    bool operator== (const Tweaks& o) const noexcept { return off == o.off && amount == o.amount && forced == o.forced; }
    bool operator!= (const Tweaks& o) const noexcept { return ! (*this == o); }
};

inline const char* stageName (Stage s)
{
    switch (s)
    {
        case Stage::Filtr:       return "Filtr dołu";
        case Stage::Korektor:    return "Korektor";
        case Stage::Odszumianie: return "Odszumianie";
        case Stage::Oddechy:     return "Oddechy";
        case Stage::Ekspander:   return "Ekspander";
        case Stage::Kompresja:   return "Kompresja";
        case Stage::Rezonanse:   return "Rezonanse";
        case Stage::DeEsser:     return "De-esser";
        case Stage::Barwa:       return "Barwa";
        default:                 return "";
    }
}

/** Jedno zdanie: co modul robi i co znaczy "wiecej". */
inline const char* stageHelp (Stage s)
{
    switch (s)
    {
        case Stage::Filtr:       return "Usuwa dudnienie i stuki poniżej głosu. Więcej = cięcie wyżej.";
        case Stage::Korektor:    return "Zabiera „błoto” i „pudełko” z głosu. Więcej = głębsze cięcia.";
        case Stage::Odszumianie: return "Ścisza szum tła. Więcej = mocniej, ale głos może brzmieć sztucznie.";
        case Stage::Oddechy:     return "Ścisza oddechy między słowami. Więcej = ciszej.";
        case Stage::Ekspander:   return "Ścisza tło w pauzach. Więcej = ciszej w pauzach.";
        case Stage::Kompresja:   return "Wyrównuje ciche i głośne fragmenty. Więcej = gęściej, jak w radiu.";
        case Stage::Rezonanse:   return "Wąsko ścina dzwoniące pasma. Więcej = głębiej.";
        case Stage::DeEsser:     return "Łagodzi ostre „s”, „sz”, „c”. Więcej = mocniej.";
        case Stage::Barwa:       return "Obecność i powietrze w głosie. Więcej = jaśniej i bliżej.";
        default:                 return "";
    }
}

/** Czy automat uzyl modulu w tych ustawieniach (przed poprawkami). */
inline bool stageUsed (const SpeechChain::Params& p, Stage s)
{
    switch (s)
    {
        case Stage::Filtr:       return p.highpassHz > 0.0;
        case Stage::Korektor:    return ! p.preEq.empty();
        case Stage::Odszumianie: return p.useDenoise;
        case Stage::Oddechy:     return p.useBreath;
        case Stage::Ekspander:   return p.useGate;
        case Stage::Kompresja:   return p.useRider || p.levelerRatio > 1.0f || p.peakRatio > 1.0f;
        case Stage::Rezonanse:   return ! p.notchEq.empty();
        case Stage::DeEsser:     return p.useDeEsser || p.useTame;
        case Stage::Barwa:       return p.lowDb != 0.0f || p.mudDb != 0.0f || p.presenceDb != 0.0f || p.airDb != 0.0f;
        default:                 return false;
    }
}

/** Co trzeba zmierzyc na nagraniu, zeby wlaczyc modul recznie. */
struct TweakContext
{
    /** Waskie piki 2-7 kHz (Hz, ile wystaja w dB) — Decide::resonances. */
    std::vector<std::pair<double, double>> resonances;
};

/** Wlacza modul, ktorego automat nie uzyl, z ustawieniami "Studyjnego"
    (Decide::studio). Rezonanse tylko te, ktore zmierzono — gdy nic nie
    wystaje, modul zostaje bez wyciec. */
inline void enableStage (SpeechChain::Params& p, Stage s, const TweakContext& c)
{
    using Bell = SpeechChain::Params::Bell;
    switch (s)
    {
        case Stage::Filtr:       p.highpassHz = 75.0; p.steepHighpass = true; break;
        case Stage::Korektor:    p.preEq = { Bell { 200.0, 1.2, -2.5f }, Bell { 380.0, 1.4, -3.0f }, Bell { 800.0, 2.0, -1.5f } }; break;
        case Stage::Odszumianie: p.useDenoise = true; p.denoiseDb = -10.0f; break;
        case Stage::Oddechy:     p.useBreath = true; p.breathDb = -12.0f; break;
        case Stage::Ekspander:   // prog liczy lancuch z nagrania (autoGate), lagodnie jak w studyjnym
            p.useGate = true; p.autoGate = true; p.gateRatio = 2.0f; p.gateFloorDb = -18.0f;
            p.gateAttackMs = 10.0; p.gateHoldMs = 120.0; p.gateReleaseMs = 300.0; p.gateLookaheadMs = 2.0;
            break;
        case Stage::Kompresja:   p.levelerRatio = 2.0f; break;
        case Stage::Rezonanse:
            p.notchEq.clear();
            for (const auto& r : c.resonances)
                p.notchEq.push_back (Bell { r.first, 6.0, (float) -std::clamp (r.second - 1.0, 2.0, 5.0) });
            break;
        case Stage::DeEsser:     p.useDeEsser = true; p.essShelfHz = 5500.0; p.essMaxCutDb = -3.0f; break;
        case Stage::Barwa:       p.presenceHz = 3500.0; p.presenceQ = 1.0; p.presenceDb = 1.5f; p.airHz = 9000.0; p.airDb = 4.0f; break;
        default: break;
    }
}

/** -1..+1 -> 0,5..2 */
inline float tweakScale (float amount) { return std::pow (2.0f, std::clamp (amount, -1.0f, 1.0f)); }

/** Nakłada poprawki na ustawienia automatu. Modul nieuzywany zostaje bez
    zmian, chyba ze wlaczono go recznie (forced). */
inline void applyTweaks (SpeechChain::Params& p, const Tweaks& t, const TweakContext& c = {})
{
    for (int i = 0; i < stageCount; ++i)
    {
        const auto s = (Stage) i;
        bool off = t.off[(size_t) i];
        if (! stageUsed (p, s))
        {
            if (! t.forced[(size_t) i]) continue;
            enableStage (p, s, c);
            if (! stageUsed (p, s)) continue;      // rezonanse: nic nie wystaje
            off = false;
        }
        const float k = tweakScale (t.amount[(size_t) i]);
        const auto ratio = [k] (float r) { return r > 1.0f ? std::clamp (1.0f + (r - 1.0f) * k, 1.0f, 20.0f) : r; };

        switch (s)
        {
            case Stage::Filtr:
                // wylaczony: zostaje 30 Hz, zeby nie przepuszczac stalej skladowej i tapniec
                if (off) { p.highpassHz = 30.0; p.steepHighpass = false; }
                else p.highpassHz = std::clamp (p.highpassHz * std::pow (1.5, (double) t.amount[(size_t) i]), 40.0, 160.0);
                break;
            case Stage::Korektor:
                if (off) p.preEq.clear();
                else for (auto& b : p.preEq) b.db = std::clamp (b.db * k, -12.0f, 12.0f);
                break;
            case Stage::Odszumianie:
                if (off) p.useDenoise = false;
                else p.denoiseDb = std::clamp (p.denoiseDb * k, -24.0f, -2.0f);
                break;
            case Stage::Oddechy:
                if (off) p.useBreath = false;
                else p.breathDb = std::clamp (p.breathDb * k, -24.0f, -3.0f);
                break;
            case Stage::Ekspander:
                if (off) p.useGate = false;
                else p.gateFloorDb = std::clamp (p.gateFloorDb * k, -30.0f, -3.0f);
                break;
            case Stage::Kompresja:
                if (off) { p.useRider = false; p.levelerRatio = 1.0f; p.peakRatio = 1.0f; }
                else { p.levelerRatio = ratio (p.levelerRatio); p.peakRatio = ratio (p.peakRatio); }
                break;
            case Stage::Rezonanse:
                if (off) p.notchEq.clear();
                else for (auto& b : p.notchEq) b.db = std::clamp (b.db * k, -9.0f, 0.0f);
                break;
            case Stage::DeEsser:
                if (off) { p.useDeEsser = false; p.useTame = false; }
                else
                {
                    p.essMaxCutDb  = std::clamp (p.essMaxCutDb * k, -15.0f, -1.0f);
                    p.tameMaxCutDb = std::clamp (p.tameMaxCutDb * k, -15.0f, -1.0f);
                }
                break;
            case Stage::Barwa:
                if (off) p.lowDb = p.mudDb = p.presenceDb = p.airDb = 0.0f;
                else for (float* v : { &p.lowDb, &p.mudDb, &p.presenceDb, &p.airDb }) *v = std::clamp (*v * k, -9.0f, 9.0f);
                break;
            default: break;
        }
    }
}

namespace tweakText
{
    /** "2,5" — przecinek, jak w calym programie. */
    inline std::string num (double v, int decimals = 1)
    {
        char b[32];
        std::snprintf (b, sizeof (b), "%.*f", decimals, v);
        std::string s (b);
        for (auto& c : s) if (c == '.') c = ',';
        if (decimals > 0 && s.size() > 2 && s.substr (s.size() - 2) == ",0") s.resize (s.size() - 2);
        return s;
    }

    /** 1 pasmo, 2 pasma, 5 pasm */
    inline std::string count (size_t n, const char* one, const char* few, const char* many)
    {
        const size_t d = n % 10, dd = n % 100;
        const char* w = n == 1 ? one : (d >= 2 && d <= 4 && (dd < 12 || dd > 14)) ? few : many;
        return std::to_string (n) + " " + w;
    }
}

/** Krotko, ile modul robi w tych (juz poprawionych) ustawieniach. */
inline std::string stageValue (const SpeechChain::Params& p, Stage s)
{
    using tweakText::num;
    switch (s)
    {
        case Stage::Filtr:       return num (p.highpassHz, 0) + " Hz";
        case Stage::Korektor:
        {
            float deepest = 0.0f;
            for (const auto& b : p.preEq) deepest = std::min (deepest, b.db);
            return tweakText::count (p.preEq.size(), "pasmo", "pasma", "pasm") + ", do " + num (-deepest) + " dB";
        }
        case Stage::Odszumianie: return "do " + num (-p.denoiseDb, 0) + " dB";
        case Stage::Oddechy:     return "ciszej o " + num (-p.breathDb, 0) + " dB";
        case Stage::Ekspander:   return "do " + num (-p.gateFloorDb, 0) + " dB";
        case Stage::Kompresja:
            return p.peakRatio > 1.0f ? num (p.levelerRatio) + ":1 i " + num (p.peakRatio) + ":1"
                                      : num (p.levelerRatio) + ":1";
        case Stage::Rezonanse:   return p.notchEq.empty() ? "nic nie wystaje"
                                                         : tweakText::count (p.notchEq.size(), "wycięcie", "wycięcia", "wycięć");
        case Stage::DeEsser:     return "do " + num (-(p.useDeEsser ? p.essMaxCutDb : p.tameMaxCutDb)) + " dB";
        case Stage::Barwa:       return "obecność " + std::string (p.presenceDb >= 0 ? "+" : "") + num (p.presenceDb)
                                      + ", powietrze " + std::string (p.airDb >= 0 ? "+" : "") + num (p.airDb);
        default:                 return {};
    }
}

/** Zdanie do listy "co zrobil miks", gdy sa poprawki; puste bez nich. */
inline std::string describeTweaks (const SpeechChain::Params& decided, const Tweaks& t)
{
    std::string out;
    for (int i = 0; i < stageCount; ++i)
    {
        const auto s = (Stage) i;
        const bool used = stageUsed (decided, s);
        if (! used && ! t.forced[(size_t) i]) continue;
        const float a = t.amount[(size_t) i];
        std::string what;
        if (! used)            what = std::abs (a) > 1e-4f ? (a > 0 ? "włączone, więcej" : "włączone, mniej") : "włączone";
        else if (t.off[(size_t) i]) what = "wyłączone";
        else if (a > 1e-4f)    what = "więcej";
        else if (a < -1e-4f)   what = "mniej";
        else continue;
        std::string name = stageName (s);
        out += (out.empty() ? "" : ", ") + name + " — " + what;
    }
    return out.empty() ? out : "Twoje zmiany w torze miksu: " + out + ".";
}

} // namespace mix
