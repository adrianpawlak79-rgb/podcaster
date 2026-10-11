/*
    ustawienia-test.cpp — czy program naprawde pamieta.

    ---------------------------------------------------------------------------
    PO CO OSOBNY TEST

    Pamietanie ustawien to jedyna czesc programu, ktorej NIE DA SIE sprawdzic
    patrzac na program. Wszystko inne widac od razu: lampka sie zapala albo nie,
    tekst jedzie albo stoi. A zapis do pliku wyglada identycznie, kiedy dziala i
    kiedy sie nie udal — zobaczy to dopiero czlowiek, ktory nastepnego dnia
    wlaczy program i znowu zobaczy kreator.

    Do tego kazdy blad w tym miejscu jest cichy: zly katalog, brak uprawnien,
    data zapisana w formacie, ktorego nie da sie odczytac. Nic sie nie wywala,
    po prostu nic nie zostaje.

    ---------------------------------------------------------------------------
    CO SPRAWDZAMY

    1. zapis i odczyt w DWOCH osobnych obiektach — czyli przez plik, a nie
       przez pamiec jednego egzemplarza
    2. date: ile dni temu, liczone z tego, co zapisalismy
    3. granice powiekszenia czcionki, takze przy wartosci z pliku
    4. wartosci domyslne, gdy pliku nie ma
    5. ze folder sesji, ktory zniknal, nie jest zwracany jako istniejacy
*/

#include <juce_data_structures/juce_data_structures.h>

#include "Settings.h"

#include <iostream>

static int errors = 0, checks = 0;

static void ok (bool cond, const juce::String& what)
{
    ++checks;

    if (! cond)
    {
        ++errors;
        std::cout << "  BLAD: " << what << std::endl;
    }
}

template <typename A, typename B>
static void eq (A a, B b, const juce::String& what)
{
    ++checks;

    if (! (a == b))
    {
        ++errors;
        std::cout << "  BLAD: " << what << " — mam " << a << ", chcialem " << b << std::endl;
    }
}

