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

/** @return pusty opis bledu przy powodzeniu. */
inline std::string read (const std::string& path, Audio& out)
{
    FILE* f = std::fopen (path.c_str(), "rb");
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

/** Zapis 24-bitowy — tyle, ile ma sens dla materialu do publikacji. */
inline std::string write24 (const std::string& path, const Audio& a)
{
    if (a.channels.empty()) return "nie ma czego zapisac";

    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return "nie moge utworzyc pliku: " + path;

    const uint16_t ch = (uint16_t) a.numChannels();
    const uint32_t frames = (uint32_t) a.frames();
    const uint16_t bits = 24;
    const uint32_t dataBytes = frames * ch * (bits / 8);
    const uint32_t riff = 36 + dataBytes;
    const uint32_t sixteen = 16;
    const uint16_t fmt = 1;
    const uint32_t rate = (uint32_t) a.sampleRate;
    const uint32_t byteRate = rate * ch * bits / 8;
    const uint16_t align = (uint16_t) (ch * bits / 8);

    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f);
    std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); std::fwrite (&sixteen, 4, 1, f);
    std::fwrite (&fmt, 2, 1, f);   std::fwrite (&ch, 2, 1, f);
    std::fwrite (&rate, 4, 1, f);  std::fwrite (&byteRate, 4, 1, f);
    std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);

    std::vector<uint8_t> row ((size_t) (ch * 3));

    for (uint32_t i = 0; i < frames; ++i)
    {
        for (int c = 0; c < ch; ++c)
        {
            float v = a.channels[(size_t) c][i];
            if (v >  1.0f) v =  1.0f;
            if (v < -1.0f) v = -1.0f;

            const int32_t x = (int32_t) (v * 8388607.0f);
            row[(size_t) (c * 3 + 0)] = (uint8_t) (x & 0xff);
            row[(size_t) (c * 3 + 1)] = (uint8_t) ((x >> 8) & 0xff);
            row[(size_t) (c * 3 + 2)] = (uint8_t) ((x >> 16) & 0xff);
        }

        std::fwrite (row.data(), 1, row.size(), f);
    }

    std::fclose (f);
    return {};
}

/** Zapis 16-bitowy — do plikow, ktore ida dalej jako MP3 albo na plyte.
    Dither przy skracaniu robi lancuch, nie ten zapis. */
inline std::string write16 (const std::string& path, const Audio& a)
{
    if (a.channels.empty()) return "nie ma czego zapisac";

    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return "nie moge utworzyc pliku: " + path;

    const uint16_t ch = (uint16_t) a.numChannels();
    const uint32_t frames = (uint32_t) a.frames();
    const uint16_t bits = 16;
    const uint32_t dataBytes = frames * ch * (bits / 8);
    const uint32_t riff = 36 + dataBytes;
    const uint32_t sixteen = 16;
    const uint16_t fmt = 1;
    const uint32_t rate = (uint32_t) a.sampleRate;
    const uint32_t byteRate = rate * ch * bits / 8;
    const uint16_t align = (uint16_t) (ch * bits / 8);

    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f);
    std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); std::fwrite (&sixteen, 4, 1, f);
    std::fwrite (&fmt, 2, 1, f);   std::fwrite (&ch, 2, 1, f);
    std::fwrite (&rate, 4, 1, f);  std::fwrite (&byteRate, 4, 1, f);
    std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);

    std::vector<int16_t> row ((size_t) ch);

    for (uint32_t i = 0; i < frames; ++i)
    {
        for (int c = 0; c < ch; ++c)
        {
            float v = a.channels[(size_t) c][i];
            if (v >  1.0f) v =  1.0f;
            if (v < -1.0f) v = -1.0f;
            row[(size_t) c] = (int16_t) std::lround (v * 32767.0f);
        }

        std::fwrite (row.data(), sizeof (int16_t), row.size(), f);
    }

    std::fclose (f);
    return {};
}

} // namespace wavio
