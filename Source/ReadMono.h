/*
    ReadMono.h — caly plik jako jeden kanal, kawalkami, bez drugiej kopii.

    Godzina nagrania 48 kHz to 0,7 GB na kopie. Dotad wycinanie i ekran oceny
    czytaly plik do AudioBuffer i dopiero z niego skladaly mono — dwie kopie
    naraz (Tools/zs-dlugo.cpp: 2,8 GB przy wycinaniu po godzinie). Tu czytamy
    po 64 tys. probek i od razu sumujemy kanaly.
*/

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>

namespace zs
{

/** false, gdy odczyt sie nie udal. Wiele kanalow — srednia. */
inline bool readMono (juce::AudioFormatReader& reader, int len, std::vector<float>& mono)
{
    const int ch = juce::jmax (1, (int) reader.numChannels);
    mono.assign ((size_t) juce::jmax (0, len), 0.0f);
    juce::AudioBuffer<float> chunk (ch, 65536);
    const float g = 1.0f / (float) ch;

    for (int at = 0; at < len; at += chunk.getNumSamples())
    {
        const int n = juce::jmin (chunk.getNumSamples(), len - at);
        if (! reader.read (&chunk, 0, n, at, true, ch > 1)) return false;
        for (int c = 0; c < ch; ++c)
        {
            const float* src = chunk.getReadPointer (c);
            float* dst = mono.data() + at;
            if (ch == 1) std::copy (src, src + n, dst);
            else for (int i = 0; i < n; ++i) dst[i] += src[i] * g;
        }
    }
    return true;
}

} // namespace zs
