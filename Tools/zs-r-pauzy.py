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

    python3 Tools/zs-r-pauzy.py KATALOG_WIERSZA MODEL_VOSK ZS_SLEDZENIE [--powtorka]

Wynik: KATALOG/r/  (nagranie z pauzami, hipotezy, r.tsv) i jedna linijka
podsumowania na wyjsciu.

--powtorka: pelna proba wyciecia. Po pauzie lektor czyta wers JESZCZE RAZ
(wklejamy jego wlasne nagranie od poczatku wersu), tak jak po prawdziwym R.
Dla kazdego R zapisujemy, gdzie wolno ciac (od konca poprzedniego wersu do
poczatku powtorki), a zs-sledzenie --r-ciecie robi to, co program po
zatrzymaniu: LineLog, CutFinder, Punch::plan i Punch::guard — i liczy
ciecia dobre, pominiete i NIEBEZPIECZNE (zabieraja dobre czytanie).
"""

import os
import subprocess
import sys

import numpy as np
import soundfile as sf

PAUZA = 1.0      # tyle czlowiek milknie po pomylce
R_PO = 0.5       # a po tylu wciska R


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    powtorka = "--powtorka" in sys.argv
    if len(args) != 3:
        print(__doc__)
        return 2
    k, model, sledzenie = args
    if powtorka:
        return z_powtorka(k, model, sledzenie)
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


def z_powtorka(k, model, sledzenie):
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

    wyniki = []
    for gdzie in ("poczatek", "srodek"):
        out = os.path.join(k, "r-" + gdzie)
        os.makedirs(out, exist_ok=True)
        kawalki, rtsv, pos, nowy = [], [], 0, 0.0
        poprz_koniec = 0.0
        for i, (od, do, wers) in enumerate(wersy):
            w = [s for s in slowa if s[0] >= od - 0.05 and s[1] <= do + 0.05]
            # pierwszy wers pomijamy: przed nim nie ma czego chronic ani gdzie wrocic
            if i == 0 or len(w) < 3:
                poprz_koniec = do
                continue
            pomylka = w[0][1] if gdzie == "poczatek" else w[len(w) // 2][1]
            start = max(poprz_koniec + 0.05, od - 0.15)       # powtorka od tego miejsca
            # [pos, pomylka] oryginal; pauza; od "start" oryginal jeszcze raz
            kawalki.append(x[int(pos * sr):int(pomylka * sr)])
            nowy += pomylka - pos
            r = nowy + R_PO
            kawalki.append(np.zeros(int(PAUZA * sr), dtype=np.int16))
            nowy += PAUZA
            powt = nowy
            # wolno ciac: od konca poprzedniego wersu do poczatku powtorki (+ 0,1 s ciszy)
            wolno_od = r - R_PO - (pomylka - poprz_koniec)
            wolno_do = powt + (od - start) + 0.1
            pomylka_od = wolno_od + (od - poprz_koniec)
            rtsv.append((r, wers, wolno_od, wolno_do, pomylka_od, powt))
            pos = start
            poprz_koniec = do
        kawalki.append(x[int(pos * sr):])
        wav = os.path.join(out, "z-powtorkami-16k.wav")
        sf.write(wav, np.concatenate(kawalki), sr, subtype="PCM_16")
        with open(os.path.join(out, "r.tsv"), "w") as f:
            for row in rtsv:
                f.write("\t".join(f"{v:.3f}" if isinstance(v, float) else str(v) for v in row) + "\n")
        tu = os.path.dirname(os.path.abspath(__file__))
        subprocess.run([sys.executable, os.path.join(tu, "vosk-hipotezy.py"), model, wav,
                        os.path.join(k, "slownik.txt"), os.path.join(out, "wynik")],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        p = subprocess.run([sledzenie, "--r-ciecie", os.path.join(k, "tekst.txt"), os.path.join(out, "wynik.hip"),
                            os.path.join(out, "r.tsv"), wav], check=True, capture_output=True, text=True)
        wyniki.append(f"{gdzie}: {p.stdout.strip()}")
    print(f"{os.path.basename(os.path.normpath(k))}: " + " | ".join(wyniki))
    return 0


if __name__ == "__main__":
    sys.exit(main())
