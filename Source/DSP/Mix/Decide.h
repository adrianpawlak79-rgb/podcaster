/*
    Decide.h — program slucha nagrania i sam ustawia lancuch.

    ---------------------------------------------------------------------------
    SKAD TO SIE WZIELO

    Przez cztery rundy odsluchu szukalismy z czlowiekiem ustawien dla jednego
    wiersza. Znalezlismy — i po drodze okazalo sie, ze domyslne ustawienia byly
    dla tego materialu zle w trzech niezaleznych miejscach naraz. Zajelo to
    wieczor, dwoje uszu i kilkanascie pomiarow.

    Podcaster tego nie zrobi. Nie dlatego, ze nie umie — dlatego, ze nie ma po
    co. Chce nacisnac jeden przycisk. Jesli program wymaga, zeby ktos rozumial,
    czym rozni sie "leveler" od "peakera", to program nie jest skonczony.

    Wiec: wszystkie liczby, ktore wtedy zmierzylismy recznie, program mierzy
    sam. Ten plik jest tylko miejscem, w ktorym z pomiarow robia sie decyzje.

    ---------------------------------------------------------------------------
    NAJWAZNIEJSZE ROZROZNIENIE: GEST CZY WEDROWKA

    Kompresja i jazda suwakiem sluza do tego, zeby wyrownac poziom. Ale nie
    kazde wahanie poziomu jest usterka:

        WEDROWKA   powolne odjezdzanie przez dziesiatki sekund. Mikrofon stoi
                   coraz dalej, glos sie meczy, drugi rozmowca mowi ciszej.
                   To jest usterka i wolno ja poprawic.

        GEST       zmiana w obrebie frazy, zwiazana z tym, CO czlowiek mowi.
                   Sciszenie przed puenta, krzyk w refrenie. To jest tresc i
                   nie wolno jej ruszac.

    Obie daja ten sam rozrzut na miernku glosnosci, wiec przez lata mierzylo
    sie je razem i traktowalo tak samo. Rozdzielaja sie, gdy rozlozyc przebieg
    glosnosci na skladowa wolna (mediana ruchoma 30 s) i to, co zostaje.
    Zmierzone:

        mowa rowna (syntezator)           wedrowka 0,1 dB   gest  3,6 dB
        ta sama, z odjazdem 10 dB         wedrowka 6,2 dB   gest  3,9 dB
        ta sama, ze skokami co 8 s        wedrowka 3,5 dB   gest 13,6 dB
        wiersz czlowieka                  wedrowka 8,5 dB   gest 37,2 dB
        jego wokal z piosenki             wedrowka 6,7 dB   gest 22,9 dB

    Program jezdzi suwakiem dokladnie tyle, ile wynosi wedrowka — i ani
    decybela wiecej. Poprzednia wersja miala na sztywno 9 dB zakresu i okno
    400 ms, wiec goniła gesty. Stad braly sie skargi "przytlumione" i
    "za agresywne": program wyrownywal wykonanie.

    ---------------------------------------------------------------------------
    CZEGO TU NIE MA

    Nie ma gustu. Kazda decyzja ma liczbe, ktora o niej zdecydowala, i zdanie,
    ktore mozna przeczytac. Jesli ktos sie nie zgadza, widzi z czym.
*/

#pragma once

