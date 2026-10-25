/*
    Trace.h — slad krokow programu w pliku, do szukania zawieszen (25.10).

    Adrian 25.10 (Windows): po "Nagraj nowe podejscie" na ekranie Gotowe
    okno przestaje odpowiadac. Na Linuksie (wirtualna karta, Vosk, podklad)
    sie nie powtarza. Kazdy krok powrotu z Gotowe dopisuje linijke z czasem;
    po zawieszeniu ostatnia linijka mowi, na czym program stanal.

    Plik: <dane aplikacji>/Zielone swiatlo/slad.txt (Windows: %APPDATA%),
    obok ustawien. Najwyzej ok. 200 kB — starszy slad jest wtedy kasowany.
*/

#pragma once
#include <juce_core/juce_core.h>

namespace zs
{
inline juce::File traceFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Zielone swiatlo").getChildFile ("slad.txt");
}

/** Z watku okna albo pracy w tle; nigdy z watku dzwieku (pisze na dysk). */
inline void trace (const juce::String& what)
{
    static juce::CriticalSection lock;
    const juce::ScopedLock sl (lock);
    const auto f = traceFile();
    if (f.getSize() > 200 * 1024) f.deleteFile();
    f.getParentDirectory().createDirectory();
    juce::FileOutputStream out (f);
    if (! out.openedOk()) return;
    out << juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S.")
        << juce::String (juce::Time::getCurrentTime().getMilliseconds()).paddedLeft ('0', 3)
        << "  " << what << "\n";
    out.flush();
}
}
