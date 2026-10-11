#!/usr/bin/env python3
"""
zs-scenariusz.py — sztuczna sesja nagraniowa z zapisana PRAWDA.

Pierwsza polowa symulacji (druga: Tools/zs-symulacja.cpp). Robi nagranie,
ktore "czyta czlowiek": wers po wersie, z pomylkami i klawiszem R, z wadami
sprzetu w znanych miejscach. Obok zapisuje, CO i KIEDY sie stalo — zeby
symulacja mogla porownac to, co zobaczyl program, z tym, co bylo naprawde.

    python3 Tools/zs-scenariusz.py KATALOG [--ziarno N]

Powstaje:
    KATALOG/tekst.txt     tekst do promptera (wers = wiersz)
    KATALOG/sesja.wav     nagranie, 48 kHz mono, float
    KATALOG/prawda.tsv    co sie stalo (kolumny opisane w naglowku)

Mowa z espeak-ng (polski). Syntezator nie mlaska, nie oddycha i nie brzmi
jak SM7B — to jest test logiki programu, nie brzmienia. Ocene brzmienia
zostawiamy czlowiekowi i prawdziwym nagraniom.

Potrzebne: espeak-ng, ffmpeg, python3 z numpy i soundfile.
"""

import argparse
import os
import subprocess
import sys
import tempfile

import numpy as np
import soundfile as sf

SR = 48000

TEKST = [
    "Dzisiaj nagrywam krótki odcinek o tym, jak słuchamy.",
    "Każdy dźwięk ma swoje miejsce i swój czas.",
    "Mikrofon stoi blisko, więc słychać każde słowo.",
    "Czytam spokojnie, wers po wersie, bez pośpiechu.",
    "Kiedy się pomylę, wracam na początek wersu.",
    "Program sam wycina to, co było nie tak.",
    "Najtrudniejsze są długie zdania pełne szczegółów.",
    "Wtedy biorę oddech i zaczynam jeszcze raz.",
    "Po nagraniu słucham całości od początku.",
    "Jeśli coś zgrzyta, poprawiam tylko to miejsce.",
    "Na końcu zapisuję plik i idę na herbatę.",
    "To wszystko na dzisiaj, dziękuję za uwagę.",
]

# Co robi "czlowiek". Kazda pozycja: (wers, rodzaj, parametry).
#   czytaj   — przeczytany dobrze
#   pomylka  — zaczyna wers, myli sie, wciska R (powtorka idzie osobna pozycja)
#   + wada   — slownik z "wada": przester / plozja / brak / mlaski, "gdzie" 0..1
SCENARIUSZ = [
    (0, "czytaj", {}),
    (1, "czytaj", {}),
    (2, "czytaj", {}),
    (3, "pomylka", {}),
    (3, "czytaj", {}),
    (4, "czytaj", {"wada": "przester", "gdzie": 0.45}),
    (5, "czytaj", {}),
    (6, "pomylka", {}),
    (6, "pomylka", {}),
    (6, "czytaj", {}),
    (7, "czytaj", {"wada": "plozja", "gdzie": 0.30}),
    (8, "czytaj", {}),
    (9, "czytaj", {"wada": "brak", "gdzie": 0.50}),
    (10, "czytaj", {"wada": "mlaski", "gdzie": 0.40}),
    (11, "czytaj", {}),
]

# czego oczekujemy przy wersie z wada (znaczek, LineGrade.h)
OCZEKIWANE = {"przester": "X", "brak": "X", "plozja": "!", "mlaski": "ok"}

PAUZA_MIEDZY = 0.45     # przerwa miedzy wersami
PO_R = 2.6              # pre-roll 2 s + odliczanie + oddech
R_PO_POMYLCE = 0.30     # reakcja: pomylka -> R


def mow(tekst, tmp, nr):
    """Wers z espeak, przyciety z ciszy, 48 kHz, szczyt 0,3."""
    raw = os.path.join(tmp, f"w{nr}.wav")
    out = os.path.join(tmp, f"w{nr}-48.wav")
    subprocess.run(["espeak-ng", "-v", "pl", "-s", "150", "-w", raw, tekst], check=True)
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", raw,
                    "-ar", str(SR), "-ac", "1", out], check=True)
    x, _ = sf.read(out, dtype="float32")
    if x.ndim > 1:
        x = x.mean(axis=1)
    env = np.abs(x) > 0.01 * np.max(np.abs(x))
    idx = np.nonzero(env)[0]
    x = x[idx[0]:idx[-1] + 1]
    return 0.3 * x / max(1e-6, float(np.max(np.abs(x))))


def najglosniej(x, p, okno=0.15, zakres=0.6):
    """Poczatek najglosniejszych 0,15 s w promieniu 0,6 s od p."""
    n = int(okno * SR)
    a, b = max(0, p - int(zakres * SR)), min(len(x) - n, p + int(zakres * SR))
    e = np.convolve(x * x, np.ones(n), mode="valid")
    return a + int(np.argmax(e[a:b]))


