/*
    punch-test.cpp — czy R zostawia czysty plik: bez pomylki, bez trzasku.

    Krok 3 punch and roll (Source/DSP/Punch.h). Sztuczne czytanie: sylaby
    po 150 ms, pauzy 400 ms miedzy wersami, tlo -60 dBFS. Nieudane podejscie
    wersu ma sylaby w INNYM tonie (300 Hz zamiast 140 Hz) — po wycieciu w
    czystym pliku nie moze zostac z niego nic. To jest caly test: slychac
    pomylke albo nie.
*/

#include "../Source/DSP/Punch.h"
#include "../Source/DSP/PrompterLines.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got = {})
{
    std::printf ("  %-5s %-58s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

static std::string num (double v, const char* u = " s") { char b[40]; std::snprintf (b, sizeof (b), "%.3f%s", v, u); return b; }

struct Gen
{
    std::vector<float> x;
    double sr = 16000.0;
    std::mt19937 rng { 7 };
    std::normal_distribution<float> n { 0.0f, 1.0f };

    double now() const { return (double) x.size() / sr; }

    void silence (double sec) { for (size_t i = 0, e = (size_t) (sec * sr); i < e; ++i) x.push_back (0.001f * n (rng)); }

    void syllables (int k, double hz)
    {
        for (int s = 0; s < k; ++s)
        {
            const size_t len = (size_t) (0.15 * sr);
            for (size_t i = 0; i < len; ++i)
                x.push_back ((float) (0.3 * std::sin (2.0 * M_PI * hz * (double) i / sr)
                                      * std::sin (M_PI * (double) i / (double) len)) + 0.001f * n (rng));
            silence (0.04);
        }
    }
};

/** energia tonu hz (Goertzel), wzgledem calkowitej */
static double toneShare (const std::vector<float>& x, double sr, double hz)
{
    const double c = 2.0 * std::cos (2.0 * M_PI * hz / sr);
    double s1 = 0, s2 = 0, all = 0;
    for (float v : x) { const double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; all += (double) v * v; }
    const double p = (s1 * s1 + s2 * s2 - c * s1 * s2) * 2.0 / (double) x.size();
    return all > 0 ? p / (all / (double) x.size() * x.size()) : 0.0;
}

static tw::LineMark mark (int line, double sec, tw::LineSource s)
{
    tw::LineMark m; m.firstWord = line * 6; m.lastWord = line * 6 + 5; m.shown = line; m.sec = sec; m.source = s;
    return m;
}

int main()
{
    std::printf ("\nPunch and roll — R i czysty plik\n================================\n\n");

    // ---- czytanie z jedna pomylka ----------------------------------------------
    Gen g;
    std::vector<tw::LineMark> marks;
    g.silence (0.6);
    const double l0 = g.now(); g.syllables (6, 140); g.silence (0.4);
    const double l1 = g.now(); g.syllables (6, 140); g.silence (0.4);
    const double bad = g.now(); g.syllables (3, 300);      // pomylka w wersie 2
    const double key = g.now() + 0.05;                      // R
    g.silence (0.7);                                        // oddech
    const double good = g.now(); g.syllables (6, 140); g.silence (0.4);
    const double l3 = g.now(); g.syllables (6, 140); g.silence (0.6);

    marks.push_back (mark (0, l0 + 0.78, tw::LineSource::Words));
    marks.push_back (mark (1, l1 + 0.78, tw::LineSource::Words));
    marks.push_back (mark (2, bad + 0.78, tw::LineSource::Words));
    marks.push_back (mark (2, key, tw::LineSource::Retake));
    marks.push_back (mark (3, l3 + 0.78, tw::LineSource::Words));

    tw::CutFinder cuts; cuts.analyse (g.x.data(), g.x.size(), g.sr);
    tw::Punch punch;
    const auto plan = punch.plan (marks, cuts);
    const auto clean = punch.render (g.x, g.sr, plan);

    check (plan.size() == 1, "jedno R = jedno wyciecie", std::to_string (plan.size()));
    if (! plan.empty())
    {
        check (plan[0].from <= bad && plan[0].from >= bad - 0.40,
               "wyciecie zaczyna sie w ciszy przed pomylka", num (plan[0].from - bad));
        check (plan[0].to <= good && plan[0].to >= good - 0.40,
               "konczy sie w ciszy przed ponownym czytaniem", num (plan[0].to - good));
    }

    const double rawBad = toneShare (g.x, g.sr, 300.0), cleanBad = toneShare (clean, g.sr, 300.0);
    char b[80]; std::snprintf (b, sizeof (b), "%.3f -> %.6f", rawBad, cleanBad);
    check (rawBad > 0.001 && cleanBad < rawBad / 100.0, "w czystym pliku nie ma tonu pomylki (300 Hz)", b);

    const double expectLen = (double) g.x.size() / g.sr - (plan.empty() ? 0 : plan[0].to - plan[0].from) - punch.crossfadeSec;
    check (std::fabs ((double) clean.size() / g.sr - expectLen) < 0.002,
           "dlugosc = surowe - wyciete - przenikanie", num ((double) clean.size() / g.sr) + " / " + num (expectLen));

    // trzask: najwiekszy skok miedzy probkami w okolicy szwu wobec ciszy
    if (! plan.empty())
    {
        const size_t seam = (size_t) std::lround (punch.remap (plan[0].to, plan) * g.sr);
        float jump = 0.0f;
        for (size_t i = seam > 400 ? seam - 400 : 1; i < std::min (clean.size(), seam + 400); ++i)
            jump = std::max (jump, std::fabs (clean[i] - clean[i - 1]));
        check (jump < 0.02f, "na szwie nie ma trzasku (sama cisza)", num (jump, ""));

        const double goodClean = punch.remap (good, plan);
        check (std::fabs (goodClean - (punch.remap (plan[0].from, plan) + 0.15)) < 0.03,
               "ponowne czytanie zaczyna sie 0,15 s po szwie", num (goodClean));
        check (std::fabs (punch.remap (l1, plan) - l1) < 1e-9, "czas przed wycieciem bez zmian");
    }

    // ---- dwa razy R na tym samym wersie -------------------------------------------
    {
        Gen h;
        std::vector<tw::LineMark> m;
        h.silence (0.6);
        const double a = h.now(); h.syllables (6, 140); h.silence (0.4);
        const double b1 = h.now(); h.syllables (2, 300);
        const double k1 = h.now() + 0.05; h.silence (0.6);
        h.syllables (3, 300);                                // druga pomylka
        const double k2 = h.now() + 0.05; h.silence (0.6);
        h.syllables (6, 140); h.silence (0.6);

        m.push_back (mark (0, a + 0.78, tw::LineSource::Words));
        m.push_back (mark (1, b1 + 0.5, tw::LineSource::Words));
        m.push_back (mark (1, k1, tw::LineSource::Retake));
        m.push_back (mark (1, k2, tw::LineSource::Retake));

        tw::CutFinder c; c.analyse (h.x.data(), h.x.size(), h.sr);
        const auto p = punch.plan (m, c);
        const auto out = punch.render (h.x, h.sr, p);
        std::snprintf (b, sizeof (b), "%zu wyciec, 300 Hz: %.6f", p.size(), toneShare (out, h.sr, 300.0));
        check (p.size() == 1 && toneShare (out, h.sr, 300.0) < 0.001,
               "dwa R na jednym wersie: obie pomylki znikaja, jeden szew", b);
    }

    // ---- szew: tlo po obu stronach ---------------------------------------------------
    {
        const auto sm = punch.seams (g.x, g.sr, plan);
        check (sm.size() == 1 && ! sm[0].audible, "to samo tlo po obu stronach: szew nieslyszalny",
               sm.empty() ? "" : num (sm[0].levelDiffDb, " dB"));

        // w pauzie po R ktos wlaczyl wentylator: tlo +8 dB, od 0,5 s przed wznowieniem
        auto loud = g.x;
        std::mt19937 r2 (9); std::normal_distribution<float> n2 (0.0f, 1.0f);
        if (! plan.empty())
            for (size_t i = (size_t) ((plan[0].to - 0.5) * g.sr); i < loud.size(); ++i) loud[i] += 0.0025f * n2 (r2);
        const auto sm2 = punch.seams (loud, g.sr, plan);
        check (sm2.size() == 1 && sm2[0].audible, "tlo glosniejsze po R: szew oznaczony jako slyszalny",
               sm2.empty() ? "" : num (sm2[0].levelDiffDb, " dB"));
    }

    // ---- R bez wczesniejszego wersu --------------------------------------------------
    {
        std::vector<tw::LineMark> m { mark (0, 1.0, tw::LineSource::Retake) };
        check (punch.plan (m, cuts).empty(), "R zanim jakikolwiek wers sie zaczal: nic nie tniemy");
    }

    // ---- zdarzenia lampki po wycieciu ------------------------------------------------
    {
        const std::vector<tw::Removal> cutsAt { { 5.0, 8.0 } };
        tw::Event a; a.timeSec = 3.0; a.redoFrom = 2.5;
        tw::Event b; b.timeSec = 6.0; b.redoFrom = 5.5;      // w pomylce
        tw::Event c; c.timeSec = 10.0; c.redoFrom = 9.0;
        const auto out = punch.remapEvents ({ a, b, c }, cutsAt);
        check (out.size() == 2, "zdarzenie z wycietej pomylki znika", std::to_string (out.size()));
        check (out.size() == 2 && out[0].timeSec == 3.0 && std::fabs (out[1].timeSec - (10.0 - 3.0 - punch.crossfadeSec)) < 1e-9
               && std::fabs (out[1].redoFrom - (9.0 - 3.0 - punch.crossfadeSec)) < 1e-9,
               "reszta na czasie czystego pliku");
    }

    // ---- STRAZNIK: nie wycinamy niczego, czego nie przeczytano jeszcze raz ------------
    {
        tw::Punch pg;
        const double lag = pg.heardLagSec;
        auto heard = [&] (std::vector<std::pair<double, int>> spoken)
        {
            std::vector<tw::HeardWord> h;
            for (auto& x : spoken) h.push_back ({ x.first + lag, x.second });   // rozpoznanie oddaje z opoznieniem
            return h;
        };
        std::vector<tw::SkippedCut> sk;

        // zwykle R: wers 2 (slowa 10-15), pomylka na 11, R, czytanie od 10
        const std::vector<tw::Removal> one { { 5.0, 8.0, 10, 15 } };
        auto ok = pg.guard (one, heard ({ { 3.0, 8 }, { 4.5, 9 }, { 5.15, 10 }, { 5.6, 11 }, { 8.15, 10 }, { 8.5, 11 }, { 9.0, 12 } }), &sk);
        check (ok.size() == 1 && sk.empty(), "zwykle R: wycina");

        // R cofnelo o wers za daleko (wycina wers 1 = slowa 5-9 i pomylke), czlowiek idzie za prompterem
        const std::vector<tw::Removal> back { { 2.0, 8.0, 5, 15 } };
        sk.clear();
        ok = pg.guard (back, heard ({ { 2.15, 5 }, { 3.0, 7 }, { 4.5, 9 }, { 5.15, 10 }, { 5.6, 11 },
                                       { 8.15, 5 }, { 9.0, 7 }, { 10.5, 9 }, { 11.0, 10 } }), &sk);
        check (ok.size() == 1, "R za daleko, czlowiek czyta poprzedni wers jeszcze raz: wycina");

        // to samo, ale czlowiek czyta od razu wers 2 — wers 1 by zginal
        sk.clear();
        ok = pg.guard (back, heard ({ { 2.15, 5 }, { 3.0, 7 }, { 4.5, 9 }, { 5.15, 10 }, { 5.6, 11 },
                                       { 8.15, 10 }, { 8.5, 11 }, { 9.0, 12 } }), &sk);
        check (ok.empty() && sk.size() == 1, "R za daleko, czlowiek czyta tylko swoj wers: NIE wycina",
               sk.empty() ? "" : sk[0].why);

        // pomylka na PIERWSZYM slowie wersu, tuz przy poczatku wyciecia; koncowka
        // poprzedniego wersu oddana z opoznieniem wpada w wyciecie — nie przeszkadza
        sk.clear();
        ok = pg.guard (one, heard ({ { 4.7, 8 }, { 5.05, 9 }, { 5.15, 10 }, { 8.15, 10 }, { 8.5, 11 } }), &sk);
        check (ok.size() == 1 && sk.empty(), "pomylka na pierwszym slowie wersu: wycina",
               sk.empty() ? "" : sk[0].why);

        // rozpoznawanie przypisalo cos w wycinanym kawalku do INNEGO wersu (podobne wersy)
        sk.clear();
        ok = pg.guard (one, heard ({ { 5.15, 10 }, { 6.5, 21 }, { 8.15, 10 } }), &sk);
        check (ok.empty() && sk.size() == 1, "w wycinanym kawalku slowo innego wersu: NIE wycina",
               sk.empty() ? "" : sk[0].why);

        sk.clear();
        ok = pg.guard (one, heard ({ { 5.6, 11 } }), &sk);
        check (ok.empty() && sk.size() == 1, "po wznowieniu nic nie uslyszane: nie wycina");

        sk.clear();
        ok = pg.guard (one, heard ({ { 8.15, 10 } }), &sk);
        check (ok.empty() && sk.size() == 1, "w wycinanym kawalku nic nie uslyszane: nie wycina");

        // ciagla mowa bez pauz: przed wersem nie ma ciszy
        Gen c;
        c.syllables (60, 140);
        tw::CutFinder cf; cf.analyse (c.x.data(), c.x.size(), c.sr);
        std::vector<tw::LineMark> m { mark (0, 3.0, tw::LineSource::Words), mark (0, 6.0, tw::LineSource::Retake) };
        std::vector<tw::SkippedCut> sk2;
        check (pg.plan (m, cf, &sk2).empty() && sk2.size() == 1, "bez ciszy przed wersem: nie tnie na oko",
               sk2.empty() ? "" : sk2[0].why);
    }

    // ---- PEWNE / DO WSKAZANIA ---------------------------------------------------------
    {
        tw::Punch pr;
        const double lag = pr.heardLagSec;
        auto heard = [&] (std::vector<std::pair<double, int>> spoken)
        {
            std::vector<tw::HeardWord> h;
            for (auto& x : spoken) h.push_back ({ x.first + lag, x.second });
            return h;
        };
        const std::vector<tw::Removal> one { { 5.0, 8.0, 10, 15 } };
        const auto normal = heard ({ { 3.0, 8 }, { 4.5, 9 }, { 5.15, 10 }, { 5.6, 11 }, { 8.15, 10 }, { 8.5, 11 } });

        auto v = pr.review (one, normal, nullptr);
        check (v.size() == 1 && v[0].sure, "zwykle R: pewne", v.empty() ? "" : v[0].why);

        // ciecie w pauzie po przecinku w POPRZEDNIM wersie: jego ostatnie slowo padlo po poczatku ciecia
        v = pr.review (one, heard ({ { 3.0, 8 }, { 5.2, 9 }, { 5.6, 10 }, { 8.15, 10 }, { 8.5, 11 } }), nullptr);
        check (v.size() == 1 && ! v[0].sure, "poprzedni wers trwal po poczatku ciecia: do wskazania", v.empty() ? "" : v[0].why);

        v = pr.review (one, normal, nullptr, { 10 });
        check (v.size() == 1 && ! v[0].sure, "wers zaczyna sie jak inny: do wskazania", v.empty() ? "" : v[0].why);

        const std::vector<tw::Removal> longCut { { 1.0, 10.0, 10, 15 } };
        v = pr.review (longCut, heard ({ { 1.15, 10 }, { 2.0, 11 }, { 10.15, 10 }, { 10.5, 11 } }), nullptr);
        check (v.size() == 1 && ! v[0].sure, "dlugie wyciecie: do wskazania", v.empty() ? "" : v[0].why);

        v = pr.review (one, heard ({ { 5.15, 10 }, { 8.15, 12 } }), nullptr);
        check (v.size() == 1 && ! v[0].sure, "straznik odrzuca: do wskazania", v.empty() ? "" : v[0].why);

        tw::PrompterText t;
        t.set ("Spadły kwiaty, za czasem, owoce zerwano\nSpadły liście, a zatem o jabłoń nie dbano.\nWidząc to rzekła jodła\n");
        const auto sp = tw::lineSpans (t);
        const auto sim = tw::similarlyStartingLines (t, sp);
        const bool firstTwo = std::find (sim.begin(), sim.end(), sp[0].firstWord) != sim.end();
        const bool third = std::find (sim.begin(), sim.end(), sp.back().firstWord) != sim.end();
        check (firstTwo && ! third, "wersy od tego samego slowa sa podobne, inne nie",
               std::to_string (sim.size()) + " podobnych");
    }

    // ---- miejsce ciecia z pre-rollu i ze strzalek -------------------------------------
    {
        tw::Punch pp;
        std::vector<tw::LineMark> m { mark (0, l0 + 0.78, tw::LineSource::Words), mark (1, l1 + 0.78, tw::LineSource::Words),
                                      mark (2, bad + 0.78, tw::LineSource::Words), mark (2, key, tw::LineSource::Retake) };
        m.back().cutFrom = bad - 0.10;  m.back().cutByHand = false;
        auto pl = pp.plan (m, cuts);
        check (pl.size() == 1 && std::fabs (pl[0].from - (bad - 0.10)) < 1e-9 && ! pl[0].byHand,
               "ciecie zaczyna sie tam, gdzie skonczyl sie pre-roll");

        // miejsce z pre-rollu w srodku mowy (nie strzalkami) — nie tniemy
        m.back().cutFrom = bad + 0.30;  m.back().cutByHand = false;
        std::vector<tw::SkippedCut> sk3;
        check (pp.plan (m, cuts, &sk3).empty() && sk3.size() == 1, "pre-roll nie w ciszy: nie tnie",
               sk3.empty() ? "" : sk3[0].why);

        m.back().cutFrom = l1 - 0.15;  m.back().cutByHand = true;     // czlowiek cofnal o wers
        pl = pp.plan (m, cuts);
        check (pl.size() == 1 && std::fabs (pl[0].from - (l1 - 0.15)) < 1e-9 && pl[0].byHand, "strzalkami przesuniete: tam");
        const auto v = pp.review (pl, {}, nullptr);
        check (v.size() == 1 && v[0].sure, "wskazane przez czlowieka: pewne", v.empty() ? "" : v[0].why);
    }

    // ---- R bez modelu mowy: tylko cofa tekst ---------------------------------------
    {
        check (tw::retakeSource (true) == tw::LineSource::Retake, "z rozpoznawaniem mowy R wycina");
        check (tw::retakeSource (false) == tw::LineSource::Key, "na samych sylabach R tylko cofa tekst");
        std::vector<tw::LineMark> m { mark (0, 1.5, tw::LineSource::Syllables),
                                      mark (0, 3.0, tw::retakeSource (false)) };
        check (punch.plan (m, cuts).empty(), "i wtedy po zatrzymaniu nic nie jest wyciete");
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
