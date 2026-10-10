# Zielone światło — prośba o ocenę kodu

## Co to jest

Program dla jednej osoby przy mikrofonie, która nagrywa sama siebie i nie ma
obok nikogo, kto by powiedział „to było dobre" albo „powtórz".

Trzy rzeczy, w tej kolejności:

1. **Lampka.** W trakcie mówienia świeci zielono, żółto albo czerwono. Mówi
   tylko o tym, czy *nagranie* się nadaje — nie o tym, czy ładnie przeczytałeś.
2. **Prompter.** Tekst jedzie sam, bo program liczy sylaby na żywo i wie, gdzie
   jesteś. Nic się nie naciska.
3. **Ekran po nagraniu.** Program słucha tego, co powstało, dobiera cały miks
   sam, miksuje i mówi jednym zdaniem, czy to się nadaje — a jeśli nie, to
   **który fragment trzeba nagrać jeszcze raz**.

Docelowy użytkownik: podcaster albo poeta, który **nie ma DAW-a i nie ma żadnej
płatnej wtyczki**. Ma mikrofon, interfejs i ten program. To nie jest wygodne
założenie — to jest powód, dla którego program w ogóle istnieje.

## Ograniczenie, które definiuje wszystko

Cały DSP jest **napisany od zera**, w nagłówkach C++17, bez bibliotek. Jedyna
zależność to JUCE (okno, dźwięk, pliki). Nie ma Eigen, nie ma FFTW, nie ma
żadnego SDK wtyczek. FFT, filtry biquad, pomiar LUFS według BS.1770-4, limiter
z nadpróbkowaniem, kompresor, brama, de-esser, odszumianie widmowe, dopasowanie
barwy — wszystko w `Source/DSP/`.

**To nie jest do zmiany i nie o to proszę.** Program ma się dać sprzedać komuś,
kto nie kupi niczego więcej, i ma działać jako jeden plik .exe.

## Co jest już zmierzone — tego nie trzeba kwestionować

Nie zgaduję w tych miejscach. Każde ma test, który to pilnuje:

- **Głośność (LUFS)** zgadza się z `pyloudnorm` (niezależna implementacja
  BS.1770) do **0,04 dB** na siedmiu różnych przebiegach. Test w
  `Tests/loudness-check.py`.
- **Liczenie sylab** sprawdzone na syntezatorze mowy o znanej treści przy
  trzech prędkościach: 107 prawdziwych sylab → wykryte 105, 112, 123
  (błąd 2%, 5%, 15%).
- **Sufit szczytu (dBTP)** jest dotrzymywany przez *pomiar po limitowaniu i
  powtórzenie*, nie przez stały zapas. Stały zapas 0,5 dB okazał się za mały —
  zmierzone 1,9 dB przekroczenia, czyli plik deklarowany jako −1 dBTP wychodził
  na +0,4 dBTP.
- **De-esser** był przepisany po tym, jak pomiar pokazał, że odejmowanie pasm
  (`high = x - low`) daje −11,4 dB na 5 kHz i −3,0 dB na 2,5 kHz przy
  deklarowanym maksimum −6 dB — przez obrót fazy na zboczu filtru. Teraz to jest
  dynamiczna półka.
- Cały zestaw: **216 sprawdzeń, 0 błędów** (`Tests/wszystko.sh`).

## Czego proszę NIE radzić

Nie dlatego, że to złe rady — dlatego, że na nie nie ma miejsca w tym projekcie
i odpowiedź, która się na nich skupi, nie pomoże:

- „użyj biblioteki X / iZotope / RNNoise / Essentia" — patrz ograniczenie wyżej
- „napisz testy" — jest ich 216, są w pliku 5
- „dodaj obsługę błędów" bez wskazania, **gdzie konkretnie** brak jej szkodzi
- „przepisz w Rust / rozbij na .cpp i .h / dodaj wzorzec projektowy"
- ogólniki w rodzaju „kod jest dobrze udokumentowany, rozważ refaktoryzację"

Komentarze w kodzie są po polsku (bez ogonków — to ograniczenie kompilatora,
nie niedbalstwo) i tłumaczą **dlaczego**, a nie co. Często opisują błąd, który
był wcześniej w tym miejscu. Warto je czytać: połowa odpowiedzi na pytanie
„czemu to tak dziwnie zrobione" już tam stoi.

## O co naprawdę proszę — pytania od najważniejszego

### 1. Czy w DSP jest błąd, którego nie widzę? (plik 2)

Najcenniejsza rzecz, jaką możesz zrobić. Konkretnie:

- `Loudness.h` — filtr K, bramkowanie −10 LU / −70 LUFS, szczyt prawdziwy przez
  czterokrotne nadpróbkowanie wielofazowe. Czy to jest zgodne z BS.1770-4?
- `Dynamics.h::DeEsser` — dynamiczna półka, której współczynniki przeliczam
  **co 16 próbek**. Czy przy szybkiej zmianie nie słychać schodków? Jaki jest
  właściwy sposób to zrobić, skoro przeliczanie co próbkę jest za drogie?
