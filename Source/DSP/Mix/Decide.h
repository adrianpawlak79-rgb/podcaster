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
#include "VoiceTone.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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

    VoiceTone::Bands tone;          ///< barwa glosu, wzgledem pasma 1 kHz
    double humDb          = 0.0;    ///< o ile przydzwiek wystaje w nagraniu
};

//==============================================================================
class Decide
{
public:
    /** Do czego to nagranie jedzie. Pusto = program wybiera sam. */
    enum class Destination { Auto, Podcast, YouTube, Wlasne };

    /** Trzy brzmienia do wyboru na ekranie miksu (Tools/ZADANIA.md, zad. 11).
        Wszystkie do tej samej glosnosci — porownanie ma byc uczciwe, glosniej
        zawsze "brzmi lepiej". */
    enum class Style
    {
        Delikatny,   ///< miks lagodny B1 (12.10): filtr dolu, lekkie odszumianie, 2:1
        Studyjny,    ///< lancuch v3 Adriana na naszych modulach (domyslny)
        Mocny        ///< studyjny + gesciej i blizej: mocniejsza kompresja, wiecej obecnosci
    };

    static const char* styleName (Style s)
    {
        return s == Style::Delikatny ? "Delikatny" : s == Style::Mocny ? "Mocny" : "Studyjny";
    }

    struct Wish
    {
        Wish() {}
        Destination where = Destination::Auto;
        double ceilingDbTp = -1.0;

        /** Miks studyjny (domyslny): lancuch "v3", ktory Adrian zlozyl na
            wtyczkach FabFilter/iZotope i ocenil jako duzo lepszy (12.10),
            odtworzony naszymi modulami — w odsluchu "podobnie". Pelny dobor
            (brama, glosek, de-esser, barwa z pomiaru) brzmial gorzej niz surowe
            nagranie. Pelna logika zostaje: studio = false. */
        bool studio = true;
        Style style = Style::Studyjny;   ///< ktore brzmienie, gdy studio = true

        /** Wynik kreatora. Pusty, gdy kreatora nie przechodzono — wtedy
            wszystko bierze sie z samego nagrania, jak dotad. */
        RoomCheck room;
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

        hum (in, sr, wish.room, d);
        roomChanged (wish.room, d);
        tone (in, sr, wish.room, headroom, d);

        if (wish.studio)
        {
            if (wish.style == Style::Delikatny) gentle (wish, headroom, d);
            else
            {
                studio (in, sr, wish, headroom, d);
                if (wish.style == Style::Mocny) strong (d);
            }
        }

        return d;
    }

    /** Miks studyjny — patrz Wish::studio. Liczby z lancucha v3 (0026):
        progi byly ustawione dla mowy o RMS -33,3 dBFS po +12 dB wzmocnienia,
        tu liczymy je od zmierzonej glosnej mowy (95. percentyl, -28,4 dBFS na
        tym samym nagraniu), wiec przesuniecia sa wzgledem niej. Uzasadnienie
        pisane od nowa, zeby nie opisywalo stopni, ktorych nie ma. */
    static constexpr double studioPodcastLufs = -19.0;

    /** Delikatny — miks lagodny z 12.10 (B1; Adrian: "raczej b1" wobec mocniejszych
        wersji B2/D). Tylko filtr dolu, lekkie odszumianie, kompresja 2:1 i limiter.
        Glosnosc ta sama co w studyjnym. */
    static constexpr float gentleDenoiseDb = -6.0f;

