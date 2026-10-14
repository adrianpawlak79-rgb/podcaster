/*
    decide-test.cpp — czy program dobiera ustawienia z tego, co slyszy.

    ---------------------------------------------------------------------------
    CO TU SIE SPRAWDZA

    Nie to, czy ustawienia sa "dobre" — tego zaden test nie rozstrzygnie. Tylko
    to, czy decyzja wynika z materialu: czy nagranie, ktore odjezdza, dostaje
    jazde suwakiem, a nagranie, ktore tylko gestykuluje, jej nie dostaje.

    Kazdy material jest tu zbudowany tak, ze wiadomo z gory, co program ma
    powiedziec. Gdyby rozroznienie gest/wedrowka przestalo dzialac, wysypia sie
    punkty 2 i 3 — i to jest dokladnie ten blad, ktory przez cztery rundy
    odsluchu brzmial jako "przytlumione i za agresywne".
*/

#include "../Source/DSP/Mix/Decide.h"
#include "../Source/Wav.h"

#include <cstdio>
#include <random>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got)
{
    std::printf ("  %-5s %-44s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static std::string num (double v, const char* unit = " dB")
{
    char b[48];
    std::snprintf (b, sizeof (b), "%.1f%s", v, unit);
    return b;
}

/** Mowa z espeaka, powielona do zadanej dlugosci. */
static std::vector<float> speech (const std::vector<float>& src, double sr, double sec)
{
    std::vector<float> out ((size_t) (sr * sec));

    for (size_t i = 0; i < out.size(); ++i)
        out[i] = src[i % src.size()];

    return out;
}

static std::vector<float> filtered (std::vector<float> x, mix::Coeffs c)
{
    mix::Biquad f;
    f.setCoeffs (c);
    for (auto& v : x) v = f.process (v);
    return x;
}

/** Filtr dobrany tak, zeby pasmo odstawalo od wzorca o dokladnie `dev` dB.
    Dzieki temu test nie zalezy od tego, jaki glos dal syntezator. */
template <typename Design>
static std::vector<float> pushBand (const std::vector<float>& x, double sr, int band,
                                    double dev, Design design)
{
    double lo = -30.0, hi = 40.0;
    std::vector<float> y = x;

    for (int i = 0; i < 30; ++i)
    {
        const double g = 0.5 * (lo + hi);
        y = filtered (x, design (g));
        const double got = mix::VoiceTone::deviation (mix::VoiceTone::measure (y, sr), band);
        (got < dev ? lo : hi) = g;
    }
    return y;
}

static void addTone (std::vector<float>& x, double sr, double hz, float amp)
{
    for (size_t i = 0; i < x.size(); ++i)
        x[i] += amp * (float) std::sin (2.0 * M_PI * hz * (double) i / sr);
}

static bool says (const mix::Decision& d, const char* fragment)
{
    for (const auto& s : d.because)
        if (s.find (fragment) != std::string::npos) return true;
    return false;
}

static void addRoom (std::vector<float>& x, float amp, unsigned seed)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> n (0.0f, amp);
    for (auto& v : x) v += n (rng);
}

/** Pelna logika doboru (bez lagodnego profilu) — to, co sprawdzaja sekcje 1-9. */
static mix::Decide::Wish zs_podcast() { mix::Decide::Wish w; w.where = mix::Decide::Destination::Podcast; return w; }
static mix::Decide::Wish full() { mix::Decide::Wish w; w.studio = false; return w; }

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("uzycie: decide-test mowa.wav\n"); return 1; }

    wavio::Audio in;
    if (! wavio::read (argv[1], in).empty()) { std::printf ("brak mowy\n"); return 1; }

    const auto base = in.toMono();
    const double sr = (double) in.sampleRate;

    std::printf ("\nDobor ustawien — czy decyzja wynika z materialu\n");
    std::printf ("===============================================\n\n");

    // ---- 1. nagranie rowne, cisza cyfrowa --------------------------------------
    {
        std::printf ("1. mowa rowna, bez tla\n");

        auto x = speech (base, sr, 70.0);
        const auto d = mix::Decide::listen (x, sr, full());

        check (! d.params.useRider, "bez jazdy suwakiem", num (d.driftDb));
        check (! d.params.useDenoise, "bez odszumiania", "cisza cyfrowa");
        check (d.params.levelerRatio <= 1.6f, "kompresja lekka",
               num (d.params.levelerRatio, ":1"));
    }

    // ---- 2. to samo, ale poziom odjezdza ---------------------------------------
    {
        std::printf ("\n2. to samo z odjazdem 10 dB przez cale nagranie\n");

        auto x = speech (base, sr, 70.0);

        for (size_t i = 0; i < x.size(); ++i)
        {
            const double t = (double) i / (double) x.size();
            x[i] *= (float) std::pow (10.0, (-10.0 * t) / 20.0);
        }

        const auto d = mix::Decide::listen (x, sr, full());

        check (d.params.useRider, "suwak wlaczony", num (d.driftDb));
        check (d.params.riderRangeDb > 5.0f, "zakres odpowiada odjazdowi",
               num (d.params.riderRangeDb));
        check (d.params.riderSlopeDbS < 1.0f, "suwak idzie wolno",
               num (d.params.riderSlopeDbS, " dB/s"));
    }

    // ---- 3. to samo, ale poziom skacze w obie strony ---------------------------
    //
    // TO JEST TEN TEST. Rozrzut poziomu jest tu wiekszy niz w punkcie 2, wiec
    // miara oparta na samym rozrzucie kazalaby sciskac jeszcze mocniej. A to
    // jest wykonanie i ma zostac.
    {
        std::printf ("\n3. to samo, ale glosniej i ciszej na przemian co 8 s\n");

        auto x = speech (base, sr, 70.0);

        for (size_t i = 0; i < x.size(); ++i)
        {
            const double t = (double) i / sr;
            const double db = 6.0 * (std::sin (2.0 * M_PI * t / 16.0) > 0.0 ? 1.0 : -1.0);
            x[i] *= (float) std::pow (10.0, db / 20.0);
        }

        const auto d = mix::Decide::listen (x, sr, full());

        check (! d.params.useRider || d.params.riderRangeDb < 3.0f,
               "suwak wylaczony albo ledwo rusza", num (d.driftDb));
        check (d.sectionDb > 4.0, "wahanie widziane jako budowa nagrania",
               num (d.sectionDb));
    }

    // ---- 4. cichy pokoj w tle --------------------------------------------------
    {
        std::printf ("\n4. to samo z tlem pokoju 30 dB pod mowa\n");

        auto x = speech (base, sr, 70.0);
        addRoom (x, 0.004f, 11);

        const auto d = mix::Decide::listen (x, sr, full());

        check (d.params.useDenoise, "odszumianie wlaczone",
               num (d.speechDb - d.noiseFloorDb) + " zapasu");
        check (d.params.useGate && ! d.params.autoGate,
               "prog bramy policzony przy doborze", num (d.params.gateThreshDb));
        check (d.params.gateThreshDb > d.noiseFloorDb + 2.0
                 && d.params.gateThreshDb < d.speechDb - 10.0,
               "prog lezy miedzy tlem a mowa",
               num (d.noiseFloorDb) + " < " + num (d.params.gateThreshDb)
                 + " < " + num (d.speechDb));
    }

    // ---- 5. cel glosnosci wynika z zapasu szczytowego ---------------------------
    {
        std::printf ("\n5. cel glosnosci\n");

        auto x = speech (base, sr, 70.0);
        const auto d = mix::Decide::listen (x, sr, full());

        const double expected = -1.0 - d.plr;

        check (std::fabs (d.freeTargetLufs - std::max (-23.0, expected)) < 0.3
                 || d.freeTargetLufs <= -22.9,
               "granica = sufit minus zapas", num (d.freeTargetLufs, " LUFS"));

        check (d.params.targetLufs >= -23.0 && d.params.targetLufs <= -14.0,
               "cel w rozsadnych granicach", num (d.params.targetLufs, " LUFS"));
    }

    // ---- 6. kazda decyzja ma zdanie --------------------------------------------
    {
        std::printf ("\n6. czy program tlumaczy sie z kazdej decyzji\n");

        auto x = speech (base, sr, 70.0);
        addRoom (x, 0.004f, 5);

        const auto d = mix::Decide::listen (x, sr, full());

        bool allHaveNumber = true;

        for (const auto& s : d.because)
            if (s.find_first_of ("0123456789") == std::string::npos)
                allHaveNumber = false;

        check (d.because.size() >= 5, "kazdy stopien ma uzasadnienie",
               std::to_string (d.because.size()) + " zdan");
        check (allHaveNumber, "w kazdym zdaniu jest liczba",
               allHaveNumber ? "tak" : "ktoras bez liczby");
    }

    // ---- 7. barwa z pomiaru glosu ----------------------------------------------
    //
    // Ten sam glos, cztery mikrofony/pokoje. Wzorzec stoi w VoiceTone.h; tu nie
    // sprawdzamy, czy wzorzec jest "dobry", tylko czy korekta idzie w strone,
    // ktora odstaje, i czy po calym lancuchu roznica jest mniejsza niz przed.
    {
        std::printf ("\n7. barwa dobrana do glosu\n");

        using mix::VoiceTone;
        namespace ds = mix::design;

        const auto x     = speech (base, sr, 20.0);
        const auto dark  = filtered (x, ds::highShelf (2000.0, sr, -9.0));
        // Syntezator prawie nie ma dolu (dol -13 dB wzgl. 1 kHz, mowa ma +1,6),
        // wiec zeby przekroczyc wzorzec, trzeba dolozyc duzo.
        auto mudAt  = [&] (double g) { return ds::peaking  (250.0,  sr, 1.0, g); };
        auto lowAt  = [&] (double g) { return ds::lowShelf (150.0,  sr, g); };
        const auto boxy  = pushBand (x, sr, VoiceTone::Mud, 8.0, mudAt);
        const auto boomy = pushBand (x, sr, VoiceTone::Low, 8.0, lowAt);

        const auto d0 = mix::Decide::listen (x, sr, full());
        const auto dD = mix::Decide::listen (dark, sr, full());
        const auto dB = mix::Decide::listen (boxy, sr, full());
        const auto dL = mix::Decide::listen (boomy, sr, full());

        char g[160];
        std::snprintf (g, sizeof (g), "dol %.1f bloto %.1f obecnosc %.1f powietrze %.1f",
                       d0.params.lowDb, d0.params.mudDb, d0.params.presenceDb, d0.params.airDb);
        check (d0.tone.valid, "barwa zmierzona", g);

        check (dD.params.presenceDb > d0.params.presenceDb + 0.5f,
               "ciemny mikrofon: wiecej obecnosci",
               num (d0.params.presenceDb) + " -> " + num (dD.params.presenceDb));
        check (std::fabs (dB.params.mudDb + 2.5f) < 0.6f,
               "bloto 8 dB ponad wzorzec: scina 250 Hz o ok. 2,5",
               num (d0.params.mudDb) + " -> " + num (dB.params.mudDb));
        check (std::fabs (dL.params.lowDb + 2.5f) < 0.6f,
               "dol 8 dB ponad wzorzec: scina o ok. 2,5",
               num (d0.params.lowDb) + " -> " + num (dL.params.lowDb));

        bool limits = true;
        for (const auto* d : { &d0, &dD, &dB, &dL })
        {
            const auto& q = d->params;
            if (q.lowDb > 0.0f || q.lowDb < -6.0f || q.mudDb > 0.0f || q.mudDb < -4.0f
                || q.presenceDb < -1.5f || q.presenceDb > 3.0f || q.airDb < -1.5f || q.airDb > 3.0f)
                limits = false;
        }
        check (limits, "korekty w granicach (dol tylko w dol, gora -1,5..+3)", "");

        // Zasada sama w sobie: w marginesie nic, poza nim polowa nadwyzki.
        check (VoiceTone::correction (2.9, 3.0, -6.0f, 3.0f) == 0.0f
                 && std::fabs (VoiceTone::correction (8.0, 3.0, -6.0f, 3.0f) + 2.5f) < 0.01f
                 && std::fabs (VoiceTone::correction (-8.0, 3.0, -6.0f, 3.0f) - 2.5f) < 0.01f
                 && VoiceTone::correction (30.0, 3.0, -6.0f, 3.0f) == -6.0f,
               "polowa tego, co poza marginesem 3 dB, w granicach", "");

        check (! d0.tone.hasAir() && d0.params.airDb == 0.0f && says (d0, "kończy się na"),
               "bez pasma powyzej 12 kHz powietrze nietkniete",
               num (d0.tone.bandwidthHz / 1000.0, " kHz"));

        // Po calym lancuchu: ciemna wersja ma byc blizej jasnej niz przed.
        auto render = [&] (const std::vector<float>& in, const mix::Decision& d)
        {
            mix::SpeechChain c;
            c.params = d.params;
            c.setSampleRate (sr);
            c.prepare (sr);
            std::vector<float> out;
            c.render (in, out);
            return VoiceTone::measure (out, sr);
        };

        const double gapIn  = VoiceTone::measure (x, sr).db[VoiceTone::Presence]
                            - VoiceTone::measure (dark, sr).db[VoiceTone::Presence];
        const double gapOut = render (x, d0).db[VoiceTone::Presence]
                            - render (dark, dD).db[VoiceTone::Presence];

        // Przy ostrych sybilantach podbicie jest ograniczone do 1 dB — stad
        // poprawa o pol decybela, a nie o polowe roznicy.
        check (gapOut < gapIn - 0.5, "po miksie ciemny glos blizej jasnego",
               num (gapIn) + " -> " + num (gapOut));

        // Kreator mowil "za blisko": mniejszy margines, wiec mocniej przy tym samym nagraniu.
        mix::Decide::Wish w; w.studio = false;
        w.room.known = true;
        w.room.proximityDb = 0.0;
        const auto light = pushBand (x, sr, VoiceTone::Low, 2.5, lowAt);
        const auto a = mix::Decide::listen (light, sr, full());
        const auto b = mix::Decide::listen (light, sr, w);
        check (b.params.lowDb < a.params.lowDb && says (b, "za blisko"),
               "kreator 'za blisko' wzmacnia ciecie dolu",
               num (a.params.lowDb) + " -> " + num (b.params.lowDb));
    }

    // ---- 8. przydzwiek: kreator mowi czego szukac, nagranie czy jest -------------
    {
        std::printf ("\n8. przydzwiek z kreatora\n");

        const auto clean = speech (base, sr, 20.0);
        auto weak = clean;   addTone (weak,   sr, 50.0, 0.0003f);   // ok. -70 dBFS
        auto strong = clean; addTone (strong, sr, 50.0, 0.05f);

        mix::Decide::Wish w; w.studio = false;
        w.room.known = true;
        w.room.humHz = 50.0;
        w.room.humExcessDb = 18.0;

        const auto a = mix::Decide::listen (weak, sr, full());
        const auto b = mix::Decide::listen (weak, sr, w);
        const auto c = mix::Decide::listen (clean, sr, w);
        const auto e = mix::Decide::listen (strong, sr, full());

        check (! a.params.deHum, "slaby przydzwiek bez kreatora: nie ruszamy", num (a.humDb));
        check (b.params.deHum && b.params.humHz == 50.0,
               "ten sam, kreator go slyszal: wycinamy 50 Hz", num (b.humDb));
        check (! c.params.deHum && says (c, "nie wystaje"),
               "kreator slyszal, w nagraniu nie ma: nie wycinamy", num (c.humDb));
        check (e.params.deHum, "wyrazny przydzwiek wycinany i bez kreatora", num (e.humDb));
    }

    // ---- 9. tlo glosniejsze niz przy sprawdzaniu ---------------------------------
    {
        std::printf ("\n9. tlo wobec kreatora\n");

        auto x = speech (base, sr, 20.0);
        addRoom (x, 0.004f, 3);

        mix::Decide::Wish quiet, same; quiet.studio = same.studio = false;
        quiet.room.known = true; quiet.room.noiseFloorDb = -75.0;
        same.room.known  = true;

        const auto a = mix::Decide::listen (x, sr, quiet);
        same.room.noiseFloorDb = a.noiseFloorDb - 2.0;
        const auto b = mix::Decide::listen (x, sr, same);

        check (says (a, "głośniejsze niż przy sprawdzaniu"),
               "tlo o wiele glosniejsze: program o tym mowi", num (a.noiseFloorDb));
        check (! says (b, "głośniejsze niż przy sprawdzaniu"),
               "tlo jak przy sprawdzaniu: cisza", "");
    }

    // ---- 10. miks studyjny (domyslny) -------------------------------------------
    {
        std::printf ("\n10. miks studyjny — lancuch v3 z odsluchu 12.10\n");

        auto x = speech (base, sr, 70.0);
        addRoom (x, 0.003f, 7);                 // tlo ok. 30-40 dB pod mowa
        const auto d = mix::Decide::listen (x, sr, zs_podcast());
        const auto& p = d.params;

        check (p.targetLufs == -19.0, "podcast: -19 LUFS", num (p.targetLufs, " LUFS"));
        check (p.highpassHz == 75.0 && p.steepHighpass && p.preEq.size() == 3, "filtr dolu 75 Hz 24 dB/okt i trzy pasma odejmowania", "");
        check (p.useGate && p.gateRatio == 2.0f && p.gateHoldMs >= 100.0 && p.gateThreshDb < d.speechDb - 20.0,
               "ekspander lagodny, ponad 20 dB pod mowa", num (p.gateThreshDb - d.speechDb));
        check (p.levelerRatio == 2.5f && p.peakRatio == 4.0f && p.peakThreshDb > p.levelerThreshDb, "dwa kompresory: wolny i szczytowy", "");
        check (! p.useTame && p.useDeEsser && p.essMaxCutDb >= -3.0f, "de-esser lagodny zamiast tlumienia odchylen", "");
        check (p.notchEq.size() <= 5, "najwyzej piec wyciec rezonansow", num ((double) p.notchEq.size(), ""));
        bool narrow = true;
        for (const auto& n : p.notchEq) narrow = narrow && n.hz >= 2000.0 && n.hz <= 7000.0 && n.db <= -2.0f && n.db >= -5.0f;
        check (narrow, "wyciecia w 2-7 kHz, 2-5 dB", "");
        check (! p.usePlosive && ! p.useDeClick && ! p.useTiltMatch && ! p.useSaturation && ! p.useRider,
               "bez glosek, trzaskow, cofania barwy, nasycenia i suwaka", "");
        check (! says (d, "Brama") && says (d, "studyjny"), "uzasadnienie opisuje ten miks", "");

        // ton 3 kHz o 10 dB ponad widmem: ma byc znaleziony jako rezonans
        auto r = x; addTone (r, sr, 3000.0, 0.02f);
        const auto dr = mix::Decide::listen (r, sr, zs_podcast());
        bool found = false;
        for (const auto& n : dr.params.notchEq) found = found || std::fabs (n.hz - 3000.0) < 20.0;
        check (found, "sztuczny rezonans 3 kHz znaleziony", "");

        mix::Decide::Wish w = full(); w.where = mix::Decide::Destination::Podcast;
        check (mix::Decide::listen (x, sr, w).params.targetLufs == -16.0, "pelna logika bez zmian: -16 LUFS", "");
    }

    // ---- trzy brzmienia do wyboru (Decide::Style) ------------------------------
    std::printf ("\n  Brzmienia: Delikatny, Studyjny, Mocny\n");
    {
        using S = mix::Decide::Style;
        mix::Decision d[3];
        for (int i = 0; i < 3; ++i)
        {
            auto w = zs_podcast();
            w.style = static_cast<S> (i);
            d[i] = mix::Decide::listen (base, sr, w);
        }
        check (d[0].params.targetLufs == d[1].params.targetLufs && d[1].params.targetLufs == d[2].params.targetLufs,
               "ta sama glosnosc we wszystkich (uczciwe porownanie)", "");
        check (d[0].params.levelerRatio < d[1].params.levelerRatio && d[1].params.levelerRatio < d[2].params.levelerRatio,
               "kompresja rosnie: delikatny < studyjny < mocny", "");
        check (! d[0].params.useGate && ! d[0].params.useDeEsser && d[0].params.presenceDb == 0.0f,
               "delikatny: bez ekspandera, de-essera i barwy", "");
        check (d[2].params.presenceDb > d[1].params.presenceDb, "mocny: wiecej obecnosci niz studyjny", "");
        check (! d[0].because.empty() && d[0].because.front().find ("delikatny") != std::string::npos
                   && d[2].because.front().find ("mocny") != std::string::npos,
               "uzasadnienie nazywa wybrane brzmienie", "");
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