def pomylka_tekst(tekst, rng):
    """Poczatek wersu z przekreconym slowem — urwany po ~60%."""
    slowa = tekst.rstrip(".,").split()
    ile = max(2, int(len(slowa) * 0.6))
    slowa = slowa[:ile]
    k = int(rng.integers(1, len(slowa)))
    slowa[k] = "eee " + slowa[k][::-1]
    return " ".join(slowa)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("katalog")
    ap.add_argument("--ziarno", type=int, default=1)
    a = ap.parse_args()

    rng = np.random.default_rng(a.ziarno)
    os.makedirs(a.katalog, exist_ok=True)

    with open(os.path.join(a.katalog, "tekst.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(TEKST) + "\n")

    kawalki, prawda = [], []
    t = 1.5
    kawalki.append(np.zeros(int(t * SR), dtype=np.float32))

    with tempfile.TemporaryDirectory() as tmp:
        for nr, (wers, rodzaj, opcje) in enumerate(SCENARIUSZ):
            tekst = TEKST[wers] if rodzaj == "czytaj" else pomylka_tekst(TEKST[wers], rng)
            x = mow(tekst, tmp, nr)
            dl = len(x) / SR

            wada = opcje.get("wada")
            if wada:
                p = int(opcje["gdzie"] * len(x))
                if wada in ("przester", "plozja"):
                    p = najglosniej(x, p)       # w slowie, nie w pauzie po przecinku
                od = t + p / SR
                if wada == "przester":
                    n = int(0.15 * SR)
                    g = 3.0 / max(1e-6, float(np.max(np.abs(x[p:p + n]))))
                    x[p:p + n] = np.clip(x[p:p + n] * g, -1.0, 1.0)
                    prawda.append(("wada", od, od + 0.15, wers, "przester", OCZEKIWANE[wada]))
                elif wada == "plozja":
                    n = int(0.08 * SR)
                    s = np.sin(np.pi * np.arange(n) / n) * np.sin(2 * np.pi * 45 * np.arange(n) / SR)
                    x[p:p + n] += 0.6 * s.astype(np.float32)
                    prawda.append(("wada", od, od + 0.08, wers, "plozja", OCZEKIWANE[wada]))
                elif wada == "brak":
                    n = int(0.60 * SR)        # program zglasza od 0,4 s (TakeWatchCore)
                    x[p:p + n] = 0.0          # zerowane PO dodaniu tla (nizej)
                    prawda.append(("wada", od, od + 0.60, wers, "brak", OCZEKIWANE[wada]))
                elif wada == "mlaski":
                    for k in range(3):
                        q = p + k * int(0.35 * SR)
                        for i in range(12):
                            x[q + i] += 0.1 * np.exp(-i / 2.5) * (1.0 if i % 3 == 0 else -0.6)
                    prawda.append(("wada", od, od + 0.75, wers, "mlaski", OCZEKIWANE[wada]))

            prawda.append(("mowa", t, t + dl, wers, "pomylka" if rodzaj == "pomylka" else "dobrze", tekst))
            kawalki.append(x)
            t += dl

            if rodzaj == "pomylka":
                r = t + R_PO_POMYLCE
                prawda.append(("R", r, r, wers, "", ""))
                przerwa = R_PO_POMYLCE + PO_R
            else:
                przerwa = PAUZA_MIEDZY * float(rng.uniform(0.85, 1.25))
            kawalki.append(np.zeros(int(przerwa * SR), dtype=np.float32))
            t += przerwa

    kawalki.append(np.zeros(int(2.0 * SR), dtype=np.float32))
    y = np.concatenate(kawalki)

    # tlo pokoju -70 dBFS; miejsca "brak sygnalu" zostaja czystym zerem
    tlo = rng.normal(0.0, 10 ** (-70 / 20), len(y)).astype(np.float32)
    zero = np.zeros(len(y), dtype=bool)
    for p in prawda:
        if p[0] == "wada" and p[4] == "brak":
            zero[int(p[1] * SR):int(p[2] * SR)] = True
    y = np.where(zero, 0.0, y + tlo).astype(np.float32)

    sf.write(os.path.join(a.katalog, "sesja.wav"), y, SR, subtype="FLOAT")

    with open(os.path.join(a.katalog, "prawda.tsv"), "w", encoding="utf-8") as f:
        f.write("# Zielone swiatlo - co sie stalo w sztucznej sesji (zs-scenariusz.py)\n")
        f.write("# typ\tod_s\tdo_s\twers\topis\tdodatkowo\n")
        f.write("#   mowa: opis = dobrze|pomylka, dodatkowo = przeczytany tekst\n")
        f.write("#   R:    czlowiek wciska R\n")
        f.write("#   wada: opis = rodzaj, dodatkowo = oczekiwany znaczek (ok, !, X)\n")
        for p in sorted(prawda, key=lambda p: p[1]):
            f.write(f"{p[0]}\t{p[1]:.3f}\t{p[2]:.3f}\t{p[3]}\t{p[4]}\t{p[5]}\n")

    print(f"{a.katalog}: {len(y) / SR:.1f} s, {len(TEKST)} wersow, "
          f"{sum(1 for p in prawda if p[0] == 'R')} x R, {sum(1 for p in prawda if p[0] == 'wada')} wad")


if __name__ == "__main__":
    sys.exit(main())
