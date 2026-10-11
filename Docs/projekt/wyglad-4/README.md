# Wygląd 3.4 — Montaż

Tryb montażu pokazuje krok 4, tytuł „Dopracuj początek i koniec”, wybrany
fragment i dwa uchwyty. Odrzucone części są przyciemnione. Uchwyty można
przeciągać; strzałki na uchwycie lub suwaku przesuwają granicę o 0,1 s.
Suwaki „Początek” i „Koniec” pokazują czasy w sekundach z częścią dziesiętną.
Granice nie mogą się minąć; pozostaje co najmniej 0,1 s (lub cały krótszy plik).

„Przejdź do zapisu →” korzysta z dotychczasowego zapisu miksu. Wybrany fragment
przycina kopię w pamięci przez `cutRange()`: najpierw usuwa końcówkę, potem
początek. Miks liczy się ponownie, a zapis czeka na wynik. Eksport nadal ma
nazwę `__miks.wav` i kolejne numery, gdy wcześniejszy miks już istnieje.
Oryginał na dysku pozostaje nietknięty. Przeliczanie blokuje kontrolki edycji.

Pod przyciskiem „Wytnij fragment ze środka” pozostają oznaczenie początku
i końca, wycinanie oraz „Cofnij cięcie”. Cofanie przywraca kopię sprzed
ostatniego cięcia, również sprzed przycięcia do eksportu. Każde zastosowane
cięcie ustawia granice wyboru na całą nową kopię. Kliknięcie przebiegu,
odtwarzanie i porównanie przed/po działają też w montażu.

Panel „Obróbka głosu” pokazuje krótkie podsumowanie rzeczywistej decyzji
z `whatIDecided()` i wyniku z `whatIDid()`: LUFS, cel i zapas szczytu,
korektę czterech pasm i pomiar barwy, odszumianie wraz z odstępem tła od mowy,
przydźwięk i jego nadwyżkę. Brak pomiaru barwy lub wysokiego pasma jest
opisany wprost. Panel przewija się; pełne uzasadnienia pozostają w Odsłuchu.

## Zrzuty

Natywna aplikacja JUCE na Linuksie pod Xvfb, `/tmp/mowa.wav` z `Tests/ci.sh`:

- [edit.png](edit.png): 1280 × 800, `--preview-ui edit.png edit /tmp/mowa.wav`.
- [edit-small.png](edit-small.png): 960 × 640, `--preview-ui edit-small.png edit-small /tmp/mowa.wav`.

## Weryfikacja

- Natywny build Release aplikacji: poprawny.
- `bash Tests/ci.sh`: kod 0; [tabela](testy.md). `wejscie-test`: 18/18,
  `odsluch-test`: 50/50. Znane błędy restore/montage pozostają jak na main.
- [Dodatkowy test komponentu JUCE](ui-test.txt): 19 sprawdzeń, 0 błędów;
  uchwyty i suwaki, kroki
  strzałek, przeciąganie, nieprzekraczanie granic, przyciemnienie, podsumowanie
  automatu, przed/po, kliknięcie przebiegu, długość wyeksportowanego fragmentu,
  porównanie oryginału bajt po bajcie, wycięcie ze środka i oba cofnięcia,
  kontrolki przy minimalnym rozmiarze, powrót do Odsłuchu.
- `bash Tests/wszystko.sh` zatrzymuje się na dwóch znanych błędach
  `restore-test`. Skrypt CI uruchamia pozostałe testy osobno.
- Bez zmian w DSP, `VoskEngine.h`, `Recorder.h` i wersji programu. Zachowano
  CRLF oraz `setRoom()`, `wish` i oba `review.run(wish)`.

Nie sprawdzono Windowsa, fizycznego mikrofonu ani odsłuchu przez sprzęt audio.
Krok 5 nadal nie ma osobnego ekranu; główny przycisk zapisuje miks i pokazuje
potwierdzenie na ekranie montażu.
