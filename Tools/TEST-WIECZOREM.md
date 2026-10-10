# Test wieczorem — dwadzieścia minut, nie trzy godziny

Cel jest jeden: **dowiedzieć się, który moduł brzmi źle.** Nie „czy brzmi dobrze" —
na to odpowiedź brzmi „chyba tak" i nic z niej nie wynika.

---

## 1. Nagraj jeden materiał, 2–3 minuty

Mów normalnie albo powiedz wiersz. **Nie czytaj testowych zdań** — chodzi o to,
jak brzmisz naprawdę.

Ale zrób po drodze świadomie te pięć rzeczy, bo każda sprawdza inny moduł:

| w nagraniu | co sprawdza |
| --- | --- |
| raz **zaśmiej się głośno** albo podnieś głos | limiter i ostrzeżenie o przesterze |
| jedno zdanie **bardzo cicho**, prawie szeptem | bramę i jazdę głośnością |
| powiedz **„Piotr popija pyszną kawę"** blisko mikrofonu | tłumik plozyw |
| powiedz **„sześćset sześćdziesiąt sześć szeleszczących szeptów"** | de-esser |
| na koniec **20 sekund ciszy** — nie ruszaj się, nie wyłączaj | odszumianie |

Te 20 sekund ciszy na końcu są ważniejsze, niż wyglądają: stamtąd program
uczy się, co jest szumem, a ja z tego zobaczę, ile naprawdę zdjął.

**Ustaw wzmocnienie tak, żeby szczyty były koło −12 dBFS.** I upewnij się, że
Windows nie ma włączonej redukcji szumu ani automatycznego wzmocnienia na tym
wejściu — to psuje sygnał zanim go zobaczymy.

---

## 2. Puść to przez porównywarkę

    zs-porownaj.exe nagranie.wav odsluch

Otwórz `odsluch\porownanie.html` w przeglądarce. Wszystkie wersje grają
równolegle, przełączasz klawiszami **1–7** i słyszysz różnicę natychmiast,
a nie z pamięci.

**Wszystkie mają tę samą głośność, łącznie z oryginałem.** To jest jedyny powód,
dla którego to narzędzie powstało. Ucho myli głośność z jakością — wystarczy
jeden decybel, żeby głośniejsza wersja wydała się cieplejsza i lepiej
zmiksowana. Gdybym dał ci „przed i po" wprost, obrobione wygrałoby nawet
gdyby psuło dźwięk. Test, który nie może przegrać, nie jest testem.

---

## 3. Słuchaj na słuchawkach, umiarkowanie głośno

Porównuj **pełny łańcuch (2) z każdym pozostałym wariantem**, nie z oryginałem.
Porównanie z oryginałem mówi tylko tyle, że coś się wydarzyło.

Pytanie za każdym razem jest to samo: **czy pełny jest lepszy od tego, który ma
jeden stopień wyłączony?** Jeśli któryś wariant brzmi lepiej niż pełny, to
znaczy, że wyłączony w nim moduł jest do poprawki. I to jest cała odpowiedź,
której potrzebuję.

Czego słuchać konkretnie:

- **„s" i „sz"** — czy sepleni, czy brzmi jak przez ścianę → de-esser (4)
- **cisza między zdaniami** — czy szum faluje, wznosi się i opada → kompresja (6)
- **głos ogólnie** — czy jest pusty, jakby przez rurę albo z pogłosem → odszumianie (5)
- **początki słów** — czy „t", „p", „k" nie tracą pierwszych milisekund → brama (1)
- **barwa** — czy nie brzmi jak telefon albo jak radio → nasycenie (3)
- **oddech** — czy nie słychać, jak program „pracuje" → kompresja (6)

---

## 4. Co mi odeślij

1. **surowy plik** — ten sprzed obróbki;
2. **numery wariantów**, które brzmiały lepiej od pełnego (albo „żaden");
3. **jedno zdanie** o tym, co w pełnym brzmi nie tak. Nie musi być fachowe —
   „jakby był w pudełku" albo „za ostro na s" wystarczy w zupełności.

Z surowego pliku sprawdzę jeszcze dwie rzeczy, których nie usłyszysz:
czy detektor sylab liczy tak samo na prawdziwym głosie jak na syntetycznym
(dziś kalibrowałem go na espeaku i to jest jedyna nastawa w całym zestawie,
która może nie przenieść się na człowieka), i czy progi bramy dobrane z
pomiaru trafiają w twój pokój.

---

## Jeśli coś nie zadziała

Napisz mi, co dokładnie wypisała konsola. `zs-mix.exe` i `zs-porownaj.exe` nie
mają instalatora, nie zapisują nic poza tym, co im wskażesz, i nie ruszają
oryginału.
