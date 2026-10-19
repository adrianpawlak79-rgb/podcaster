# Miks na wielu głosach (19.10.2026)

Audyt 14.10: „Miks oceniony na głosie autora nie potwierdza jakości na różnych głosach i mikrofonach.
Stałe wycięcia pasm, dodawanie powietrza i ściszanie oddechów mogą pogorszyć inne nagrania.”

**Materiał:** 10 nagrań Wolnych Lektur, 8 lektorów (w tym 2 kobiety: Masza Bogucka, Joanna Niemirska),
42–121 s każde (licencja CC BY-SA 3.0, nagrań nie ma w repo — `Tools/zs-prawdziwe.py` pokazuje, skąd).
Każde w dwóch wersjach wejścia:

- **studyjne** — tak, jak przyszło (to już obrobione audiobooki: miks nie powinien ich zepsuć);
- **laptop** — 14 dB ciszej, szum wentylatora −62 dBFS i przydźwięk 50 Hz −66 dBFS (tani mikrofon w zwykłym pokoju).

Każda przez trzy brzmienia, te same klasy co w programie. Narzędzie: `Tools/zs-glosy.cpp`
(`g++ -O2 -std=c++17 -ISource Tools/zs-glosy.cpp -o zs-glosy && zs-glosy raport.md *.wav`).

## Wnioski

1. **Głośność i szczyt — wszędzie trafione.** 60 z 60: −19,0 LUFS, szczyt −1,0 … −1,5 dBTP.
2. **Tło w pauzach — zawsze ciszej, nigdy głośniej.**
   - Studyjny i Mocny: zwykle o 10–21 dB; najmniej na „Pieśni IX” (Skonieczny), o 4–6 dB.
   - Laptop, Studyjny i Mocny: z −29…−38 do −46…−59 dB pod mową.
   - Delikatny: o 0–13 dB (z założenia lekko).
3. **Wyrównanie** (rozpiętość głośności w mowie, studyjne):
   - Studyjny i Mocny: zwykle spada o 1–5 dB; na dwóch nagraniach bez zmian albo +1 dB.
   - Delikatny: od −3 do +1,6 dB.
4. **Barwa — ten sam kierunek na wszystkich głosach, kobiecych i męskich:**
   - Studyjny: 2–5 kHz −0,4…+3,5 dB, 5–10 kHz −1,6…+3,8 dB, 200–500 Hz −2,3…+0,6 dB względem 0,5–2 kHz.
   - Mocny: 2–5 kHz +1,1…+5,0 dB, 5–10 kHz −1,2…+4,8 dB.
   - Delikatny: wszystkie pasma w ±2,9 dB.
   - Nic nie odjechało o więcej niż 6 dB.
5. **Jedyne flagi (2 z 60):** Mocny na głosie Piotra Głowackiego („Między nami nic nie było”) —
   góra 5–10 kHz +4,8 dB (próg 4). Ten głos jest już jasny, a Mocny celowo dokłada obecności.
   **Do posłuchania:** czy „s” nie kłuje. Jeśli tak — Mocny powinien mniej podbijać górę, gdy pomiar
   sybilantów (`tameWouldDb`) jest wysoki. Nie zmieniam bez odsłuchu.

**To nie jest odsłuch.** Liczby mówią, gdzie posłuchać, a nie, że brzmi dobrze. Do zrobienia przez
człowieka: Mocny na jasnym głosie (wyżej), Delikatny na „laptopie” (tło zostaje słyszalne — z założenia).

## Pełna tabela

