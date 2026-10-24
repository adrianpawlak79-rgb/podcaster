/*
    Settings.h — co program pamieta miedzy uruchomieniami.

    ---------------------------------------------------------------------------
    PO CO

    Program, ktory przy kazdym wlaczeniu kaze przejsc kreator i odszukac plik z
    tekstem, jest programem do jednorazowego obejrzenia. Przy drugim razie
    czlowiek zaczyna go omijac, przy trzecim przestaje wlaczac.

    A to nie jest drobiazg o wygodzie. Kreator ustala rzeczy, ktorych pozniej
    nie da sie odtworzyc z samego nagrania: ile wynosi tlo pokoju, czy system
    maczal palce w sygnale, jak glosno gra odsluch. Zmuszanie do powtarzania
    tego codziennie konczy sie tym, ze ktos klika "pomin" — i wszystkie dalsze
    decyzje programu oparte sa na niczym.

    ---------------------------------------------------------------------------
    CO SIE PAMIETA, A CZEGO NIE

    Pamietamy to, co opisuje SPRZET i NAWYKI: wynik kreatora, folder sesji,
    ostatni tekst, wielkosc czcionki, polozenie okna.

    Nie pamietamy niczego, co opisuje KONKRETNE NAGRANIE — bo nastepne bedzie
    inne, a ustawienie przeniesione z poprzedniego to najcichszy rodzaj bledu:
    taki, ktorego nikt nie szuka.

    ---------------------------------------------------------------------------
    GDZIE

    W katalogu danych uzytkownika, nie obok programu. Program moze lezec w
    "Program Files", gdzie zapis wymaga uprawnien administratora — i wtedy
    ustawienia po cichu nie zapisuja sie wcale.
*/

#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "DSP/Mix/RoomCheck.h"

//==============================================================================
class Settings
{
public:
    Settings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "Zielone swiatlo";
        o.filenameSuffix      = "settings";
        o.osxLibrarySubFolder = "Application Support";
        o.folderName          = "Zielone swiatlo";

