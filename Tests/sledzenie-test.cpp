/*
    sledzenie-test.cpp — sledzenie tekstu po rozpoznanych slowach (WordTracker)
    i czyszczenie skryptu (ScriptNormalize).

    ---------------------------------------------------------------------------
    CZEGO TEN TEST NIE MOWI

    Nie mierzy Vosk. Hipotezy sa tu SYMULOWANE: slowa przychodza z
    opoznieniem 1,2 s, czesc jest zgubiona, zamieniona na inne slowo ze
    skryptu albo na "[unk]". Model bledow jest wymyslony, wiec procenty
    ponizej mowia tylko, jak logika sledzenia radzi sobie z TAKIM wejsciem —
    i pilnuja, zeby zmiana logiki tego nie pogorszyla. Liczby z prawdziwego
    glosu (66% / 88%) pochodza z osobnego pomiaru.
*/

#include "../Source/DSP/WordTracker.h"
#include "../Source/DSP/PrompterLines.h"
#include "../Source/DSP/Follower.h"
#include "../Source/ModelWords.h"

#include <cstring>
#include <fstream>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int fails = 0;

static void check (bool ok, const char* what, const std::string& got)
{
    std::printf ("  %-5s %-52s %s\n", ok ? "OK" : "ZLE", what, got.c_str());
    if (! ok) ++fails;
}

//==============================================================================
static const char* kScript =
    "Dzisiaj jest piątek i nagrywam pierwszy odcinek. Kot siedzi na płocie, a pies śpi pod stołem.\n"
    "Nagranie musi być czyste, żeby dało się go słuchać bez zmęczenia. Mikrofon stoi blisko ust, "
    "więc słychać każdy oddech i każde mlaśnięcie.\n"
    "Czytam dalej równo i spokojnie, aż do końca strony. Potem robię przerwę, piję wodę "
    "i wracam do tekstu tam, gdzie skończyłem.\n"
    "Najtrudniejsze są długie zdania, w których trzeba rozłożyć oddech na kilka fraz, "
    "a jednocześnie nie zgubić sensu ani tempa opowieści.\n"
    "Słuchacz nie widzi tekstu, słyszy tylko głos, dlatego każda pauza mówi mu, "
    "gdzie kończy się jedna myśl i zaczyna następna.\n"
    "Na koniec dziękuję za wysłuchanie i zapraszam na kolejny odcinek w przyszłym tygodniu.";

/** Scenariusz testu: ciagla mowa i slowa oddawane 1,2 s po wypowiedzeniu —
    tu sprawdzamy MECHANIZM jazdy i kompensacji. Domyslne wartosci programu
    (0 i 0) pochodza z pomiaru na prawdziwym nagraniu, nie z tej symulacji. */
static void ciaglaMowa (tw::WordTracker& t)
{
    t.params.latencySec = 1.2;
    t.params.maxCoastSec = 1.5;
}

struct Rng
{
    uint32_t s;
    double next() { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0; }
};

struct Spoken { double start, end; bool endsUtterance; };

/** Kiedy kazde slowo pada: ok. 0,11 s na sylabe, pauza na interpunkcji. */
static std::vector<Spoken> speak (const tw::PrompterText& t, double from, int skipAt = -1, int skipTo = -1)
{
    std::vector<Spoken> out ((size_t) t.wordCount(), { -1.0, -1.0, false });
    double now = from;

    for (int i = 0; i < t.wordCount(); ++i)
    {
        if (i == skipAt) { out[(size_t) (i - 1)].endsUtterance = true; now += 1.0; i = skipTo; }

        const auto& w = t.all()[(size_t) i];
        out[(size_t) i].start = now;
        now += 0.10 + 0.11 * w.syllables;
        out[(size_t) i].end = now;

        if (w.breathAfter >= 0.5f) { out[(size_t) i].endsUtterance = true; now += 0.25 + 0.35 * w.breathAfter; }
    }

    return out;
}

struct Errors { double drop = 0, swap = 0, unk = 0, near = 0; };

struct Result
{
    double within1 = 0, within2 = 0;
    int    resyncs = 0;
    double resyncAfterSkipSec = -1;   ///< ile od przeskoku do odnalezienia miejsca
    double after2 = 0;                ///< % chwil z bledem <= 2 slowa, od 2 s po resyncu
};

