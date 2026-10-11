# Wygląd 3.5 — Gotowe

Po udanym `save()` ekran przechodzi do kroku 5: znak ✓, „Nagranie gotowe”,
krótka informacja o zapisanym WAV i główny przycisk „Pokaż w folderze”.
Przycisk wskazuje zapisany miks. Pod nim jest pełna ścieżka, którą można
zaznaczyć i skopiować; długie ścieżki zawijają się i przewijają.

Panel „Plik końcowy” pokazuje nazwę, „WAV 24-bit”, długość faktycznie
wyeksportowanego fragmentu, LUFS i szczyt dBTP z `whatIDid()`. Nazwa pliku
jest w przewijanym polu. Pod formatem pozostawiono miejsce na drugi wiersz;
wyboru formatu ani MP3 nie dodano.

„Wróć do montażu” przywraca dotychczasowe uchwyty, suwaki i kontrolki cięcia.
„Nagraj nowe podejście” wywołuje istniejący powrót do nagrywania. Esc na
Gotowe wraca do montażu. Po otwarciu innego nagrania ekran zaczyna od Odsłuchu.

Oba zapisy — z Odsłuchu i Montażu — przechodzą przez dotychczasowe `save()`.
Przycięty fragment czeka na ponowne obliczenie miksu. Gotowe pojawia się
dopiero po sukcesie `wavio::write24`; błąd zapisu lub odmowa publikacji
pozostawiają komunikat na poprzednim ekranie. Oryginał nie jest nadpisywany,
a kolejne miksy zachowują nazwy `__miks`, `__miks-2` itd.

## Zrzuty

Natywna aplikacja JUCE na Linuksie pod Xvfb, `/tmp/mowa.wav` z `Tests/ci.sh`:

- [done.png](done.png): 1280 × 800.
- [done-small.png](done-small.png): 960 × 640.

Polecenia: `--preview-ui done.png done /tmp/mowa.wav` i
`--preview-ui done-small.png done-small /tmp/mowa.wav`. Podgląd wykonuje
rzeczywisty zapis przez `save()`; jeśli miks już istnieje, dostaje kolejny numer.

## Weryfikacja

- Natywny build Release: poprawny.
- `bash Tests/ci.sh`: kod 0; [tabela](testy.md). `wyglad-test`: 33/33,
  `odsluch-test`: 50/50, `wejscie-test`: 18/18. Znane błędy restore/montage
  pozostają jak na main.
- [Test komponentu JUCE](ui-test.txt): 21 sprawdzeń, 0 błędów. Objął zapis
  z obu ekranów, przycięty eksport, format 24-bit, nazwy i ścieżki, krok 5,
  powrót do montażu i nagrywania, brak nadpisania miksu i oryginału,
  odmowę publikacji, rzeczywisty błąd otwarcia pliku do zapisu, długą ścieżkę
  i reset stanu po wczytaniu innego nagrania. Kontrolki mieszczą się w obu
  wymaganych rozmiarach.
- `bash Tests/wszystko.sh` zatrzymuje się na dwóch znanych błędach
  `restore-test`. Skrypt CI uruchamia wszystkie testy osobno.
- Zachowano CRLF, `setRoom()`, `wish`, oba `review.run(wish)`, komunikat
  miksowania nad kontrolkami, `folderButton`, `mouseDrag()` i `dragRange()`.
  Bez zmian w DSP, `VoskEngine.h`, `Recorder.h` i wersji programu.

Nie sprawdzono Windowsa, fizycznego sprzętu audio ani otwarcia Eksploratora
przez „Pokaż w folderze”. Test sprawdził dostępność przycisku i istnienie
zapisanego pliku; callback korzysta z istniejącego `revealToUser()`.
