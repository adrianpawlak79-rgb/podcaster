/*
    Id3.h — tag ID3v2.4 na poczatku pliku MP3. Bez JUCE, bez systemu.

    ---------------------------------------------------------------------------
    PO CO

    MP3 prosto z kodera nie mowi, co w nim jest: odtwarzacz i hosting podcastu
    pokazuja nazwe pliku zamiast tytulu, a autora nie pokazuja wcale. Tag
    dopisujemy na poczatku gotowego pliku — koder Media Foundation (Mp3.cpp)
    o tagach nic nie wie, wiec nie ma z czym sie bic.

    ---------------------------------------------------------------------------
    CO

    ID3v2.4, tekst w UTF-8 (bajt kodowania 3), polskie litery bez przygod.
    Ramki: TIT2 (tytul), TPE1 (autor), TRCK (numer odcinka), TDRC (rok),
    COMM (komentarz). Puste pola nie dostaja ramki.

    Rozmiary: w naglowku i w ramkach v2.4 zapisane "synchsafe" (7 bitow na
    bajt, najstarszy bit zawsze 0), zeby w tagu nie bylo ciagu 0xFF 0xEx, ktory
    odtwarzacz wzialby za poczatek ramki dzwieku.

    Gdy plik ma juz tag ID3v2 na poczatku, jest zastepowany, nie dublowany.

    parse() czyta z powrotem to, co build() zapisal — do testu (id3-test.cpp).
    Wystarcza do tagow tej wersji; nie jest czytnikiem calego standardu.
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace zs
{
namespace id3
{

struct Tags
{
    std::string title;      ///< TIT2 (UTF-8)
    std::string artist;     ///< TPE1
    std::string episode;    ///< TRCK, np. "12"
    std::string year;       ///< TDRC, np. "2026"
    std::string comment;    ///< COMM
};

namespace detail
{
    inline void putSynchsafe (std::vector<uint8_t>& out, uint32_t v)
    {
        out.push_back ((uint8_t) ((v >> 21) & 0x7f));
        out.push_back ((uint8_t) ((v >> 14) & 0x7f));
        out.push_back ((uint8_t) ((v >> 7) & 0x7f));
        out.push_back ((uint8_t) (v & 0x7f));
    }

    inline uint32_t getSynchsafe (const uint8_t* p)
    {
        return ((uint32_t) (p[0] & 0x7f) << 21) | ((uint32_t) (p[1] & 0x7f) << 14)
             | ((uint32_t) (p[2] & 0x7f) << 7)  |  (uint32_t) (p[3] & 0x7f);
    }

    inline void putFrame (std::vector<uint8_t>& out, const char* id, const std::vector<uint8_t>& body)
    {
        out.insert (out.end(), id, id + 4);
        putSynchsafe (out, (uint32_t) body.size());
        out.push_back (0); out.push_back (0);                 // flagi
        out.insert (out.end(), body.begin(), body.end());
    }

    inline void putTextFrame (std::vector<uint8_t>& out, const char* id, const std::string& text)
    {
        if (text.empty()) return;
        std::vector<uint8_t> body { 3 };                      // 3 = UTF-8
        body.insert (body.end(), text.begin(), text.end());
        putFrame (out, id, body);
    }
}

/** Sam tag (naglowek + ramki), bez dzwieku. Pusty, gdy nie ma zadnego pola. */
inline std::vector<uint8_t> build (const Tags& t)
{
    std::vector<uint8_t> frames;
    detail::putTextFrame (frames, "TIT2", t.title);
    detail::putTextFrame (frames, "TPE1", t.artist);
    detail::putTextFrame (frames, "TRCK", t.episode);
    detail::putTextFrame (frames, "TDRC", t.year);
    if (! t.comment.empty())
    {
        // kodowanie, jezyk (3 litery), opis (pusty, zakonczony zerem), tekst
        std::vector<uint8_t> body { 3, 'p', 'o', 'l', 0 };
        body.insert (body.end(), t.comment.begin(), t.comment.end());
        detail::putFrame (frames, "COMM", body);
    }
    if (frames.empty()) return {};

    std::vector<uint8_t> out { 'I', 'D', '3', 4, 0, 0 };      // wersja 2.4.0, bez flag
    detail::putSynchsafe (out, (uint32_t) frames.size());
    out.insert (out.end(), frames.begin(), frames.end());
    return out;
}

