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
- `main` = wersja 0.4.0 (stan z 4 października).
- Do odtworzenia po resecie środowiska (kod 0.5.x przepadł): ekran nagrywania z przyciskami Start/Stop/Powtórz wers, kreator, wspólny wygląd (Look/StepBar/Fonts), powtarzanie wersu w prompterze.
- Prompter: kierunek to Vosk (słownik ograniczony do słów skryptu + kompensacja opóźnienia 1–1,5 s), licznik sylab jako rezerwa. Pomiar na prawdziwym głosie: błąd ≤1 słowo w 66% chwil, ≤2 słowa w 88%; to wystarcza do podświetlania wersu, nie pojedynczych słów (potrzeba ≥85%). Metoda „klas głosek" dała 0% na prawdziwym głosie i została odrzucona.
- Jeszcze nie sprawdzone: lampka na żywym SM7B, kalibracja `proximityDb`.
