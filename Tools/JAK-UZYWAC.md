# zs-mix — miks mowy bez DAW-a i bez komercyjnych wtyczek

Jeden plik `.exe`, zero instalacji, zero zależności. Wrzuć obok swoich nagrań
i uruchom z wiersza poleceń.

    zs-mix.exe wokal.wav wokal-po-miksie.wav

Tyle. Program sam zmierzy nagranie, sam dobierze próg bramy, sam wyrówna
głośność do normy podcastowej i powie, co zrobił.

---

## Co jest w środku — wszystko nasze

    usuwanie mlasków                 pojedyncze trzaski, naprawiane interpolacją
    odszumianie widmowe              szum POD mową, którego brama nie ruszy
    filtr górnoprzepustowy 80 Hz     stukanie w biurko, kroki, wiatr z ust
    wycięcie przydźwięku             50 Hz i dwie harmoniczne, tylko gdy wykryty
    tłumik plozyw                    grzmot z „p" i „b", dynamicznie
    brama                            tło pokoju między zdaniami
    jazda głośnością                 zdanie wobec zdania, w sekundach
    wyrównywacz 2:1                  zdanie ma brzmieć tak samo głośno jak poprzednie
    de-esser                         sybilanty
    barwa                            -2 dB na 250 Hz, +1,5 dB na 3 kHz, +1 dB powietrza
    nasycenie                        ciepło, z nadpróbkowaniem
    dociskacz 4:1                    pojedyncze szczyty
    wyrównanie do -16 LUFS           według ITU-R BS.1770-4
    limiter -1 dBTP                  ze szczytem międzypróbkowym

Żadna z tych rzeczy nie jest cudzą wtyczką. To jest warunek wersji na sprzedaż:
podcaster nie ma ani DAW-a, ani Wavesa, ani iZotope.

---

## Opcje

    --lufs <x>       cel głośności, domyślnie -16
                     -14 dla YouTube'a, -19 dla radia w Europie
    --sufit <x>      sufit szczytu międzypróbkowego, domyślnie -1 dBTP
    --bez-bramy      nie tłum tła między zdaniami
    --bez-deessera   nie tłum sybilantów
    --hum <50|60|0>  wymuś częstotliwość przydźwięku (0 = wyłącz wycinanie)
    --sucho          bez barwy i bez kompresji: tylko filtr, brama i głośność
    --bez-szumu      nie odszumiaj widmowo
    --bez-trzaskow   nie usuwaj mlasków
    --bez-ciepla     bez nasycenia
    --szum <dB>      siła odszumiania, domyślnie 14
    --bity <16|24>   rozdzielczość zapisu; 16 włącza dither
    --raport <plik>  zapisz raport do pliku

`--sucho` przydaje się do porównania: puść ten sam plik raz sucho, raz normalnie,
i posłuchaj, co robi sama kompresja z barwą.

---

## Co znaczą liczby w raporcie

    tlo pokoju -47.8 dBFS, mowa -19.6 dBFS — roznica 28 dB

Różnica poniżej 30 dB znaczy, że w pokoju jest za głośno. Brama przyciszy tło
**między** zdaniami, ale pod mową ono zostanie — tego już nie da się usunąć bez
szkody dla głosu. To jest liczba, którą warto poprawić przed nagraniem, nie po.

    co bylo  -23.2 LUFS   szczyt  0.6 dBTP

Szczyt powyżej zera znaczy przester: część próbek została ścięta przy nagraniu
i żadna wtyczka tego nie cofnie. Celuj w -12 dBFS na szczytach przy nagrywaniu.

    brama -10.9 dB    wyrownywacz -3.4 dB    de-esser -12.0 dB

Ile każdy stopień musiał ująć. Wyrównywacz powyżej 12 dB znaczy, że różnice
głośności między zdaniami były duże — zwykle to kwestia zmiennej odległości od
mikrofonu.

---

## Skąd wiadomo, że to działa

Nie z tego, że „brzmi dobrze". Zestaw testów w `Tests/` liczy:

- **głośność** — nasz miernik BS.1770 porównany z niezależną implementacją
  (pyloudnorm) na siedmiu przebiegach: zgodność do **0,04 LU**;
- **kompresor** — krzywa statyczna 3:1 w siedmiu punktach, zgodność do 0,5 dB,
  oraz zmierzone czasy ataku i zwolnienia;
- **brama** — tło stłumione o 15 dB, a początek frazy traci **0,35 dB**
  (to zasługa wyprzedzenia — bez niego ginęły pierwsze milisekundy spółgłosek);
- **de-esser** — sybilant w dół o 13 dB, 1 kHz nietknięte w granicach 0,3 dB;
- **limiter** — nic nie wychodzi ponad -1 dBTP, a materiał między szczytami
  przechodzi bez zmiany.

Uruchomienie całości:

    Tests/wszystko.sh

---

## Czego jeszcze nie ma

- **stereo** — wszystko schodzi do mono, bo mowa jest mono.
- **duckowanie podkładu** — moduł jest napisany i przetestowany, ale nie ma go
  jeszcze w narzędziu: potrzebuje drugiego pliku wejściowego.
- **przepróbkowanie w locie** — moduł jest, w narzędziu jeszcze nie.
- **najważniejsze: nikt tego nie słyszał.** Wszystkie 57 pomiarów jest
  liczbowych. Nie mówią, czy kompresor oddycha, czy de-esser nie sepleni i czy
  barwa nie brzmi jak telefon. To sprawdzisz tylko uchem.