- `TiltMatch.h` — dopasowanie barwy przez ważoną metodę najmniejszych kwadratów
  (Gauss-Newton) na czterech gładkich kształtach. Czy to zadanie jest dobrze
  uwarunkowane? Czy może się rozbiec?
- `Tame.h` — tłumienie tego, co wystaje ponad **własną wolną średnią pasma**
  (na wzór soothe). Czy konstrukcja gwarantuje to, co obiecuję, czyli że stała
  barwa głosu nigdy nie jest tłumiona?
- `Restore.h` — odszumianie widmowe. Czy odejmowanie widmowe jest tu zrobione
  tak, żeby nie dawało „dzwoniących" artefaktów?

### 2. Czy wątek dźwięku jest bezpieczny? (plik 4)

`Recorder.h` — `audioDeviceIOCallbackWithContext`. Szukam: przydzielania
pamięci, blokad, których nie wolno, wyścigów. Ten kod musi się zmieścić w
budżecie czasowym i **nigdy** nie czekać.

W pliku 4 jest też `TakeReview.h::Audition` — odtwarzacz A/B czytany z wątku
dźwięku, z `try_to_lock` i podmianą buforów tylko przy zatrzymanym graniu. Czy
to jest poprawne, czy tylko wygląda na poprawne?

### 3. Czy reguły doboru ustawień mają sens? (plik 2, `Decide.h`)

Program sam słucha nagrania i wybiera cały miks: cel głośności, zakres jazdy
suwakiem, siłę kompresji, de-esser, bramę, odszumianie. Każda decyzja ma zdanie
z liczbą, która ją podjęła.

Pytanie nie brzmi „czy to brzmi dobrze" (tego nie usłyszysz), tylko: **czy te
progi dają się obronić i czy któraś reguła nie zadziała odwrotnie** na materiale,
którego nie przewidziałem. Szukam wejścia, na którym program zrobi coś głupiego.

Dwa miejsca, gdzie już raz tak było:
- rozróżnienie „poziom odjeżdża" (wada, prostować) od „gest wykonawczy"
  (treść, zostawić) — `Decide.h::levelShape()`
- `clickRate()` — na śpiewie bierze atak nuty za trzask; dlatego usuwanie
  trzasków włącza się tylko tam, gdzie znajduje *mało*

### 4. Czy czegoś nie ma, a powinno być?

Patrząc jak podcaster bez DAW-a: co jest potrzebne do wydania odcinka, a czego
w tym programie w ogóle nie ma? (Montaż jest zaczęty —
`MontagePlanner.h` / `MontageRender.h` — ale nie ma dla niego ekranu.)

### 5. Polszczyzna

Program mówi do człowieka pełnymi zdaniami. Te zdania to jest produkt, nie
komunikaty błędów. Szukaj miejsc, gdzie brzmią sztucznie, protekcjonalnie albo
niejasno. Są w `Decide.h` (`because`), `TakeReview.h` (`buildLines`,
`verdictLine`), `MainComponent.h` (`namespace pl`), `WizardComponent.h`.

## Znane słabe miejsca — nie musisz ich szukać

Wiem o nich, piszę je, żebyś nie tracił na nie czasu:

- `Setup.h::proximityDb` jest oznaczone **PROWIZORKA** — próg nie pochodzi z
  pomiaru na prawdziwym mikrofonie, tylko z oszacowania.
- `Follower.h::showLagSyll = 0` — zero też nie pochodzi z pomiaru. Poprzednia
  wartość (1,5) pochodziła z pomiaru, ale **mierzyłem nie to, co trzeba**, i
  wyszło gorzej. Komentarz w kodzie o tym mówi wprost.
- Lampka nie była sprawdzana na żywym mikrofonie od około trzech tygodni.
  Program buduje się na Linuksie (MinGW → Windows), a pod Wine da się zobaczyć
  tylko pierwszą klatkę okna — więc wszystko, co się *rusza*, sprawdza dopiero
  człowiek u siebie.
- `DeClick` na czystej mowie syntetycznej znajduje 21 „trzasków" na minutę,
  czyli fałszywie. Dlatego domyślnie wyłącza się powyżej 10 na minutę — objaw
  obszedłem, przyczyny nie usunąłem.

## Jak czytać te pliki

| plik | co w nim jest |
|---|---|
| `1-pomiar-i-ocena-nagrania.txt` | co program słyszy na żywo: wykrywanie zdarzeń, lampka, kreator sprzętu |
| `2-miks.txt` | cały DSP miksu: filtry, FFT, głośność, dynamika, barwa, dobór ustawień |
| `3-prompter.txt` | tekst → sylaby → podążanie za głosem → ekran do czytania |
| `4-program-i-interfejs.txt` | wejście dźwięku, okna, ekran po nagraniu, zapis ustawień |
| `5-testy.txt` | wszystkie testy — tu widać, co jest naprawdę sprawdzone, a co nie |

Każdy plik to sklejone źródła z nagłówkami `PLIK: <ścieżka>`. Kolejność jest
taka, żeby dało się czytać od początku: rzeczy podstawowe idą przed tymi, które
z nich korzystają.

**Jeśli masz przeczytać tylko jeden: plik 2.** Tam jest najwięcej matematyki i
najwięcej do zepsucia.
