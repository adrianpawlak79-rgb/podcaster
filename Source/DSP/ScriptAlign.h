/*
    ScriptAlign.h — co naprawde padlo, a co mialo pasc.

    ---------------------------------------------------------------------------
    PO CO

    Prompter zna tekst. Dokladny zapis tego, co padlo w nagraniu (Whisper,
    slowa z czasami), mozna wiec porownac z tekstem slowo po slowie i
    powiedziec czlowiekowi wprost:

      - POWTORZENIE  — ten fragment przeczytales dwa razy; wczesniejsze
                       podejscie jest do wyciecia (zostaje ostatnie),
      - POZA TEKSTEM — slowa, ktorych nie ma w tekscie (komentarz, "jeszcze
                       raz", kaszel z gadaniem) — zwykle do wyciecia,
      - PRZEJEZYCZENIE — w ostatnim podejsciu padlo inne slowo niz w tekscie,
      - POMINIETE    — fragment tekstu, ktory nie padl ani razu.

    ---------------------------------------------------------------------------
    JAK

    Czytanie z promptera to nie jest jedno przejscie przez tekst: czlowiek
    sie myli, wraca do poczatku zdania (R), czyta wers jeszcze raz. Zwykle
    dopasowanie dwoch ciagow (Levenshtein) by tego nie zrozumialo. Tu jest
    Viterbi po pozycji w tekscie, z ruchami:

      - slowo pasuje do kolejnego slowa tekstu (calkiem albo prawie),
      - dwa slowa zapisu na jedno slowo tekstu (Whisper rozbija rzadkie
        slowa: "najtemn przymi" zamiast "najtepszymi"),
      - slowo spoza tekstu (pozycja w tekscie stoi),
      - pominiete slowa tekstu (pozycja idzie naprzod bez slowa),
      - POWROT — skok wstecz w tekscie: poczatek nowego podejscia.

    Kazde podejscie to ciag slow miedzy powrotami. Dla kazdego slowa tekstu
    liczy sie ostatnie podejscie, ktore je pokrylo; wczesniejsze pokrycia sa
    powtorzeniami.

    Pamiec: N slow zapisu x M slow tekstu wskaznikow wstecz. Dla kilku minut
    to drobiazg; godzine trzeba bedzie dopasowywac kawalkami (wers po wersie,
    tak jak liczy prompter) — to juz przy wpinaniu w program.
*/

