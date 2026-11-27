# Raport: integracja i droga do wydania (27.11.2026)

## 1. Ocena: 6/10
Instalator da się zbudować w całości (w CI i, po doinstalowaniu narzędzi, w tym kontenerze), ale wydanie 1.0 blokuje licencja JUCE, numer wersji i brak podpisu kodu.

**Co sprawdziłem i jak:**
- Zainstalowałem w kontenerze `mingw-w64`, `g++-mingw-w64-x86-64-posix` i `nsis` (apt, Ubuntu noble, gcc 13.2).
- Zbudowałem Windows do `/tmp/zs-win` (`mingw-toolchain.cmake`, JUCE 7.0.12): `Zielone swiatlo.exe` ma 16 485 705 B, kompilacja trwała ok. 4 min.
- `Tools/instalator.sh` złożył instalator `/tmp/zs-win/instalator.exe` o rozmiarze 55 MB.
- Pobrane pliki: `vosk-win64-0.3.45.zip` ma 14,9 MB, `vosk-model-small-pl-0.22.zip` ma 53,0 MB.
- Importy .exe to tylko systemowe DLL (KERNEL32, USER32, ole32, msvcrt, dxgi, WINMM…), bez DLL MinGW. Statyczna konsolidacja działa.
- CI `Instalator` (gh run list): 3 ostatnie przebiegi na `main` (#140, #141, #142) mają stan `success`, każdy trwał ok. 4 min 20 s. Artefakt „instalator” jest trzymany 30 dni.
- Kompilacja Linux z `-Wall -Wextra` (`/tmp/zs-wall`): w `Source/` są tylko 2 ostrzeżenia (szczegóły w p. 3). Kompilacja MinGW: 0 ostrzeżeń z `Source/`.

## 2. Blokery wydania

**B1. Licencja JUCE 7 a wyłączony ekran „Made with JUCE”**
- Stan:
  - `CMakeLists.txt:40-41` ustawia `JUCE_DISPLAY_SPLASH_SCREEN=0` i `JUCE_REPORT_APP_USAGE=0`.
  - Licencja JUCE 7 Personal (darmowa) wymaga tego ekranu, a jego wyłączenie wymaga licencji Indie/Pro albo wydania programu na GPLv3 (`JUCE-7.0.12/LICENSE.md`).
  - Repo nie ma pliku LICENSE. W `LICENCJE.txt` (instalator.sh) jest tylko zdanie „JUCE / GPLv3”.
- Ryzyko: wydanie łamie licencję JUCE.
- Krok: decyzja Adriana, jedna z trzech dróg:
  - (a) GPLv3: dodać `LICENSE` (GPLv3) do repo, publiczny kod źródłowy i link do niego w LICENCJE.txt i w „O programie”;
  - (b) ekran JUCE: przywrócić `JUCE_DISPLAY_SPLASH_SCREEN=1` (Personal, przychód < 50 tys. USD);
  - (c) kupić licencję Indie.
- Najprościej (a), bo kod i tak jest na GitHubie. Trzeba tylko sprawdzić, czy repo jest publiczne.

**B2. Wersja niespójna i nie „1.0”**
- Stan: `CMakeLists.txt:2` ma `VERSION 0.4.0`, a `Source/Main.cpp:13` ma zapisane na sztywno `"0.4.0-test"`.
- Ryzyko: instalator 1.0 pokaże w programie „0.4.0-test”.
- Krok: w Main.cpp zwracać `ProjectInfo::versionString` (JUCE generuje to z CMake). Wtedy wersja jest w jednym miejscu. Przy scaleniu podbić do 1.0.0 i dodać tag `v1.0.0`.

**B3. Brak podpisu kodu**
- Stan: ani .exe, ani instalator nie są podpisane. W workflow nie ma kroku `signtool`/`osslsigncode`.
- Ryzyko: SmartScreen pokaże „System Windows ochronił komputer”. Dla nietechnicznego słuchacza to koniec instalacji. Do tego niepodpisany NSIS bywa oznaczany przez antywirusy.
- Krok:
  - minimum 1.0: w instrukcji pobrania opisać „Więcej informacji → Uruchom mimo to”;
  - docelowo: certyfikat (Azure Trusted Signing ok. 10 USD/mies. albo OV) i `osslsigncode` w `instalator.yml`, podpisujący .exe, Odinstaluj.exe i instalator.

**B4. Wydanie nie jest „wydaniem”**
- Stan: CI tworzy tylko artefakt Actions, który znika po 30 dniach i wymaga konta GitHub. Nie ma GitHub Release ani tagu.
- Ryzyko: Adrian i słuchacze nie mają stałego linku do pobrania.
- Krok: dopisać do `instalator.yml` zadanie na `push: tags: v*`, które wrzuca instalator do Release (`softprops/action-gh-release` albo `gh release create`).

**B5. Pobieranie Vosk bez sprawdzania sumy**
- Stan: `Tools/instalator.sh:20-31` pobiera ZIP-y z GitHub/alphacephei i nie sprawdza SHA-256. Cache CI ma stały klucz.
- Ryzyko: podmieniona albo uszkodzona paczka trafi do podpisanego instalatora.
- Krok: wpisać sumy SHA-256 obu plików do skryptu i sprawdzać je przez `sha256sum -c`.

## 3. Ważne, ale nie blokuje

**Vosk**
- Licencje: `libvosk.dll` 0.3.45 ma Apache 2.0, model `small-pl-0.22` też Apache 2.0.
- Razem z biblioteką idą 3 DLL MinGW: libgcc_s_seh, libstdc++, winpthread (GPL z wyjątkiem runtime, OK).
- Program szuka biblioteki i modelu obok .exe (`VoskEngine.h:143`).
- Rozmiar: ZIP-y mają 15 + 53 MB, cały instalator 55 MB (LZMA).
- Krok: do LICENCJE.txt dołączyć pełny tekst Apache 2.0 (licencja wymaga kopii, samo zdanie nie wystarcza).

**Fonty**
- Inter i JetBrains Mono są na licencji OFL. Pliki `Resources/Fonts/OFL*.txt` są w repo, ale nie trafiają do instalatora.
- LICENCJE.txt nie wymienia fontów, a OFL wymaga dołączenia licencji przy dystrybucji.
- Krok: dodać oba pliki OFL do ZRODLO i do LICENCJE.txt.

**MP3**
- Nie ma LAME. `Source/Mp3.cpp` koduje przez Windows Media Foundation, a do odczytu podkładu służy `JUCE_USE_MP3AUDIOFORMAT`.
- Patenty MP3 wygasły w 2017 r. Licencyjnie czysto.
- Ryzyko: na Windows N (bez Media Feature Pack) MP3 się nie zapisze. Trzeba sprawdzić, czy komunikat jest czytelny po polsku.

**Wielkie pliki** (w liniach, wszystkie z CRLF w każdej linii):
- `MainComponent.h`: 2594;
- `ReviewComponent.h`: 2509;
- `PrompterComponent.h`: 2046;
- razem ok. 54% z 13 423 linii w `Source/*.h,*.cpp` (bez DSP).

Ryzyko: konflikty w PR-ach i trudne przeglądy. Krok: po 1.0 wydzielić z ReviewComponent miks/tło (`runJob`, podkład) i montaż do osobnych plików. Przed 1.0 nie ruszać.

**Wątki**
- ReviewComponent ma naraz 3 mechanizmy:
  - `juce::Thread` miksu (`:130`);
  - `std::thread job` w `runJob` (`:1599-1612`), sprzątany przez `join` w `finishJob`/`abandonJob`;
  - dodatkowy `std::thread raw` w `renderBed` (`:2368`).
- Zabezpieczenie: `isThreadRunning() || job != nullptr` plus zasłona `jobShield`. Wygląda poprawnie, ale opiera się na dyscyplinie.
- `MainComponent.h:1710`: pętla `while(!done) sleep(10)` na wątku okna, czekająca na wczytanie podkładu. Przy długim MP3 zamraża okno. Jest kandydatem na niepowtórzone „zawieszenie po Nagraj nowe podejście”, do sprawdzenia przez test.
- `Recorder.h`: sesja przez `sessionSlot` i mutex tylko na wątku sterowania, callback na atomikach. Struktura poprawna.

**Ustawienia, nagrania i deinstalacja**
- Ustawienia: `juce::PropertiesFile` w `%APPDATA%\Zielone swiatlo` (`Settings.h:46-50`).
- Ślad: `%APPDATA%\Zielone swiatlo\slad.txt` (`Trace.h:20`).
- Nagrania: `Muzyka\Zielone swiatlo` (`Settings.h:225`).
- Instalacja: per użytkownik w `%LOCALAPPDATA%\Programs`, bez UAC. Deinstalator usuwa tylko swoje pliki oraz klucze HKCU (`zielone-swiatlo.nsi`), a ustawienia i nagrania zostają.
- Krok: ślad `slad.txt` nie ma limitu rozmiaru. Sprawdzić, czy się przycina.

**Aktualizacje**
- Nie ma mechanizmu aktualizacji. Instalator nadpisuje starą wersję w miejscu (okno „Zamknij program i kliknij Ponów”).
- Dla 1.0 wystarczy. Krok: w „O programie” dodać link do strony Releases.

**Ikony**
- Ikona `Resources/ikona.png` → .exe (ICON_BIG), `ikona.ico` → instalator i deinstalator. OK.

**README**
- `README.md` jest nieaktualny (AGENTS.md o tym mówi). Przed 1.0 trzeba krótkie README / „Jak zainstalować” po polsku.

## 4. Drobne
- Ostrzeżenia z `-Wall -Wextra`:
  - `ReviewComponent.h:1232` (`-Wmisleading-indentation`): `if (step) step(...); return true;` w jednej linii. Działa dobrze, ale lepiej rozbić na 2 linie;
  - `LampLogic.h:82`: nieużywany parametr `kind`.
- `CutReviewPanel` nie jest martwym kodem: po #137 nadal pokazuje decyzje po R w montażu (`ReviewComponent.h:421-424, 958, 1270`) i ma test `Tests/ciecia-panel-test.cpp`.
- CRLF jest zgodny z AGENTS.md: 7 plików w całości CRLF (MainComponent, PrompterComponent, ReviewComponent, CutReviewPanel, WizardComponent, Main.cpp, CMakeLists.txt). Nie ma plików mieszanych. Brakuje `.gitattributes`, który by to pilnował.
- Komentarz w `mingw-toolchain.cmake` mówi o „RackRunner”, `-DRACK_JUCE_PATH` i „plug-in”. To pozostałość, powinno być `ZS_JUCE_PATH`.
- `CMakeLists.txt`: cele testowe (ZsSymulacja, ZsDlugo…) budują się w każdym `cmake --build` bez `--target`. Warto dodać opcję `ZS_TESTY`.
- Spójność po ChatGPT (#141/#142): obie kompilacje są czyste, a CI dla #142 miało `success`. Logiki podmiany nie przeglądałem w tej rundzie (to zadanie innej roli).

## 5. Czego nie sprawdziłem
- Instalatora i .exe nie uruchomiłem na Windows ani pod Wine, bo w kontenerze nie ma Wine. Nie wiem, jak się zachowują SmartScreen i Media Foundation MP3.
- `Tests/ci.sh` nie puszczałem ponownie. Według briefu przechodzi, a CI na `main` jest zielone.
- Nie wiem, czy repo jest publiczne, a od tego zależy droga GPL w B1.

## Jak powtórzyć build Windows w kontenerze
    apt-get install -y g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix mingw-w64 nsis
    cmake -S . -B /tmp/zs-win -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake -DZS_JUCE_PATH=/home/user/JUCE-7.0.12
    cmake --build /tmp/zs-win --target ZieloneSwiatlo -j8
    ZS_CACHE=/tmp/zs-cache bash Tools/instalator.sh /tmp/zs-win/ZieloneSwiatlo_artefacts/Release /tmp/zs-win/instalator.exe