        props.setStorageParameters (o);
    }

    juce::PropertiesFile& file() { return *props.getUserSettings(); }

    void flush() { props.saveIfNeeded(); }

    //==========================================================================
    // ---- kreator -------------------------------------------------------------

    bool hardwareChecked() const
    {
        return props.getUserSettings()->getBoolValue ("hardwareChecked", false);
    }

    void setHardwareChecked (bool done, const juce::String& summary = {})
    {
        auto& f = *props.getUserSettings();
        f.setValue ("hardwareChecked", done);
        f.setValue ("hardwareSummary", summary);
        f.setValue ("hardwareCheckedAt", juce::Time::getCurrentTime().toISO8601 (true));
        props.saveIfNeeded();
    }

    /** Pomiary pokoju dla miksu. known = false, gdy kreatora nie przechodzono
        albo zapisala go wersja programu, ktora tych liczb jeszcze nie znala. */
    void setRoomCheck (const mix::RoomCheck& r)
    {
        auto& f = *props.getUserSettings();
        f.setValue ("roomKnown",          r.known);
        f.setValue ("roomNoiseFloorDb",   r.noiseFloorDb);
        f.setValue ("roomHumHz",          r.humHz);
        f.setValue ("roomHumExcessDb",    r.humExcessDb);
        f.setValue ("roomProximityDb",    r.proximityDb);
        f.setValue ("roomProximityLimit", r.proximityLimitDb);
        f.setValue ("roomBleedPercent",   r.bleedPercent);
        f.setValue ("roomBleedLimit",     r.bleedLimit);
        props.saveIfNeeded();
    }

    mix::RoomCheck roomCheck() const
    {
        auto& f = *props.getUserSettings();
        mix::RoomCheck r;
        r.known            = f.getBoolValue   ("roomKnown", false);
        r.noiseFloorDb     = f.getDoubleValue ("roomNoiseFloorDb", -140.0);
        r.humHz            = f.getDoubleValue ("roomHumHz", 0.0);
        r.humExcessDb      = f.getDoubleValue ("roomHumExcessDb", 0.0);
        r.proximityDb      = f.getDoubleValue ("roomProximityDb", 0.0);
        r.proximityLimitDb = f.getDoubleValue ("roomProximityLimit", -5.0);
        r.bleedPercent     = f.getDoubleValue ("roomBleedPercent", 0.0);
        r.bleedLimit       = f.getDoubleValue ("roomBleedLimit", 4.0);
        r.daysAgo          = r.known ? daysSinceCheck() : -1;
        return r;
    }

    juce::String hardwareSummary() const
    {
        return props.getUserSettings()->getValue ("hardwareSummary");
    }

    /** Ile dni temu sprawdzano sprzet. -1 = nigdy.

        Program nie blokuje nagrywania po miesiacu, ale ma prawo przypomniec:
        mikrofon mogl zostac przestawiony, a pokoj umeblowany. */
    int daysSinceCheck() const
    {
        const auto s = props.getUserSettings()->getValue ("hardwareCheckedAt");

        if (s.isEmpty()) return -1;

        const auto then = juce::Time::fromISO8601 (s);
        return (int) ((juce::Time::getCurrentTime().toMilliseconds()
                     - then.toMilliseconds()) / (1000LL * 60 * 60 * 24));
    }

    //==========================================================================
    // ---- karta dzwiekowa -----------------------------------------------------

    /** Stan AudioDeviceManager (XML): sterownik, karta, wlaczone wejscia.
        Bez tego program po kazdym uruchomieniu bral domyslna karte systemu i
        dwa pierwsze wejscia — a czlowiek wybieral swoje od nowa. */
    juce::String audioState() const
    {
        return props.getUserSettings()->getValue ("audioState");
    }

    void setAudioState (const juce::String& xml)
    {
        props.getUserSettings()->setValue ("audioState", xml);
        props.saveIfNeeded();
    }

    //==========================================================================
    // ---- prompter ------------------------------------------------------------

    juce::File lastScript() const
    {
        const auto p = props.getUserSettings()->getValue ("lastScript");
        return p.isEmpty() ? juce::File() : juce::File (p);
    }

    void setLastScript (const juce::File& f)
    {
        props.getUserSettings()->setValue ("lastScript", f.getFullPathName());
        props.saveIfNeeded();
    }

    /** Powiekszenie czcionki promptera, 1,0 = tyle, ile zaprojektowano. */
    double fontScale() const
    {
        return juce::jlimit (0.6, 2.2,
            props.getUserSettings()->getDoubleValue ("fontScale", 1.0));
    }

    void setFontScale (double v)
    {
        props.getUserSettings()->setValue ("fontScale", juce::jlimit (0.6, 2.2, v));
        props.saveIfNeeded();
    }

    /** Odbicie lustrzane — do szybki promptera, ktora odbija obraz z monitora. */
    /** Czy R wycina pomylki samo po zatrzymaniu (punch and roll).

        Domyslnie NIE. Proba na prawdziwym glosie (11 lektorow, 354 R,
        Docs/badania/SYMULACJA.md): nawet ze straznikiem (Punch::guard) 5%
        wyciec zabieralo dobre czytanie — glownie koncowke poprzedniego wersu,
        gdy szukanie ciszy trafilo w pauze po przecinku. Wyciete slowo w
        podcascie to katastrofa, zostawiona pomylka — niewygoda. Wlaczymy,
        gdy ciecia beda sprawdzane dokladnymi czasami slow i proba da 0
        niebezpiecznych. Do tego czasu R zaznacza miejsca, a czlowiek tnie na
        ekranie Montaz. */
    /** Wzorzec brzmienia (DSP/Mix/VoiceProfile.h, tekst z toText()). Pusty =
        bez wzorca. To nawyk, nie konkretne nagranie: barwa, ktora czlowiek
        sam zatwierdzil, i ktora maja miec kolejne odcinki. */
    juce::String voiceProfile() const
    {
        return props.getUserSettings()->getValue ("voiceProfile");
    }

    void setVoiceProfile (const juce::String& text)
    {
        if (text.isEmpty()) props.getUserSettings()->removeValue ("voiceProfile");
        else                props.getUserSettings()->setValue ("voiceProfile", text);
        props.saveIfNeeded();
    }

    bool punchAutoCut() const
    {
        return props.getUserSettings()->getBoolValue ("punchAutoCut", false);
    }

    bool mirrored() const
    {
        return props.getUserSettings()->getBoolValue ("mirrored", false);
    }

    void setMirrored (bool v)
    {
        props.getUserSettings()->setValue ("mirrored", v);
        props.saveIfNeeded();
    }

    //==========================================================================
    // ---- nagrania --------------------------------------------------------------

    juce::File sessionFolder() const
    {
        const auto p = props.getUserSettings()->getValue ("sessionFolder");

        if (p.isNotEmpty() && juce::File (p).isDirectory())
            return juce::File (p);

        return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                   .getChildFile ("Zielone swiatlo");
    }

    void setSessionFolder (const juce::File& f)
    {
        props.getUserSettings()->setValue ("sessionFolder", f.getFullPathName());
        props.saveIfNeeded();
    }

    //==========================================================================
    // ---- okno --------------------------------------------------------------------

    juce::String windowState() const
    {
        return props.getUserSettings()->getValue ("window");
    }

    void setWindowState (const juce::String& s)
    {
        props.getUserSettings()->setValue ("window", s);
        props.saveIfNeeded();
    }

private:
    /** mutable, bo getUserSettings() tworzy plik przy pierwszym pytaniu — czyli
        samo CZYTANIE ustawienia zmienia stan tego obiektu. Zapytanie "jaka
        czcionka" jest dla wolajacego operacja stala i ma taka zostac, wiec
        klamstwo zostaje tutaj, w jednym miejscu, zamiast rozlazc sie po
        wszystkich wywolaniach jako brak const. */
    mutable juce::ApplicationProperties props;
};
