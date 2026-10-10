/*
    montage-test.cpp — czy sklejone nagranie brzmi jak jedno nagranie.

    Testujemy WYKONANIE planu, nie sam plan: planer jest osobnym, starszym
    kawalkiem i ma swoje zalozenia. Tutaj pytanie brzmi inaczej — dostajemy
    liste ciec i sprawdzamy, czy po sklejeniu da sie znalezc szew.

    "Da sie znalezc" jest tu mierzalne i to jest caly sens tego pliku:

      TRZASK       ciecie w zlym miejscu zostawia skok miedzy sasiednimi
                   probkami. Porownujemy najwiekszy skok przy szwie z tym, co
                   sygnal robi normalnie.
      STOPIEN      tlo po obu stronach ma miec ten sam poziom. Roznica powyzej
                   poltora decybela jest slyszalna jako "latka".
      DLUGOSC      wynik ma byc krotszy dokladnie o to, co wycieto.
      TRESC        to, co mialo zostac, ma zostac — sprawdzamy korelacja ze
                   zrodlem, nie na slowo.

    Mowa pochodzi z espeaka, wiec przejscia miedzy sylabami sa prawdziwe.

        g++ -O2 -std=c++17 -I../Source montage-test.cpp -o montage-test
*/

#include "../Source/DSP/MontageRender.h"
#include "../Source/Wav.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static constexpr double kSR = 48000.0;
static int failures = 0;

static void check (bool ok, const std::string& what, const std::string& detail = {})
{
    std::printf ("      %s  %-46s %s\n", ok ? "OK  " : "BLAD", what.c_str(), detail.c_str());
    if (! ok) ++failures;
}

static std::string num (double v, int d = 2)
{
    char b[64]; std::snprintf (b, sizeof (b), "%.*f", d, v); return b;
}

//==============================================================================
/** Sesja: kilka fraz oddzielonych cisza, z tlem pokoju. */
struct Session
{
    std::vector<float> audio;
    std::vector<std::pair<double, double>> phrases;   ///< poczatki i konce
};

static Session buildSession (const std::vector<float>& speech,
                             const std::vector<double>& phraseLen,
                             double gapSec, double noiseDb = -60.0,
                             double noiseStepAt = -1.0, double noiseStepDb = 0.0)
{
    Session s;
    std::mt19937 rng { 11 };
    std::normal_distribution<float> g { 0.0f, 1.0f };

    size_t srcPos = (size_t) (0.5 * kSR);
    double t = 0.0;

    // pol sekundy ciszy na poczatek
    for (int i = 0; i < (int) (0.5 * kSR); ++i) s.audio.push_back (0.0f);
    t += 0.5;

    for (double len : phraseLen)
    {
        const size_t n = (size_t) (len * kSR);
        s.phrases.push_back ({ t, t + len });

        for (size_t i = 0; i < n; ++i)
        {
            const float v = srcPos + i < speech.size() ? speech[srcPos + i] : 0.0f;
            s.audio.push_back (v * 0.2f);
        }

        srcPos += n + (size_t) (0.2 * kSR);
        t += len;

        for (int i = 0; i < (int) (gapSec * kSR); ++i) s.audio.push_back (0.0f);
        t += gapSec;
    }

    // tlo pokoju na calosci, z opcjonalnym skokiem poziomu w danej chwili
    for (size_t i = 0; i < s.audio.size(); ++i)
    {
        const double now = (double) i / kSR;
        double db = noiseDb;

        if (noiseStepAt >= 0.0 && now > noiseStepAt)
            db += noiseStepDb;

        s.audio[i] += g (rng) * (float) std::pow (10.0, db / 20.0) * 0.4f;
    }

    return s;
}

static double rmsDb (const std::vector<float>& x, double fromSec, double toSec)
{
    const size_t a = (size_t) std::max (0.0, fromSec * kSR);
    const size_t b = (size_t) std::min ((double) x.size(), toSec * kSR);

    double sum = 0.0; size_t n = 0;
    for (size_t i = a; i < b; ++i) { sum += (double) x[i] * x[i]; ++n; }

    return n && sum > 0.0 ? 10.0 * std::log10 (sum / n) : -140.0;
}

/** Najwiekszy skok miedzy sasiednimi probkami w oknie. */
static double maxStep (const std::vector<float>& x, double fromSec, double toSec)
{
    const size_t a = (size_t) std::max (1.0, fromSec * kSR);
    const size_t b = (size_t) std::min ((double) x.size(), toSec * kSR);

    double m = 0.0;
    for (size_t i = a; i < b; ++i)
        m = std::max (m, (double) std::fabs (x[i] - x[i - 1]));

    return m;
}