/** Ile bajtow zajmuje tag ID3v2 na poczatku danych (0 = nie ma). */
inline size_t existingTagSize (const uint8_t* data, size_t size)
{
    if (size < 10 || data[0] != 'I' || data[1] != 'D' || data[2] != '3') return 0;
    if (data[3] == 0xff || data[4] == 0xff) return 0;
    if ((data[6] | data[7] | data[8] | data[9]) & 0x80) return 0;
    const size_t total = 10 + (size_t) detail::getSynchsafe (data + 6) + ((data[5] & 0x10) ? 10 : 0);
    return total <= size ? total : 0;
}

/** Plik MP3 z nowym tagiem na poczatku (stary ID3v2, jesli byl, odpada). */
inline std::vector<uint8_t> withTag (const std::vector<uint8_t>& mp3, const Tags& t)
{
    std::vector<uint8_t> out = build (t);
    const size_t skip = existingTagSize (mp3.data(), mp3.size());
    out.insert (out.end(), mp3.begin() + (std::ptrdiff_t) skip, mp3.end());
    return out;
}

/** Ramki tekstowe tagu: identyfikator -> tekst (COMM: sam tekst, bez jezyka i
    opisu). Pusta mapa, gdy na poczatku nie ma poprawnego tagu v2.4. */
inline std::map<std::string, std::string> parse (const std::vector<uint8_t>& data)
{
    std::map<std::string, std::string> out;
    const size_t total = existingTagSize (data.data(), data.size());
    if (total == 0 || data[3] != 4) return out;

    size_t at = 10;
    const size_t end = 10 + detail::getSynchsafe (data.data() + 6);
    while (at + 10 <= end && data[at] != 0)
    {
        const std::string id (reinterpret_cast<const char*> (&data[at]), 4);
        const size_t len = detail::getSynchsafe (&data[at + 4]);
        at += 10;
        if (len == 0 || at + len > end) break;
        size_t from = at + 1;                                  // za bajtem kodowania
        if (id == "COMM")
        {
            from += 3;                                         // jezyk
            while (from < at + len && data[from] != 0) ++from; // opis
            ++from;
        }
        if (from <= at + len)
            out[id] = std::string (reinterpret_cast<const char*> (&data[from]), at + len - from);
        at += len;
    }
    return out;
}

/** Numer odcinka z nazwy: "Odcinek 12" (wielkosc liter bez znaczenia) albo
    "#12". Pusty, gdy w nazwie nie ma ani jednego, ani drugiego. */
inline std::string episodeFromName (const std::string& name)
{
    const auto digitsAt = [&] (size_t i)
    {
        while (i < name.size() && (name[i] == ' ' || name[i] == '_')) ++i;
        std::string n;
        while (i < name.size() && name[i] >= '0' && name[i] <= '9' && n.size() < 4) n += name[i++];
        return n;
    };
    std::string lower = name;
    for (auto& c : lower) if (c >= 'A' && c <= 'Z') c = (char) (c - 'A' + 'a');

    const std::string word = "odcinek";
    for (size_t p = lower.find (word); p != std::string::npos; p = lower.find (word, p + 1))
    {
        const bool startsWord = p == 0 || ! ((lower[p - 1] >= 'a' && lower[p - 1] <= 'z'));
        const auto n = startsWord ? digitsAt (p + word.size()) : std::string();
        if (! n.empty()) return std::to_string (std::stoi (n));
    }
    for (size_t p = name.find ('#'); p != std::string::npos; p = name.find ('#', p + 1))
    {
        const auto n = digitsAt (p + 1);
        if (! n.empty()) return std::to_string (std::stoi (n));
    }
    return {};
}

} // namespace id3
} // namespace zs
