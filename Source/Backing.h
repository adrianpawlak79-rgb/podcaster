/*
  ==============================================================================

    Backing.h — podklad muzyczny: wczytanie pliku do pamieci (24.10).

    Adrian nagrywa wiersze do bitu: muzyka gra w sluchawkach, glos ma w nia
    trafic. Muzyka jest nieruszalna — nie tniemy jej, nie przesuwamy; gra od
    poczatku, a po R od miejsca, w ktorym glos wypadnie po cieciach
    (Recorder::cueBacking, EditedView::editedTime).

    Caly plik idzie do pamieci w czestotliwosci karty (Resampler z Colour.h),
    bo watek audio nie moze czytac z dysku. 20 minut stereo w 48 kHz to
    ok. 460 MB — dluzszych nie przyjmujemy.

    Obok nagrania zostaje "<nagranie>.podklad.txt" ze sciezka muzyki: ekran
    montazu i miks wiedza wtedy, do czego glos byl nagrywany.

  ==============================================================================
*/

#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "Recorder.h"
#include "DSP/Mix/Colour.h"

namespace zs
{
struct BackingTrack
{
    std::vector<float> left, right;   ///< right puste = mono
    double rate = 0.0;
    double seconds() const noexcept { return rate > 0.0 ? (double) left.size() / rate : 0.0; }

    static constexpr double maxMinutes = 20.0;

    /** Wczytuje i przelicza do targetRate. Zwraca opis bledu albo pusty napis. */
    juce::String load (const juce::File& file, double targetRate)
    {
        left.clear(); right.clear(); rate = 0.0;
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr || reader->sampleRate <= 0.0)
            return PL ("Nie umiem otworzyć muzyki „") + file.getFileName() + PL ("” (WAV, MP3, FLAC, AIFF).");
        if ((double) reader->lengthInSamples / reader->sampleRate > maxMinutes * 60.0)
            return PL ("Muzyka jest dłuższa niż 20 minut.");

        const int channels = juce::jlimit (1, 2, (int) reader->numChannels);
        const int length = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> buf (channels, length);
        if (! reader->read (&buf, 0, length, 0, true, channels > 1))
            return PL ("Nie udało się przeczytać muzyki „") + file.getFileName() + PL ("”.");

        const auto take = [&] (int ch)
        {
            std::vector<float> v (buf.getReadPointer (ch), buf.getReadPointer (ch) + length);
            if (std::abs (reader->sampleRate - targetRate) > 0.5)
            {
                mix::Resampler rs;
                rs.prepare ((int) std::lround (reader->sampleRate), (int) std::lround (targetRate), 32);
                std::vector<float> out;
                rs.render (v, out);
                v.swap (out);
            }
            return v;
        };
        left = take (0);
        if (channels > 1) right = take (1);
        rate = targetRate;
        return {};
    }

    /** "<nagranie>.podklad.txt" obok nagrania. */
    static juce::File noteFor (const juce::File& wav)
    {
        return wav.getSiblingFile (wav.getFileNameWithoutExtension() + ".podklad.txt");
    }
    static void writeNote (const juce::File& wav, const juce::File& music)
    {
        noteFor (wav).replaceWithText (music.getFullPathName() + "\n");
    }
    /** Muzyka, do ktorej nagrano wav (pusty File, gdy bez podkladu). */
    static juce::File musicFor (const juce::File& wav)
    {
        const auto note = noteFor (wav);
        if (! note.existsAsFile()) return {};
        return juce::File (note.loadFileAsString().trim());
    }
};
}
