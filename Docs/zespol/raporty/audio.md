# Raport: inżynier dźwięku (DSP i miks), 27.11.2026

Pomiary: program pomocniczy `/tmp/au/h.cpp` woła `zs::TakeReview::run` dokładnie tak jak `ReviewComponent.h` (`podcastWish()`, styl, `previewSec=15`, potem `reuseDecision`) oraz `mix::MusicBed::render` z domyślnymi `Params`. Materiał: polska mowa espeak-ng (48 kHz), szum pokoju różowy −50 dB, „telewizor” = angielska mowa espeak −25 dB RMS pod głosem, cichy mikrofon (−22 dB), przester (×6, obcięcie 0,99), syk biały −30 dB, wersja „dynamiczna” (zdania ±5 dB, pauzy 0,6–1,8 s), 20 min. Głośność: ffmpeg ebur128 + pyloudnorm. Skrypty i pliki: `/tmp/au/`.

## 1. Ocena: 7/10
Tor jest stabilny, powtarzalny i trafia w głośność/szczyt co do 0,1 dB na każdym materiale, próbka 15 s jest wierna pełnemu miksowi; do publikacji brakuje: tło typu „telewizor” nadal rośnie względem głosu (+2,7 dB Studyjny, +4 dB Mocny), ogon podkładu ucina się bez wyciszenia, a plik mono ma −19 LUFS (−16 dopiero w stereo/z podkładem).

### Wyniki zbiorcze (sam głos, plik mono)
| materiał | styl | LUFS | dBTP | czas (122 s) |
|---|---|---|---|---|
| czysty / cichy −22 dB / przester / TV | wszystkie 3 | −19,0…−19,1 | −1,4…−1,7 | 4,0–6,6 s |
| 20 min (TV) | Studyjny | −19,1 | −1,4 | próbka 4,5 s, całość 33,4 s |
| głos + podkład (TV) | Studyjny | −16,07 (część z głosem), −16,35 całość | −1,41 | — |

## 2. Blokery wydania

**B1. Telewizor/tło na poziomie −25 dB rośnie po miksie; ekspander go nie łapie.**
Pomiar (materiał dynamiczny, długie pauzy, ≥300 ms od słowa): stosunek mowa/tło wejście 26,2 dB → Delikatny 24,1 (+2,1 dB tła), Studyjny 23,5 (+2,7), Mocny 22,2 (+4,0). Bezwzględnie tło w pauzach: −53,7 → −41,8 dBFS (Studyjny). Na materiale bez długich pauz: +0,4 / +1,2 / +2,3 dB. Dla porównania cichy szum pokoju (−50 dB) jest w długich pauzach ścinany dobrze: −3,5 / −15,0 / −13,8 dB.
Przyczyna: `Decide.h:612–613` próg ekspandera `max(speechDb − 25,5, noiseFloor + 6)` leży dokładnie na poziomie TV (−49 dBFS w tym pliku), więc ekspander jest otwarty; do tego pomiar tła źle widzi TV: „Bez odszumiania: tło jest 45 dB pod mową” (`Decide.h:593`), choć jest 25 dB pod mową (noise floor brany z najcichszych ramek, a TV ma własne przerwy). Kompresja 2,5:1 + 4:1 z wyrównaniem do celu podnosi wszystko w pauzach.
Wniosek co do #139: #139 naprawił tylko część z podkładem (normalizacja liczona z części z głosem, `MusicBed.h:150–159`; zmierzone: głos w podkładzie −16,07 LUFS, wzmocnienie głosu −0,09 dB, czyli już bez +kilku dB). Sam tor mowy wciąż podnosi TV o 2–4 dB.
Propozycja: (a) `Decide.h` ~332: tło mierzyć jako medianę/60. percentyl ramek „bez mowy” (VAD), nie minimum; przy tle 20–30 dB pod mową włączyć odszumianie (`Restore.h`) z ograniczeniem 6–8 dB; (b) `Decide.h:613` próg ekspandera = `max(speechDb − 20, pauseLevel + 3)`, dla Mocny `gateRatio` 3:1; (c) test w `Tests/`: mowa + inna mowa −25 dB, wymaganie „tło w pauzach nie rośnie względem mowy o więcej niż 1 dB” dla wszystkich 3 stylów.

**B2. Ogon podkładu: cała reszta utworu bez wyciszenia, ucięcie na końcu.**
Wynik: muzyka dłuższa od głosu o 20 s gra w całości po głosie (−27,2 LUFS w ogonie), ostatnie 50 ms ma amplitudę 0,137 (≈ −17 dBFS) — słyszalne kliknięcie/ucięcie. `MusicBed.h:124` `len = max(voice, music)`, brak fade-in/out; `Backing.h` też nie przycina. Przy 6-min utworze i 45 s głosu wyjdzie 5 min samej muzyki.
Propozycja: `MusicBed::render`: długość = głos + `tailSec` (np. 4 s, parametr w `Params`), fade-out 3 s (krzywa cos²), fade-in 20 ms na starcie; test na długość i wartość ostatniej próbki.

