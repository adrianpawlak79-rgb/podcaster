/*
    Follower.h — gdzie w tekscie jest czlowiek, ktory czyta.

    ---------------------------------------------------------------------------
    ZASADA

    Licznik sylab mowi, ile sylab padlo. Tekst mowi, ile sylab ma za soba kazde
    slowo. Zlozenie jednego z drugim daje pozycje. Tyle teorii.

    W praktyce jest jeden powazny klopot i jedno proste lekarstwo.

    KLOPOT: blad sie kumuluje. Licznik myli sie o kilka procent — raz w gore,
    raz w dol — i po dwoch minutach czytania roznica siega kilkunastu sylab,
    czyli kilku slow. Prompter jedzie wtedy o wers obok i jest gorszy niz zaden,
    bo czlowiek mu ufa.

    LEKARSTWO: pauzy. Czlowiek czytajacy tekst przestaje mowic w miejscach, w
    ktorych stoi kropka, przecinek albo koniec wersu — nie gdzie indziej. Wiec
    kazda dluzsza cisza jest okazja do kasowania bledu: pozycja dosuwa sie do
    najblizszego znaku interpunkcyjnego. Miedzy pauzami prompter szacuje, na
    pauzach wie.

    ---------------------------------------------------------------------------
    PLYNNOSC

    Sylaby przychodza skokowo, co 150-250 ms. Gdyby tekst przesuwal sie skokami,
    czytalo by sie to fatalnie. Wiec pozycja jedzie plynnie z oszacowana
    predkoscia, a przyjscie sylaby tylko ja koryguje. Predkosc bierze sie ze
    sredniej z ostatnich kilkunastu sylab, czyli sama sie dostraja do tego, jak
    szybko ktos czyta.

    ---------------------------------------------------------------------------
    UCZCIWOSC

    Program musi wiedziec, kiedy nie wie. Gdy dosuniecie na pauzie wypada daleko
    od tego, co program szacowal, znaczy to, ze sie zgubil — i wtedy ma to
    pokazac zamiast dalej pewnie przewijac. Czlowiek, ktory widzi "nie nadazam",
    poprawi strzalka w sekunde. Czlowiek, ktoremu prompter pewnie pokazuje zly
    wers, traci podejscie.
*/

#pragma once

#include "PrompterText.h"

#include <algorithm>
#include <cmath>
#include <deque>

namespace tw
{

//==============================================================================
class Follower
{
public:
    struct Params
    {
        /** Ile sylab pod rzad musi paść, zanim prompter w ogole ruszy.

            Czlowiek zglosil to tak: "startuje dziwnie, moze na kazdy dzwiek.
            Musze wtedy cofac strzalkami". I tak bylo — bo trzask krzesla,
            chrzakniecie albo stuk mikrofonu daja w pasmie samoglosek taki sam
            szczyt energii jak sylaba. Licznik nie mial jak ich odroznic.

            Roznica nie siedzi w pojedynczym dzwieku, tylko w RYTMIE. Mowa to
            seria sylab o ludzkim odstepie; trzask jest jeden i po nim cisza.
            Wiec prompter czeka, az zobaczy trzy sylaby z rzedu, kazda w
            odstepie od 0,1 do 0,6 sekundy — czyli cos, czego nie da sie
            zrobic przypadkiem.

            Do tego momentu tekst stoi na pierwszym slowie i nie rusza sie o
            wlos, choćby w pokoju bylo glosno. */
        int    armSyllables  = 3;
        double armMinGap     = 0.10;
        double armMaxGap     = 0.60;

        double startRateSyll = 4.5;   ///< zanim cokolwiek zmierzy, sylab na sekunde
        double catchUp       = 0.25;  ///< jak mocno sylaba koryguje pozycje
        /** Jak daleko wolno szukac znaku interpunkcyjnego przy dosuwaniu.

            Bylo 6 slow w KAZDA strone i to byl blad, ktory czlowiek nazwal
            "skacze jak chce": na prawdziwym czytaniu 3 z 5 dosuniec cofalo
            pozycje, bo najblizszy przecinek czesto lezal ZA nim. Czlowiek
            czytajacy nie wraca — wiec program tez nie ma prawa. */
        int    snapAhead     = 8;     ///< do przodu wolno szukac dalej
        int    snapBack      = 2;     ///< w tyl tylko tyle, ile trwa pomylka

        /** Po jak dlugiej ciszy wolno dosunac.

            Bylo 0,3 s i to za malo: czlowiek robi krotkie przerwy w srodku
            zdania, a kazda z nich przestawiala pozycje. Koniec zdania trwa
            dluzej. */
        double snapAfterSec  = 0.55;

