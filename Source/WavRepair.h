/*
    WavRepair.h — naprawa naglowka WAV po przerwanym nagraniu.

    Audyt 14.10: po awarii programu (albo wylaczonym pradzie) w trakcie
    nagrania dane sa na dysku, ale naglowek mowi tyle, ile bylo przy jego
    ostatnim zapisie (dawniej: 0 probek, teraz co ~2 s — DiskWriter). Czytnik
    wierzy naglowkowi, wiec nagranie wygladalo na puste.

    Tu: dlugosc danych = to, co naprawde jest w pliku od poczatku chunku
    "data" do konca (przyciete do pelnej ramki), i taki sam rozmiar RIFF.
    Nic poza dwoma polami rozmiaru nie jest zmieniane.

    Test: Tests/odzysk-test.cpp.
*/

#pragma once

#include "Wav.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace wavio
{

struct RepairResult
{
    bool   ok = false;          ///< plik jest teraz czytelny
    bool   changed = false;     ///< naglowek trzeba bylo poprawic
    double seconds = 0.0;       ///< ile nagrania jest w pliku
    std::string why;            ///< gdy nie ok
};

inline RepairResult repairHeader (const std::string& path)
{
    RepairResult r;
    FILE* f = detail::openFile (path, "r+b");
    if (! f) { r.why = "nie moge otworzyc pliku"; return r; }

    auto fail = [&] (const char* why) { std::fclose (f); r.why = why; return r; };
    auto u32 = [] (const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); };
    auto u16 = [] (const uint8_t* p) { return (uint16_t) (p[0] | (p[1] << 8)); };

    if (std::fseek (f, 0, SEEK_END) != 0) return fail ("nie moge odczytac dlugosci");
    const long long size = std::ftell (f);
    if (size < 44) return fail ("plik za krotki na naglowek WAV");
    std::fseek (f, 0, SEEK_SET);

    uint8_t h[12];
    if (std::fread (h, 1, 12, f) != 12 || std::memcmp (h, "RIFF", 4) != 0 || std::memcmp (h + 8, "WAVE", 4) != 0)
        return fail ("to nie jest plik WAV");

    long long pos = 12, dataAt = -1;
    uint16_t blockAlign = 0;
    uint32_t rate = 0;
    while (pos + 8 <= size)
    {
        uint8_t c[8];
        std::fseek (f, (long) pos, SEEK_SET);
        if (std::fread (c, 1, 8, f) != 8) break;
        const uint32_t len = u32 (c + 4);
        if (std::memcmp (c, "fmt ", 4) == 0)
        {
            uint8_t fmt[16];
            if (len < 16 || std::fread (fmt, 1, 16, f) != 16) return fail ("uszkodzony fragment fmt");
            rate = u32 (fmt + 4);
            blockAlign = u16 (fmt + 12);
        }
        if (std::memcmp (c, "data", 4) == 0) { dataAt = pos; break; }
        pos += 8 + (long long) len + (len & 1);
    }
    if (dataAt < 0 || blockAlign == 0 || rate == 0) return fail ("nie znalazlem danych dzwieku");

    const long long avail = size - (dataAt + 8);
    const long long dataLen = (avail / blockAlign) * blockAlign;
    if (dataLen + dataAt + 8 - 8 > 0xFFFFFFFFll) return fail ("plik wiekszy niz 4 GB");
    r.seconds = (double) (dataLen / blockAlign) / (double) rate;

    uint8_t cur[4];
    std::fseek (f, (long) dataAt + 4, SEEK_SET);
    if (std::fread (cur, 1, 4, f) != 4) return fail ("nie moge odczytac naglowka");
    uint8_t riffNow[4];
    std::fseek (f, 4, SEEK_SET);
    if (std::fread (riffNow, 1, 4, f) != 4) return fail ("nie moge odczytac naglowka");
    const uint32_t wantData = (uint32_t) dataLen, wantRiff = (uint32_t) (dataAt + 8 + dataLen - 8);

    if (u32 (cur) != wantData || u32 (riffNow) != wantRiff)
    {
        auto put = [&] (long at, uint32_t v)
        {
            const uint8_t b[4] = { (uint8_t) v, (uint8_t) (v >> 8), (uint8_t) (v >> 16), (uint8_t) (v >> 24) };
            return std::fseek (f, at, SEEK_SET) == 0 && std::fwrite (b, 1, 4, f) == 4;
        };
        if (! put (4, wantRiff) || ! put ((long) dataAt + 4, wantData)) return fail ("nie moge zapisac poprawionego naglowka");
        r.changed = true;
    }
    if (std::fclose (f) != 0) { r.why = "nie moge zamknac pliku"; return r; }
    r.ok = true;
    return r;
}

} // namespace wavio
