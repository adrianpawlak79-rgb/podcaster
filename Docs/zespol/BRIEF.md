# Zielone światło: brief zespołu (27.11.2026)

## Sytuacja
Poprzedni zespół (Claude + ChatGPT/Codex, ok. 140 PR-ów) przestał nad tym pracować. Program jest w dużej części zrobiony, ale nie został wydany. Nowy zespół ma doprowadzić go do **wersji 1.0 do wydania**. Właściciel: Adrian, autor podcastu. Nie jest programistą i testuje program na Windows (mikrofon SSL 2). Pisze i czyta tylko po polsku.

## Co to jest
Program na Windows (C++17 + JUCE 7, jeden plik .exe z instalatorem NSIS) do nagrywania podcastu głosowego. Prowadzi człowieka bez wiedzy o dźwięku przez 5 kroków:
1. **Przygotowanie:** nazwa, tekst do promptera albo nagranie bez tekstu, opcjonalnie podkład muzyczny, kreator sprawdzenia sprzętu.
2. **Nagranie:**
   - prompter z rozpoznawaniem mowy (Vosk, polski) przesuwa tekst za głosem;
   - fala na żywo jak w DAW;
   - pauza; wybór wersu albo miejsca na fali, od którego nagrywa się od nowa („Nagraj od linii”: rozbieg w słuchawkach, odliczanie, punch-in);
   - R = powtórz wers.
3. **Odsłuch i montaż:**
   - wycinanie i wyciszanie fragmentów, przycięcie początku i końca, zoom;
   - podmiana fragmentu („Nagraj poprawkę”: od PR #141/#142);
   - z podkładem: odsłuch głosu z muzyką.
4. **Miks:**
   - automatyczny tor (filtr, korektor, odszumianie, oddechy, ekspander, kompresja, rezonanse, de-esser, barwa, limiter, cel −16 LUFS);
   - 3 brzmienia: Delikatny, Studyjny, Mocny;
   - dla dłuższych nagrań najpierw próbka 15 s, całość po akceptacji;
   - zaawansowane kafle modułów; z podkładem: głos z muzyką i ściszanie pod mową.
5. **Gotowe:** WAV 24-bit + MP3, „Pokaż w folderze”, „Nagraj nowe podejście”, „Nowy projekt”.

## Gdzie co jest
- Zasady repo: `AGENTS.md`.
- Ekrany:
  - `Source/StartComponent.h`: start;
  - `Source/MainComponent.h`: przejścia, przygotowanie, panel boczny;
  - `Source/PrompterComponent.h`: nagranie;
  - `Source/ReviewComponent.h`: odsłuch, montaż, miks, gotowe;
  - `Source/WizardComponent.h`: kreator sprzętu;
  - `Source/StepBar.h`: pasek kroków.
- Wygląd: `Source/UiRules.h` (paleta), `Source/Look.h`, `Source/ScreenFrame.h`, specyfikacja w `Docs/projekt/ciemny-sprzet/`.
- DSP (wszystko napisane od zera, bez bibliotek):
  - `Source/DSP/`, w tym `Mix/SpeechChain.h` (tor), `Mix/Decide.h` (dobór), `Mix/MusicBed.h` (podkład);
  - `DSP/TakeReview.h`: miks i opis;
  - `DSP/ReplaceRange.h`: podmiana fragmentu.
- Audio na żywo: `Source/Recorder.h`, `Source/Backing.h`.
- Testy: `Tests/ci.sh` (bez karty dźwiękowej). Dziś przechodzi w całości.
- Podglądy ekranów z ostatnich zmian: `Docs/Z13-podglady/*.png`.

## Narzędzia w tym kontenerze
- **Zbudowany program (Linux):** `"/home/user/build-linux/ZieloneSwiatlo_artefacts/Release/Zielone swiatlo"`.
- **Zrzut ekranu bez mikrofonu:** `Xvfb :<nr> -screen 0 1400x900x24 &`, potem `DISPLAY=:<nr> "<program>" --preview-ui <plik.png> <tryb> [nagranie.wav]`.
  - Tryby: `start`, `witaj`, `settings`, `wizard`, `prompter`, `prompter-rec`, `prompter-podklad`, `prompter-podklad-pauza`, `prompter-znaczki`, `prompter-wskazanie`, `prompter-pasek`, `recording`, `review`, `review-ciecia`, `review-big`, `edit`, `tor`, `done`. Dopisek `-small` daje okno 960×640. Pełna lista jest w `Source/Main.cpp`.
  - Nagranie testowe: `Tests/t2.wav`. **Każdy agent używa własnego numeru ekranu**, wskazanego w zadaniu.
- **Testy:** `bash Tests/ci.sh`.
- **Historia:** `git log`. PR-y #134–#142 to ostatnie zmiany. #141 i #142 (ChatGPT) nie miały lokalnych testów; dziś `ci.sh` przechodzi.

## Ostatnie zgłoszenia Adriana (październik), które warto zweryfikować
- Program startował pod innymi oknami (#135).
- Prompter cofał się do miejsca pauzy, a w odliczaniu nie było słychać nagrania (#134).
- Uchwyty przycięcia łapały się zamiast zaznaczenia (#136).
- Mylił się w liście „prób do akceptacji” w montażu (#137 usunęło listę).
- Miks godzinnego nagrania trwał ok. 3 min na każde brzmienie (#138: próbka).
- Miks z podkładem: za głośny głos i telewizor w tle (#139).
- Nie znalazł przycisku przywracania ustawień (#140).
- „Wróć do nagrywania” zaczynał od zera (#141/#142: poprawka fragmentu).
- Wcześniej: zawieszenie po „Nagraj nowe podejście” na Windows (niepowtórzone).

## Zasady dla członków zespołu
- **W tej rundzie tylko diagnoza.** Nie zmieniasz kodu, nie robisz commitów, nie pushujesz. Możesz budować do własnego katalogu w `/tmp`, uruchamiać testy, robić zrzuty i pisać małe programy pomocnicze w `/tmp`.
- **Raport** zapisujesz po polsku w `Docs/zespol/raporty/<rola>.md`. Struktura:
  1. Ocena gotowości do wydania z twojej perspektywy (0–10) i jedno zdanie dlaczego.
  2. **Blokery wydania:** bez nich 1.0 nie wychodzi. Każdy z dowodem (plik:linia, zrzut, wynik pomiaru) i propozycją rozwiązania.
  3. Ważne, ale nie blokuje.
  4. Drobne.
  5. Czego nie dało się sprawdzić i dlaczego.
- **Konkretnie i z dowodami.** Liczby tylko z pomiaru. Zrzuty zapisuj w `Docs/zespol/raporty/zrzuty/<rola>-*.png` i odwołuj się do nich.
- **Pisz krótko.** Raport ma najwyżej ok. 150 linii, lista priorytetów jest ważniejsza niż opis.
