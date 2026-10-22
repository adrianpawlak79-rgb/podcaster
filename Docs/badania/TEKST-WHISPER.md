# Zapis nagrania wobec tekstu promptera (Whisper) — pierwszy pomiar

22.10.2026. Punkt 4 z planu rozwoju: montaż według tekstu. Zanim cokolwiek
trafi do programu — czy to w ogóle działa po polsku, na naszym nagraniu, w
rozsądnym czasie.

## Nagranie

Surowe nagranie Adriana z 12.10 (SM7B, 1:50): „Nic dwa razy” Szymborskiej
czytane z promptera, z powtórzeniami po R i komentarzami między
podejściami („no i tutaj, jak jest w tym miejscu, to jest…”, „tutaj też
ucięło”). Czyli dokładnie to, co program ma rozpoznać.

## whisper.cpp — który model

whisper.cpp (licencja MIT, modele Whisper — MIT): darmowy, także w programie
na sprzedaż, działa bez internetu. Pomiar w kontenerze, 4 rdzenie, bez GPU.

| model | rozmiar | czas na 1:50 | na godzinę | uwagi |
|---|---|---|---|---|
| base | 148 MB | 17 s | ok. 9 min | dużo błędów w słowach („pąbrzemy”, „dzięć się nie po chtórzy”) |
| **small** | 488 MB | 44 s | ok. 24 min | dobry; pojedyncze rzadkie słowa („najtemn przymi” = najtępszymi) |
| large-v3-turbo (q5_0) | 574 MB | 109 s | ok. 60 min | najlepszy w czytanym tekście, ale **zgubił komentarze** i powtórzył ostatnie zdanie |

**Wybór: small.** Duży model „wygładza” wypowiedź — pominął dwa komentarze
Adriana. Dla nas to najgorszy możliwy błąd: właśnie to, co odbiega od
tekstu, ma zostać pokazane. Small zapisał wszystko.

Czasy słów: `-ml 1 -sow` daje każde słowo z początkiem i końcem (np. „Nic”
0,00–0,32 s). Są przybliżone; do cięcia granice i tak dociągamy do ciszy
(`CutFinder`, jak przy R).

## Dopasowanie do tekstu (`Source/DSP/ScriptAlign.h`)

Viterbi po pozycji w tekście z powrotami wstecz (nowe podejście), słowami
spoza tekstu, pominięciami i sklejaniem dwóch słów zapisu w jedno słowo
tekstu. Dla każdego słowa tekstu liczy się ostatnie podejście; wcześniejsze
to powtórzenia.

Wynik `zs-tekst` na tym nagraniu (model small, tekst: cały wiersz):

```
0:07.1–0:08.0  PRZEJĘZYCZENIE  „pombrzemy” zamiast „pomrzemy”
0:11.1–0:12.3  PRZEJĘZYCZENIE  „najtemn przymi” zamiast „najtępszymi”
0:22.9–0:26.2  POWTÓRZENIE     nie ma dwóch podobnych nocy,
0:33.7–0:35.5  POZA TEKSTEM    No i tutaj jak jest
0:33.7         POMINIĘTE       dwóch jednakich spojrzeń
0:35.7–0:38.8  POZA TEKSTEM    tym miejscu, to jest dupa.
0:38.8–0:55.8  POWTÓRZENIE     Wczoraj kiedy twoje imię, … tutaj też ucieło, … Wczoraj kiedy twoje imię, ktoś,
0:55.8–1:01.4  POZA TEKSTEM    znaczy jak na pierwszym jest na przykład w razie,
…
1:07.7–1:13.3  POWTÓRZENIE     Wczoraj kiedy twoje imię, ktoś wymówił przy mnie głośno, …
1:20.2         POMINIĘTE       Dziś, kiedy jesteśmy razem, … Jesteś a więc musisz minąć.
1:40.8–1:47.0  POZA TEKSTEM    No i tu za szybko kończy jakby ten,
```

Co trafione:
- **wszystkie komentarze** Adriana wyszły jako POZA TEKSTEM albo w środku
  POWTÓRZENIA (do wycięcia tak czy inaczej);
- **powtórzone wersy** („Wczoraj, kiedy…” czytane cztery razy, „Uśmiechnięci…”
  dwa) — wcześniejsze podejścia jako POWTÓRZENIE, zostaje ostatnie;
- **pominięte strofy** — Adrian czytał wybór, nie cały wiersz; program to
  widzi.

Co do poprawy:
- **przejęzyczenia to w większości błędy rozpoznawania**, nie czytania
  („pombrzemy”, „kropla”). W programie: „sprawdź to miejsce”, nie „pomyłka”;
  tylko z odsłuchem jednym kliknięciem.
- krótkie słowa („w”, „to jest”) z komentarza czasem przyklejają się do
  tekstu — dzielą jeden komentarz na dwa.
- gdy ostatnie podejście urwało się w połowie (koniec nagrania testowego),
  zostaje ono, a pełne wcześniejsze idzie do wycięcia — na końcu nagrania
  trzeba pytać, nie zakładać.

## Co dalej

1. Więcej nagrań z tekstem (TXT), w tym bez komentarzy — policzyć trafność:
   ile prawdziwych powtórzeń i komentarzy łapie, ile fałszywych zgłoszeń.
2. Czas: godzina = ok. 24 min modelu small tutaj. W programie liczyć w tle
   **w trakcie nagrywania**, kawałek po każdej pauzie — po „Zakończ” zapis
   jest prawie gotowy.
3. Model 488 MB: w instalatorze albo pobierany przy pierwszym użyciu.
4. Ekran montażu: lista z punktu wyżej jako propozycje cięć (jak po R), z
   odsłuchem i „wytnij / zostaw”.

Narzędzia: `Tools/zs-tekst.cpp` (raport), `Tests/tekst-test.cpp` (CI).
