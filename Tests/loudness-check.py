#!/usr/bin/env python3
"""
loudness-check.py — porownanie naszego miernika glosnosci z niezalezna
implementacja BS.1770 (pyloudnorm).

Sens jest taki: nasza implementacja i tamta nie maja ze soba nic wspolnego
poza tekstem standardu. Jesli obie daja te sama liczbe na siedmiu roznych
przebiegach, to znaczy, ze czytamy standard tak samo — a to jedyny sposob,
zeby sie upewnic bez placenia za certyfikacje.

Tolerancja 0,1 LU. Standard dopuszcza wiecej, ale rozbieznosc wieksza niz
0,1 LU zwykle znaczy, ze cos jest inaczej zrozumiane, nie ze cos sie
zaokraglilo.
"""

import sys
import glob
import os

import numpy as np
import soundfile as sf
import pyloudnorm as pyln

TOL = 0.1

directory = sys.argv[1] if len(sys.argv) > 1 else "/tmp/lufs"
files = sorted(glob.glob(os.path.join(directory, "*.wav")))

if not files:
    print(f"nie ma plikow w {directory} — najpierw uruchom ./loudness-test")
    sys.exit(2)

print(f"{'przebieg':<24} {'nasz':>9} {'pyloudnorm':>11} {'roznica':>9}")
print("-" * 56)

bad = 0

for path in files:
    name = os.path.splitext(os.path.basename(path))[0]

    data, rate = sf.read(path)
    meter = pyln.Meter(rate)
    theirs = meter.integrated_loudness(data)

    ours_path = os.path.join(directory, name + ".nasz")
    ours = float(open(ours_path).read().strip()) if os.path.exists(ours_path) else float("nan")

    diff = ours - theirs
    ok = abs(diff) <= TOL
    if not ok:
        bad += 1

    print(f"{name:<24} {ours:9.2f} {theirs:11.2f} {diff:+9.2f}  {'OK' if ok else 'BLAD'}")

print("-" * 56)
print("zgodne" if bad == 0 else f"{bad} rozbieznosci ponad {TOL} LU")
sys.exit(0 if bad == 0 else 1)
