# Miks na prawdziwym głosie (12–13.10.2026)

Pierwszy odsłuch miksu programu na surowym nagraniu Adriana (SM7B → SSL2, „Nic dwa razy”,
1:50). Wszystkie liczby z tego nagrania; wersje do odsłuchu robione na tym samym pliku.

## Sprzęt i nagranie

- **SM7B + SSL2:** SM7B ma bardzo niską czułość i potrzebuje ok. 60 dB wzmocnienia, SSL2 daje
  najwyżej ok. 62 dB. Nagranie wyszło ciche: **−34,5 LUFS, szczyt −11 dBFS**. Miks musiał
  podnieść je o ok. 24 dB (do −16 LUFS), a z głosem szum przedwzmacniacza i pokój.
- Tło **41 dB** pod mową (pomiar programu), ok. 30 dB według RX (RMS mowy −33,3, szum −64,1).
- Dół 80–180 Hz **+13,4 dB** względem 1 kHz (przeciętna mowa +1,6) — efekt zbliżeniowy, dla
  SM7B normalny przy mówieniu z bliska.
- Zalecenie: wzmacniacz liniowy (Adrian ma **Klark Teknik CT1**, +25 dB) — gałka SSL2 niżej,
  mniej szumu. Ewentualnie przełącznik „bass roll-off” z tyłu SM7B.

## Co psuł stary miks (−16 LUFS, pełny dobór)

Adrian: *„studnia, mlaski, oddechy, trochę przester”*. Po wyrównaniu głośności z surowym:

| | 40–160 Hz | 2,5–8 kHz | tło | głos nad tłem |
|---|---|---|---|---|
| stary miks | **−7…−9 dB** | **−3…−4 dB** | −52 dB | 40 dB |
| surowe (ta sama głośność) | 0 | 0 | −53 dB | 38,5 dB |

- Zabierał naraz dół i wyrazistość, głos był cienki i stłumiony („studnia”).
- Pracowało za dużo stopni naraz: tłumienie głosek do −15 dB, brama −14 dB, wyrównywacz −9 dB,
  de-esser, tłumienie odchyleń, odszumianie 14 dB przy tle 41 dB pod mową.
- **Wycinanie przydźwięku 50 Hz z harmonicznymi** (wąskie filtry Q 25 na 50/100/150 Hz) dzwoni
  ok. 80 ms po każdym słowie. Ton podstawowy Adriana to ok. 92 Hz — tuż przy 100 Hz — więc
  dzwonienie trzyma otwarty ekspander: przez **100–300 ms po słowie +20 dB** w porównaniu
  z wersją bez wycinania (tło 10. percentyla −64,5 zamiast −67,8 dBFS). Same filtry na samym
  szumie działają poprawnie. W profilu studyjnym wycinanie jest wyłączone — filtr 75 Hz
  24 dB/okt zabiera 50 Hz sam.

## Odsłuchy (wersje robione ffmpeg, nie naszymi modułami — tylko kierunek)

| wersja | co | ocena Adriana |
|---|---|---|
| A | −16 LUFS, kompresja 2:1, filtr 70 Hz | — |
| **B** | jak A, −19 LUFS | „chyba najlepsze, ale lekki szum” |
| C | A + przyciszanie pauz | oddechy głośniejsze niż w B |
| **B1** | B + odszumianie 6 dB | **lepsze** |
| B2 | B + odszumianie 10 dB | **„nie do akceptu, coś złego z głosem”** |
| D | B2 + cisza w pauzach | „źle” |

Wniosek: przy tym nagraniu odszumianie 10 dB (ffmpeg `afftdn`) już psuje głos, 6 dB nie.
Szum był w surowym nagraniu (głos/tło: surowe 38,5 dB, B 42,8 dB) — nie słychać go było tylko
dlatego, że surowe gra 15 dB ciszej.

## Wzorzec: łańcuch v3 Adriana (FabFilter, iZotope)

Adrian złożył łańcuch na profesjonalnych wtyczkach; wersja v3 (plik `0026-sesja-wokal-v3`)
**„dużo lepsza”**, choć „leciutki szum jest”:

1. Pro-Q 3: low cut 75 Hz 24 dB/okt, 200 Hz −2,5 (Q 1,2), 380 Hz −3 (Q 1,4), 800 Hz −1,5 (Q 2),
   4755 Hz dynamicznie −7 dB (Q 12 — gwizd tylko w 11 % czasu), wyjście +12 dB
2. RX 11 Voice De-noise: adaptacyjnie, dialog, chirurgicznie, 14 dB
3. RX 11 Breath Control: −18 dB (nie do zera)
4. Pro-G: ekspander −42 dB (21 dB pod mową), 2:1, zakres 12 dB, 10/120/300 ms, wyprzedzenie 2 ms
5. Pro-C 2 wolny: −20 dB, 2,5:1, 12/120 ms
6. soothe2: głębokość 3, miękko, od 450 Hz (rezonanse 2443, 2654, 2930, 5959 Hz)
7. Pro-DS: 5,5–11 kHz, zakres 3 dB
8. Pro-C 2 szybki: −14 dB, 4:1, 3/90 ms
9. Pro-Q 3: 3,5 kHz +1,5, półka 9 kHz +2
10. Virtual Tape Machines (Adrian: nie pomagało)
11. Pro-L 2: −1 dBTP

Pomiar v3 wobec surowego: dół −1,5…+0,2, 320–640 Hz −2,2, 2,5–5 kHz −2,8, 8–20 kHz +3,8…+5,1,
tło −67,6 dBFS, −19,7 LUFS, szczyt −1,0 dBTP. (Wcześniejsza wersja pro bez Pro-L 2 miała
szczyt **+0,1 dBFS** — stąd „przester”.)

## v3 na naszych modułach

Odtworzone wyłącznie modułami programu; Adrian: **„jest podobnie i dużo lepiej niż wcześniejsze”**.
Barwa w granicach ±2 dB od v3 w całym paśmie, tło −66,7 dBFS.

Po drodze:
- **Tłumienie odchyleń (`Tame.h`) jako zamiennik soothe jest za agresywne.** Od 450 Hz bierze
  formanty samogłosek za rezonanse i zabiera środek o ~4 dB; od 2 kHz ścina 5–8 dB góry.
  Zastąpione wąskimi wycięciami (Q 6, 2–5 dB) na rezonansach znalezionych w głosie.
- Nasze wyszukiwanie rezonansów (średnie widmo głośnej mowy wobec mediany z tercji, 2–7 kHz,
  ≥ 3 dB): 2479, 2924, 5051, 5303, 6820 Hz. Łańcuch Adriana: 2443, 2654, 2930, 4755, 5959 Hz.
  Gwizdu 4755 Hz średnie widmo nie łapie (jest w 11 % czasu) — potrzebne wykrywanie dynamiczne.

W programie: `Decide::Wish::studio` (domyślne, #48). Brakuje: tłumienia oddechów (RX Breath
Control) i dynamicznego wycięcia gwizdu.

## Do zrobienia

- Moduł tłumienia oddechów (−18 dB, nie do zera).
- Dynamiczne wykrywanie i wycinanie gwizdu.
- `Tame.h` do przerobienia (węższe pasma, mniejsza głębokość) albo do wycofania.
- Wycinanie przydźwięku: tylko ton podstawowy albo filtry o mniejszym Q — harmoniczne 100/150 Hz
  leżą w tonie męskiego głosu.
- Nagranie z CT1 (Adrian) — ten sam pomiar jeszcze raz.