| nagranie | wejście | brzmienie | LUFS | szczyt | tło przed→po | rozpiętość przed→po | Δ 80–200 | Δ 200–500 | Δ 2–5 k | Δ 5–10 k | |
|---|---|---|---|---|---|---|---|---|---|---|---|
| fraszki-ksiegi-pierwsze-o-… | studyjne | Delikatny | -19.0 | -1.2 | -72→-75 | 14.5→11.4 | +1.3 | +1.8 | +1.9 | +2.9 |  |
| fraszki-ksiegi-pierwsze-o-… | studyjne | Studyjny | -19.0 | -1.4 | -72→-86 | 14.5→9.6 | +2.3 | +0.1 | +3.4 | +3.0 |  |
| fraszki-ksiegi-pierwsze-o-… | studyjne | Mocny | -19.0 | -1.4 | -72→-84 | 14.5→9.8 | +2.3 | -0.0 | +4.6 | +3.8 |  |
| fraszki-ksiegi-pierwsze-o-… | laptop | Delikatny | -19.0 | -1.2 | -29→-38 | 27.1→11.5 | +1.2 | +1.8 | +1.9 | +2.9 |  |
| fraszki-ksiegi-pierwsze-o-… | laptop | Studyjny | -19.0 | -1.4 | -29→-48 | 27.1→11.0 | +2.2 | +0.1 | +3.4 | +3.0 |  |
| fraszki-ksiegi-pierwsze-o-… | laptop | Mocny | -19.0 | -1.4 | -29→-46 | 27.1→9.9 | +2.2 | +0.0 | +4.6 | +3.8 |  |
| jodla-i-jablon-bajki-nowe.… | studyjne | Delikatny | -19.0 | -1.4 | -69→-76 | 11.8→10.4 | +0.2 | +0.3 | +0.7 | +0.9 |  |
| jodla-i-jablon-bajki-nowe.… | studyjne | Studyjny | -19.0 | -1.4 | -69→-81 | 11.8→10.1 | -0.1 | -0.8 | -0.4 | +1.9 |  |
| jodla-i-jablon-bajki-nowe.… | studyjne | Mocny | -19.0 | -1.4 | -69→-80 | 11.8→8.8 | +0.3 | -0.6 | +1.1 | +2.9 |  |
| jodla-i-jablon-bajki-nowe.… | laptop | Delikatny | -19.0 | -1.4 | -36→-46 | 11.8→10.4 | +0.2 | +0.3 | +0.7 | +0.9 |  |
| jodla-i-jablon-bajki-nowe.… | laptop | Studyjny | -19.0 | -1.4 | -36→-53 | 11.8→10.1 | -0.2 | -0.9 | -0.4 | +2.0 |  |
| jodla-i-jablon-bajki-nowe.… | laptop | Mocny | -19.0 | -1.4 | -36→-51 | 11.8→9.9 | +0.2 | -0.6 | +1.1 | +3.0 |  |
| konopnicka-pszczoly.wav | studyjne | Delikatny | -19.0 | -1.4 | -65→-68 | 14.5→16.1 | +0.1 | +0.3 | +0.3 | +0.7 |  |
| konopnicka-pszczoly.wav | studyjne | Studyjny | -19.0 | -1.0 | -65→-78 | 14.5→14.6 | +0.4 | -1.1 | +2.0 | +1.9 |  |
| konopnicka-pszczoly.wav | studyjne | Mocny | -19.0 | -1.0 | -65→-77 | 14.5→14.2 | +1.1 | -0.8 | +3.1 | +3.0 |  |
| konopnicka-pszczoly.wav | laptop | Delikatny | -19.0 | -1.4 | -34→-44 | 17.0→16.2 | +0.1 | +0.3 | +0.3 | +0.7 |  |
| konopnicka-pszczoly.wav | laptop | Studyjny | -19.0 | -1.2 | -34→-56 | 17.0→14.6 | +0.4 | -1.1 | +1.9 | +1.9 |  |
| konopnicka-pszczoly.wav | laptop | Mocny | -19.0 | -1.1 | -34→-55 | 17.0→14.2 | +1.1 | -0.8 | +3.1 | +3.0 |  |
| kwiaty-zla-olbrzymka.wav | studyjne | Delikatny | -19.0 | -1.5 | -79→-92 | 12.8→11.9 | -0.4 | -0.2 | +0.3 | +0.3 |  |
| kwiaty-zla-olbrzymka.wav | studyjne | Studyjny | -19.0 | -1.4 | -79→-98 | 12.8→10.4 | -0.1 | -1.5 | +2.6 | +2.3 |  |
| kwiaty-zla-olbrzymka.wav | studyjne | Mocny | -19.0 | -1.2 | -79→-97 | 12.8→9.3 | +0.2 | -1.4 | +4.0 | +3.3 |  |
| kwiaty-zla-olbrzymka.wav | laptop | Delikatny | -19.0 | -1.5 | -38→-48 | 12.8→11.8 | -0.4 | -0.2 | +0.3 | +0.3 |  |
| kwiaty-zla-olbrzymka.wav | laptop | Studyjny | -19.0 | -1.5 | -38→-59 | 12.8→10.3 | -0.1 | -1.5 | +2.6 | +2.3 |  |
| kwiaty-zla-olbrzymka.wav | laptop | Mocny | -19.0 | -1.2 | -38→-58 | 12.8→9.4 | +0.2 | -1.4 | +4.0 | +3.3 |  |
| miedzy-nami-nic-nie-bylo.wav | studyjne | Delikatny | -19.0 | -1.4 | -74→-78 | 13.1→12.5 | +1.0 | +1.6 | +1.4 | +2.2 |  |
| miedzy-nami-nic-nie-bylo.wav | studyjne | Studyjny | -19.0 | -1.4 | -74→-90 | 13.1→14.1 | +2.2 | +0.6 | +2.9 | +3.7 |  |
| miedzy-nami-nic-nie-bylo.wav | studyjne | Mocny | -19.0 | -1.4 | -74→-89 | 13.1→12.9 | +2.6 | +0.8 | +4.3 | +4.8 | ⚠ |
| miedzy-nami-nic-nie-bylo.wav | laptop | Delikatny | -19.0 | -1.4 | -33→-41 | 16.0→12.5 | +1.0 | +1.6 | +1.4 | +2.1 |  |
| miedzy-nami-nic-nie-bylo.wav | laptop | Studyjny | -19.0 | -1.4 | -33→-53 | 16.0→13.4 | +2.2 | +0.7 | +3.0 | +3.8 |  |
| miedzy-nami-nic-nie-bylo.wav | laptop | Mocny | -19.0 | -1.4 | -33→-51 | 16.0→12.3 | +2.6 | +0.9 | +4.3 | +4.8 | ⚠ |
| napoj-cienisty-magda.wav | studyjne | Delikatny | -19.0 | -1.5 | -48→-49 | 10.5→10.3 | -1.1 | -0.3 | +0.3 | +0.3 |  |
| napoj-cienisty-magda.wav | studyjne | Studyjny | -19.0 | -1.3 | -48→-59 | 10.5→9.7 | -0.3 | -2.3 | +0.8 | -1.6 |  |
| napoj-cienisty-magda.wav | studyjne | Mocny | -19.0 | -1.2 | -48→-57 | 10.5→9.0 | -0.4 | -2.6 | +1.9 | -1.1 |  |
| napoj-cienisty-magda.wav | laptop | Delikatny | -19.0 | -1.5 | -34→-43 | 11.9→10.5 | -1.1 | -0.3 | +0.3 | +0.3 |  |
| napoj-cienisty-magda.wav | laptop | Studyjny | -19.0 | -1.5 | -34→-54 | 11.9→9.7 | -0.4 | -2.3 | +0.8 | -1.6 |  |
| napoj-cienisty-magda.wav | laptop | Mocny | -19.0 | -1.2 | -34→-53 | 11.9→9.1 | -0.5 | -2.6 | +1.9 | -1.2 |  |
| piesni-ksiegi-pierwsze-pie… | studyjne | Delikatny | -19.0 | -1.4 | -71→-71 | 11.2→10.5 | -0.2 | +0.2 | +0.2 | +0.4 |  |
| piesni-ksiegi-pierwsze-pie… | studyjne | Studyjny | -19.0 | -1.4 | -71→-77 | 11.2→9.4 | +0.6 | -1.0 | +2.0 | +2.2 |  |
| piesni-ksiegi-pierwsze-pie… | studyjne | Mocny | -19.0 | -1.3 | -71→-75 | 11.2→7.9 | +1.1 | -0.7 | +3.0 | +3.0 |  |
| piesni-ksiegi-pierwsze-pie… | laptop | Delikatny | -19.0 | -1.4 | -36→-46 | 11.2→10.5 | -0.2 | +0.2 | +0.2 | +0.4 |  |
| piesni-ksiegi-pierwsze-pie… | laptop | Studyjny | -19.0 | -1.4 | -36→-53 | 11.2→9.0 | +0.6 | -1.0 | +2.0 | +2.3 |  |
| piesni-ksiegi-pierwsze-pie… | laptop | Mocny | -19.0 | -1.3 | -36→-51 | 11.2→8.0 | +1.1 | -0.7 | +3.0 | +3.1 |  |
| sonety-krymskie-gora-kikin… | studyjne | Delikatny | -19.0 | -1.4 | -80→-88 | 14.3→12.4 | -0.2 | +0.6 | +0.6 | +0.7 |  |
| sonety-krymskie-gora-kikin… | studyjne | Studyjny | -19.0 | -1.4 | -80→-101 | 14.3→12.1 | +0.8 | -0.9 | +2.0 | +2.4 |  |
| sonety-krymskie-gora-kikin… | studyjne | Mocny | -19.0 | -1.4 | -80→-99 | 14.3→10.7 | +1.0 | -0.8 | +3.2 | +3.3 |  |
| sonety-krymskie-gora-kikin… | laptop | Delikatny | -19.0 | -1.4 | -35→-45 | 14.3→12.5 | -0.1 | +0.6 | +0.6 | +0.7 |  |
| sonety-krymskie-gora-kikin… | laptop | Studyjny | -19.0 | -1.4 | -35→-58 | 14.3→12.1 | +0.7 | -0.9 | +2.0 | +2.5 |  |
| sonety-krymskie-gora-kikin… | laptop | Mocny | -19.0 | -1.4 | -35→-56 | 14.3→10.7 | +1.0 | -0.8 | +3.2 | +3.3 |  |
| sonety-krymskie-widok-gor-… | studyjne | Delikatny | -19.0 | -1.4 | -77→-81 | 15.7→14.6 | +0.7 | +1.4 | +1.8 | +2.2 |  |
| sonety-krymskie-widok-gor-… | studyjne | Studyjny | -19.0 | -1.4 | -77→-93 | 15.7→13.2 | +2.4 | +0.3 | +3.5 | +1.4 |  |
| sonety-krymskie-widok-gor-… | studyjne | Mocny | -19.0 | -1.4 | -77→-91 | 15.7→11.9 | +2.8 | +0.5 | +5.0 | +2.5 |  |
| sonety-krymskie-widok-gor-… | laptop | Delikatny | -19.0 | -1.4 | -35→-44 | 15.9→14.8 | +0.7 | +1.5 | +1.8 | +2.2 |  |
| sonety-krymskie-widok-gor-… | laptop | Studyjny | -19.0 | -1.4 | -35→-57 | 15.9→13.4 | +2.4 | +0.4 | +3.5 | +1.3 |  |
| sonety-krymskie-widok-gor-… | laptop | Mocny | -19.0 | -1.4 | -35→-55 | 15.9→12.1 | +2.8 | +0.5 | +5.0 | +2.4 |  |
| wilk-pokutujacy.wav | studyjne | Delikatny | -19.0 | -1.4 | -48→-52 | 14.9→14.2 | +0.2 | +0.9 | +0.8 | +1.2 |  |
| wilk-pokutujacy.wav | studyjne | Studyjny | -19.0 | -1.4 | -48→-60 | 14.9→12.7 | +1.1 | -0.6 | +2.8 | +1.0 |  |
| wilk-pokutujacy.wav | studyjne | Mocny | -19.0 | -1.4 | -48→-59 | 14.9→11.5 | +1.3 | -0.5 | +4.0 | +1.8 |  |
| wilk-pokutujacy.wav | laptop | Delikatny | -19.0 | -1.4 | -33→-42 | 16.5→14.3 | +0.1 | +0.9 | +0.8 | +1.2 |  |
| wilk-pokutujacy.wav | laptop | Studyjny | -19.0 | -1.3 | -33→-52 | 16.5→14.6 | +1.0 | -0.6 | +2.9 | +1.0 |  |
| wilk-pokutujacy.wav | laptop | Mocny | -19.0 | -1.4 | -33→-51 | 16.5→12.6 | +1.3 | -0.5 | +4.0 | +1.8 |  |

2 z 60 przebiegów z flagą ⚠.
