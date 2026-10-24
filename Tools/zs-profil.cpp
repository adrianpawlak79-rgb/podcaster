/*
    zs-profil — czy wzorzec brzmienia wyrownuje barwe miedzy odcinkami.

      zs-profil wzorzec.wav [inne.wav ...]

    1. Miksuje wzorzec.wav (Studyjny) i zdejmuje z miksu wzorzec brzmienia.
    2. Kazde inne nagranie miksuje dwa razy: bez wzorca i ze wzorcem.
    3. Mierzy, o ile barwa gotowego miksu rozni sie od wzorca (RMS na
       siatce 1/16 oktawy, wagi jak w TiltMatch) — przed i po.

    Bez innych plikow robi je sam z wzorca, jak "inny dzien przy mikrofonie":
    blizej (+5 dB dolu ponizej 150 Hz, ciemniej o 2 dB na 3 kHz) i dalej
    (-4 dB dolu, +3 dB powyzej 5 kHz) — tak zmienia barwe odleglosc od
    dynamicznego mikrofonu (efekt zblizeniowy).

    Budowanie: g++ -std=c++17 -O2 -ISource Tools/zs-profil.cpp -o zs-profil
*/

#include "../Source/DSP/Mix/Decide.h"
#include "../Source/DSP/Mix/SpeechChain.h"
#include "../Source/DSP/Mix/VoiceProfile.h"
#include "../Source/Wav.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static std::vector<float> mixIt (const std::vector<float>& in, double sr, const mix::VoiceProfile& profile, mix::ChainReport* out = nullptr)
{
    mix::Decide::Wish w;
    w.where = mix::Decide::Destination::Podcast;
    w.profile = profile;
    auto d = mix::Decide::listen (in, sr, w);
    mix::SpeechChain chain;
    chain.params = d.params;
    chain.setSampleRate (sr);
    chain.prepare (sr);
    std::vector<float> y;
    const auto rep = chain.render (in, y);
    if (out) *out = rep;
    return y;
}

static double distance (const std::vector<float>& x, double sr, const mix::VoiceProfile& p) { return p.distance (x, sr); }

static std::vector<float> colour (const std::vector<float>& x, double sr, double lowDb, double presDb, double highDb)
{
    mix::Biquad a, b, c;
    a.setCoeffs (mix::design::lowShelf (150.0, sr, lowDb, 0.8));
    b.setCoeffs (mix::design::peaking (3000.0, sr, 1.0, presDb));
    c.setCoeffs (mix::design::highShelf (5000.0, sr, highDb, 0.7));
    std::vector<float> y (x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = c.process (b.process (a.process (x[i])));
    return y;
}

static std::vector<float> mono (const wavio::Audio& a)
{
    std::vector<float> m (a.frames(), 0.0f);
    for (const auto& ch : a.channels) for (size_t i = 0; i < m.size(); ++i) m[i] += ch[i] / (float) a.numChannels();
    return m;
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("uzycie: zs-profil wzorzec.wav [inne.wav ...]\n"); return 2; }

    wavio::Audio a;
    if (auto e = wavio::read (argv[1], a); ! e.empty()) { std::printf ("%s\n", e.c_str()); return 1; }
    const double sr = a.sampleRate;
    const auto base = mono (a);

    const auto baseMix = mixIt (base, sr, {});
    auto profile = mix::VoiceProfile::measure (baseMix, sr);
    profile.label = "wzorzec";
    if (! profile.valid()) { std::printf ("nie da sie zdjac wzorca\n"); return 1; }
    // zapis i odczyt jak w ustawieniach — test przy okazji
    profile = mix::VoiceProfile::fromText (profile.toText());
    std::printf ("wzorzec z %s: %zu punktow, rozrzut wlasny %.2f dB\n\n", argv[1], profile.hz.size(), distance (baseMix, sr, profile));

    struct Case { std::string name; std::vector<float> x; };
    std::vector<Case> cases;
    for (int i = 2; i < argc; ++i)
    {
        wavio::Audio b;
        if (auto e = wavio::read (argv[i], b); ! e.empty()) { std::printf ("%s\n", e.c_str()); continue; }
        if (b.sampleRate != a.sampleRate) { std::printf ("%s: inna czestotliwosc, pomijam\n", argv[i]); continue; }
        cases.push_back ({ argv[i], mono (b) });
    }
    if (cases.empty())
    {
        cases.push_back ({ "blizej mikrofonu (+5 dB dol, -2 dB 3 kHz)", colour (base, sr, 5.0, -2.0, 0.0) });
        cases.push_back ({ "dalej od mikrofonu (-4 dB dol, +3 dB gora)", colour (base, sr, -4.0, 0.0, 3.0) });
        cases.push_back ({ "ten sam (kontrola)", base });
    }

    std::printf ("%-46s %10s %10s   %s\n", "nagranie", "bez wz.", "ze wzorcem", "korekta: 300 Hz / 500 / 1400 / 5 kHz");
    for (const auto& c : cases)
    {
        const double before = distance (mixIt (c.x, sr, {}), sr, profile);
        mix::ChainReport rep;
        const auto withMix = mixIt (c.x, sr, profile, &rep);
        const double after = distance (withMix, sr, profile);
        std::printf ("%-46s %8.2f dB %8.2f dB   %+.1f / %+.1f / %+.1f / %+.1f%s\n", c.name.c_str(), before, after,
                     rep.profileDb[0], rep.profileDb[1], rep.profileDb[2], rep.profileDb[3], rep.profileUsed ? "" : "  (bez zmian)");
    }
    return 0;
}