    static void gentle (const Wish& wish, double headroom, Decision& d)
    {
        auto& p = d.params;
        char b[400];
        std::vector<std::string> why;

        if (wish.where == Destination::Podcast) p.targetLufs = studioPodcastLufs;
        d.squashDb = std::max (0.0, p.targetLufs - d.freeTargetLufs);
        std::snprintf (b, sizeof (b), "Miks delikatny: głośność %.1f LUFS, szczyt najwyżej %.1f dBTP.",
                       p.targetLufs, wish.ceilingDbTp);
        why.push_back (b);

        p.highpassHz = 70.0; p.steepHighpass = false;
        p.preEq.clear(); p.notchEq.clear();
        p.useRider = false;
        p.levelerThreshDb = (float) std::clamp (d.speechDb - 2.0, -60.0, -6.0);
        p.levelerRatio = 2.0f;
        p.peakRatio = 1.0f;
        std::snprintf (b, sizeof (b), "Kompresja 2:1 od %.1f dBFS, tuż pod głośną mową. Bez osobnego ściskania szczytów.",
                       p.levelerThreshDb);
        why.push_back (b);

        if (p.useDenoise)
        {
            p.denoiseDb = std::max (p.denoiseDb, gentleDenoiseDb);
            std::snprintf (b, sizeof (b), "Odszumianie lekkie, najwyżej %.0f dB: tło jest %.0f dB pod mową.",
                           -p.denoiseDb, headroom);
        }
        else
            std::snprintf (b, sizeof (b), "Bez odszumiania: tło jest %.0f dB pod mową albo nie ma go z czego zmierzyć.", headroom);
        why.push_back (b);

        p.useGate = false; p.usePlosive = false; p.useDeClick = false; p.useBreath = false;
        p.useTame = false; p.useDeEsser = false; p.useTiltMatch = false; p.useSaturation = false;
        p.deHum = false;
        p.lowDb = p.mudDb = p.presenceDb = p.airDb = 0.0f;
        why.push_back ("Bez ekspandera, de-essera i korekty barwy — głos zostaje taki, jak go nagrałeś, tylko równiejszy.");
        d.because = why;
    }

    /** Mocny — na studyjnym: gesciej i blizej, w strone radia. Mocniejsza
        kompresja (3:1 i 6:1 na szczytach) i wiecej obecnosci. Bez
        podbicia dolu: SM7B z bliska ma go az nadto (pomiar 14.10). Glosnosc ta sama, wiec roznica jest w gestosci, nie w poziomie. */
    static void strong (Decision& d)
    {
        auto& p = d.params;
        char b[400];
        p.levelerThreshDb = (float) std::clamp (d.speechDb - 6.0, -60.0, -6.0);
        p.levelerRatio = 3.0f; p.levelerAttackMs = 10.0; p.levelerReleaseMs = 100.0;
        p.peakThreshDb = (float) std::clamp (d.speechDb + 1.0, -50.0, -3.0);
        p.peakRatio = 6.0f; p.peakAttackMs = 2.0; p.peakReleaseMs = 70.0;
        p.presenceHz = 3200.0; p.presenceDb = 2.5f;
        for (auto& line : d.because)
            if (line.rfind ("Miks studyjny", 0) == 0) line.replace (0, std::strlen ("Miks studyjny"), "Miks mocny");
        std::snprintf (b, sizeof (b), "Mocniej niż studyjny: kompresja 3:1 od %.1f dBFS i 6:1 na szczytach od %.1f dBFS, "
                       "obecność 3,2 kHz +2,5 dB — gęściej i bliżej, jak w radiu.",
                       p.levelerThreshDb, p.peakThreshDb);
        d.because.push_back (b);
    }