//==============================================================================
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    std::cout << "ustawienia-test" << std::endl;

    // Kazdy przebieg zaczyna od czystego pliku. Test, ktory zalezy od tego, co
    // zostalo po poprzednim przebiegu, klamie dokladnie raz: wtedy, gdy ktos go
    // uruchomi na nowej maszynie.
    {
        Settings s;
        s.file().getFile().deleteFile();
        s.file().reload();
    }

    // ---- 1. domyslne, gdy nic nie zapisano -------------------------------
    {
        Settings s;

        ok (! s.hardwareChecked(),        "swiezy program nie udaje, ze sprzet jest sprawdzony");
        eq (s.daysSinceCheck(), -1,       "nigdy nie sprawdzano = -1 dni");
        eq (s.fontScale(), 1.0,           "domyslna czcionka to 1,0");
        ok (! s.mirrored(),               "domyslnie bez odbicia");
        ok (s.lastScript() == juce::File(), "bez ostatniego tekstu");
        ok (s.audioState().isEmpty(),     "bez zapisanej karty: domyslna systemu");
        ok (s.sessionFolder().getFullPathName().isNotEmpty(),
                                          "folder sesji ma jakas wartosc domyslna");
    }

    // ---- 2. zapis idzie na dysk, nie do pamieci --------------------------
    //
    // Dlatego zapisuje jeden obiekt, a czyta DRUGI, zbudowany od nowa. Pierwsza
    // wersja tego testu pytala ten sam egzemplarz i przechodzila nawet wtedy,
    // gdy plik nie powstawal wcale.
    {
        Settings w;
        w.setHardwareChecked (true, "tlo -61.4 dBFS, szczyty -11.2 dBFS");
        w.setFontScale (1.3);
        w.setMirrored (true);
        w.setAudioState ("<DEVICESETUP deviceType=\"Windows Audio\" audioInputDeviceName=\"Focusrite USB\" audioDeviceInChans=\"100\"/>");
        w.flush();
    }

    {
        Settings r;

        ok (r.hardwareChecked(),  "sprawdzenie sprzetu przetrwalo");
        eq (r.hardwareSummary(), juce::String ("tlo -61.4 dBFS, szczyty -11.2 dBFS"),
                                  "liczby z kreatora przetrwaly");
        eq (r.fontScale(), 1.3,   "czcionka przetrwala");
        ok (r.mirrored(),         "odbicie przetrwalo");
        eq (r.daysSinceCheck(), 0, "sprawdzone przed chwila to zero dni");
        ok (r.audioState().contains ("Focusrite USB") && r.audioState().contains ("audioDeviceInChans=\"100\""),
                                  "karta i wybrane wejscie (3) przetrwaly");
    }

    // ---- 3. data: ile dni temu -------------------------------------------
    //
    // Liczba dni decyduje o przypomnieniu po miesiacu, wiec musi wychodzic z
    // zapisanego napisu, a nie z nadziei. Podkladamy date wprost pod klucz, bo
    // tylko tak da sie sprawdzic cokolwiek innego niz "dzisiaj".
    {
        Settings s;

        const auto then = juce::Time::getCurrentTime() - juce::RelativeTime::days (35.0);
        s.file().setValue ("hardwareCheckedAt", then.toISO8601 (true));
        s.flush();

        Settings r;
        eq (r.daysSinceCheck(), 35, "35 dni temu");

        // dokladnie na granicy przypomnienia
        const auto edge = juce::Time::getCurrentTime() - juce::RelativeTime::days (30.0);
        s.file().setValue ("hardwareCheckedAt", edge.toISO8601 (true));
        s.flush();

        Settings r2;
        ok (r2.daysSinceCheck() >= 30, "30 dni liczy sie jako miesiac");
    }

    // ---- 4. granice czcionki ----------------------------------------------
    //
    // Nie chodzi o estetyke. Czcionka 0,1 to ekran, z ktorego nie da sie czytac,
    // a 9,0 to jedno slowo na ekranie — i w obu wypadkach czlowiek nie wie, co
    // sie stalo, bo nie widzi juz podpowiedzi o klawiszach.
    {
        Settings s;

        s.setFontScale (99.0);
        eq (s.fontScale(), 2.2, "czcionka obcieta od gory");

        s.setFontScale (0.01);
        eq (s.fontScale(), 0.6, "czcionka obcieta od dolu");

        // wartosc wpisana do pliku z zewnatrz — recznie albo przez starsza
        // wersje programu — tez musi byc obcieta przy CZYTANIU
        s.file().setValue ("fontScale", 14.0);
        s.flush();

        Settings r;
        eq (r.fontScale(), 2.2, "bzdura z pliku tez obcieta");
    }

    // ---- 5. folder sesji, ktory zniknal ----------------------------------
    //
    // Pendrive, dysk zewnetrzny, folder przeniesiony w sprzatanie. Program musi
    // wtedy zaproponowac cokolwiek istniejacego, a nie sciezke, pod ktora nic
    // nie ma — bo nagrywanie do nieistniejacego folderu konczy sie bledem
    // dokladnie w chwili, w ktorej ktos nacisnal "nagrywaj".
    {
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("zs-test-folder");
        tmp.createDirectory();

        Settings s;
        s.setSessionFolder (tmp);

        Settings r;
        eq (r.sessionFolder().getFullPathName(), tmp.getFullPathName(),
            "istniejacy folder sesji wraca taki, jaki byl");

        tmp.deleteRecursively();

        Settings r2;
        ok (r2.sessionFolder() != tmp,                "zniknietego folderu nie podajemy dalej");
        ok (r2.sessionFolder().getParentDirectory().exists()
            || r2.sessionFolder().exists(),           "zastepczy folder da sie utworzyc");
    }

    // ---- 6. ostatni tekst --------------------------------------------------
    {
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                     .getChildFile ("zs-test-tekst.txt");
        f.replaceWithText ("Dwa zdania na probe.");

        Settings s;
        s.setLastScript (f);

        Settings r;
        eq (r.lastScript().getFullPathName(), f.getFullPathName(), "ostatni tekst przetrwal");
        ok (r.lastScript().existsAsFile(),  "i nadal istnieje");

        f.deleteFile();

        Settings r2;
        ok (! r2.lastScript().existsAsFile(),
            "usuniety plik przestaje byc gotowy do otwarcia jednym klikniecem");
    }

    // ---- 7. polozenie okna -------------------------------------------------
    {
        Settings s;
        s.setWindowState ("0 100 200 940 620");

        Settings r;
        eq (r.windowState(), juce::String ("0 100 200 940 620"), "polozenie okna przetrwalo");
    }

    // porzadek po sobie
    {
        Settings s;
        s.file().getFile().deleteFile();
    }

    std::cout << (errors == 0 ? "  OK " : "  BLEDY ") << checks << " sprawdzen, "
              << errors << " bledow" << std::endl;

    return errors == 0 ? 0 : 1;
}
