/*
    LineLog.h — kiedy w nagraniu zaczyna sie ktory wers.

    ---------------------------------------------------------------------------
    PO CO

    Pierwszy krok nagrywania "punch and roll" (Tools/ZADANIA.md, zad. 4):
    zeby po R cofnac sie na poczatek wersu i nagrac dalej, program musi
    wiedziec, GDZIE w nagraniu ten wers sie zaczal. Prompter wie to w trakcie
    nagrania — do tej pory po zatrzymaniu ta wiedza przepadala.

    Tu tylko zbieramy i porzadkujemy. Czas jest czasem PRZYJSCIA informacji
    (slowa z Vosk przychodza z opoznieniem 0,5–1,5 s, sylaby szybciej);
    dosuniecie do ciszy przed wersem to krok 2 i osobny plik.

    ---------------------------------------------------------------------------
    CO JEST WERSEM

    Numer wersu na ekranie zalezy od szerokosci okna — dlugi wers lamie sie
    na dwa. Dlatego wers rozpoznajemy po numerze PIERWSZEGO SLOWA w tekscie
    (PrompterText), ktory sie nie zmienia, a numer na ekranie zapisujemy tylko
    pomocniczo.

    ---------------------------------------------------------------------------
    POWTORKI

    Gdy wers czytany jest drugi raz (R, strzalki, prompter cofnal sie), liczy
    sie OSTATNIE przeczytanie — i wszystko, co bylo po nim za pierwszym razem,
    przestaje sie liczyc. To jest dokladnie zasada punch and roll: od punktu
    cofniecia nagranie idzie od nowa.
*/