#include "Reference.h"
#include "SpeechChain.h"
#include "Tame.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace mix
{

//==============================================================================
struct Decision
{
    SpeechChain::Params params;

    /** Po jednym zdaniu na decyzje, z liczba, ktora ja podjela. */
    std::vector<std::string> because;

    // ---- co zmierzono
    double lufs        = 0.0;
    double plr         = 0.0;
    double driftDb     = 0.0;   ///< monotoniczna wedrowka: ile poziom odjechal
    double sectionDb   = 0.0;   ///< wahanie miedzy czesciami utworu
    double gestureDb   = 0.0;   ///< szybka zmiana w obrebie fraz
    double tameWouldDb = 0.0;   ///< ile sybilantow zdjelo by tlumienie odchylen
    double quietPct    = 0.0;
    double noiseFloorDb = -140.0;
    double speechDb     = -140.0;

    double freeTargetLufs = -16.0;  ///< najglosniej bez ruszania szczytu
    double squashDb       = 0.0;    ///< ile trzeba bedzie sciagnac przy celu
};

//==============================================================================
class Decide
{
public:
    /** Do czego to nagranie jedzie. Pusto = program wybiera sam. */
    enum class Destination { Auto, Podcast, YouTube, Wlasne };

    struct Wish
    {
        Wish() {}
        Destination where = Destination::Auto;
        double ceilingDbTp = -1.0;
    };

    //==========================================================================
    static Decision listen (const std::vector<float>& in, double sr, Wish wish = {})
    {
        Decision d;
        char b[400];

        const auto profile = Reference::measure (in, sr);

        d.lufs     = profile.lufs;
        d.plr      = profile.plr;
        d.quietPct = profile.quietPct;

        levelShape (in, sr, d.driftDb, d.sectionDb, d.gestureDb);

        d.noiseFloorDb = percentileBlock (in, sr, 0.10);
        d.speechDb     = percentileBlock (in, sr, 0.95);

        d.tameWouldDb = tameDepth (in, sr);

        d.freeTargetLufs = Reference::loudestUntouched (profile, wish.ceilingDbTp);

        auto& p = d.params;
        p.ceilingDbTp = (float) wish.ceilingDbTp;
        p.deHum = false; // No hum measurement in this decision path.
        p.useSaturation = false; // Colour is not a repair.


        // ---- 1. glosnosc ------------------------------------------------------
        switch (wish.where)
        {
            case Destination::Podcast: p.targetLufs = -16.0; break;
            case Destination::YouTube: p.targetLufs = -14.0; break;
            case Destination::Wlasne:  p.targetLufs = d.freeTargetLufs; break;

            case Destination::Auto:
            default:
                // Bez deklaracji celujemy tam, gdzie nic nie trzeba scinac —
                // ale nie ciszej niz -23 LUFS, bo ponizej tego nagranie zaczyna
                // wymagac od sluchacza podkrecania glosnika.
                p.targetLufs = std::clamp (d.freeTargetLufs, -23.0, -14.0);
                break;
        }

        d.squashDb = std::max (0.0, p.targetLufs - d.freeTargetLufs);

        std::snprintf (b, sizeof (b),
            "Głośność: %.1f LUFS. Zapas między szczytem a głośnością wynosi %.1f dB, "
            "więc przy suficie %.1f dBTP najgłośniej bez ruszania najgłośniejszego "
            "momentu jest %.1f LUFS.",
            p.targetLufs, d.plr, wish.ceilingDbTp, d.freeTargetLufs);
        d.because.push_back (b);

        if (d.squashDb > 0.5)
        {
            std::snprintf (b, sizeof (b),
                "Cel jest o %.1f dB powyżej granicy dla surowego nagrania. "
                "Końcowe ograniczenie szczytów zależy także od pozostałych korekt.", d.squashDb);
            d.because.push_back (b);
        }

        // ---- 2. jazda suwakiem ------------------------------------------------
        //
        // Suwak prostuje WEDROWKE, czyli to, co odjezdza w jedna strone przez
        // cale nagranie: mikrofon, zmeczenie glosu, drugi rozmowca siedzacy
        // dalej. Nie prostuje ani gestu, ani roznicy miedzy zwrotka a refrenem.
        //
        // Rozroznienie jest mierzalne. Wolna skladowa przebiegu glosnosci ma w
        // sobie dwie rzeczy: prosta (trend) i to, co wokol niej oscyluje.
        // Zmierzone:
        //
        //     syntetyczny odjazd 10 dB     trend -8,0 dB   oscylacja 0,8 dB
        //     skoki co 8 sekund            trend -1,9 dB   oscylacja 2,8 dB
        //     wiersz czlowieka             trend -4,8 dB   oscylacja 7,6 dB
        //     jego wokal z piosenki        trend -1,3 dB   oscylacja 6,8 dB
        //
        // Piosenka ma prawie caly ruch w oscylacji, bo to zwrotki i refreny.
        // Sam rozrzut wolnej skladowej nie odroznilby jej od mikrofonu, ktory
        // odjechal — trend odroznia.
        if (d.driftDb < 1.5)
        {
            p.useRider = false;

            std::snprintf (b, sizeof (b),
                "Bez jazdy suwakiem: poziom nie odjeżdża (%.1f dB przez całe nagranie). "
                "Wahania, które słychać, to budowa nagrania (%.1f dB) i gesty w obrębie "
                "fraz (%.1f dB) — jedno i drugie zostaje.",
                d.driftDb, d.sectionDb, d.gestureDb);
            d.because.push_back (b);
        }
        else
        {
            p.useRider       = true;
            p.riderRangeDb   = (float) std::clamp (d.driftDb, 1.5, 9.0);

            // Okno osiem sekund i 0,3 dB na sekunde: przejscie calego zakresu
            // trwa wtedy kilkanascie sekund, wiec suwak nie ma jak pojsc za
            // zdaniem, nawet gdyby chcial.
            p.riderWindowMs  = 8000.0;
            p.riderSlopeDbS  = 0.3f;

            std::snprintf (b, sizeof (b),
                "Powolne wyrównanie do %.1f dB, najwyżej 0,3 dB na sekundę. "
                "Zmiany we frazach: %.1f dB, między częściami: %.1f dB. "
                "Automat nie rozpoznaje intencji; porównaj spokojne i głośne fragmenty.",
                p.riderRangeDb, d.gestureDb, d.sectionDb);
            d.because.push_back (b);
        }

        // ---- 3. kompresja -----------------------------------------------------
        if (d.squashDb < 1.0)
        {
            p.levelerThreshDb = -18.0f;  p.levelerRatio = 1.5f;
            p.peakThreshDb    = -8.0f;   p.peakRatio    = 2.5f;

            std::snprintf (b, sizeof (b),
                "Kompresja lekka (%.1f:1 od %.0f dBFS): przy tym celu nie trzeba nic "
                "mocno ograniczać szczytów. Sprawdź, czy odpowiada Ci wyrównanie głosu.",
                p.levelerRatio, p.levelerThreshDb);
            d.because.push_back (b);
        }
        else if (d.squashDb < 4.0)
        {
            p.levelerThreshDb = -20.0f;  p.levelerRatio = 2.0f;
            p.peakThreshDb    = -10.0f;  p.peakRatio    = 3.0f;

            std::snprintf (b, sizeof (b),
                "Kompresja średnia: %.1f dB do nadrobienia między tym, co jest, "
                "a żądaną głośnością.", d.squashDb);
            d.because.push_back (b);
        }
        else
        {
            p.levelerThreshDb = -24.0f;  p.levelerRatio = 2.5f;
            p.peakThreshDb    = -12.0f;  p.peakRatio    = 4.0f;

            std::snprintf (b, sizeof (b),
                "Kompresja mocna: %.1f dB do nadrobienia. Przy takim ściskaniu nagranie "
                "przestaje oddychać — jeśli dynamika jest tu treścią, lepiej obniżyć cel "
                "niż to wyrównać.", d.squashDb);
            d.because.push_back (b);
        }

        const float inputOffset = (float)std::clamp(d.speechDb + 18.0, -40.0, 20.0);
        p.levelerThreshDb += inputOffset;
        p.peakThreshDb += inputOffset;
        std::snprintf(b,sizeof(b),"Progi kompresji dopasowane do poziomu nagrania: %.1f i %.1f dBFS.",
                      p.levelerThreshDb,p.peakThreshDb);
        d.because.push_back(b);

        // ---- 4. sybilanty -----------------------------------------------------
        //
        // Miara jest prosta: ile decybeli tlumienie odchylen ZDJELOBY z zakresu
        // 4-14 kHz przy ustawieniach domyslnych. Nie "jak jasne jest nagranie" —
        // stala barwa nie jest odchyleniem i nie ma tu nic do rzeczy.
        p.useTame    = true;
        p.useDeEsser = false;

        if (d.tameWouldDb > 6.0)
        {
            p.tameThreshDb = 5.0f;  p.tameMaxCutDb = -12.0f;
            p.useDeEsser   = true;  p.essThreshDb = -8.0f; p.essMaxCutDb = -4.0f;

            std::snprintf (b, sizeof (b),
                "Sybilanty ostre: przy zwykłych ustawieniach zeszłoby z nich %.1f dB. "
                "Włączam tłumienie odchyleń mocniej i dokładam łagodną półkę.",
                d.tameWouldDb);
            d.because.push_back (b);
        }
        else if (d.tameWouldDb > 2.5)
        {
            p.tameThreshDb = 6.0f;  p.tameMaxCutDb = -10.0f;

            std::snprintf (b, sizeof (b),
                "Sybilanty zwyczajne (%.1f dB do zdjęcia): samo tłumienie odchyleń, "
                "bez dodatkowego de-essera. Sprawdź brzmienie głosek w odsłuchu.", d.tameWouldDb);
            d.because.push_back (b);
        }
        else
        {
            p.tameThreshDb = 8.0f;  p.tameMaxCutDb = -6.0f;

            std::snprintf (b, sizeof (b),
                "Sybilanty łagodne (%.1f dB): tłumienie ledwo dotyka, ale zostaje "
                "włączone — pojedyncze ostre „s” zdarza się każdemu.", d.tameWouldDb);
            d.because.push_back (b);
        }

        // ---- 5. szum i brama --------------------------------------------------
        const double headroom = d.speechDb - d.noiseFloorDb;

        if (d.noiseFloorDb < -100.0)
        {
            // Tlo ponizej -100 dBFS nie jest cichym pokojem, tylko cisza
            // cyfrowa: material syntetyczny, juz zbramkowany albo wyciszony.
            p.useDenoise = false;

            d.because.push_back (
                "Bez odszumiania: w przerwach jest cisza cyfrowa, a nie tło pokoju. "
                "To nagranie było już czymś przetworzone albo nie pochodzi z mikrofonu.");
        }
        else if (headroom < 12.0)
        {
            p.useDenoise = false;
            d.because.push_back("Nie znalazłem wyraźnie cichszych przerw. Bez pewnego profilu tła nie odszumiam automatycznie.");
        }
        else if (headroom > 45.0)
        {
            p.useDenoise = false;

            std::snprintf (b, sizeof (b),
                "Bez odszumiania: tło jest %.0f dB pod mową, czyli poniżej progu "
                "słyszalności w nagraniu. Każde przetwarzanie widmowe czymś płaci, "
                "a tu nie ma czego kupić.", headroom);
            d.because.push_back (b);
        }
        else
        {
            p.useDenoise = true;
            p.denoiseDb  = headroom < 30.0 ? -18.0f : -14.0f;

            std::snprintf (b, sizeof (b),
                "Odszumianie do %.0f dB: tło jest tylko %.0f dB pod mową.",
                -p.denoiseDb, headroom);
            d.because.push_back (b);
        }

        // ---- 6. brama ---------------------------------------------------------
        //
        // Prog bramy jako stala liczba jest zgadywaniem: -45 dBFS jest w sam raz
        // w cichym pokoju z dobrym mikrofonem, a w mieszkaniu przy ulicy lezy
        // PONIZEJ tla i brama nie robi wtedy nic. Wiec liczy sie go z nagrania:
        // osiem decybeli nad tlem, ale nie blizej niz dwadziescia pod mowa.
        if (headroom > 12.0 && d.noiseFloorDb > -100.0)
        {
            p.useGate      = true;
            p.autoGate     = false;
            p.gateThreshDb = (float) std::clamp (
                std::min (d.noiseFloorDb + 8.0, d.speechDb - 20.0), -70.0, -20.0);

            std::snprintf (b, sizeof (b),
                "Brama przy %.0f dBFS. Ciche fragmenty mają około %.0f dBFS, "
                "a głośne około %.0f dBFS. Sprawdź końcówki słów.",
                p.gateThreshDb, d.noiseFloorDb, d.speechDb);
            d.because.push_back (b);
        }
        else
        {
            p.useGate = false;

            // Dwa zupelnie rozne powody, ktore dotad mialy jedno zdanie — i to
            // zdanie bylo przy drugim z nich falszywe. Przy ciszy cyfrowej
            // program pisal "miedzy mowa a tlem jest 185 dB, za malo, zeby
            // ustawic prog". Liczba byla prawdziwa, wniosek odwrotny do niej, a
            // czlowiek, ktory to czyta, przestaje wierzyc takze w te zdania,
            // ktore sa w porzadku.
            if (d.noiseFloorDb <= -100.0)
                std::snprintf (b, sizeof (b),
                    "Bez bramy: w przerwach jest cisza cyfrowa (%.0f dBFS), a nie tło "
                    "pokoju. Nie ma czego zamykać — brama miałaby tu tylko obciąć "
                    "końcówki słów.", d.noiseFloorDb);
            else
                std::snprintf (b, sizeof (b),
                    "Bez bramy: między mową (%.0f dBFS) a tłem (%.0f dBFS) jest tylko "
                    "%.0f dB. Za mało, żeby ustawić próg, który zamknie się na pokoju, "
                    "a nie na końcu słowa.",
                    d.speechDb, d.noiseFloorDb, headroom);

            d.because.push_back (b);
        }

        // ---- 7. trzaski -------------------------------------------------------
        //
        // Usuwanie trzaskow ma jedna wade: na spiewie bierze atak nuty za trzask.
        // Na wierszu czlowieka "naprawilo" 34 miejsca, z ktorych zadne nie bylo
        // trzaskiem. Dlatego wlacza sie tylko tam, gdzie znajduje malo — bo duzo
        // znaczy, ze znajduje material, a nie usterki.
        const double perMinute = clickRate (in, sr);

        if (perMinute > 10.0)
        {
            p.useDeClick = false;

            std::snprintf (b, sizeof (b),
                "Bez usuwania trzasków: wykryłem %.0f impulsów na minutę. "
                "Część może należeć do mowy lub śpiewu; automatyczne usunięcie byłoby ryzykowne.", perMinute);
            d.because.push_back (b);
        }
        else if (perMinute > 0.5)
        {
            p.useDeClick = true;

            std::snprintf (b, sizeof (b),
                "Usuwanie trzasków włączone: %.1f krótkich impulsów na minutę. Sprawdź ich miejsca w odsłuchu.",
                perMinute);
            d.because.push_back (b);
        }
        else
        {
            p.useDeClick = false;

            std::snprintf (b, sizeof (b),
                "Bez usuwania trzasków: wykrywacz znalazł %.1f na minutę, czyli "
                "praktycznie nic. Nie ma czego naprawiać.", perMinute);
            d.because.push_back (b);
        }

        return d;
    }

private:
    //==========================================================================
    /** Rozklad przebiegu glosnosci na trend, oscylacje i gest. */
    static void levelShape (const std::vector<float>& x, double sr,
                            double& driftDb, double& sectionDb, double& gestureDb)
    {
        driftDb = sectionDb = gestureDb = 0.0;

        const size_t win = (size_t) (1.0 * sr);
        const size_t hop = (size_t) (0.25 * sr);

        std::vector<double> level, times;

        for (size_t i = 0; i + win <= x.size(); i += hop)
        {
            std::vector<float> w (x.begin() + (long) i, x.begin() + (long) (i + win));

            LoudnessMeter m;
            m.prepare (sr, 1);
            const float* ch[1] = { w.data() };
            m.process (ch, 1, (int) w.size());

            const double l = m.integrated();
            if (l > -60.0) { level.push_back(l); times.push_back(i/sr); }
        }

        if (level.size() < 40) return;

        // Wolna skladowa: mediana ruchoma 30 s. Mediana, nie srednia — srednia
        // daje sie przesunac jednemu krzykowi, a szukamy tu tla, nie zdarzen.
        // The window spans 30 seconds of wall time, including pauses.
        std::vector<double> slow (level.size()), fast (level.size());

        for (size_t i = 0; i < level.size(); ++i)
        {
            const size_t lo = (size_t)(std::lower_bound(times.begin(),times.end(),times[i]-15.0)-times.begin());
            const size_t hi = (size_t)(std::upper_bound(times.begin(),times.end(),times[i]+15.0)-times.begin());

            std::vector<double> w (level.begin() + (long) lo, level.begin() + (long) hi);
            std::sort (w.begin(), w.end());

            slow[i] = w[w.size() / 2];
            fast[i] = level[i] - slow[i];
        }

        // Prosta przez wolna skladowa. Jej calkowity przyrost to wedrowka;
        // to, co zostaje wokol niej, to budowa nagrania.
        const double n = (double) slow.size();
        double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;

        for (size_t i = 0; i < slow.size(); ++i)
        {
            const double t = times[i];
            sx += t; sy += slow[i]; sxx += t * t; sxy += t * slow[i];
        }

        const double denom = n * sxx - sx * sx;
        const double a = denom != 0.0 ? (n * sxy - sx * sy) / denom : 0.0;
        const double c = (sy - a * sx) / n;

        std::vector<double> osc (slow.size());
        for (size_t i = 0; i < slow.size(); ++i) osc[i] = slow[i] - (a * times[i] + c);

        driftDb   = std::fabs (a * (times.back() - times.front()));
        sectionDb = spread (osc);
        gestureDb = spread (fast);
    }

    static double spread (std::vector<double> v)
    {
        if (v.size() < 10) return 0.0;
        std::sort (v.begin(), v.end());
        return v[(size_t) (v.size() * 0.90)] - v[(size_t) (v.size() * 0.10)];
    }

    //==========================================================================
    /** Ile tlumienie odchylen zdjeloby z 4-14 kHz przy zwyklych ustawieniach. */
    static double tameDepth (const std::vector<float>& x, double sr)
    {
        // na dlugim nagraniu wystarczy fragment — to jest pomiar, nie render
        const size_t maxLen = (size_t) (sr * 60.0);
        std::vector<float> part (x.begin(),
                                 x.begin() + (long) std::min (maxLen, x.size()));

        if (part.size() < (size_t) (sr * 4.0)) return 0.0;

        Tame t;
        t.params.thresholdDb = 6.0f;
        t.params.maxCutDb    = -10.0f;
        t.prepare (sr);

        std::vector<float> out;
        t.render (part, out);

        return bandDb (part, sr, 4000.0, 14000.0) - bandDb (out, sr, 4000.0, 14000.0);
    }

    static double bandDb (const std::vector<float>& v, double sr, double lo, double hi)
    {
        constexpr size_t N = 4096;

        if (v.size() < N * 2) return -140.0;

        std::vector<double> win;
        hann (win, (int) N);

        Fft f;
        f.prepare ((int) N);

        std::vector<double> re (N), im (N);
        double sum = 0.0;
        int frames = 0;

        for (size_t p = 0; p + N <= v.size(); p += N * 4)
        {
            for (size_t i = 0; i < N; ++i) { re[i] = v[p + i] * win[i]; im[i] = 0.0; }

            f.run (re, im, false);

            for (size_t i = 0; i <= N / 2; ++i)
            {
                const double hz = (double) i * sr / (double) N;
                if (hz >= lo && hz < hi) sum += re[i] * re[i] + im[i] * im[i];
            }

            ++frames;
        }

        return 10.0 * std::log10 (sum / std::max (1, frames) + 1.0e-30);
    }

    //==========================================================================
    static double clickRate (const std::vector<float>& x, double sr)
    {
        const size_t maxLen = (size_t) (sr * 90.0);
        std::vector<float> part (x.begin(),
                                 x.begin() + (long) std::min (maxLen, x.size()));

        if (part.size() < (size_t) (sr * 4.0)) return 0.0;

        DeClick dc;
        std::vector<float> out;
        dc.render (part, out);

        return (double) dc.repaired / (part.size() / sr / 60.0);
    }

    static double percentileBlock (const std::vector<float>& v, double sr, double p)
    {
        const int block = std::max (1, (int) (sr * 0.05));
        std::vector<double> lv;

        for (size_t i = 0; i + (size_t) block <= v.size(); i += (size_t) block)
        {
            double s = 0.0;
            for (int k = 0; k < block; ++k) s += (double) v[i + (size_t) k] * v[i + (size_t) k];
            lv.push_back (10.0 * std::log10 (s / block + 1.0e-20));
        }

        if (lv.empty()) return -140.0;

        std::sort (lv.begin(), lv.end());
        return lv[std::min (lv.size() - 1, (size_t) (p * (double) (lv.size() - 1)))];
    }
};

} // namespace mix