#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace tw
{

struct HeardWord
{
    std::string text;     ///< jak w zapisie (UTF-8, z interpunkcja)
    double from = 0.0, to = 0.0;
};

struct ScriptFinding
{
    enum class Kind { Repeat, OffScript, Slip, Skipped };
    Kind kind = Kind::Slip;
    double from = 0.0, to = 0.0;     ///< czas w nagraniu (Skipped: miejsce, gdzie powinno byc)
    int scriptFrom = -1, scriptTo = -1; ///< zakres slow tekstu (wlacznie), -1 gdy nie dotyczy
    std::string expected, heard;    ///< slowa tekstu i to, co padlo
};

namespace align
{
    //--------------------------------------------------------------------------
    // Slowa porownujemy bez wielkich liter i interpunkcji; polskie litery
    // zostaja ("laska" i "łaska" to rozne slowa).

    inline std::u32string decode (const std::string& s)
    {
        std::u32string out;
        for (size_t i = 0; i < s.size();)
        {
            const unsigned char c = (unsigned char) s[i];
            char32_t cp = 0xFFFD; int len = 1;
            if (c < 0x80) { cp = c; }
            else if ((c >> 5) == 6 && i + 1 < s.size()) { cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F); len = 2; }
            else if ((c >> 4) == 14 && i + 2 < s.size()) { cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F); len = 3; }
            else if ((c >> 3) == 30 && i + 3 < s.size()) { cp = ((c & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12) | ((s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F); len = 4; }
            out += cp; i += (size_t) len;
        }
        return out;
    }

    inline char32_t lower (char32_t c)
    {
        if (c >= U'A' && c <= U'Z') return c + 32;
        switch (c)
        {
            case U'Ą': return U'ą'; case U'Ć': return U'ć'; case U'Ę': return U'ę';
            case U'Ł': return U'ł'; case U'Ń': return U'ń'; case U'Ó': return U'ó';
            case U'Ś': return U'ś'; case U'Ź': return U'ź'; case U'Ż': return U'ż';
            default:   return c;
        }
    }

    inline bool letter (char32_t c)
    {
        return (c >= U'a' && c <= U'z') || (c >= U'0' && c <= U'9')
            || c == U'ą' || c == U'ć' || c == U'ę' || c == U'ł' || c == U'ń'
            || c == U'ó' || c == U'ś' || c == U'ź' || c == U'ż' || (c >= 0xC0 && c < 0x250);
    }

    /** "Zdarzy." -> "zdarzy"; "—" -> "" (samo znaki: nie jest slowem). */
    inline std::u32string norm (const std::string& word)
    {
        std::u32string out;
        for (char32_t c : decode (word)) { c = lower (c); if (letter (c)) out += c; }
        return out;
    }

    /** Slowa tekstu (UTF-8), w kolejnosci; linie zaczynajace sie od # to
        uwagi dla czytajacego (jak w prompterze) — pomijane. */
    inline std::vector<std::string> scriptWords (const std::string& text)
    {
        std::vector<std::string> out;
        size_t lineStart = 0;
        while (lineStart <= text.size())
        {
            size_t lineEnd = text.find ('\n', lineStart);
            if (lineEnd == std::string::npos) lineEnd = text.size();
            const std::string line = text.substr (lineStart, lineEnd - lineStart);
            const size_t first = line.find_first_not_of (" \t\r");
            if (first == std::string::npos || line[first] != '#')
            {
                std::string cur;
                for (char ch : line)
                {
                    if (ch == ' ' || ch == '\t' || ch == '\r')
                    { if (! norm (cur).empty()) out.push_back (cur); cur.clear(); }
                    else cur += ch;
                }
                if (! norm (cur).empty()) out.push_back (cur);
            }
            lineStart = lineEnd + 1;
        }
        return out;
    }

    /** Podobienstwo 0..1: 1 - odleglosc edycyjna / dluzsze slowo. */
    inline double similarity (const std::u32string& a, const std::u32string& b)
    {
        if (a == b) return 1.0;
        if (a.empty() || b.empty()) return 0.0;
        std::vector<int> prev (b.size() + 1), cur (b.size() + 1);
        for (size_t j = 0; j <= b.size(); ++j) prev[j] = (int) j;
        for (size_t i = 1; i <= a.size(); ++i)
        {
            cur[0] = (int) i;
            for (size_t j = 1; j <= b.size(); ++j)
                cur[j] = std::min ({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1) });
            std::swap (prev, cur);
        }
        return 1.0 - (double) prev[b.size()] / (double) std::max (a.size(), b.size());
    }

    // Punkty ruchow (wiecej = lepiej). Dobrane na nagraniach z promptera:
    // powrot musi kosztowac wiecej niz pare slow spoza tekstu, inaczej kazde
    // przejezyczenie zamienia sie w "nowe podejscie".
    struct Costs
    {
        double match     = 2.0;   ///< slowo jak w tekscie
        double nearOk    = 0.55;  ///< od tego podobienstwa slowo "prawie" pasuje
        double mismatch  = -3.0;  ///< inne slowo w miejscu slowa tekstu (drozej niz slowo spoza
                                  ///< tekstu + pominiete slowo: komentarz nie "udaje" tekstu)
        double extra     = -1.0;  ///< slowo spoza tekstu
        double skipOne   = -1.2;  ///< jedno pominiete slowo tekstu
        double skipJump  = -6.0;  ///< dowolnie dlugi skok naprzod (pominiety fragment)
        double restart   = -4.0;  ///< powrot wstecz: nowe podejscie
    };

    inline double wordScore (double sim, const Costs& c)
    {
        return sim >= 1.0 ? c.match : sim >= c.nearOk ? c.match * sim - 0.6 : c.mismatch;
    }
}

