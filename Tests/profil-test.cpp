/*
    profil-test.cpp — wzorzec brzmienia (DSP/Mix/VoiceProfile.h).

    Wzorzec zdjety z miksu nagrania, a potem to samo nagranie "z innego dnia"
    (blizej mikrofonu: wiecej dolu, ciemniej) — miks ze wzorcem ma byc
    wyraznie blizej wzorca niz bez niego. To samo nagranie nie moze dostac
    zadnej korekty, a Delikatny nie uzywa wzorca wcale.

    Zmierzone na nagraniach Adriana (Tools/zs-profil.cpp, 22.10): blizej
    1,54 -> 0,72 dB, dalej 2,22 -> 1,02 dB, dwie rozne sesje 2,41 -> 1,38
    i 2,14 -> 1,50 dB, to samo nagranie 0 -> 0 (bez korekty).
*/

#include "../Source/DSP/Mix/Decide.h"
#include "../Source/DSP/Mix/SpeechChain.h"
#include "../Source/DSP/Mix/VoiceProfile.h"
#include "../Source/Wav.h"

#include <cmath>
#include <cstdio>
#include <string>

static int fails = 0;
static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static std::vector<float> mixIt (const std::vector<float>& in, double sr, const mix::VoiceProfile& p,
                                 mix::ChainReport& rep, mix::Decide::Style style = mix::Decide::Style::Studyjny)
{
    mix::Decide::Wish w;
    w.where = mix::Decide::Destination::Podcast;
    w.style = style;
    w.profile = p;
    const auto d = mix::Decide::listen (in, sr, w);
    mix::SpeechChain chain;
    chain.params = d.params;
    chain.setSampleRate (sr);
    chain.prepare (sr);
    std::vector<float> y;
    rep = chain.render (in, y);
    return y;
}

static double distance (const std::vector<float>& x, double sr, const mix::VoiceProfile& p) { return p.distance (x, sr); }

int main (int argc, char** argv)
{
    std::printf ("\nWzorzec brzmienia\n=================\n\n");
    wavio::Audio a;
    if (argc < 2 || ! wavio::read (argv[1], a).empty()) { std::printf ("brak mowy\n"); return 1; }
    const double sr = a.sampleRate;
    std::vector<float> x;
    for (int k = 0; k < 3; ++k) x.insert (x.end(), a.channels[0].begin(), a.channels[0].end());   // dluzej: stabilniejsze widmo

    mix::ChainReport rep;
    const auto baseMix = mixIt (x, sr, {}, rep);
    auto p = mix::VoiceProfile::measure (baseMix, sr);
    p.label = "test 22.10";
    check (p.valid(), "wzorzec zdjety z miksu", std::to_string (p.hz.size()) + " punktow");

    const auto back = mix::VoiceProfile::fromText (p.toText());
    bool same = back.valid() && back.hz.size() == p.hz.size() && back.label == p.label;
    for (size_t i = 0; same && i < p.hz.size(); ++i) same = std::fabs (back.db[i] - p.db[i]) < 0.006;
    check (same, "zapis i odczyt (ustawienia) bez strat");
    check (! mix::VoiceProfile::fromText ("zepsute").valid() && ! mix::VoiceProfile::fromText ("x|100:1;50:2").valid(),
           "zepsuty zapis = brak wzorca, nie smieci");

    // ten sam material: zadnej korekty
    mixIt (x, sr, p, rep);
    check (! rep.profileUsed, "to samo nagranie: bez korekty",
           std::to_string (rep.profileDb[0]) + " " + std::to_string (rep.profileDb[3]));

    // "blizej mikrofonu": +5 dB dolu, -2 dB na 3 kHz
    std::vector<float> close (x.size());
    {
        mix::Biquad lo, pr;
        lo.setCoeffs (mix::design::lowShelf (150.0, sr, 5.0, 0.8));
        pr.setCoeffs (mix::design::peaking (3000.0, sr, 1.0, -2.0));
        for (size_t i = 0; i < x.size(); ++i) close[i] = pr.process (lo.process (x[i]));
    }
    const double before = distance (mixIt (close, sr, {}, rep), sr, p);
    const auto withP = mixIt (close, sr, p, rep);
    const double after = distance (withP, sr, p);
    char b[96]; std::snprintf (b, sizeof (b), "%.2f -> %.2f dB, dol %+.1f dB", before, after, rep.profileDb[0]);
    check (rep.profileUsed && after < before * 0.75, "blizej mikrofonu: barwa wyraznie blizej wzorca", b);
    bool capped = true;
    for (float g : rep.profileDb) capped = capped && std::fabs (g) <= 3.0f + 1e-4f;
    check (capped, "korekta najwyzej +-3 dB");

    // Delikatny: wzorca nie uzywa
    mixIt (close, sr, p, rep, mix::Decide::Style::Delikatny);
    check (! rep.profileUsed, "Delikatny nie rusza barwy wzorcem");

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
