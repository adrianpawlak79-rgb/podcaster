/*
    MontageRender.h — wykonanie planu montazu na dzwieku.

    Planer (MontagePlanner.h) mowi, CO wyciac. Ten plik robi to tak, zeby nie
    bylo slychac. To sa dwa zupelnie rozne zadania i drugie jest trudniejsze:
    lista ciec jest poprawna albo nie, a szew jest slyszalny albo nie, i o tym
    drugim decyduja rzeczy, ktorych w planie nie ma.

    Trzy zasady, na ktorych to stoi:

    1. NIE TNIEMY TAM, GDZIE KAZE PLAN — tniemy w najcichszym punkcie BLISKO
       tego miejsca. Granica frazy wyznaczona przez detektor lezy tam, gdzie
       ucichl glos, a to zwykle jeszcze nie jest cisza: konczy sie ogon "s",
       dochodzi oddech. Ciecie w tym miejscu slychac zawsze. Szukamy wiec dolka
       w promieniu kilkuset milisekund i tniemy w nim.

    2. GDY DOLKA NIE MA, CIECIE SIE NIE ODBYWA. Jesli w calym oknie poszukiwan
       nie ma miejsca dosc cichego, znaczy to, ze fraza przechodzi w nastepna
       bez przerwy — i wtedy kazde ciecie bedzie slychac. Lepiej zostawic
       material dluzszym, niz oddac nagranie ze szwem w srodku slowa. Takie
       przypadki wracaja w raporcie, nie znikaja po cichu.

    3. KAZDY SZEW JEST ZMIERZONY I OPISANY. Program sam sprawdza, czy po obu
       stronach sklejenia tlo ma ten sam poziom i te sama barwe, i mowi, gdy
       nie ma. To jest ta sama zasada, co w calym programie: nie "zaufaj mi",
       tylko liczba, ktora da sie sprawdzic.

    Nic nie jest kasowane. Raport niesie czasy ze ZRODLA, wiec kazde ciecie da
    sie cofnac, a odrzucone podejscia zostaja w pliku sesji.
*/

#pragma once