//==============================================================================
/** Porownuje zapis (slowa z czasami) z tekstem promptera. */
inline std::vector<ScriptFinding> compareWithScript (const std::vector<HeardWord>& heardIn,
                                                     const std::string& scriptText,
                                                     const align::Costs& c = {})
{
    using namespace align;

    // slowa bez liter ("—", "...") nie biora udzialu
    std::vector<HeardWord> heard;
    std::vector<std::u32string> h;
    for (const auto& w : heardIn) if (! norm (w.text).empty()) { heard.push_back (w); h.push_back (norm (w.text)); }

    const auto words = scriptWords (scriptText);
    std::vector<std::u32string> s;
    for (const auto& w : words) s.push_back (norm (w));

    const int N = (int) h.size(), M = (int) s.size();
    std::vector<ScriptFinding> out;
    if (M == 0) return out;

    // stan j = ile slow tekstu juz za nami (0..M). score[i][j] po i slowach zapisu.
    enum Move : uint8_t { None, Match, Merge, Extra, Skip, Jump, Restart };
    struct Back { int32_t j = -1; uint8_t move = None; };
    const double minusInf = -1.0e18;
    std::vector<double> prev ((size_t) M + 1, minusInf), cur ((size_t) M + 1), prev2 ((size_t) M + 1, minusInf);
    std::vector<Back> back ((size_t) (N + 1) * (size_t) (M + 1));
    auto B = [&] (int i, int j) -> Back& { return back[(size_t) i * (size_t) (M + 1) + (size_t) j]; };

    // przed pierwszym slowem: start od poczatku albo od dowolnego miejsca (skok)
    prev[0] = 0.0;
    for (int j = 1; j <= M; ++j) { prev[j] = c.skipJump; B (0, j) = { 0, Jump }; }
    // skoki naprzod i pomijanie w obrebie jednego wiersza (bez slowa zapisu)
    auto relaxSkips = [&] (std::vector<double>& sc, int i)
    {
        double bestJump = minusInf; int bestFrom = -1;
        for (int j = 0; j <= M; ++j)
        {
            if (j > 0 && sc[j - 1] + c.skipOne > sc[j]) { sc[j] = sc[j - 1] + c.skipOne; B (i, j) = { j - 1, Skip }; }
            if (bestFrom >= 0 && bestJump + c.skipJump > sc[j]) { sc[j] = bestJump + c.skipJump; B (i, j) = { bestFrom, Jump }; }
            if (sc[j] > bestJump) { bestJump = sc[j]; bestFrom = j; }
        }
    };
    relaxSkips (prev, 0);

    for (int i = 1; i <= N; ++i)
    {
        std::fill (cur.begin(), cur.end(), minusInf);
        // powrot: do slowa j-1 z dowolnego stanu dalej w tekscie (j' >= j) —
        // najlepszy taki stan to maksimum "od konca"
        std::vector<int> sufBest ((size_t) M + 2, -1);
        for (int j = M; j >= 0; --j)
            sufBest[(size_t) j] = (sufBest[(size_t) j + 1] >= 0 && prev[(size_t) sufBest[(size_t) j + 1]] >= prev[(size_t) j]) ? sufBest[(size_t) j + 1] : j;

        for (int j = 0; j <= M; ++j)
        {
            // slowo spoza tekstu: pozycja stoi
            if (prev[j] + c.extra > cur[j]) { cur[j] = prev[j] + c.extra; B (i, j) = { j, Extra }; }
            if (j > 0)
            {
                // slowo tekstu j-1 zostalo przeczytane tym slowem
                const double sc = wordScore (similarity (h[(size_t) i - 1], s[(size_t) j - 1]), c);
                if (prev[j - 1] + sc > cur[j]) { cur[j] = prev[j - 1] + sc; B (i, j) = { j - 1, Match }; }
                // to samo po powrocie: nowe podejscie zaczyna sie od slowa j-1
                const int from = sufBest[(size_t) j];
                if (from >= 0 && prev[(size_t) from] + c.restart + sc > cur[j])
                { cur[j] = prev[(size_t) from] + c.restart + sc; B (i, j) = { from, Restart }; }
                // dwa slowa zapisu na jedno slowo tekstu
                if (i >= 2)
                {
                    // tylko gdy razem pasuja wyraznie lepiej niz pierwsze slowo samo
                    // ("pocałunków. No" to nie jedno slowo)
                    const double sim2 = similarity (h[(size_t) i - 2] + h[(size_t) i - 1], s[(size_t) j - 1]);
                    const double sim1 = similarity (h[(size_t) i - 2], s[(size_t) j - 1]);
                    if (sim2 >= 0.6 && sim2 > sim1 + 0.15 && prev2[j - 1] + wordScore (sim2, c) > cur[j])
                    { cur[j] = prev2[j - 1] + wordScore (sim2, c); B (i, j) = { j - 1, Merge }; }
                }
            }
        }
        relaxSkips (cur, i);
        prev2 = prev; prev = cur;
    }

    // koniec: najlepszy stan (reszta tekstu po nim wyjdzie jako pominieta)
    int j = 0; for (int k = 1; k <= M; ++k) if (prev[k] > prev[j]) j = k;

    // sciezka wstecz: dla kazdego slowa zapisu — ktore slowo tekstu (albo -1)
    // i numer podejscia
    std::vector<int> wordOf ((size_t) N, -1), pass ((size_t) N, 0);
    std::vector<uint8_t> merged ((size_t) N, 0);
    std::vector<bool> skippedWord ((size_t) M, true);
    std::vector<int> restartsAt;                       // indeksy i, gdzie zaczyna sie nowe podejscie
    for (int i = N; i >= 0;)
    {
        const Back b = B (i, j);
        if (b.move == None) break;
        switch (b.move)
        {
            case Match: wordOf[(size_t) i - 1] = j - 1; --i; break;
            case Merge: wordOf[(size_t) i - 1] = wordOf[(size_t) i - 2] = j - 1; merged[(size_t) i - 1] = merged[(size_t) i - 2] = 1; i -= 2; break;
            case Restart: wordOf[(size_t) i - 1] = j - 1; restartsAt.push_back (i - 1); --i; break;
            case Extra: --i; break;
            default: break;                          // Skip, Jump: i stoi
        }
        j = b.j;
        if (j < 0) break;
    }
    // numery podejsc rosna w czasie
    std::sort (restartsAt.begin(), restartsAt.end());
    for (int i = 0, r = 0; i < N; ++i)
    {
        while (r < (int) restartsAt.size() && restartsAt[(size_t) r] <= i) { ++r; }
        pass[(size_t) i] = r;
    }

    // ostatnie podejscie dla kazdego slowa tekstu
    std::vector<int> lastPass ((size_t) M, -1), lastWord ((size_t) M, -1);
    for (int i = 0; i < N; ++i)
        if (wordOf[(size_t) i] >= 0) { lastPass[(size_t) wordOf[(size_t) i]] = pass[(size_t) i]; lastWord[(size_t) wordOf[(size_t) i]] = i; skippedWord[(size_t) wordOf[(size_t) i]] = false; }

    auto joinHeard = [&] (int a, int b) { std::string t; for (int k = a; k <= b; ++k) { if (! t.empty()) t += ' '; t += heard[(size_t) k].text; } return t; };
    auto joinScript = [&] (int a, int b) { std::string t; for (int k = a; k <= b; ++k) { if (! t.empty()) t += ' '; t += words[(size_t) k]; } return t; };

    // POWTORZENIA: slowa zapisu dopasowane do slowa tekstu, ktore pozniej
    // przeczytano jeszcze raz (w pozniejszym podejsciu) — ciagi takich slow
    // razem ze slowami spoza tekstu pomiedzy nimi.
    std::vector<uint8_t> earlier ((size_t) N, 0);
    for (int i = 0; i < N; ++i)
    {
        const int w = wordOf[(size_t) i];
        if (w >= 0 && lastPass[(size_t) w] > pass[(size_t) i]) earlier[(size_t) i] = 1;
    }

    // POZA TEKSTEM: ciagi slow bez dopasowania. Pojedyncze krotkie slowo
    // (np. "i", "no") to czesto szum zapisu — liczymy od 2 slow albo 0,6 s.
    for (int i = 0; i < N;)
    {
        if (earlier[(size_t) i])
        {
            int k = i;
            while (k + 1 < N && (earlier[(size_t) k + 1] || (wordOf[(size_t) k + 1] < 0 && pass[(size_t) k + 1] == pass[(size_t) i]))) ++k;
            while (k > i && wordOf[(size_t) k] < 0) --k;          // komentarz na koncu to osobna sprawa
            ScriptFinding f; f.kind = ScriptFinding::Kind::Repeat;
            f.from = heard[(size_t) i].from; f.to = heard[(size_t) k].to;
            int a = M, b = -1;
            for (int q = i; q <= k; ++q) if (wordOf[(size_t) q] >= 0) { a = std::min (a, wordOf[(size_t) q]); b = std::max (b, wordOf[(size_t) q]); }
            f.scriptFrom = a; f.scriptTo = b;
            f.heard = joinHeard (i, k); f.expected = b >= a ? joinScript (a, b) : std::string();
            out.push_back (f);
            i = k + 1;
            continue;
        }
        if (wordOf[(size_t) i] < 0)
        {
            int k = i;
            while (k + 1 < N && wordOf[(size_t) k + 1] < 0 && ! earlier[(size_t) k + 1]) ++k;
            const double len = heard[(size_t) k].to - heard[(size_t) i].from;
            if (k > i || len >= 0.6)
            {
                ScriptFinding f; f.kind = ScriptFinding::Kind::OffScript;
                f.from = heard[(size_t) i].from; f.to = heard[(size_t) k].to;
                f.heard = joinHeard (i, k);
                out.push_back (f);
            }
            i = k + 1;
            continue;
        }
        ++i;
    }

    // PRZEJEZYCZENIA: w ostatnim podejsciu slowo inne niz w tekscie
    for (int w = 0; w < M; ++w)
    {
        const int i = lastWord[(size_t) w];
        if (i < 0) continue;
        const int i0 = merged[(size_t) i] && i > 0 && wordOf[(size_t) i - 1] == w ? i - 1 : i;
        std::u32string got = h[(size_t) i0]; if (i0 != i) got += h[(size_t) i];
        if (got == s[(size_t) w]) continue;
        ScriptFinding f; f.kind = ScriptFinding::Kind::Slip;
        f.from = heard[(size_t) i0].from; f.to = heard[(size_t) i].to;
        f.scriptFrom = f.scriptTo = w;
        f.expected = words[(size_t) w]; f.heard = joinHeard (i0, i);
        out.push_back (f);
    }

    // POMINIETE: ciagi slow tekstu, ktore nie padly ani razu
    for (int w = 0; w < M;)
    {
        if (! skippedWord[(size_t) w]) { ++w; continue; }
        int e = w; while (e + 1 < M && skippedWord[(size_t) e + 1]) ++e;
        ScriptFinding f; f.kind = ScriptFinding::Kind::Skipped;
        f.scriptFrom = w; f.scriptTo = e; f.expected = joinScript (w, e);
        double at = 0.0; for (int q = w - 1; q >= 0; --q) if (lastWord[(size_t) q] >= 0) { at = heard[(size_t) lastWord[(size_t) q]].to; break; }
        f.from = f.to = at;
        out.push_back (f);
        w = e + 1;
    }

    std::stable_sort (out.begin(), out.end(), [] (const ScriptFinding& a, const ScriptFinding& b) { return a.from < b.from; });
    return out;
}

} // namespace tw