        /** Ulamek predkosci przy jezdzie miedzy sylabami.

            Celowo mniej niz jeden. Licznik sylab lekko PRZELICZA (na mowie
            syntetycznej 40 wykrytych przy 38 prawdziwych), wiec pozycja sama
            z siebie ucieka do przodu. Prompter, ktory jest odrobine z tylu,
            jest do uzycia; taki, ktory wyprzedza, nie jest. */
        double coastFactor   = 0.90;
        /** Kiedy program ma przyznac, ze nie wie, gdzie jest.

            Nie po jednym dalekim dosunieciu — to zdarza sie normalnie, gdy
            ktos przeczyta pol zdania szybciej, niz licznik zdazyl policzyc.
            Dopiero po dwoch z rzedu, bo wtedy to juz nie jest przypadek. */
        double lostSyllables = 8.0;
        int    lostInARow    = 2;
        double minRate       = 1.5;
        double maxRate       = 9.0;

        /** O ile sylab cofnac to, co pokazujemy. Teraz zero — i to jest
            poprawka mojej wlasnej pomylki, wiec warto zapisac, na czym
            polegala.

            Zmierzylem blad wzgledem "ktore slowo wlasnie zaczelo padac" i
            wyszlo, ze podswietlenie idzie o jedno slowo za daleko. Cofnalem je
            wiec o 1,5 sylaby i mediana bledu spadla do zera. Liczba sie
            poprawila, a prompter sie pogorszyl.

            Bo mierzylem nie to, co trzeba. Oczy czytajacego sa DWA DO CZTERECH
            SLOW przed jego ustami — inaczej nie dalo by sie czytac plynnie.
            Podswietlenie, ktore stoi dokladnie na wymawianym slowie, jest z
            punktu widzenia czytajacego o cale pol wersu z tylu. Czlowiek
            powiedzial to tak: "ramka jeszcze jest na poprzednim wierszu a ja
            juz czytam nastepny".

            Wlasciwa miara nie jest wiec "gdzie sa usta", tylko "gdzie sa oczy".
            Tej drugiej nie zmierzylem zadnym testem i uczciwiej jest to
            napisac, niz udawac, ze zero wzielo sie z pomiaru. */
        double showLagSyll   = 0.0;
    };

    Params params;

    //==========================================================================
    void begin (const PrompterText* t)
    {
        text = t;

        position = 0.0;
        heard = 0;
        rate = params.startRateSyll;

        gaps.clear();
        lastTickSec = 0.0;
        lost = false;
        lastSnapError = 0.0;
        snaps = 0;
        shown = 0;
        bigSnaps = 0;
        manual = false;

        armed = false;
        armTicks.clear();
    }

    //==========================================================================
    /** Woluj co blok. @param nowSec czas od startu, @param newSyllables ile
        sylab przybylo, @param pauseSec dlugosc trwajacej ciszy (0 = mowi). */
    void advance (double nowSec, int newSyllables, double pauseSec)
    {
        if (text == nullptr) return;

        const double dt = std::max (0.0, nowSec - lastCallSec);
        lastCallSec = nowSec;

        // ---- zanim ruszymy: czy to na pewno mowa ----------------------------
        if (! armed)
        {
            for (int i = 0; i < newSyllables; ++i)
            {
                armTicks.push_back (nowSec);
                if (armTicks.size() > (size_t) params.armSyllables) armTicks.pop_front();
            }

            if ((int) armTicks.size() >= params.armSyllables)
            {
                bool rhythm = true;

                for (size_t k = 1; k < armTicks.size(); ++k)
                {
                    const double gap = armTicks[k] - armTicks[k - 1];

                    if (gap < params.armMinGap || gap > params.armMaxGap)
                    {
                        rhythm = false;
                        break;
                    }
                }

                if (rhythm)
                {
                    // Ruszamy — i liczymy od tych sylab, ktore wlasnie padly.
                    // Wszystko, co bylo przed nimi, bylo halasem i nie ma prawa
                    // przesunac tekstu.
                    armed = true;
                    heard = params.armSyllables;
                    position = params.armSyllables;
                    lastTickSec = nowSec;
                }
            }

            // Dopoki nie ruszylismy, tekst stoi. Zadnego jechania, zadnego
            // dosuwania — pierwsze slowo ma czekac, az czlowiek zacznie czytac.
            position = armed ? position : 0.0;
            return;
        }

        // ---- sylaby ----------------------------------------------------------
        if (gaps.size() >= 4)
        {
            double sum = 0.0;
            for (double g : gaps) sum += g;

            const double mean = sum / (double) gaps.size();
            if (mean > 0.0) rate = std::clamp (1.0 / mean, params.minRate, params.maxRate);
        }

        // ---- jazda miedzy sylabami -------------------------------------------
        if (pauseSec <= 0.0 && ! manual)
            position += rate * params.coastFactor * dt;

        // ---- pauza: kasowanie nazbieranego bledu ------------------------------
        //
        // Dosuwamy RAZ na pauze, nie co blok — inaczej pozycja przykleilaby sie
        // do znaku i przestala reagowac na to, ze czlowiek juz mowi dalej.
        if (pauseSec > params.snapAfterSec && ! snappedThisPause && ! manual)
        {
            snappedThisPause = true;
            snapToBreath();
        }

        if (pauseSec <= 0.0) snappedThisPause = false;

        position = std::clamp (position, 0.0, (double) text->totalSyllables());
    }