#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace tw
{

/** Skad wiadomo, ze zaczal sie wers.
    Key     — czlowiek poprawil pozycje (strzalki, Home): ten sam tekst, nie nowe czytanie;
    Retake  — czlowiek wcisnal R: czyta wers jeszcze raz, to, co bylo, ma zniknac (Punch.h). */
enum class LineSource { Words, Syllables, Key, Retake };

inline const char* sourceName (LineSource s)
{
    return s == LineSource::Words ? "slowa" : s == LineSource::Syllables ? "sylaby"
         : s == LineSource::Retake ? "powtorka" : "klawisz";
}

/** Co robi R: wycina pomylke (Retake) czy tylko cofa tekst (Key).

    Wyciecie opiera sie na tym, KTORY wers wedlug promptera jest czytany. Z
    rozpoznawaniem mowy (Vosk) prompter trafia we wlasciwy wers w ok. 82%
    chwil; na samych sylabach — w 13% (mediana, 12 nagran lektorow,
    Docs/badania/SYMULACJA.md). Bez modelu mowy R cofalby wiec do zlego
    wersu i wycinal dobre czytanie. Wtedy R tylko przesuwa tekst; pomylke
    czlowiek usuwa sam na ekranie Montaz. */
inline LineSource retakeSource (bool followsSpeech)
{
    return followsSpeech ? LineSource::Retake : LineSource::Key;
}

struct LineMark
{
    double      sec       = 0.0;   ///< czas w nagraniu
    int         firstWord = 0;     ///< staly identyfikator wersu
    int         lastWord  = 0;
    int         shown     = 0;     ///< numer wersu na ekranie (zalezy od okna)
    LineSource  source    = LineSource::Syllables;
    std::string text;              ///< UTF-8, do czytania przez czlowieka
};

class LineLog
{
public:
    void begin() { all.clear(); current = -1; }

    /** Wywolywane co klatke z wersem, ktory prompter teraz pokazuje.

        Ta sama zasada co wyswietlacz w PrompterComponent: do przodu zawsze,
        o jeden wers do tylu nie (drgniecie sledzenia), o dwa i wiecej tak
        (prawdziwa poprawka pozycji). */
    void observe (const LineMark& m)
    {
        if (! all.empty() && m.firstWord == all.back().firstWord) return;   // ten sam wers (np. po zmianie okna)
        if (current >= 0 && m.shown == current) return;
        if (current >= 0 && m.shown == current - 1) return;

        add (m);
    }

    /** R, strzalki, Home: czlowiek sam wskazal wers — zapisujemy zawsze,
        takze gdy to ten sam wers (R = przeczytaj go jeszcze raz). */
    void restart (LineMark m, LineSource why = LineSource::Key)
    {
        m.source = why;
        add (m);
    }

    const std::vector<LineMark>& marks() const noexcept { return all; }

    /** Wersy, ktore zostaja w nagraniu, uporzadkowane po tekscie.

        Powtorke robi tylko CZLOWIEK (R, strzalki, Home): od tego miejsca
        wszystko, co bylo przeczytane wczesniej, przestaje sie liczyc.

        Cofniecie sledzenia samo z siebie NIE jest powtorka. Pomiar na
        nagraniu Adriana (11 pazdziernika): gdy kazde cofniecie traktowac jak
        powtorke, z 28 wersow zostawalo 15, bo poprawki pozycji kasowaly dobrze
        przeczytane wersy. Bez klawisza bierzemy wiec dla kazdego wersu jego
        PIERWSZE pojawienie sie — pozniejsze to poprawki sledzenia. */
    std::vector<LineMark> finalLines() const
    {
        std::vector<LineMark> keep;

        for (const auto& m : all)
        {
            if (m.source == LineSource::Key || m.source == LineSource::Retake)
            {
                keep.erase (std::remove_if (keep.begin(), keep.end(),
                                            [&] (const LineMark& k) { return k.firstWord >= m.firstWord; }),
                            keep.end());
                keep.push_back (m);
                continue;
            }

            const bool seen = std::any_of (keep.begin(), keep.end(),
                                           [&] (const LineMark& k) { return k.firstWord == m.firstWord; });
            if (! seen) keep.push_back (m);
        }

        std::sort (keep.begin(), keep.end(),
                   [] (const LineMark& a, const LineMark& b) { return a.firstWord < b.firstWord; });
        return keep;
    }

    //==========================================================================
    /** Plik obok nagrania (sesja_<data>.wersy.txt). Kolumny oddzielone
        tabulatorem, zeby dalo sie go otworzyc w arkuszu i przeczytac okiem.
        Zapisujemy WSZYSTKIE zdarzenia — kolejnosc powtorek tez jest wiedza. */
    std::string toText() const
    {
        std::string s = "# Zielone swiatlo - poczatki wersow w nagraniu\n"
                        "# czas_s\tslowo_od\tslowo_do\twers_na_ekranie\tzrodlo\ttekst\n";
        char b[96];

        for (const auto& m : all)
        {
            std::snprintf (b, sizeof (b), "%.3f\t%d\t%d\t%d\t%s\t",
                           m.sec, m.firstWord, m.lastWord, m.shown, sourceName (m.source));
            s += b;

            std::string t = m.text;
            std::replace (t.begin(), t.end(), '\t', ' ');
            std::replace (t.begin(), t.end(), '\n', ' ');
            s += t + "\n";
        }
        return s;
    }

    static std::vector<LineMark> parse (const std::string& text)
    {
        std::vector<LineMark> out;
        std::istringstream in (text);
        std::string line;

        while (std::getline (in, line))
        {
            if (line.empty() || line[0] == '#') continue;
            if (! line.empty() && line.back() == '\r') line.pop_back();

            std::vector<std::string> col;
            size_t from = 0;
            for (int i = 0; i < 5; ++i)
            {
                const size_t tab = line.find ('\t', from);
                if (tab == std::string::npos) break;
                col.push_back (line.substr (from, tab - from));
                from = tab + 1;
            }
            if (col.size() < 5) continue;

            LineMark m;
            m.sec       = std::atof (col[0].c_str());
            m.firstWord = std::atoi (col[1].c_str());
            m.lastWord  = std::atoi (col[2].c_str());
            m.shown     = std::atoi (col[3].c_str());
            m.source    = col[4] == "slowa" ? LineSource::Words
                        : col[4] == "klawisz" ? LineSource::Key
                        : col[4] == "powtorka" ? LineSource::Retake : LineSource::Syllables;
            m.text      = line.substr (from);
            out.push_back (m);
        }
        return out;
    }

private:
    void add (const LineMark& m)
    {
        all.push_back (m);
        current = m.shown;
    }

    std::vector<LineMark> all;
    int current = -1;
};

} // namespace tw
