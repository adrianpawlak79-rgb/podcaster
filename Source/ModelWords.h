/*
    ModelWords.h — jakie slowa zna model Vosk i czym zastapic te, ktorych nie zna.

    ---------------------------------------------------------------------------
    PO CO

    Slownik Vosk ograniczamy do slow skryptu. Ale Vosk moze szukac tylko slow,
    ktore model ZNA; reszte pomija z ostrzezeniem. W "Nic dwa razy" model
    small-pl 0.22 nie zna siedmiu: zrodzilismy, najtepszymi, repetowac,
    jednakich, godzino, miniesz, wspolobjeci. Takie slowo nigdy nie trafi i
    prompter traci w tym miejscu punkt zaczepienia.

    Z pelnym slownikiem Vosk slyszal je jako "zrobilismy", "najlepszymi",
    "ratowac" — czyli najblizsze slowo, ktore zna. Wiec dajemy mu to slowo
    sami: dla kazdego nieznanego slowa skryptu szukamy w slowach modelu
    najpodobniejszego i dokladamy je do slownika jako ZAMIENNIK. Gdy Vosk je
    uslyszy, WordTracker liczy to jako trafienie oryginalu.

    Lista slow modelu siedzi w graph/Gr.fst jako tablica symboli OpenFst
    (nie ma osobnego words.txt). Ten plik ja czyta — bez JUCE, zeby
    sprawdzal to test i narzedzie pomiarowe.
*/

#pragma once

#include "DSP/ScriptNormalize.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tw
{

/** Slowa modelu z graph/Gr.fst. Pusto, gdy pliku nie ma albo nie ma w nim
    tablicy symboli. */
inline std::vector<std::string> readModelWords (const std::string& grFstPath)
{
    std::ifstream f (grFstPath, std::ios::binary);
    if (! f) return {};

    std::stringstream b;
    b << f.rdbuf();
    const std::string d = b.str();

    // Tablica symboli: magic, nazwa ("...words.txt"), available_key, size, a
    // potem pary (dlugosc, napis, klucz). Szukamy po nazwie, bo naglowek FST
    // przed nia ma zmienna dlugosc.
    const size_t at = d.find ("words.txt");
    if (at == std::string::npos) return {};

    size_t p = at + std::strlen ("words.txt");

    auto i32 = [&] (int32_t& v) { if (p + 4 > d.size()) return false; std::memcpy (&v, d.data() + p, 4); p += 4; return true; };
    auto i64 = [&] (int64_t& v) { if (p + 8 > d.size()) return false; std::memcpy (&v, d.data() + p, 8); p += 8; return true; };

    int64_t available = 0, size = 0;
    if (! i64 (available) || ! i64 (size) || size <= 0 || size > 10000000) return {};

    std::vector<std::string> words;
    words.reserve ((size_t) size);

    for (int64_t k = 0; k < size; ++k)
    {
        int32_t n = 0;
        int64_t key = 0;
        if (! i32 (n) || n < 0 || p + (size_t) n > d.size()) return {};
        std::string w = d.substr (p, (size_t) n);
        p += (size_t) n;
        if (! i64 (key)) return {};

        if (! w.empty() && w[0] != '<' && w[0] != '#' && w[0] != '!' && w[0] != '[')
            words.push_back (std::move (w));
    }

    return words;
}

/** Najpodobniejsze slowo modelu (podobienstwo jak w WordTracker) albo pusto,
    gdy zadne nie dochodzi do minSim. Szukamy tylko wsrod slow o podobnej
    dlugosci — reszta i tak nie moze dojsc do progu. */
inline std::string nearestModelWord (const std::string& key, const std::vector<std::string>& modelWords,
                                     double minSim = 0.6)
{
    const size_t len = charCount (key);
    std::string best;
    double bestSim = minSim;

    for (const auto& w : modelWords)
    {
        const size_t l = charCount (w);
        if (l + 3 < len || l > len + 3) continue;

        const double s = similarity (key, w);
        if (s > bestSim) { bestSim = s; best = w; }
    }

    return best;
}

/** Dla slow slownika, ktorych model nie zna: (slowo skryptu, zamiennik).
    Slowa bez dosc podobnego zamiennika nie wystepuja na liscie. */
inline std::vector<std::pair<std::string, std::string>>
    substitutes (const std::vector<std::string>& vocabulary, const std::vector<std::string>& modelWords)
{
    std::vector<std::pair<std::string, std::string>> out;
    if (modelWords.empty()) return out;

    const std::unordered_set<std::string> known (modelWords.begin(), modelWords.end());

    for (const auto& w : vocabulary)
        if (known.count (w) == 0)
        {
            auto sub = nearestModelWord (w, modelWords);
            if (! sub.empty()) out.push_back ({ w, std::move (sub) });
        }

    return out;
}

} // namespace tw