## 3. Ważne, ale nie blokuje

**W1. Głośność pliku mono −19 LUFS, a brief/ekran mówi „−16”.** `Decide.h:493` `studioPodcastLufs = -19.0`, zapis mono (`ReviewComponent.h:1887`, WAV i MP3 mono). To zgodne z AES TD1008 (mono −19 ≈ stereo −16 przy odtwarzaniu na 2 kanały) i z podkładem wychodzi −16,07, ale serwisy mierzące plik wprost (YouTube, część hostingów) uznają go za 3 dB cichszy od innych podcastów. Opis na ekranie: „Miks studyjny: głośność −19.0 LUFS”. Propozycja: zapisywać głos jako stereo (dual-mono), wtedy −16 LUFS w pliku; albo zostawić mono, ale w tekście na ekranie napisać „−16 LUFS w odtwarzaczu (−19 mono)”. Decyzja Adriana/PM.

**W2. Szum biały −30 dB: poprawa tylko 3,6 / 8,5 / 7,4 dB w pauzach** (29,5 → 33,1 / 38,0 / 36,9 dB). Odszumianie „do 10 dB” + ekspander −18 dB powinny dać więcej. Artefaktów „muzycznego szumu” brak (rozrzut widma w pauzie 5,6 → 5,8 dB, jak szum biały). Propozycja: sprawdzić, czy ekspander faktycznie zamyka się na takim tle (próg `speechDb − 25,5` ≈ poziom szumu po odszumianiu); jak w B1.

**W3. Przester niewykrywany przez tor miksu.** Wejście: 115 393 próbek ≥0,98; tekst decyzji nie wspomina przesteru, nie ma declippingu. Wyjście bez obcięć (crest 11,6 → 18,0 dB, bo limiter i −19 LUFS), ale płaskie szczyty zostają słyszalne. Ostrzeżenie może dawać montaż (TakeWatch) — nie sprawdzałem w UI. Propozycja: w `Decide::listen` linia „Nagranie przesterowane w N miejscach — nagraj te fragmenty jeszcze raz” (licznik serii ≥3 próbek przy |x|≥0,98).

**W4. Proporcje podkładu.** Muzyka pod głosem 14,5 LU niżej (−11 z `musicDb` + 4 dB ściszania − sumowanie). Mieści się w praktyce (−12…−20), ale razem z cichym ogonem (−27 LUFS) muzyka po głosie nie podnosi się do poziomu intro. Na espeak (pauzy <250 ms) ściszanie nigdy nie puszcza (pod mową i w pauzach −16,7 dB) — poprawne dzięki `hold = 0,25 s`.

## 4. Drobne
- Sybilanty: szczyt pasma 5–10 kHz względem średniej: wej −5,2 dB, Delikatny −4,6, Studyjny −7,2, Mocny −6,1 — de-esser działa, Delikatny lekko je podbija (barwa bez de-essera). Espeak słabo symuluje „s/sz”; potwierdzić na prawdziwym nagraniu.
- Rezonanse „ścięte wąsko” wybierane na espeak w 5 miejscach 2,5–6,9 kHz — na syntetyku podejrzanie dużo; sprawdzić na głosie Adriana, czy nie matowią.
- Cichy mikrofon (−22 dB) daje identyczny wynik jak normalny (różnica 0,0–0,1 dB) — dobrze.

## 5. Zgodność próbki i czasy
- 20 min, próbka 15 s z 695 s: −19,01 LUFS, ten sam fragment w pełnym miksie −19,04, różnica sygnałów −45,8 dB względem sygnału — praktycznie identyczne. `reuseDecision` działa.
- Czas (Linux, -O2, 1 wątek): próbka 4,5 s, całość 20 min 33,4 s (≈1,7 s/min nagrania) → godzina ≈ 100 s na Windows może być 2× dłużej.

## 6. Czego nie sprawdziłem
- Prawdziwy głos i prawdziwy telewizor (espeak ma mały zakres dynamiki, LRA 1,3–1,6 LU; na prawdziwej mowie kompresja podniesie tło jeszcze bardziej — wynik B1 to raczej dolna granica).
- Odsłuch uchem i MP3 (`zs::mp3`) — mierzyłem tylko WAV z toru.
- Ostrzeżenia z TakeWatch w montażu (podałem pustą listę zdarzeń).