    static void studio (const std::vector<float>& in, double sr, const Wish& wish, double headroom, Decision& d)
    {
        auto& p = d.params;
        char b[400];
        std::vector<std::string> why;
        using Bell = SpeechChain::Params::Bell;

        if (wish.where == Destination::Podcast) p.targetLufs = studioPodcastLufs;
        d.squashDb = std::max (0.0, p.targetLufs - d.freeTargetLufs);
        std::snprintf (b, sizeof (b), "Miks studyjny: głośność %.1f LUFS, szczyt najwyżej %.1f dBTP.",
                       p.targetLufs, wish.ceilingDbTp);
        why.push_back (b);

        // odejmowanie: dol 24 dB/okt, bloto i pudlo
        p.highpassHz = 75.0; p.steepHighpass = true;
        p.preEq = { Bell { 200.0, 1.2, -2.5f }, Bell { 380.0, 1.4, -3.0f }, Bell { 800.0, 2.0, -1.5f } };
        why.push_back ("Korektor: filtr dołu 75 Hz (24 dB/okt), 200 Hz −2,5 dB, 380 Hz −3 dB, "
                       "800 Hz −1,5 dB — mniej dudnienia i „pudełka”.");

        if (p.useDenoise)
        {
            p.denoiseDb = -10.0f;
            std::snprintf (b, sizeof (b), "Odszumianie do 10 dB: tło jest %.0f dB pod mową.", headroom);
        }
        else
            std::snprintf (b, sizeof (b), "Bez odszumiania: tło jest %.0f dB pod mową albo nie ma go z czego zmierzyć.", headroom);
        why.push_back (b);

        // oddechy: sciszone przed kompresja, nie do zera
        {
            BreathControl bc;
            const auto found = bc.find (in, sr);
            p.useBreath = ! found.empty();
            p.breathDb = bc.params.reductionDb;
            if (p.useBreath)
                std::snprintf (b, sizeof (b), "Oddechy ściszone o %.0f dB (nie do zera): znalazłem %d w przerwach między słowami.",
                               -p.breathDb, (int) found.size());
            else
                std::snprintf (b, sizeof (b), "Oddechów nie ściszam: nie znalazłem żadnego %.0f dB pod mową w przerwach.",
                               bc.params.belowSpeechDb);
            why.push_back (b);
        }

        // ekspander zamiast bramy: lagodnie, z przytrzymaniem, zeby nie zatrzaskiwal ciszy miedzy slowami
        const bool canExpand = headroom > 12.0 && d.noiseFloorDb > -100.0;
        p.useGate = canExpand; p.autoGate = false;
        p.gateThreshDb = (float) std::max (d.speechDb - 25.5, d.noiseFloorDb + 6.0);
        p.gateRatio = 2.0f; p.gateFloorDb = -18.0f;
        p.gateAttackMs = 10.0; p.gateHoldMs = 120.0; p.gateReleaseMs = 300.0; p.gateLookaheadMs = 2.0;
        if (canExpand)
            std::snprintf (b, sizeof (b), "Ekspander w pauzach: od %.0f dBFS, 2:1, najwyżej −18 dB, "
                           "przytrzymanie 120 ms — ścisza tło między zdaniami, nie końcówki słów.", p.gateThreshDb);
        else
            std::snprintf (b, sizeof (b), "Bez ekspandera: tło tylko %.0f dB pod mową.", headroom);
        why.push_back (b);

        // dwa kompresory: wolny od glosnej mowy, szybki na szczyty
        p.useRider = false;
        p.levelerThreshDb = (float) std::clamp (d.speechDb - 3.5, -60.0, -6.0);
        p.levelerRatio = 2.5f; p.levelerAttackMs = 12.0; p.levelerReleaseMs = 120.0;
        p.peakThreshDb = (float) std::clamp (d.speechDb + 2.5, -50.0, -3.0);
        p.peakRatio = 4.0f; p.peakAttackMs = 3.0; p.peakReleaseMs = 90.0;
        std::snprintf (b, sizeof (b), "Kompresja w dwóch krokach: 2,5:1 od %.1f dBFS (wolna), 4:1 od %.1f dBFS (szczyty).",
                       p.levelerThreshDb, p.peakThreshDb);
        why.push_back (b);

        // rezonanse zamiast soothe: waskie wyciecia tam, gdzie widmo glosu wystaje
        p.notchEq.clear();
        std::string list;
        for (const auto& r : resonances (in, sr))
        {
            p.notchEq.push_back (Bell { r.first, 6.0, (float) -std::clamp (r.second - 1.0, 2.0, 5.0) });
            std::snprintf (b, sizeof (b), "%s%.0f Hz", list.empty() ? "" : ", ", r.first);
            list += b;
        }
        if (list.empty()) why.push_back ("Rezonanse: żadne pasmo 2–7 kHz nie wystaje o 3 dB ponad sąsiednie.");
        else              why.push_back ("Rezonanse ścięte wąsko (o 2–5 dB): " + list + ".");

        p.useTame = false;
        p.useDeEsser = true; p.essShelfHz = 5500.0; p.essMaxCutDb = -3.0f;
        why.push_back ("De-esser łagodny: od 5,5 kHz, najwyżej 3 dB.");

        // barwa na koncu: obecnosc i powietrze
        p.lowDb = 0.0f; p.mudDb = 0.0f;
        p.presenceHz = 3500.0; p.presenceQ = 1.0; p.presenceDb = 1.5f;
        p.airHz = 9000.0; p.airDb = 4.0f;
        why.push_back ("Barwa: 3,5 kHz +1,5 dB i półka 9 kHz +4 dB — po odszumianiu, więc nie podnosi syku.");

        // Przydzwiek: filtr 75 Hz 24 dB/okt zabiera 50 Hz sam. Osobne wyciecia
        // (Q 25 na 50/100/150 Hz) dzwonia ok. 80 ms po kazdym slowie, a ton
        // podstawowy meskiego glosu (Adrian: ok. 92 Hz) lezy tuz przy 100 Hz —
        // dzwonienie trzymalo otwarty ekspander: 100-300 ms po slowie +20 dB,
        // tlo -64,5 zamiast -67,8 dBFS (Docs/badania/MIKS-NA-GLOSIE.md).
        p.deHum = false;

        p.usePlosive = false; p.useDeClick = false; p.useTiltMatch = false; p.useSaturation = false;
        d.because = why;
    }

