# DNSMOS — sieć, która ocenia, „czy człowiek by to usłyszał” (notatka, 11 października)

Stan: **odłożone**. Do decyzji Adriana, czy przepisać. Pomysł z rozmowy o opiniach ChatGPT/Gemini:
największym ryzykiem programu jest brak zaufania do lampki; druga para „uszu” mogłaby ją potwierdzać.

## Co to jest
DNSMOS P.835 (Microsoft, Reddy i in., ICASSP 2022, arXiv 2110.01763). Ocenia mowę bez wzorca,
jak słuchacze w teście P.835: osobno **głos**, **tło** i **całość**, w skali 1–5.
Źródło: <https://github.com/microsoft/DNS-Challenge>, katalog `DNSMOS/`.

## Licencja
- Kod: **MIT** (`LICENSE-CODE`). Pozostała treść repozytorium: **CC-BY 4.0** (`LICENSE`).
- Pliki z wagami (`.onnx`) nie są wprost przypisane do żadnej z dwóch; obie pozwalają na użycie
  komercyjne z podaniem autora. Wystarczy wpis w „O programie”: „DNSMOS © Microsoft, MIT / CC-BY 4.0”.
- Zgodność z `AGENTS.md`: kod inferencji byłby nasz (nagłówek w `Source/DSP/`), wagi to dane
  Microsoftu. Wyjątek podobny do Vosk, ale bez DLL — wymaga decyzji Adriana.

## Budowa (z pliku `DNSMOS/sig_bak_ovr.onnx`, opset 12)
- wejście: 144 160 próbek = 9,01 s przy 16 kHz,
- widmo przez dwa sploty 161×320 (część rzeczywista i urojona), potem moc i logarytm,
- 7 splotów 3×3 (128 → 64 → 64 → 32 → 32 → 32 → 64 kanałów), ReLU, 3× MaxPool, maksimum globalne,
- 3 warstwy gęste (64→128→64→3),
- wyjście surowe przeliczane wielomianem 2. stopnia (współczynniki w `dnsmos_local.py`),
- **287 292 wagi** (ok. 1,1 MB jako float),
- czas: **285 ms na okno 9 s**, jeden wątek, onnxruntime (CPU kontenera).

Przepisanie: ok. 200–300 linii C++ (splot 2D, ReLU, MaxPool, mnożenie macierzy), bez ONNX.
Test zgodności: te same pliki, różnica wyników z onnxruntime < 0,01.

## Pomiar: na co reaguje
Materiał: 20 s nagrania Adriana (`nic-dwa-razy.mp3`, od 30 s), 16 kHz, wersje zepsute ffmpeg-iem.

| Wersja | Głos | Tło | Całość |
|---|---|---|---|
| czyste | 3,19 | 3,31 | 2,72 |
| szum różowy słaby (a=0,003) | 3,15 | 3,13 | 2,62 |
| szum mocny (a=0,02) | 3,10 | 2,63 | 2,41 |
| szum bardzo mocny (a=0,08) | 2,05 | 1,52 | **1,54** |
| przester lekki (limiter +18 dB) | 2,91 | 3,19 | 2,52 |
| przester mocny (twarde obcięcie +30 dB) | 2,31 | 2,96 | **2,16** |
| pogłos (aecho 60/130/210 ms) | 1,48 | 2,00 | **1,44** |
| przydźwięk 50 Hz (amplituda 0,03) | 3,12 | 3,00 | 2,60 |
| pasmo telefonu (1–3,4 kHz) | 3,15 | 3,76 | 2,80 |
| syntezator espeak | 3,28 | 3,41 | 2,76 |

Wnioski:
- **Łapie dobrze:** pogłos, mocny szum, mocny przester. Pogłos to punkt 3 z rozmowy o miksie
  (do rozważenia) — tu DNSMOS pasuje najlepiej.
- **Prawie nie widzi:** przydźwięku, lekkiego przesteru, słabego szumu.
- **Nie widzi:** barwy i pasma (telefon wyżej niż oryginał), uderzeń „p”, odległości od mikrofonu.
  Lampka musi dalej stać na własnych pomiarach.
- **Liczby bezwzględne mylą:** czyste nagranie człowieka dostaje 2,72. Porównywać ze wzorcem
  (np. wynik z kreatora), nie ze stałym progiem.
- **Okno 9 s:** w lampce reakcja z opóźnieniem kilku sekund; na ekranie oceny bez problemu.

## Własna sieć zamiast DNSMOS
- Od zera: potrzebne tysiące nagrań z ocenami ludzi; publiczne zbiory często zabraniają użytku
  komercyjnego. Teraz nierealne.
- Uczona na wynikach DNSMOS: sens dopiero, gdy potrzebne będzie coś, czego DNSMOS nie ma
  (krótkie okno do lampki, 48 kHz).
- Później: nagrania od testerów z oceną „słychać / nie słychać” jako własne dane.

## Proponowana kolejność (gdy wrócimy)
1. DNSMOS w C++ w `Source/DSP/` + test zgodności z onnxruntime.
2. Ekran oceny: druga opinia o szumie i pogłosie, względem wyniku z kreatora.
3. Lampka dopiero po teście trafności na SM7B (też odłożonym).

## Jak powtórzyć pomiar
```bash
git clone --depth 1 --filter=blob:none --sparse https://github.com/microsoft/DNS-Challenge.git
cd DNS-Challenge && git sparse-checkout set DNSMOS
python3 -m venv venv && venv/bin/pip install onnxruntime numpy soundfile
```
```python
import sys, numpy as np, soundfile as sf, onnxruntime as ort
s = ort.InferenceSession("DNSMOS/DNSMOS/sig_bak_ovr.onnx")
P = [np.poly1d([-0.08397278, 1.22083953, 0.0052439]),     # głos
     np.poly1d([-0.13166888, 1.60915514, -0.39604546]),   # tło
     np.poly1d([-0.06766283, 1.11546468, 0.04602535])]    # całość
L = 144160
for f in sys.argv[1:]:                    # pliki mono 16 kHz
    a, fs = sf.read(f); a = a.astype("float32")
    while len(a) < L: a = np.concatenate([a, a])
    r = np.mean([s.run(None, {"input_1": a[i:i+L][None, :]})[0][0]
                 for i in range(0, len(a) - L + 1, 16000)], 0)
    print(f, *(round(float(p(v)), 2) for p, v in zip(P, r)))
```