/** Puszcza "Vosk" po tekscie i mierzy blad pozycji co 0,1 s. */
static Result run (const tw::PrompterText& text, tw::WordTracker& tr, const std::vector<Spoken>& sp,
                   Errors e, double latency, uint32_t seed, int skipAt = -1)
{
    Rng rng { seed };
    tr.begin (&text);

    // co Vosk "uslyszy" dla kazdego slowa — losowane raz, jak prawdziwa pomylka
    std::vector<std::string> heard ((size_t) text.wordCount());
    for (int i = 0; i < text.wordCount(); ++i)
    {
        auto w = tw::spokenKey (text.wordText (i));
        const double r = rng.next();

        if (r < e.drop) w.clear();
        else if (r < e.drop + e.swap) w = tw::spokenKey (text.wordText ((int) (rng.next() * text.wordCount())));
        else if (r < e.drop + e.swap + e.unk) w = "[unk]";
        else if (r < e.drop + e.swap + e.unk + e.near && w.size() >= 6) w.back() = w.back() == 'a' ? 'e' : 'a';

        heard[(size_t) i] = w;
    }

    double end = 0;
    for (auto& s : sp) end = std::max (end, s.end);

    int firstSpoken = -1;
    for (int i = 0; i < (int) sp.size(); ++i) if (sp[(size_t) i].start >= 0) { firstSpoken = i; break; }

    size_t utterStart = (size_t) firstSpoken;
    int n = 0, in1 = 0, in2 = 0;
    double resyncAt = -1;
    int nAfter = 0, in2After = 0;
    double skipSec = skipAt >= 0 ? sp[(size_t) skipAt - 1].end : 1e9;

    for (double now = 0.0; now < end + 2.0; now += 0.05)
    {
        // ---- Vosk: co 0,25 s hipoteza biezacej wypowiedzi ---------------------
        if (std::fmod (now + 1e-9, 0.25) < 0.05)
        {
            std::vector<std::string> hyp;
            size_t i = utterStart;
            bool closed = false;

            for (; i < sp.size(); ++i)
            {
                if (sp[i].start < 0) continue;
                if (sp[i].end + latency > now) break;
                if (! heard[i].empty()) hyp.push_back (heard[i]);
                if (sp[i].endsUtterance) { closed = true; ++i; break; }
            }

            tr.hypothesis (now, hyp);

            if (closed)
            {
                tr.endUtterance();
                while (i < sp.size() && sp[i].start < 0) ++i;
                utterStart = i;
            }
        }

        // ---- blad: ktore slowo naprawde pada ----------------------------------
        if (now < sp[(size_t) firstSpoken].start + 2.0 || now > end) continue;

        int truth = firstSpoken;
        for (int i = 0; i < (int) sp.size(); ++i)
            if (sp[(size_t) i].start >= 0 && sp[(size_t) i].start <= now) truth = i;

        const double err = std::abs (tr.word (now) - truth);
        ++n;
        if (err <= 1) ++in1;
        if (err <= 2) ++in2;

        if (now > skipSec && resyncAt < 0 && tr.resyncCount() > 0) resyncAt = now;
        if (resyncAt > 0 && now > resyncAt + 2.0) { ++nAfter; if (err <= 2) ++in2After; }
    }

    return { 100.0 * in1 / n, 100.0 * in2 / n, tr.resyncCount(),
             resyncAt > 0 ? resyncAt - skipSec : -1.0,
             nAfter > 0 ? 100.0 * in2After / nAfter : 0.0 };
}