    /** Waskie piki w 2-7 kHz: srednie widmo glosnej mowy wobec mediany z
        tercji wokol. Na nagraniu Adriana (SM7B): 2479, 2924, 5051, 5303,
        6820 Hz; jego lancuch zmierzyl 2443, 2654, 2930, 4755 (gwizd tylko w
        11 % czasu — srednia go nie lapie), 5959 Hz. */
    static std::vector<std::pair<double, double>> resonances (const std::vector<float>& x, double sr)
    {
        const int N = 8192, H = N / 2;
        std::vector<std::pair<double, double>> out;
        if (x.size() < (size_t) N * 4) return out;

        std::vector<double> win; hann (win, N);
        std::vector<double> lev;
        for (size_t i = 0; i + (size_t) N <= x.size(); i += (size_t) H)
        {
            double e = 0.0; for (int k = 0; k < N; ++k) e += (double) x[i + (size_t) k] * x[i + (size_t) k];
            lev.push_back (10.0 * std::log10 (e / N + 1.0e-20));
        }
        auto sorted = lev; std::sort (sorted.begin(), sorted.end());
        const double loud = sorted[sorted.size() * 6 / 10];

        Fft fft; fft.prepare (N);
        std::vector<double> pw ((size_t) N / 2 + 1, 0.0), re, im;
        int frames = 0;
        for (size_t f = 0; f < lev.size(); ++f)
        {
            if (lev[f] <= loud) continue;
            re.assign ((size_t) N, 0.0); im.assign ((size_t) N, 0.0);
            for (int k = 0; k < N; ++k) re[(size_t) k] = x[f * (size_t) H + (size_t) k] * win[(size_t) k];
            fft.run (re, im, false);
            for (size_t k = 0; k < pw.size(); ++k) pw[k] += re[k] * re[k] + im[k] * im[k];
            ++frames;
        }
        if (frames == 0) return out;

        std::vector<double> db (pw.size());
        for (size_t k = 0; k < pw.size(); ++k) db[k] = 10.0 * std::log10 (pw[k] + 1.0e-30);
        const double binHz = sr / N;

        std::vector<std::pair<double, double>> cand;      // (nadwyzka, Hz)
        for (size_t k = 3; k + 3 < db.size(); ++k)
        {
            const double hz = (double) k * binHz;
            if (hz < 2000.0 || hz > 7000.0) continue;
            bool peak = true;
            for (size_t j = k - 3; j <= k + 3; ++j) if (db[j] > db[k]) peak = false;
            if (! peak) continue;
            const size_t lo = (size_t) (hz / std::pow (2.0, 1.0 / 6.0) / binHz);
            const size_t hi = std::min (db.size() - 1, (size_t) (hz * std::pow (2.0, 1.0 / 6.0) / binHz));
            std::vector<double> nb (db.begin() + (long) lo, db.begin() + (long) hi + 1);
            std::nth_element (nb.begin(), nb.begin() + (long) (nb.size() / 2), nb.end());
            const double excess = db[k] - nb[nb.size() / 2];
            if (excess >= 3.0) cand.push_back ({ excess, hz });
        }
        std::sort (cand.begin(), cand.end(), [] (auto& a, auto& b) { return a.first > b.first; });
        for (const auto& c : cand)
        {
            bool far = true;
            for (const auto& o : out) if (std::fabs (o.first - c.second) < 150.0) far = false;
            if (far) out.push_back ({ c.second, c.first });
            if (out.size() == 5) break;
        }
        std::sort (out.begin(), out.end());
        return out;
    }

private:
    //==========================================================================
    /* ---- 8. przydzwiek sieciowy ------------------------------------------

       Kreator slucha pokoju w ciszy, wiec przydzwiek slychac tam czysto. W
       nagraniu przykrywa go mowa — ale tez nie musi go juz byc (zasilacz
       wypiety po kreatorze). Dlatego kreator mowi, CZEGO szukac i ze warto
       szukac uwaznie (prog 3 dB), a nagranie mowi, czy to nadal jest.
       Bez kreatora wycinamy tylko wyrazny przydzwiek (10 dB, tyle co kreator). */
    static void hum (const std::vector<float>& in, double sr, const RoomCheck& room, Decision& d)
    {
        auto& p = d.params;
        char b[400];
        p.deHum = false;

        if (room.known && room.humHz > 0.0)
        {
            d.humDb = VoiceTone::humExcessDb (in, sr, room.humHz);

            if (d.humDb > 3.0)
            {
                p.deHum = true;
                p.humHz = room.humHz;
                std::snprintf (b, sizeof (b),
                    "Wycinam przydźwięk %.0f Hz i dwie harmoniczne: kreator słyszał go w pokoju "
                    "(wystawał o %.0f dB), a w nagraniu wystaje o %.1f dB.",
                    room.humHz, room.humExcessDb, d.humDb);
            }
            else
                std::snprintf (b, sizeof (b),
                    "Bez wycinania przydźwięku: kreator słyszał %.0f Hz, ale w tym nagraniu "
                    "nie wystaje (%.1f dB). Pewnie zasilacz został odpięty.",
                    room.humHz, d.humDb);

            d.because.push_back (b);
            return;
        }

        const double h50 = VoiceTone::humExcessDb (in, sr, 50.0);
        const double h60 = VoiceTone::humExcessDb (in, sr, 60.0);
        d.humDb = std::max (h50, h60);

        if (d.humDb > 10.0)
        {
            p.deHum = true;
            p.humHz = h50 >= h60 ? 50.0 : 60.0;
            std::snprintf (b, sizeof (b),
                "Wycinam przydźwięk %.0f Hz: wystaje w nagraniu o %.1f dB. %s",
                p.humHz, d.humDb, room.known ? "Kreator go nie słyszał — coś doszło po sprawdzeniu."
                                             : "Kreator by go wskazał wcześniej.");
            d.because.push_back (b);
        }
    }

