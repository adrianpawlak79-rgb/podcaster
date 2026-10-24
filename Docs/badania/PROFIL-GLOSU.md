# Wzorzec brzmienia — ta sama barwa między odcinkami

22.10.2026. Punkt 5 z planu rozwoju (dźwięk).

## Problem

Głośność odcinków jest równa (każdy miks celuje w −19 LUFS). Barwa nie:
inna odległość od SM7B (efekt zbliżeniowy), inny pokój, inny dzień głosu.
Miks „Studyjny” ma stały korektor — dwa nagrania o różnej barwie wychodzą
z niego różne.

## Rozwiązanie

- **Wzorzec** (`Source/DSP/Mix/VoiceProfile.h`): średnie widmo głośnych
  fragmentów zatwierdzonego miksu, krzywa na siatce 1/16 oktawy (tercje),
  bez średniej (kształt, nie głośność). ~120 liczb w ustawieniach.
- **Dociąganie** (`SpeechChain`, po wszystkich stopniach, przed limiterem):
  tym samym narzędziem co cofanie barwy po kompresji (`TiltMatch`) — półka
  300 Hz, dzwony 500 i 1400 Hz, półka 5 kHz; najwyżej ±3 dB, góra przy
  de-esserze najwyżej +1,5 dB; poniżej 0,75 dB różnicy nic.
- Tylko **Studyjny i Mocny**. Delikatny z założenia zostawia głos jak nagrany.
- Ekran **Gotowe**: „Zapamiętaj to brzmienie”. **Mikrofon i sprzęt**:
  wzorzec i „Usuń wzorzec”.

## Pomiar (`Tools/zs-profil.cpp`)

Różnica barwy gotowego miksu od wzorca (ważone RMS krzywej, dB). Wzorzec
z miksu nagrania Adriana z 12.10.

| nagranie | bez wzorca | ze wzorcem | korekta 300 / 500 / 1400 / 5k |
|---|---|---|---|
| bliżej mikrofonu (+5 dB dołu, −2 dB 3 kHz) | 1,54 | **0,72** | −2,4 / +1,1 / +0,4 / +1,5 |
| dalej od mikrofonu (−4 dB dołu, +3 dB góry) | 2,22 | **1,02** | +2,5 / −1,0 / −0,2 / 0,0 |
| to samo nagranie (kontrola) | 0,00 | 0,00 | bez korekty |
| inna sesja (0024) wobec 12.10 | 2,41 | **1,38** | +0,5 / +3,0 / −0,1 / −3,0 |
| 12.10 wobec wzorca z 0024 | 2,14 | **1,50** | −0,3 / −3,0 / +0,5 / +1,5 |

Różnica spada o 30–55 %. Reszty nie zdejmujemy celowo: trzy szerokie
kształty i ±3 dB to przechył, nie zamiana czyjegoś głosu.

**Do odsłuchu:** czy po dociągnięciu głos nie brzmi „przefiltrowany” —
zwłaszcza przy korekcie ±3 dB (sesje z różnych dni).

## Po drodze

Pierwsza wersja porównywała wygładzony wzorzec z niewygładzonym widmem
i na głosie o wyraźnych harmonicznych (espeak) widziała ±2 dB zmarszczek
tam, gdzie różnicy nie było. Teraz obie strony mierzone tak samo.
Druga: półka 5 kHz dopasowywana tylko do punktów pod 4 kHz „zgadywała”
górę — teraz dopasowanie bierze całe pasmo, a podbicie góry ogranicza
osobny warunek.