//==============================================================================
int main()
{
    char b[160];
    std::printf ("\nSledzenie tekstu po rozpoznanych slowach\n========================================\n\n");

    // ---- 1. czyszczenie tekstu --------------------------------------------------
    std::printf ("  1. tekst skryptu\n");
    {
        // "nagrywanie" z cyrylickim "е" (U+0435) i "а" (U+0430)
        const std::string cyr = "nagryw\xd0\xb0ni\xd0\xb5";
        check (tw::spokenKey (cyr) == "nagrywanie", "cyrylickie \xd0\xb5 i \xd0\xb0 -> lacinskie e i a", tw::spokenKey (cyr));

        tw::PrompterText a, c;
        a.set ("nagrywanie");
        c.set (cyr);
        std::snprintf (b, sizeof (b), "%d i %d sylab", a.totalSyllables(), c.totalSyllables());
        check (a.totalSyllables() == c.totalSyllables() && c.totalSyllables() == 4,
               "licznik sylab liczy je tak samo", b);

        check (tw::spokenKey ("\xe2\x80\x9eRaz,") == "raz" && tw::spokenKey ("\xc5\x81\xc3\x93" "DKA.") == "\xc5\x82\xc3\xb3" "dka",
               "cudzyslowy i kropki precz, wielkie polskie -> male", tw::spokenKey ("\xc5\x81\xc3\x93" "DKA."));

        tw::PrompterText nb;
        nb.set ("raz\xc2\xa0" "dwa\xc2\xad" "trzy\xe2\x80\x8b");
        check (nb.wordCount() == 2 && tw::spokenKey (nb.wordText (1)) == "dwatrzy",
               "twarda spacja dzieli, miekki lacznik i zero-szer. nie", std::to_string (nb.wordCount()) + " slowa");

        std::snprintf (b, sizeof (b), "%.2f", tw::similarity ("nagrywanie", "nagrywania"));
        check (tw::similarity ("nagrywanie", "nagrywania") >= 0.8, "podobienstwo liczone na znakach", b);
        std::snprintf (b, sizeof (b), "%.2f", tw::similarity ("\xc5\x82\xc3\xb3" "dka", "lodka"));
        check (tw::similarity ("\xc5\x82\xc3\xb3" "dka", "lodka") == 0.6, "\xc5\x82 to jeden znak, nie dwa bajty", b);
    }

    tw::PrompterText text;
    text.set (kScript);

    // ---- 1b. slowa, ktorych model nie zna -----------------------------------------
    std::printf ("\n  1b. zamienniki slow nieznanych modelowi\n");
    {
        // tablica symboli jak w graph/Gr.fst: nazwa, available, size, (dlugosc, slowo, klucz)...
        std::string fst = "naglowek-fst exp/lgraph/words.txt";
        const std::vector<std::string> w { "<eps>", "[unk]", "urodzilismy", "najlepszymi", "kot", "#0" };
        auto i64 = [&] (int64_t v) { fst.append ((const char*) &v, 8); };
        auto i32 = [&] (int32_t v) { fst.append ((const char*) &v, 4); };
        i64 ((int64_t) w.size()); i64 ((int64_t) w.size());
        for (size_t k = 0; k < w.size(); ++k) { i32 ((int32_t) w[k].size()); fst += w[k]; i64 ((int64_t) k); }
        std::ofstream ("/tmp/zs-test-Gr.fst", std::ios::binary) << fst;

        const auto model = tw::readModelWords ("/tmp/zs-test-Gr.fst");
        std::snprintf (b, sizeof (b), "%zu slow (bez <eps>, [unk], #0)", model.size());
        check (model.size() == 3, "lista slow modelu czytana z Gr.fst", b);

        const auto subs = tw::substitutes ({ "kot", "urodzilysmy", "najtepszymi" }, model);
        std::string got;
        for (auto& sb : subs) got += sb.first + "->" + sb.second + " ";
        check (subs.size() == 2 && subs[0].second == "urodzilismy" && subs[1].second == "najlepszymi",
               "nieznane slowo dostaje najblizszy znany zamiennik", got);

        tw::PrompterText t;
        t.set ("Nie bedziemy repetowac zadnej zimy");
        tw::WordTracker tr;
        tr.begin (&t);
        tr.addAlias ("reperowac", "repetowac");
        tr.hypothesis (1.0, { "nie", "bedziemy" });
        tr.hypothesis (1.4, { "nie", "bedziemy", "ratowac" });
        const int before = tr.lastHitWord();
        tr.hypothesis (1.8, { "nie", "bedziemy", "reperowac" });
        std::snprintf (b, sizeof (b), "przed %d, po %d", before, tr.lastHitWord());
        check (before == 1 && tr.lastHitWord() == 2, "uslyszany zamiennik trafia w oryginal", b);
    }

    // ---- 1b. muzyka w pokoju (24.10): sylaby tylko zaraz po trafionych slowach ----
    {
        tw::PrompterText t;
        t.set ("Nie bedziemy repetowac zadnej zimy ani lata");
        tw::WordTracker tr;
        tr.begin (&t);
        check (! tr.syllablesTrusted (5.0), "przed pierwszym slowem sylaby nie przesuwaja (muzyka na starcie)", "");
        tr.hypothesis (10.0, { "nie", "bedziemy" });
        check (tr.syllablesTrusted (12.0) && tr.syllablesTrusted (16.0), "do 6 s po trafionym slowie sylaby jada dalej", "");
        check (! tr.syllablesTrusted (16.5), "potem stoja, az Vosk znow cos trafi", "");
        tr.endUtterance();
        tr.hypothesis (20.0, { "repetowac", "zadnej", "zimy" });
        check (tr.syllablesTrusted (21.0), "nowe trafienie znow pozwala", "");
    }

    // ---- 2. zasady dopasowania ----------------------------------------------------
    std::printf ("\n  2. zasady\n");
    {
        tw::WordTracker t;
        t.begin (&text);

        const auto vocab = t.vocabulary();
        std::snprintf (b, sizeof (b), "%zu slow w slowniku, %d w tekscie", vocab.size(), text.wordCount());
        check (! vocab.empty() && (int) vocab.size() < text.wordCount(), "slownik: kazde slowo raz", b);

        t.hypothesis (1.0, { "dzisiaj", "jest" });
        check (t.lastHitWord() == 1 && t.started(), "pierwsze slowa trafiaja od poczatku", std::to_string (t.lastHitWord()));

        t.hypothesis (1.2, { "dzisiaj", "jest", "[unk]" });
        check (t.lastHitWord() == 1, "[unk] nic nie przesuwa", std::to_string (t.lastHitWord()));

        // "odcinek" stoi 4 slowa dalej — w oknie
        t.hypothesis (1.5, { "dzisiaj", "jest", "[unk]", "odcinek" });
        check (t.lastHitWord() == 6, "slowo 4 dalej trafia (okno do +8)", std::to_string (t.lastHitWord()));

        // "stołem" stoi 12 dalej — poza oknem, nie wolno skoczyc
        t.hypothesis (1.8, { "dzisiaj", "jest", "[unk]", "odcinek", "sto\xc5\x82" "em" });
        check (t.lastHitWord() == 6, "slowo 12 dalej nie trafia (poza oknem)", std::to_string (t.lastHitWord()));

        // krotkie slowa nie zgaduja: "kat" != "kot"
        tw::WordTracker s;
        s.begin (&text);
        s.jumpTo (7, 0.0);
        s.hypothesis (0.5, { "kat" });
        check (s.hitCount() == 0, "krotkie slowo musi byc dokladne (kat != kot)", std::to_string (s.hitCount()));
        s.hypothesis (0.6, { "kat", "siedzia" });
        check (s.lastHitWord() == 8, "dluzsze podobne >= 0,8 trafia (siedzia ~ siedzi)", std::to_string (s.lastHitWord()));

        // tempo: absurdalnie szybko -> 5 slow/s
        tw::WordTracker f;
        ciaglaMowa (f);
        f.begin (&text);
        std::vector<std::string> hyp;
        for (int i = 0; i < 10; ++i) { hyp.push_back (tw::spokenKey (text.wordText (i))); f.hypothesis (0.1 + i * 0.06, hyp); }
        std::snprintf (b, sizeof (b), "%.1f slow/s", f.wordsPerSec());
        check (f.wordsPerSec() <= 5.0, "tempo nie wyjdzie ponad 5 slow/s", b);

        // w pauzie (licznik sylab mowi: cisza) pozycja stoi
        {
            tw::WordTracker q;
            ciaglaMowa (q);
            q.begin (&text);
            std::vector<std::string> hh;
            for (int i = 0; i < 6; ++i) { hh.push_back (tw::spokenKey (text.wordText (i))); q.speaking (0.1 + i * 0.4, true); q.hypothesis (0.1 + i * 0.4, hh); }
            const double t0 = 0.1 + 5 * 0.4;
            for (double t = t0; t < t0 + 3.0; t += 0.1) q.speaking (t, false);
            const double still = q.position (t0 + 3.0);
            for (double t = t0 + 3.0; t < t0 + 3.5; t += 0.1) q.speaking (t, true);
            std::snprintf (b, sizeof (b), "po 3 s ciszy %.2f, po 0,5 s mowy %.2f (trafienie na 5)", still, q.position (t0 + 3.5));
            check (q.position (t0 + 3.5) > still && still < 5.0 + q.wordsPerSec() * (q.params.latencySec + 0.11),
                   "w pauzie jazda stoi, w mowie rusza", b);
        }

        // jazda z tempem tylko przez 1,5 s
        const double p1 = f.position (0.1 + 9 * 0.06 + 1.5), p2 = f.position (0.1 + 9 * 0.06 + 10.0);
        std::snprintf (b, sizeof (b), "po 1,5 s %.1f, po 10 s %.1f", p1, p2);
        check (std::fabs (p1 - p2) < 1e-9, "po 1,5 s bez trafienia pozycja stoi", b);
    }

    // ---- 2b. przejscie do sylab i rezerwa ------------------------------------------
    std::printf ("\n  2b. slowa -> sylaby, licznik sylab jako rezerwa\n");
    {
        const auto& w3 = text.all()[3];
        std::snprintf (b, sizeof (b), "slowo 3,5 -> %.2f sylaby (slowo 3 zaczyna sie na %d, ma %d)",
                       tw::syllableAtWord (text, 3.5), w3.beforeMe, w3.syllables);
        check (std::fabs (tw::syllableAtWord (text, 3.5) - (w3.beforeMe + 0.5 * w3.syllables)) < 1e-9,
               "pozycja w slowach przechodzi na sylaby liniowo", b);

        // Follower dostaje pewna pozycje, potem jedzie sam dalej od niej
        tw::Follower f;
        f.begin (&text);
        f.syncTo (text.all()[40].beforeMe);
        std::snprintf (b, sizeof (b), "slowo %d", f.currentWord());
        check (f.currentWord() == 40, "licznik sylab przejmuje pozycje ze slow", b);

        f.syncTo (text.all()[20].beforeMe);
        std::snprintf (b, sizeof (b), "slowo %d", f.currentWord());
        check (f.currentWord() == 20, "i moze sie cofnac, gdy slowa tak mowia", b);
    }

    // ---- 3. czytanie calego tekstu ------------------------------------------------
    std::printf ("\n  3. caly tekst, opoznienie 1,2 s (wejscie symulowane)\n");

    const auto sp = speak (text, 1.0);
    const Errors none {};
    const Errors noisy { 0.07, 0.08, 0.05, 0.08 };

    double clean2 = 0;
    {
        tw::WordTracker t;
        ciaglaMowa (t);
        const auto r = run (text, t, sp, none, 1.2, 1);
        clean2 = r.within2;
        std::snprintf (b, sizeof (b), "<=1: %.0f%%   <=2: %.0f%%", r.within1, r.within2);
        check (r.within2 >= 85.0, "bez pomylek rozpoznawania", b);
    }

    double noisy2 = 0, noCoast2 = 0;
    {
        double s1 = 0, s2 = 0;
        for (uint32_t seed = 1; seed <= 20; ++seed)
        {
            tw::WordTracker t;
            ciaglaMowa (t);
            const auto r = run (text, t, sp, noisy, 1.2, seed);
            s1 += r.within1; s2 += r.within2;
        }
        noisy2 = s2 / 20;
        std::snprintf (b, sizeof (b), "<=1: %.0f%%   <=2: %.0f%% (srednia z 20)", s1 / 20, s2 / 20);
        check (noisy2 >= 85.0, "z pomylkami (7% zgub., 8% zamian, 5% unk)", b);
    }

    {
        double s2 = 0;
        for (uint32_t seed = 1; seed <= 20; ++seed)
        {
            tw::WordTracker t;
            ciaglaMowa (t);
            t.params.latencySec = 0.0;
            s2 += run (text, t, sp, noisy, 1.2, seed).within2;
        }
        noCoast2 = s2 / 20;
        std::snprintf (b, sizeof (b), "bez kompensacji <=2: %.0f%%, z nia %.0f%%", noCoast2, noisy2);
        check (noisy2 > noCoast2 + 5.0, "kompensacja opoznienia pomaga", b);
    }

    // ---- 4. przeskok w tekscie --------------------------------------------------
    std::printf ("\n  4. czlowiek przeskakuje akapit\n");
    {
        const int skipAt = text.wordCount() / 3, skipTo = skipAt + 25;
        const auto jumped = speak (text, 1.0, skipAt, skipTo);

        tw::WordTracker t;
        ciaglaMowa (t);
        const auto r = run (text, t, jumped, none, 1.2, 3, skipAt);

        // Wiecej niz 4 s: w tym tekscie "oddech" stoi dwa razy (slowa 34 i 67),
        // wiec przeczytane po przeskoku trafia jeszcze w stare okno i zegar
        // 4 s liczy sie od nowa. Tak dziala zasada — tekst jest niejednoznaczny.
        std::snprintf (b, sizeof (b), "po %.1f s od przeskoku; potem <=2 slowa w %.1f%% chwil (bez przeskoku %.1f%%)",
                       r.resyncAfterSkipSec, r.after2, clean2);
        check (r.resyncs >= 1 && r.resyncAfterSkipSec > 0 && r.after2 >= clean2 - 10.0,
               "szeroki resync odnajduje miejsce", b);
    }

    std::printf ("\n%s\n\n", fails == 0 ? "wszystko przeszlo" : "SA BLEDY");
    return fails;
}