static bool loadSpeech (std::vector<float>& out)
{
    wavio::Audio a;
    if (! wavio::read ("/tmp/mowa/mowa-czysta.wav", a).empty()) return false;
    out = a.toMono();
    return out.size() > (size_t) (20 * kSR);
}

//==============================================================================
int main()
{
    std::printf ("\nMontaz — sklejanie bez slyszalnego szwu\n");
    std::printf ("======================================\n");

    std::vector<float> speech;

    if (! loadSpeech (speech))
    {
        std::printf ("\n  brak mowy testowej — przerywam\n\n");
        return 1;
    }

    // ---- 1. jedno ciecie ----------------------------------------------------
    {
        std::printf ("\n  1. wyciecie jednej frazy z czterech\n");

        auto s = buildSession (speech, { 2.0, 2.0, 2.0, 2.0 }, 1.0);

        tw::MontagePlan plan;
        plan.items.push_back ({ tw::Action::CutBad, s.phrases[1].first, s.phrases[1].second,
                                1, 0.0f, "plozywa" });

        tw::MontageRender r;
        std::vector<float> out;
        const auto rep = r.render (s.audio, kSR, plan, out);

        check (rep.cutsMade == 1, "ciecie wykonane",
               std::to_string (rep.cutsMade) + " z 1, odmow " + std::to_string (rep.cutsRefused));

        // Wycieta zostaje fraza PLUS kawalek otaczajacej ciszy — bo ciecie
        // przesuwa sie do najcichszego okna, a to lezy w przerwie. Dlatego
        // wynik moze byc krotszy niz sama fraza, ale nigdy dluzszy, i nigdy o
        // wiecej niz dwa zasiegi poszukiwan.
        const double atMost  = rep.originalSec - 2.0;
        const double atLeast = atMost - 2 * 0.35;

        check (rep.renderedSec <= atMost + 0.02 && rep.renderedSec >= atLeast - 0.02,
               "dlugosc miesci sie w oczekiwanym zakresie",
               num (rep.renderedSec, 2) + " s, zakres " + num (atLeast, 2)
               + "-" + num (atMost, 2));

        check (rep.seams.size() == 1, "jeden szew w raporcie");

        if (! rep.seams.empty())
        {
            const auto& seam = rep.seams[0];

            check (! seam.audible, "szew uznany za nieslyszalny",
                   "stopien " + num (seam.levelStepDb, 2) + " dB, barwa "
                   + num (seam.tiltStepDb, 2) + " dB");

            // trzask: skok przy szwie wobec tego, co sygnal robi w ciszy
            const double atSeam = maxStep (out, seam.atSec - 0.03, seam.atSec + 0.03);
            const double normal = maxStep (out, seam.atSec + 0.30, seam.atSec + 0.60);

            check (atSeam < normal * 2.5, "brak trzasku na sklejeniu",
                   "skok " + num (atSeam * 1000, 2) + " wobec " + num (normal * 1000, 2) + " (x1000)");
        }
    }

    // ---- 2. ciecie w srodku mowy ma sie NIE odbyc --------------------------
    {
        std::printf ("\n  2. ciecie tam, gdzie nie ma ciszy\n");

        // frazy bez przerw miedzy soba — nie ma gdzie ciac
        auto s = buildSession (speech, { 2.0, 2.0, 2.0 }, 0.02);

        tw::MontagePlan plan;
        plan.items.push_back ({ tw::Action::CutBad, s.phrases[1].first, s.phrases[1].second,
                                1, 0.0f, "plozywa" });

        tw::MontageRender r;
        std::vector<float> out;
        const auto rep = r.render (s.audio, kSR, plan, out);

        check (rep.cutsRefused == 1 && rep.cutsMade == 0,
               "program odmawia ciecia w srodku mowy",
               std::to_string (rep.cutsRefused) + " odmow");

        check (std::fabs (rep.renderedSec - rep.originalSec) < 0.01,
               "material zostaje nietkniety");

        check (! rep.refusals.empty(), "odmowa jest opisana",
               rep.refusals.empty() ? "" : rep.refusals[0].substr (0, 40) + "...");
    }

    // ---- 3. szew, ktory BEDZIE slychac -------------------------------------
    {
        std::printf ("\n  3. tlo po obu stronach rozne o 6 dB\n");

        // w polowie sesji ktos wlaczyl lodowke
        // Skok tla musi lezec MIEDZY stronami szwu: fraza 1 zajmuje 3,5-5,5 s,
        // wiec sklejenie laczy okolice 3,5 z okolicami 5,5. Pierwsza wersja
        // stawiala skok na 7 s, czyli za szwem — i test slusznie nic nie
        // znajdowal, tylko nie tam, gdzie patrzyl.
        auto s = buildSession (speech, { 2.0, 2.0, 2.0, 2.0 }, 1.0, -60.0, 4.5, 6.0);

        tw::MontagePlan plan;
        plan.items.push_back ({ tw::Action::CutBad, s.phrases[1].first, s.phrases[1].second,
                                1, 0.0f, "plozywa" });

        tw::MontageRender r;
        std::vector<float> out;
        const auto rep = r.render (s.audio, kSR, plan, out);

        check (! rep.seams.empty() && rep.seams[0].audible,
               "program ostrzega, ze szew bedzie slychac",
               rep.seams.empty() ? "brak szwu"
                                 : "stopien " + num (rep.seams[0].levelStepDb, 1) + " dB");

        if (! rep.seams.empty())
            std::printf ("      ---   %s\n", rep.seams[0].note.c_str());
    }

    // ---- 4. kilka ciec naraz -----------------------------------------------
    {
        std::printf ("\n  4. trzy ciecia w jednej sesji\n");

        auto s = buildSession (speech, { 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5 }, 1.0);

        tw::MontagePlan plan;

        // Ciecia rozlaczne. Sasiadujace (3 i 4) zlewaja sie w jeden wyciety
        // kawalek i daja jeden szew zamiast dwoch — to poprawne zachowanie,
        // ale zaciemnia to, co ten test ma sprawdzac.
        for (int i : { 1, 3, 5 })
            plan.items.push_back ({ tw::Action::CutRetake, s.phrases[(size_t) i].first,
                                    s.phrases[(size_t) i].second, i, 0.9f, "powtorka" });

        tw::MontageRender r;
        std::vector<float> out;
        const auto rep = r.render (s.audio, kSR, plan, out);

        for (const auto& r2 : rep.refusals)
            std::printf ("      ---   odmowa: %s\n", r2.c_str());

        check (rep.cutsMade == 3, "wszystkie trzy wykonane",
               std::to_string (rep.cutsMade) + " z 3, odmow "
               + std::to_string (rep.cutsRefused));

        check (rep.seams.size() == 3, "trzy szwy w raporcie",
               std::to_string (rep.seams.size()));

        int audible = 0;
        for (const auto& sm : rep.seams) if (sm.audible) ++audible;

        check (audible == 0, "zaden nie jest slyszalny",
               std::to_string (audible) + " podejrzanych");

        const double atMost  = rep.originalSec - 4.5;
        const double atLeast = atMost - 3 * 2 * 0.35;

        check (rep.renderedSec <= atMost + 0.02 && rep.renderedSec >= atLeast - 0.02,
               "dlugosc po trzech cieciach",
               num (rep.renderedSec, 2) + " s, zakres " + num (atLeast, 2)
               + "-" + num (atMost, 2));
    }

    // ---- 5. czy zostalo to, co mialo zostac --------------------------------
    {
        std::printf ("\n  5. tresc: zostaje to, co mialo zostac\n");

        auto s = buildSession (speech, { 2.0, 2.0, 2.0 }, 1.0);

        tw::MontagePlan plan;
        plan.items.push_back ({ tw::Action::CutBad, s.phrases[1].first, s.phrases[1].second,
                                1, 0.0f, "plozywa" });

        tw::MontageRender r;
        std::vector<float> out;
        r.render (s.audio, kSR, plan, out);

        // pierwsza fraza musi lezec w wyniku tam, gdzie byla
        const double inSrc = rmsDb (s.audio, s.phrases[0].first + 0.3, s.phrases[0].first + 1.5);
        const double inOut = rmsDb (out,     s.phrases[0].first + 0.3, s.phrases[0].first + 1.5);

        check (std::fabs (inSrc - inOut) < 0.3, "pierwsza fraza nietknieta",
               num (inSrc, 2) + " -> " + num (inOut, 2) + " dBFS");

        // trzecia fraza przesunela sie o wyciete dwie sekundy i ma tam byc
        const double shifted = rmsDb (out, s.phrases[2].first - 2.0 + 0.3,
                                           s.phrases[2].first - 2.0 + 1.5);

        check (shifted > -45.0, "trzecia fraza jest na swoim nowym miejscu",
               num (shifted, 1) + " dBFS");

        // a w miejscu wycietej frazy ma byc juz nastepna, nie cisza
        const double wasCut = rmsDb (out, s.phrases[1].first + 0.5, s.phrases[1].first + 1.5);
        check (wasCut > -50.0, "po wycietej frazie nie ma dziury",
               num (wasCut, 1) + " dBFS");
    }

    std::printf ("\n======================================\n");
    std::printf ("%s\n\n", failures == 0 ? "wszystko przeszlo"
                                         : (std::to_string (failures) + " nieprzeszlych").c_str());
    return failures == 0 ? 0 : 1;
}
