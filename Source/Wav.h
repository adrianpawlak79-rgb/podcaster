/*
    Wav.h — odczyt i zapis WAV bez zadnych bibliotek.

    Obsluguje to, co naprawde wychodzi z kart dzwiekowych i DAW-ow: PCM 16, 24
    i 32 bity oraz float 32. Nieznane kawalki (LIST, bext, cue) sa przeskakiwane
    po dlugosci, a nie zakladane — pliki ze Studio One maja ich kilka i czytnik
    liczacy naglowek na sztywno wyladuje w srodku danych.

    Do programu z interfejsem uzywamy JUCE. To jest do narzedzi konsolowych,
    ktore maja sie budowac w dwie sekundy i nie ciagnac za soba pol frameworka.
*/

#pragma once

#ifdef _WIN32
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace wavio
{

struct Audio
{
    std::vector<std::vector<float>> channels;   ///< [kanal][probka]
    int sampleRate = 48000;

    size_t frames() const { return channels.empty() ? 0 : channels[0].size(); }
    int    numChannels() const { return (int) channels.size(); }

    /** Suma kanalow do mono, ze spadkiem o 3 dB na kanal, zeby nie przesterowac. */
    std::vector<float> toMono() const
    {
        if (channels.empty()) return {};
        if (channels.size() == 1) return channels[0];

        std::vector<float> m (channels[0].size(), 0.0f);
        const float g = 1.0f / (float) channels.size();

        for (const auto& c : channels)
            for (size_t i = 0; i < m.size() && i < c.size(); ++i)
                m[i] += c[i] * g;

        return m;
    }
};

namespace detail
{
    inline uint32_t rd32 (const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }
    inline uint16_t rd16 (const uint8_t* p) { return (uint16_t) ((uint32_t) p[0] | ((uint32_t) p[1] << 8)); }
}

namespace detail
{
    /** fopen, ktory na Windows rozumie UTF-8 (polskie litery w nazwie i folderze). */
    inline FILE* openFile (const std::string& path, const char* mode)
    {
       #ifdef _WIN32
        auto widen = [] (const std::string& s)
        {
            std::wstring w;
            const int n = MultiByteToWideChar (CP_UTF8, 0, s.c_str(), (int) s.size(), nullptr, 0);
            w.resize ((size_t) std::max (0, n));
            if (n > 0) MultiByteToWideChar (CP_UTF8, 0, s.c_str(), (int) s.size(), &w[0], n);
            return w;
        };
        return _wfopen (widen (path).c_str(), widen (mode).c_str());
       #else
        return std::fopen (path.c_str(), mode);
       #endif
    }

    inline bool removeFile (const std::string& path)
    {
       #ifdef _WIN32
        std::wstring w; const int n = MultiByteToWideChar (CP_UTF8, 0, path.c_str(), (int) path.size(), nullptr, 0);
        w.resize ((size_t) std::max (0, n)); if (n > 0) MultiByteToWideChar (CP_UTF8, 0, path.c_str(), (int) path.size(), &w[0], n);
        return _wremove (w.c_str()) == 0;
       #else
        return std::remove (path.c_str()) == 0;
       #endif
    }

    inline bool renameFile (const std::string& from, const std::string& to)
    {
       #ifdef _WIN32
        auto widen = [] (const std::string& s)
        {
            std::wstring w; const int n = MultiByteToWideChar (CP_UTF8, 0, s.c_str(), (int) s.size(), nullptr, 0);
            w.resize ((size_t) std::max (0, n)); if (n > 0) MultiByteToWideChar (CP_UTF8, 0, s.c_str(), (int) s.size(), &w[0], n);
            return w;
        };
        return MoveFileExW (widen (from).c_str(), widen (to).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
       #else
        return std::rename (from.c_str(), to.c_str()) == 0;
       #endif
    }
}

/** @return pusty opis bledu przy powodzeniu. */
inline std::string read (const std::string& path, Audio& out)
{
    FILE* f = detail::openFile (path, "rb");
    if (! f) return "nie moge otworzyc pliku: " + path;

    std::fseek (f, 0, SEEK_END);
    const long size = std::ftell (f);
    std::fseek (f, 0, SEEK_SET);

    if (size < 44) { std::fclose (f); return "plik za krotki, zeby byl WAV-em"; }

    std::vector<uint8_t> buf ((size_t) size);
    const size_t got = std::fread (buf.data(), 1, (size_t) size, f);
    std::fclose (f);

    if (got != (size_t) size) return "nie udalo sie wczytac calosci";

    using namespace detail;

    if (std::memcmp (buf.data(), "RIFF", 4) != 0 || std::memcmp (buf.data() + 8, "WAVE", 4) != 0)
        return "to nie jest plik WAV";

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* data = nullptr;
    uint32_t dataBytes = 0;

    size_t p = 12;

    while (p + 8 <= buf.size())
    {
        const char* id = (const char*) (buf.data() + p);
        const uint32_t len = rd32 (buf.data() + p + 4);
        const size_t body = p + 8;

        if (std::memcmp (id, "fmt ", 4) == 0 && body + 16 <= buf.size())
        {
            format   = rd16 (buf.data() + body);
            channels = rd16 (buf.data() + body + 2);
            rate     = rd32 (buf.data() + body + 4);
            bits     = rd16 (buf.data() + body + 14);

            // WAVE_FORMAT_EXTENSIBLE — prawdziwy format siedzi w GUID
            if (format == 0xFFFE && body + 26 <= buf.size())
                format = rd16 (buf.data() + body + 24);
        }
        else if (std::memcmp (id, "data", 4) == 0)
        {
            data = buf.data() + body;
            dataBytes = (uint32_t) std::min ((size_t) len, buf.size() - body);
        }

        p = body + len + (len & 1);   // kawalki sa wyrownane do parzystej dlugosci
    }

    if (data == nullptr || channels == 0 || bits == 0)
        return "brak naglowka fmt albo danych";

    if (format != 1 && format != 3)
        return "nieobslugiwany format (tylko PCM i float)";

    // Uszkodzony naglowek (np. bits=1) dawal dzielenie przez zero (audyt 14.10).
    if (! (bits == 16 || bits == 24 || bits == 32) || (format == 3 && bits != 32))
        return "nieobslugiwana glebia bitowa: " + std::to_string (bits);
    if (channels > 64 || rate < 1000 || rate > 768000)
        return "podejrzany naglowek (kanaly albo czestotliwosc)";

    const int bytesPerSample = bits / 8;
    const size_t frames = dataBytes / (size_t) (bytesPerSample * channels);

    out.sampleRate = (int) rate;
    out.channels.assign ((size_t) channels, std::vector<float> (frames, 0.0f));

    for (size_t i = 0; i < frames; ++i)
    {
        for (int c = 0; c < channels; ++c)
        {
            const uint8_t* s = data + (i * (size_t) channels + (size_t) c) * (size_t) bytesPerSample;
            float v = 0.0f;

            if (format == 3 && bits == 32)
            {
                uint32_t bitsval = rd32 (s);
                std::memcpy (&v, &bitsval, 4);
            }
            else if (bits == 16)
            {
                v = (float) (int16_t) rd16 (s) / 32768.0f;
            }
            else if (bits == 24)
            {
                int32_t x = (int32_t) ((uint32_t) s[0] << 8 | (uint32_t) s[1] << 16 | (uint32_t) s[2] << 24);
                v = (float) (x >> 8) / 8388608.0f;
            }
            else if (bits == 32)
            {
                v = (float) (int32_t) rd32 (s) / 2147483648.0f;
            }

            out.channels[(size_t) c][i] = v;
        }
    }

    return {};
}

/*  ZAPIS — bezpieczny (audyt 14.10).

    Dawniej zapis ignorowal wyniki fwrite i fclose i zawsze zwracal sukces:
    przy pelnym dysku ekran pokazywal "Gotowe", a pliku nie bylo albo byl
    uciety. Teraz:
      - kazdy zapis i zamkniecie jest sprawdzane,
      - piszemy do pliku obok (".zapis.tmp") i dopiero po poprawnym zamknieciu
        podmieniamy plik docelowy — przerwany zapis nie zostawia polowy pliku
        pod wlasciwa nazwa,
      - blokami po 64 tys. probek, nie po probce (godzina = sekundy, nie minuty),
      - bez kopii materialu: writePcm bierze wskazniki na kanaly. */

/** PCM 16 albo 24 bity. channels: wskazniki na kanaly po frames probek.
    progress (opcjonalnie) dostaje 0..1. @return pusty opis bledu przy powodzeniu. */
inline std::string writePcm (const std::string& path, const std::vector<const float*>& channels,
                             size_t frames, int sampleRate, int bits, double* progress = nullptr)
{
    if (channels.empty() || frames == 0) return "nie ma czego zapisac";
    if (bits != 16 && bits != 24) return "nieobslugiwana glebia bitowa";
    if (sampleRate <= 0) return "zla czestotliwosc probkowania";

    const uint64_t ch = channels.size();
    const uint64_t bytesPer = (uint64_t) bits / 8;
    const uint64_t dataBytes64 = (uint64_t) frames * ch * bytesPer;
    if (dataBytes64 + 36 > 0xFFFFFFFFull)
        return "nagranie za dlugie na jeden plik WAV (limit 4 GB)";

    const std::string temp = path + ".zapis.tmp";
    FILE* f = detail::openFile (temp, "wb");
    if (! f) return "nie moge utworzyc pliku: " + path;

    bool ok = true;
    auto put = [&] (const void* d, size_t n) { if (ok && std::fwrite (d, 1, n, f) != n) ok = false; };

    const uint32_t dataBytes = (uint32_t) dataBytes64, riff = 36 + dataBytes, sixteen = 16;
    const uint16_t fmt = 1, chs = (uint16_t) ch, bps = (uint16_t) bits, align = (uint16_t) (ch * bytesPer);
    const uint32_t rate = (uint32_t) sampleRate, byteRate = rate * (uint32_t) align;
    put ("RIFF", 4); put (&riff, 4); put ("WAVE", 4);
    put ("fmt ", 4); put (&sixteen, 4); put (&fmt, 2); put (&chs, 2);
    put (&rate, 4); put (&byteRate, 4); put (&align, 2); put (&bps, 2);
    put ("data", 4); put (&dataBytes, 4);

    const size_t block = 65536;
    std::vector<uint8_t> buf ((size_t) (block * ch * bytesPer));
    for (size_t at = 0; at < frames && ok; at += block)
    {
        const size_t n = std::min (block, frames - at);
        uint8_t* o = buf.data();
        for (size_t i = 0; i < n; ++i)
            for (uint64_t c = 0; c < ch; ++c)
            {
                float v = channels[(size_t) c][at + i];
                if (! (v == v)) v = 0.0f;                 // NaN nie trafia do pliku
                v = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
                if (bits == 24)
                {
                    const int32_t x = (int32_t) (v * 8388607.0f);
                    *o++ = (uint8_t) (x & 0xff); *o++ = (uint8_t) ((x >> 8) & 0xff); *o++ = (uint8_t) ((x >> 16) & 0xff);
                }
                else
                {
                    const int16_t x = (int16_t) std::lround (v * 32767.0f);
                    *o++ = (uint8_t) (x & 0xff); *o++ = (uint8_t) ((x >> 8) & 0xff);
                }
            }
        put (buf.data(), (size_t) (o - buf.data()));
        if (progress != nullptr) *progress = (double) (at + n) / (double) frames;
    }

    if (ok && std::fflush (f) != 0) ok = false;
    if (ok && std::ferror (f)) ok = false;
    if (std::fclose (f) != 0) ok = false;

    if (! ok)
    {
        detail::removeFile (temp);
        return "zapis sie nie udal (pelny dysk albo brak dostepu): " + path;
    }
    if (! detail::renameFile (temp, path))
    {
        detail::removeFile (temp);
        return "nie moge zapisac pod wlasciwa nazwa: " + path;
    }
    return {};
}

inline std::string writeBits (const std::string& path, const Audio& a, int bits)
{
    std::vector<const float*> ptr;
    for (const auto& c : a.channels)
    {
        if (c.size() != a.frames()) return "kanaly maja rozna dlugosc";
        ptr.push_back (c.data());
    }
    return writePcm (path, ptr, a.frames(), a.sampleRate, bits);
}

/** Zapis 24-bitowy — tyle, ile ma sens dla materialu do publikacji. */
inline std::string write24 (const std::string& path, const Audio& a) { return writeBits (path, a, 24); }

/** Zapis 16-bitowy — do plikow, ktore ida dalej jako MP3 albo na plyte.
    Dither przy skracaniu robi lancuch, nie ten zapis. */
inline std::string write16 (const std::string& path, const Audio& a) { return writeBits (path, a, 16); }

} // namespace wavio
