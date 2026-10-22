/*
    Mp3.h — zapis MP3 koderem wbudowanym w Windows (Media Foundation).

    ---------------------------------------------------------------------------
    DLACZEGO MEDIA FOUNDATION, A NIE LAME

    Windows 8, 10 i 11 maja wlasny koder MP3. Program nie wozi wtedy zadnej
    biblioteki ani licencji (LAME to LGPL — trzeba by dolaczac DLL i zrodla).
    Patenty MP3 wygasly w 2017, wiec koder systemowy jest po prostu dostepny.

    Wyjatek: Windows w wersji "N" (Europa) nie ma Media Foundation, dopoki
    ktos nie doinstaluje "Media Feature Pack". Wtedy write() zwraca opis
    bledu, a WAV jest juz zapisany — program mowi, czego brakuje.

    ---------------------------------------------------------------------------
    JAK

    Plik powstaje pod nazwa tymczasowa i dostaje wlasciwa dopiero po
    poprawnym zamknieciu (jak WAV w Wav.h) — przerwany zapis nie zostawia
    pliku, ktory wyglada na gotowy. Kontener MP3 jest podany wprost, bo
    nazwa tymczasowa nie konczy sie na .mp3.

    Koder przyjmuje 16 bitow PCM; float zamieniamy z ditherem TPDF, zeby
    ciche koncowki slow nie dostaly zgrzytu kwantyzacji.

    Bitrate: koder ma liste dozwolonych wartosci (zalezna od czestotliwosci
    i liczby kanalow). Bierzemy najblizsza zadanej — dla glosu mono 128
    kbit/s to jakosc, ktorej nikt nie odrozni od WAV-a.

    Na Linuksie (testy, symulacja) MP3 nie ma: write() zwraca opis bledu.
*/

#pragma once

#include <string>
#include <vector>

namespace zs
{
namespace mp3
{

/** Czy na tym komputerze da sie zapisac MP3 (sprawdza obecnosc kodera). */
bool available();

/** Zapisuje MP3. channels: 1 albo 2 wskazniki na frames probek float.
    progress (opcjonalnie) dostaje 0..1. @return pusty opis przy powodzeniu.
    Implementacja w Mp3.cpp — osobno, bo windows.h (makra near, far, small)
    psuje reszte programu. */
std::string write (const std::string& path, const std::vector<const float*>& channels,
                   size_t frames, int sampleRate, int kbps = 128, double* progress = nullptr);

} // namespace mp3
} // namespace zs