    /* ---- 9. tlo wobec kreatora ----------------------------------------------
       Kreator zmierzyl pokoj. Jesli w nagraniu tlo jest wyraznie glosniejsze,
       cos sie wlaczylo (wentylator, lodowka, ulica) i warto to wiedziec, zanim
       nagra sie reszte. Tylko mowimy — miks i tak bierze tlo z nagrania. */
    static void roomChanged (const RoomCheck& room, Decision& d)
    {
        if (! room.known || room.noiseFloorDb < -100.0 || d.noiseFloorDb < -100.0) return;

        const double louder = d.noiseFloorDb - room.noiseFloorDb;
        if (louder < 6.0) return;

        char b[400];
        std::snprintf (b, sizeof (b),
            "Tło w nagraniu jest o %.0f dB głośniejsze niż przy sprawdzaniu pokoju "
            "(%.0f zamiast %.0f dBFS). Coś się włączyło — wentylator, lodówka, okno?",
            louder, d.noiseFloorDb, room.noiseFloorDb);
        d.because.push_back (b);
    }

    /* ---- 10. barwa ----------------------------------------------------------
       Zasady i wzorzec: VoiceTone.h. Tu tylko granice:
         - dol i bloto tylko scinamy (do 6 i 4 dB) — podbicie dolu to gust;
         - obecnosc i powietrze podbijamy do 3 dB, ale scinamy najwyzej o 1,5 dB.
           Po czterech rundach odsluchu skarga brzmiala "przytlumione" — wiec
           sciemnianie jasnego glosu robimy ostroznie; jasny glos to czesto
           po prostu glos, a nie usterka;
         - przy ostrych sybilantach nie podbijamy gory o wiecej niz 1 dB,
           bo podbilibysmy wlasnie je;
         - przy szumie blisko mowy nie podbijamy powietrza o wiecej niz 1 dB,
           bo tam siedzi syk;
         - nagranie bez pasma powyzej 12 kHz zostawiamy bez powietrza;
         - kreator, ktory zmierzyl "za blisko", zmniejsza margines dolu
           z 3 do 1,5 dB: drugi pomiar mowi to samo, wiec mu wierzymy. */
    static void tone (const std::vector<float>& in, double sr, const RoomCheck& room,
                      double headroom, Decision& d)
    {
        auto& p = d.params;
        char b[400];

        d.tone = VoiceTone::measure (in, sr);
        p.lowDb = p.mudDb = p.presenceDb = p.airDb = 0.0f;

        if (! d.tone.valid)
        {
            d.because.push_back ("Barwa bez zmian: nagranie za krótkie, "
                                 "żeby zmierzyć widmo głosu.");
            return;
        }

        const auto& t = d.tone;
        const double dLow = VoiceTone::deviation (t, VoiceTone::Low);
        const double dMud = VoiceTone::deviation (t, VoiceTone::Mud);
        const double dPre = VoiceTone::deviation (t, VoiceTone::Presence);
        const double dAir = VoiceTone::deviation (t, VoiceTone::Air);

        const bool  close    = room.tooClose();
        const float boostCap = d.tameWouldDb > 6.0 ? 1.0f : 3.0f;
        const bool  hiss     = d.noiseFloorDb > -100.0 && headroom < 30.0;

        p.lowDb      = VoiceTone::correction (dLow, close ? 1.5 : 3.0, -6.0f, 0.0f);
        p.mudDb      = VoiceTone::correction (dMud, 3.0, -4.0f, 0.0f);
        p.presenceDb = VoiceTone::correction (dPre, 3.0, -1.5f, boostCap);

        if (t.hasAir())
            p.airDb = VoiceTone::correction (dAir, 3.0, -1.5f, std::min (boostCap, hiss ? 1.0f : 3.0f));

        // ---- dol
        if (p.lowDb < 0.0f)
            std::snprintf (b, sizeof (b),
                "Dół (80–180 Hz) %+.1f dB względem 1 kHz, przeciętna mowa ma %+.1f. "
                "Półka 150 Hz %.1f dB.%s",
                t.db[VoiceTone::Low], VoiceTone::reference[VoiceTone::Low], p.lowDb,
                close ? " Kreator też mówił, że jesteś za blisko mikrofonu." : "");
        else
            std::snprintf (b, sizeof (b),
                "Dół (80–180 Hz) bez zmian: %+.1f dB względem 1 kHz, przeciętna mowa ma %+.1f.%s",
                t.db[VoiceTone::Low], VoiceTone::reference[VoiceTone::Low],
                close ? " Kreator mówił, że jesteś za blisko, ale w nagraniu tego nie słychać." : "");
        d.because.push_back (b);

        // ---- bloto
        std::snprintf (b, sizeof (b), p.mudDb < 0.0f
            ? "Pudełkowe brzmienie (180–355 Hz) %+.1f dB, przeciętnie %+.1f. Ścinam 250 Hz o %.1f dB."
            : "Środek dołu (180–355 Hz) bez zmian: %+.1f dB, przeciętnie %+.1f.",
            t.db[VoiceTone::Mud], VoiceTone::reference[VoiceTone::Mud], -p.mudDb);
        d.because.push_back (b);

        // ---- obecnosc
        if (std::fabs (p.presenceDb) > 0.05f)
            std::snprintf (b, sizeof (b),
                "Obecność (2,2–4,5 kHz) %+.1f dB, przeciętnie %+.1f. %s 3 kHz o %.1f dB%s.",
                t.db[VoiceTone::Presence], VoiceTone::reference[VoiceTone::Presence],
                p.presenceDb > 0 ? "Podbijam" : "Ścinam", std::fabs (p.presenceDb),
                p.presenceDb > 0 && boostCap < 2.0f ? " (mniej, bo sybilanty są ostre)" : "");
        else
            std::snprintf (b, sizeof (b),
                "Obecność (2,2–4,5 kHz) bez zmian: %+.1f dB, przeciętnie %+.1f.",
                t.db[VoiceTone::Presence], VoiceTone::reference[VoiceTone::Presence]);
        d.because.push_back (b);

        // ---- powietrze
        if (! t.hasAir())
            std::snprintf (b, sizeof (b),
                "Powietrze bez zmian: nagranie kończy się na %.1f kHz, "
                "więc podbicie góry podniosłoby tylko szum.", t.bandwidthHz / 1000.0);
        else if (std::fabs (p.airDb) > 0.05f)
            std::snprintf (b, sizeof (b),
                "Powietrze (7–14 kHz) %+.1f dB, przeciętnie %+.1f. %s półkę 10 kHz o %.1f dB%s.",
                t.db[VoiceTone::Air], VoiceTone::reference[VoiceTone::Air],
                p.airDb > 0 ? "Podnoszę" : "Obniżam", std::fabs (p.airDb),
                p.airDb > 0 && hiss ? " (mniej, bo w tle jest szum)" : "");
        else
            std::snprintf (b, sizeof (b),
                "Powietrze (7–14 kHz) bez zmian: %+.1f dB, przeciętnie %+.1f.",
                t.db[VoiceTone::Air], VoiceTone::reference[VoiceTone::Air]);
        d.because.push_back (b);
    }

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