#include "MontagePlanner.h"
#include "Mix/Biquad.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace tw
{

//==============================================================================
struct Seam
{
    double atSec        = 0.0;    ///< gdzie w WYNIKU
    double fromSec      = 0.0;    ///< co sklejono: koniec lewej strony w zrodle
    double toSec        = 0.0;    ///< i poczatek prawej
    double levelStepDb  = 0.0;    ///< roznica tla po obu stronach
    double tiltStepDb   = 0.0;    ///< roznica barwy tla (gora minus dol)
    bool   audible      = false;  ///< czy szew moze byc slyszalny
    std::string note;
};

struct RenderReport
{
    double originalSec = 0.0;
    double renderedSec = 0.0;
    int    cutsMade    = 0;
    int    cutsRefused = 0;       ///< nie bylo gdzie ciac
    std::vector<Seam> seams;
    std::vector<std::string> refusals;
};

//==============================================================================
class MontageRender
{
public:
    struct Options
    {
        double searchSec    = 0.35;  ///< jak daleko szukamy dolka na ciecie
        double edgeSec      = 0.10;  ///< cisza musi zaczynac sie tak blisko granicy
        double fadeSec      = 0.020; ///< dlugosc przenikania
        double quietWindow  = 0.030; ///< okno, w ktorym liczymy "jak cicho"
        double quietMargin  = 6.0;   ///< dolek musi byc o tyle cichszy od frazy
        double seamLevelDb  = 1.5;   ///< wieksza roznica tla = szew slyszalny
        double seamTiltDb   = 3.0;   ///< ...to samo dla barwy
    };

    Options options;

    /** @param in    cale nagranie sesji
        @param plan  co zostawic, co wyciac
        @param out   wynik */
    RenderReport render (const std::vector<float>& in, double sampleRate,
                         const MontagePlan& plan, std::vector<float>& out)
    {
        sr = sampleRate;
        RenderReport rep;
        rep.originalSec = in.size() / sr;

        if (in.empty() || plan.items.empty())
        {
            out = in;
            rep.renderedSec = rep.originalSec;
            return rep;
        }

        buildEnvelope (in);

        // ---- co zostaje, jako przedzialy w zrodle ---------------------------
        // Plan mowi, co WYCIAC. Latwiej sklejac to, co zostaje, wiec najpierw
        // zamieniamy jedno na drugie.
        // ---- najpierw scal to, co sie nakłada ------------------------------
        // Planer potrafi wystawic dwa elementy na ten sam kawalek: wyciecie
        // powtorzonej frazy i skrocenie przerwy, ktora ta fraza zajmowala.
        // Przetwarzane po kolei, liczyly sie jako dwa ciecia i przesuwaly
        // kursor dwa razy. Suma obszarow jest tym, o co plan naprawde prosi,
        // wiec liczymy ja wprost — a przy okazji raport przestaje klamac o
        // liczbie ciec.
        std::vector<std::pair<double, double>> want;

        for (const auto& it : plan.items)
            if (it.action != Action::Keep && it.endSec > it.startSec)
                want.push_back ({ it.startSec, it.endSec });

        std::sort (want.begin(), want.end());

        std::vector<std::pair<double, double>> merged;

        for (const auto& w : want)
        {
            if (! merged.empty() && w.first <= merged.back().second)
                merged.back().second = std::max (merged.back().second, w.second);
            else
                merged.push_back (w);
        }

        std::vector<std::pair<double, double>> keep;
        double cursor = 0.0;

        for (const auto& region : merged)
        {
            double from = region.first, to = region.second;

            // ---- ciecie w dolku, nie na granicy frazy ------------------------
            const bool okFrom = snapToQuiet (from, in);
            const bool okTo   = snapToQuiet (to, in);

            if (! okFrom || ! okTo)
            {
                ++rep.cutsRefused;

                char b2[160];
                std::snprintf (b2, sizeof (b2),
                               "%02d:%02d — nie ma gdzie ciąć, fraza przechodzi "
                               "w następną bez przerwy",
                               (int) region.first / 60, (int) region.first % 60);
                rep.refusals.push_back (b2);
                continue;
            }

            if (to <= from || from < cursor) continue;

            if (from > cursor)
                keep.push_back ({ cursor, from });

            cursor = to;
            ++rep.cutsMade;
        }

        if (cursor < rep.originalSec)
            keep.push_back ({ cursor, rep.originalSec });

        if (keep.empty())
        {
            out = in;
            rep.renderedSec = rep.originalSec;
            return rep;
        }

        // ---- sklejanie z przenikaniem ---------------------------------------
        assemble (in, keep, out, rep);

        rep.renderedSec = out.size() / sr;
        return rep;
    }

private:
    //--------------------------------------------------------------------------
    /** Poziom w oknach 10 ms — po tym szukamy dolkow. */
    void buildEnvelope (const std::vector<float>& in)
    {
        hop = std::max (1, (int) (sr * 0.010));
        env.clear();

        for (size_t s = 0; s + (size_t) hop <= in.size(); s += (size_t) hop)
        {
            double sum = 0.0;
            for (int i = 0; i < hop; ++i)
                sum += (double) in[s + (size_t) i] * in[s + (size_t) i];

            env.push_back (sum > 1.0e-20 ? 10.0 * std::log10 (sum / hop) : -140.0);
        }

        if (env.empty()) return;

        auto sorted = env;
        std::sort (sorted.begin(), sorted.end());
        floorDb  = sorted[sorted.size() / 10];
        speechDb = sorted[sorted.size() * 9 / 10];
    }

    /** Przesuwa @p t do najcichszego punktu w oknie. Zwraca false, gdy w calym
        oknie nie ma nic dosc cichego — czyli gdy ciecie w tym miejscu bylo by
        cieciem w srodku mowy. */
    bool snapToQuiet (double& t, const std::vector<float>& in)
    {
        if (env.empty()) return false;

        // ---- tylko cisza PRZYLEGAJACA do granicy -------------------------
        // Wczesniej szukalismy najcichszego okna w promieniu 0,35 s. Gdy
        // frazy szly jedna za druga bez przerwy, w tym promieniu zawsze byla
        // jakas przerwa miedzy SLOWAMI w srodku sasiedniej frazy — i montaz
        // tam cial, zamiast odmowic (znany blad montage-test, 3 z 19). Teraz
        // ciecie wolno tylko w ciagu cichych blokow, ktory zaczyna sie nie
        // dalej niz edgeSec od zadanej granicy i miesci przenikanie.
        const double quiet = floorDb + (speechDb - floorDb) * 0.35;
        const int centre = (int) (t * sr / hop);
        const int tol    = std::max (1, (int) (options.edgeSec * sr / hop));
        const int need   = std::max (1, (int) (options.fadeSec * sr / hop));
        const int half   = need;
        const int last   = (int) env.size() - 1;

        int runLo = -1, runHi = -1;

        for (int i = std::max (0, centre - tol); i <= std::min (last, centre + tol); ++i)
        {
            if (env[(size_t) i] > quiet) continue;

            int a = i, b = i;
            while (a > 0 && env[(size_t) (a - 1)] <= quiet) --a;
            while (b < last && env[(size_t) (b + 1)] <= quiet) ++b;

            if (b - a > runHi - runLo) { runLo = a; runHi = b; }
            i = b;
        }

        if (runLo < 0 || runHi - runLo + 1 < 2 * half + 1) return false;

        // w tym ciagu: okno, w ktorym najglosniejszy blok jest najcichszy,
        // najblizej granicy (nie dalej niz searchSec)
        const int span = std::max (1, (int) (options.searchSec * sr / hop));
        int best = -1;
        double bestDb = 1.0e9;

        for (int c = std::max (runLo + half, centre - span); c <= std::min (runHi - half, centre + span); ++c)
        {
            double worst = -1.0e9;
            for (int i = c - half; i <= c + half; ++i)
                worst = std::max (worst, env[(size_t) i]);

            if (worst < bestDb - 0.01 || (std::fabs (worst - bestDb) <= 0.01 && std::abs (c - centre) < std::abs (best - centre)))
            { bestDb = worst; best = c; }
        }

        if (best < 0) return false;

        t = (double) best * hop / sr;
        (void) in;
        return true;
    }

    //--------------------------------------------------------------------------
    void assemble (const std::vector<float>& in,
                   const std::vector<std::pair<double, double>>& keep,
                   std::vector<float>& out, RenderReport& rep)
    {
        const int fade = std::max (1, (int) (options.fadeSec * sr));
        out.clear();
        out.reserve (in.size());

        for (size_t k = 0; k < keep.size(); ++k)
        {
            const size_t from = (size_t) std::max (0.0, keep[k].first * sr);
            const size_t to   = (size_t) std::min ((double) in.size(), keep[k].second * sr);

            if (to <= from) continue;

            if (out.empty())
            {
                out.insert (out.end(), in.begin() + (long) from, in.begin() + (long) to);
                continue;
            }

            // ---- przenikanie rownej mocy ------------------------------------
            // Tlo pokoju po obu stronach szwu to dwa NIEZALEZNE szumy. Przy
            // przenikaniu liniowym ich suma ma w polowie o 3 dB za malo mocy i
            // slychac to jako dziurke. Przenikanie sinus/kosinus trzyma moc
            // sumy stala i wlasnie dlatego jest tu wlasciwe.
            const int n = std::min ((int) fade, (int) std::min (out.size(), to - from));
            const size_t start = out.size() - (size_t) n;

            const double seamAt = (double) start / sr;
            Seam seam;
            seam.atSec = seamAt;
            seam.fromSec = keep[k - 1].second;
            seam.toSec = keep[k].first;
            measureSeam (in, keep[k - 1].second, keep[k].first, seam);
            rep.seams.push_back (seam);

            for (int i = 0; i < n; ++i)
            {
                const double x = (double) i / n;
                const double a = std::cos (x * juce_halfPi());
                const double b = std::sin (x * juce_halfPi());

                out[start + (size_t) i] = (float) (out[start + (size_t) i] * a
                                                 + in[from + (size_t) i] * b);
            }

            out.insert (out.end(), in.begin() + (long) (from + (size_t) n),
                                   in.begin() + (long) to);
        }
    }

    static double juce_halfPi() { return 1.5707963267948966; }

    //--------------------------------------------------------------------------
    /** Czy po obu stronach sklejenia tlo jest takie samo — poziomem i barwa.

        To jest to, czego zaden montazysta nie sprawdza recznie, a co decyduje
        o tym, czy latka slychac. Dwa fragmenty z tego samego pokoju, nagrane w
        odstepie minuty, potrafia miec tlo rozne o kilka decybeli: ktos wlaczyl
        lodowke, ktos przesunal krzeslo. Poziom da sie wyrownac, barwy juz nie —
        i wtedy uczciwiej jest o tym powiedziec. */
    void measureSeam (const std::vector<float>& in, double leftEnd, double rightStart, Seam& seam)
    {
        const double win = 0.12;

        const double lDb = levelIn (in, leftEnd - win, leftEnd);
        const double rDb = levelIn (in, rightStart, rightStart + win);

        const double lTilt = tiltIn (in, leftEnd - win, leftEnd);
        const double rTilt = tiltIn (in, rightStart, rightStart + win);

        seam.levelStepDb = rDb - lDb;
        seam.tiltStepDb = rTilt - lTilt;

        const bool levelBad = std::fabs (seam.levelStepDb) > options.seamLevelDb;
        const bool tiltBad  = std::fabs (seam.tiltStepDb) > options.seamTiltDb;

        seam.audible = levelBad || tiltBad;

        if (levelBad && tiltBad)
            seam.note = "tło po obu stronach różni się poziomem i barwą";
        else if (levelBad)
            seam.note = "tło po jednej stronie jest głośniejsze";
        else if (tiltBad)
            seam.note = "tło po obu stronach ma inną barwę";
        else
            seam.note = "szew niesłyszalny";
    }

    double levelIn (const std::vector<float>& in, double fromSec, double toSec) const
    {
        const size_t a = (size_t) std::max (0.0, fromSec * sr);
        const size_t b = (size_t) std::min ((double) in.size(), toSec * sr);

        double sum = 0.0;
        size_t n = 0;

        for (size_t i = a; i < b; ++i) { sum += (double) in[i] * in[i]; ++n; }

        return n && sum > 0.0 ? 10.0 * std::log10 (sum / n) : -140.0;
    }

    /** Barwa tla jednym parametrem: ile jest gory wobec dolu. */
    double tiltIn (const std::vector<float>& in, double fromSec, double toSec) const
    {
        const size_t a = (size_t) std::max (0.0, fromSec * sr);
        const size_t b = (size_t) std::min ((double) in.size(), toSec * sr);

        if (b <= a + 64) return 0.0;

        mix::Biquad lo, hi;
        lo.setCoeffs (mix::design::lowpass (700.0, sr, 0.7071));
        hi.setCoeffs (mix::design::highpass (2500.0, sr, 0.7071));

        double sumLo = 0.0, sumHi = 0.0;

        for (size_t i = a; i < b; ++i)
        {
            const float l = lo.process (in[i]);
            const float h = hi.process (in[i]);
            sumLo += (double) l * l;
            sumHi += (double) h * h;
        }

        const double n = (double) (b - a);
        const double dbLo = sumLo > 1.0e-20 ? 10.0 * std::log10 (sumLo / n) : -140.0;
        const double dbHi = sumHi > 1.0e-20 ? 10.0 * std::log10 (sumHi / n) : -140.0;

        return dbHi - dbLo;
    }

    //--------------------------------------------------------------------------
    double sr = 48000.0, floorDb = -60.0, speechDb = -20.0;
    int hop = 480;
    std::vector<double> env;
};

} // namespace tw
