/*
    wav.h — zapis i odczyt WAV float 32-bit. Tylko na potrzeby testow:
    program wlasciwy uzywa JUCE. Tu chodzi o to, zeby dalo sie porownac nasz
    pomiar z niezalezna implementacja na TYM SAMYM pliku.
*/

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace wav
{

/** @param data  przeplecione probki [n * channels + c] */
inline bool writeFloat (const std::string& path, const std::vector<float>& data,
                        int channels, int sampleRate)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return false;

    const uint32_t bytes = (uint32_t) (data.size() * sizeof (float));
    const uint32_t riff  = 36 + bytes;
    const uint16_t fmt   = 3;                       // IEEE float
    const uint16_t bits  = 32;
    const uint32_t rate  = (uint32_t) sampleRate;
    const uint16_t ch    = (uint16_t) channels;
    const uint32_t byteRate = rate * ch * bits / 8;
    const uint16_t align    = (uint16_t) (ch * bits / 8);
    const uint32_t sixteen  = 16;

    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f);
    std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); std::fwrite (&sixteen, 4, 1, f);
    std::fwrite (&fmt, 2, 1, f);   std::fwrite (&ch, 2, 1, f);
    std::fwrite (&rate, 4, 1, f);  std::fwrite (&byteRate, 4, 1, f);
    std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&bytes, 4, 1, f);
    std::fwrite (data.data(), 1, bytes, f);

    std::fclose (f);
    return true;
}

} // namespace wav
