#!/usr/bin/env python3
"""
zs-r-pauzy.py — czy R trafia we wlasciwy wers, gdy czlowiek naprawde przerywa.

Pomiar "R wcisniete w losowej chwili czytania" jest zbyt surowy: lektor czyta
dalej, a rozpoznawanie mowy jest o 0,6-0,8 s z tylu, wiec w pierwszej sekundzie
wersu prompter jest jeszcze w poprzednim. Czlowiek, ktory sie pomylil, PRZERYWA
i dopiero wtedy wciska R — a rozpoznawanie przez ten czas dogania.

Tu to odtwarzamy na prawdziwym nagraniu (katalog z Tools/zs-prawdziwe.py):
po wybranych slowach wstawiamy 1 s ciszy ("zatrzymal sie po pomylce"),
R pada 0,5 s w te cisze. Slowa: pierwsze slowo wersu (najtrudniej — pomylka
na samym poczatku) i slowo ze srodka wersu. Potem Vosk na zmienionym nagraniu
i zs-sledzenie --r liczy, dokad R cofa.

    python3 Tools/zs-r-pauzy.py KATALOG_WIERSZA MODEL_VOSK ZS_SLEDZENIE

Wynik: KATALOG/r/  (nagranie z pauzami, hipotezy, r.tsv) i jedna linijka
podsumowania na wyjsciu.
"""

import os
import subprocess
import sys

import numpy as np
import soundfile as sf

PAUZA = 1.0      # tyle czlowiek milknie po pomylce
R_PO = 0.5       # a po tylu wciska R


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    k, model, sledzenie = sys.argv[1:]
    out = os.path.join(k, "r")
    os.makedirs(out, exist_ok=True)

    x, sr = sf.read(os.path.join(k, "sesja-16k.wav"), dtype="int16")
    slowa = []
    with open(os.path.join(k, "wynik.czasy")) as f:
        for l in f:
            p = l.split()
            if len(p) >= 3:
                slowa.append((float(p[0]), float(p[1])))
    wersy = []
    with open(os.path.join(k, "prawda.tsv"), encoding="utf-8") as f:
        for l in f:
            c = l.rstrip("\n").split("\t")
            if c and c[0] == "mowa":
                wersy.append((float(c[1]), float(c[2]), int(c[3])))

    # proby: pierwsze i srodkowe slowo kazdego wersu (z czasow Vosk w tym wersie)
    proby = []
    for od, do, wers in wersy:
        w = [s for s in slowa if s[0] >= od - 0.05 and s[1] <= do + 0.05]
        if not w:
            continue
        proby.append((w[0][1], wers, "poczatek"))
        if len(w) >= 3:
            proby.append((w[len(w) // 2][1], wers, "srodek"))
    proby.sort()

    # nagranie z pauzami; czas R w nowym nagraniu
    kawalki, rtsv, pos, dodane = [], [], 0, 0.0
    for koniec, wers, gdzie in proby:
        cut = int(koniec * sr)
        kawalki.append(x[pos:cut])
        kawalki.append(np.zeros(int(PAUZA * sr), dtype=np.int16))
        rtsv.append((koniec + dodane + R_PO, wers, gdzie))
        dodane += PAUZA
        pos = cut
    kawalki.append(x[pos:])
    wav = os.path.join(out, "z-pauzami-16k.wav")
    sf.write(wav, np.concatenate(kawalki), sr, subtype="PCM_16")

    with open(os.path.join(out, "r.tsv"), "w") as f:
        for t, w, g in rtsv:
            f.write(f"{t:.3f}\t{w}\t{g}\n")

    tu = os.path.dirname(os.path.abspath(__file__))
    subprocess.run([sys.executable, os.path.join(tu, "vosk-hipotezy.py"), model, wav,
                    os.path.join(k, "slownik.txt"), os.path.join(out, "wynik")],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    p = subprocess.run([sledzenie, "--r", os.path.join(k, "tekst.txt"), os.path.join(out, "wynik.hip"),
                        os.path.join(out, "r.tsv")], check=True, capture_output=True, text=True)
    print(f"{os.path.basename(os.path.normpath(k))}: {p.stdout.strip()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
