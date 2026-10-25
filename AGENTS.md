# Zasady pracy w tym repo (dla ChatGPT/Codex, Claude i człowieka)

Ten plik czytają wszyscy, którzy tu pracują. `CLAUDE.md` odsyła do niego.

## Co to jest
Zielone światło: Windows, C++17 + JUCE 7. Lampka nagrania, prompter, miks z głosu. Opis i założenia: `0-PRZECZYTAJ-NAJPIERW.md`.

## Twarde ograniczenia (nie do negocjacji)
- Całe DSP jest napisane od zera w nagłówkach w `Source/DSP/`. Żadnych zewnętrznych bibliotek DSP, żadnych SDK wtyczek. Jedyna zależność to JUCE. Wyjątek do omówienia: Vosk (rozpoznawanie mowy w prompterze), tylko jako osobna DLL obok .exe.
- Program ma być jednym plikiem .exe, bez płatnych wtyczek.
- Interfejs i komunikaty po polsku.
- Każda zmiana w DSP ma test w `Tests/`. Przed wypchnięciem: `cd Tests && ./wszystko.sh` (bez mikrofonu i karty dźwiękowej).
- GitHub Actions puszcza przy każdym PR `Tests/ci.sh`: te same testy, każdy osobno, z tabelą. Testy, które nie przechodzą na `main` od dawna, są w `Tests/znane-bledy.txt` — PR jest oblany, gdy psuje się coś spoza tej listy albo gdy test z listy zaczyna przechodzić (wtedy usuń go z listy).
- Obok, w zadaniu `testy-juce`, idą testy potrzebujące JUCE (ustawienia, punch na plikach, panel cięć, Recorder): `bash Tests/juce-testy.sh <folder budowania>`. Nowy test JUCE dopisz tam i do listy celów w `.github/workflows/testy.yml`.
- Liczby w opisach zmian mają pochodzić z pomiaru (test, nagranie), nie z przypuszczenia.

## Jak nie wchodzić sobie w drogę
- Nikt nie pcha prosto do `main`. Każdy pracuje na własnej gałęzi: `claude/<temat>`, `codex/<temat>`, `adrian/<temat>`, i otwiera pull request.
- Jedna gałąź = jeden temat. Małe commity, opis po polsku: co i dlaczego.
- Przed rozpoczęciem: `git pull` na `main`. Nie zmieniaj tego samego pliku, który ma otwarty PR kogoś innego, bez uzgodnienia w PR.
- Nie commituj: `.exe`, `.dll`, `.wav`, katalogów `build*`, modeli Vosk. Są w `.gitignore`.
- Wersja jest w `CMakeLists.txt` i `Source/Main.cpp`. Podbija ją tylko ten, kto scala PR.

## Zadania dla Gemini przez GitHub
Zgłoszenie (issue) z etykietą `gemini`, nadaną przez właściciela repo, uruchamia `.github/workflows/gemini.yml`: Gemini robi zadanie na maszynie GitHuba, a automat robi commit na gałęzi `gemini/zgloszenie-<nr>`, puszcza `Tests/ci.sh` i otwiera PR z wynikiem. Wymaga sekretu `GEMINI_API_KEY`. Treść zgłoszenia ma być samowystarczalna (przykłady: `Tools/ZADANIA.md`).

## Budowanie
    cmake -B build -DZS_JUCE_PATH=/sciezka/do/JUCE-7.0.12
    cmake --build build
Cross-kompilacja Linux→Windows: `mingw-toolchain.cmake`.

## Stan i plan
- `main` jest aktualny (październik 2026): ekran startowy, przygotowanie z kreatorem sprzętu, nagranie z prompterem (Vosk), pauzą, R i cięciem, odsłuch i montaż, miks z trzema brzmieniami i torem modułów, zapis WAV i MP3, instalator (NSIS), podkład muzyczny (krok 1: muzyka w słuchawkach). Plik `README.md` jest nieaktualny.
- Wygląd: specyfikacja „ciemny sprzęt z poświatą” w `Docs/projekt/ciemny-sprzet/`. Paleta w `Source/UiRules.h`, rysowanie w `Source/Look.h`, ramy ekranów w `Source/ScreenFrame.h`.
- Podkład muzyczny (wątek audio, synchronizacja: `Recorder.h`, `Backing.h`, `DSP/EditedView.h`) robi Claude. Nie zmieniaj tych plików bez uzgodnienia w PR.
- Prompter: Vosk (słownik ograniczony do słów skryptu), licznik sylab jako rezerwa, tylko do 6 s po ostatnim rozpoznanym słowie.

## Końce wierszy
Pliki z końcówkami CRLF zostają w CRLF (m.in. `Source/MainComponent.h`, `Source/PrompterComponent.h`, `Source/ReviewComponent.h`, `Source/Main.cpp`, `CMakeLists.txt`). Nie zmieniaj końcówek całego pliku przy małej zmianie.

## Podgląd ekranów bez mikrofonu
`"Zielone swiatlo" --preview-ui plik.png <tryb> [nagranie.wav]`. Tryby: `witaj`, `settings`, `wizard`, `prompter`, `prompter-rec`, `prompter-znaczki`, `prompter-wskazanie`, `review`, `review-ciecia`, `edit`, `tor`, `done`; dopisek `-small` daje okno 960×640. Zmiany wyglądu pokazuj w PR zrzutami w obu rozmiarach.