    //==========================================================================
    /** Reczna poprawka strzalkami. Od tej chwili program przestaje sam jechac,
        az czlowiek znowu zacznie mowic — bo skoro poprawil, to znaczy, ze
        program byl w zlym miejscu i jego wlasne oszacowanie jest nic niewarte. */
    void jumpToWord (int index)
    {
        if (text == nullptr || text->all().empty()) return;

        const int i = std::clamp (index, 0, text->wordCount() - 1);

        position = text->all()[(size_t) i].beforeMe;
        heard = (int) std::llround (position);
        shown = i;
        bigSnaps = 0;
        lost = false;
        armed = true;             // skoro czlowiek poprawil recznie, to czyta
        armTicks.clear();
        manual = false;
        gaps.clear();
    }

    void nudgeWords (int delta)
    {
        if (text == nullptr) return;
        jumpToWord (currentWord() + delta);
    }

    //==========================================================================
    /** Slowo do pokazania.

        Nie to samo co pozycja. Pozycja drga o ulamki sylaby w obie strony —
        to normalne i niewidoczne. Ale gdy takie drganie wypadnie na granicy
        wersu, CALY tekst skacze o wers w gore i w dol, i to jest widoczne
        natychmiast.

        Wiec to, co pokazujemy, idzie tylko do przodu. Wyjatkiem jest reczna
        poprawka i duze dosuniecie na pauzie — czyli dwa przypadki, w ktorych
        cofniecie jest DECYZJA, a nie drganiem. */
    int currentWord() const
    {
        if (text == nullptr) return 0;

        const int raw = text->wordAtSyllable (std::max (0.0, position - params.showLagSyll));

        if (raw > shown) shown = raw;

        return shown;
    }

    /** Czy prompter uznal, ze czlowiek zaczal czytac. */
    bool   hasStarted()       const noexcept { return armed; }

    double syllablePosition() const noexcept { return position; }
    double syllablesPerSec()  const noexcept { return rate; }
    bool   isLost()           const noexcept { return lost; }
    int    snapCount()        const noexcept { return snaps; }
    double lastSnapDistance() const noexcept { return lastSnapError; }

private:
    //==========================================================================
    void snapToBreath()
    {
        const int here = text->wordAtSyllable (position);

        // Szukamy osobno w przod i w tyl, bo to nie sa rownowazne kierunki.
        // Do przodu wolno siegnac daleko: czlowiek mogl przeczytac wiecej, niz
        // program zdazyl policzyc. W tyl — prawie wcale, bo czytajacy nie wraca,
        // a cofniecie tekstu w trakcie nagrania jest tym, co najbardziej
        // przeszkadza.
        int best = -1;
        double bestDist = 1.0e9;

        for (int d = 0; d <= params.snapAhead; ++d)
        {
            const int i = here + d;

            if (i < text->wordCount() && text->all()[(size_t) i].breathAfter >= 0.5f)
            {
                best = i;
                bestDist = d;
                break;
            }
        }

        for (int d = 1; d <= params.snapBack; ++d)
        {
            const int i = here - d;

            if (i >= 0 && text->all()[(size_t) i].breathAfter >= 0.5f && d < bestDist)
            {
                best = i;
                bestDist = d;
                break;
            }
        }

        if (best < 0) return;                       // nie ma gdzie dosunac

        const auto& w = text->all()[(size_t) best];
        const double target = w.beforeMe + w.syllables;

        lastSnapError = target - position;

        if (std::fabs (lastSnapError) > params.lostSyllables) ++bigSnaps;
        else                                                  bigSnaps = 0;

        lost = bigSnaps >= params.lostInARow;

        position = target;
        heard = (int) std::llround (position);
        ++snaps;

        // Cofniecie o wiecej niz dwa slowa to nie drganie, tylko poprawka —
        // wtedy wyswietlacz ma prawo pojsc za nia w tyl.
        const int now = text->wordAtSyllable (std::max (0.0, position - params.showLagSyll));

        if (now < shown - 2) shown = now;
    }

    const PrompterText* text = nullptr;

    double position = 0.0;
    int    heard = 0;
    double rate = 4.5;

    std::deque<double> gaps;
    double lastTickSec = 0.0, lastCallSec = 0.0;
    double lastSnapError = 0.0;

    int  snaps = 0;
    mutable int shown = 0;
    int  bigSnaps = 0;
    bool armed = false;
    std::deque<double> armTicks;
    bool lost = false, manual = false, snappedThisPause = false;
};

} // namespace tw
