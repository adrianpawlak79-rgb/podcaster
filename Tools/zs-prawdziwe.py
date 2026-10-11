#!/usr/bin/env python3
"""
zs-prawdziwe.py — scenariusze symulacji z PRAWDZIWYM glosem (Wolne Lektury).

Syntezator (zs-scenariusz.py) nie oddycha, nie zwalnia na koncu wersu i
mowi rowno jak metronom — dla licznika sylab to zupelnie inny swiat niz zywy
glos. Tu bierzemy krotkie wiersze z audiobookow Wolnych Lektur: czytaja je
aktorzy, tekst jest obok, licencje wolne (teksty w domenie publicznej,
nagrania CC BY-SA 3.0 — https://wolnelektury.pl/info/zasady-wykorzystania/).
Nagran NIE trzymamy w repozytorium: skrypt pobiera je do katalogu roboczego.

    python3 Tools/zs-prawdziwe.py KATALOG MODEL_VOSK ZS_SLEDZENIE [slug ...]

Dla kazdego wiersza powstaje KATALOG/<slug>/ z plikami jak z zs-scenariusz.py
(tekst.txt, sesja.wav 48 kHz, prawda.tsv) plus sesja-16k.wav, wynik.hip,
wynik.czasy — do pomiaru sledzenia z rozpoznawaniem mowy (zs-sledzenie).

PRAWDA = czasy slow z Vosk dopasowane do tekstu (zs-sledzenie --prawda). To
nie jest niezalezny wzorzec — ten sam model — ale dla pytania "ktory wiersz
jest teraz czytany" wystarcza: blad Vosk w czasie slowa to dziesiate czesci
sekundy, a wers trwa kilka sekund. Zapowiedz lektora (autor, tytul) i stopke
wycinamy: nagranie zaczyna sie 0,8 s przed pierwszym slowem tekstu.

Potrzebne: ffmpeg, python3 z numpy, soundfile i vosk, model vosk-model-small-pl.
"""

import json
import os
import subprocess
import sys
import urllib.request

import numpy as np
import soundfile as sf

API = "https://wolnelektury.pl/api/books/"

# 11 wierszy, 8 lektorow (2 kobiety), wszystkie teksty w domenie publicznej
DOMYSLNE = [
    "konopnicka-pszczoly",                          # Mariusz Rzepka
    "sonety-krymskie-widok-gor-ze-stepow-kozlowa",  # Jan Dravnel
    "piesni-ksiegi-pierwsze-piesn-ix",              # Krzysztof Skonieczny
    "napoj-cienisty-magda",                         # Wiktor Korzeniewski
    "wilk-pokutujacy",                              # Bartosz Mazur
    "wrobelek-i-kurczatko",                         # Mariusz Rzepka
    "sonety-krymskie-gora-kikineis",                # Piotr Glowacki
    "jodla-i-jablon-bajki-nowe",                    # Masza Bogucka
    "kwiaty-zla-olbrzymka",                         # Joanna Niemirska
    "fraszki-ksiegi-pierwsze-o-zywocie-ludzkim-wieczna-mysli-ktor",  # Michal Michalski
    "miedzy-nami-nic-nie-bylo",                     # Piotr Glowacki (przyslal Adrian)
]


def pobierz(url, cel):
    if not os.path.exists(cel):
        with urllib.request.urlopen(url, timeout=120) as r, open(cel, "wb") as f:
            f.write(r.read())
    return cel


def tekst_wiersza(txt):
    """Sam wiersz: bez naglowka (autor, zbior, tytul) i bez stopki Wolnych Lektur."""
    cialo = txt.replace("\r\n", "\n").split("\n-----\n")[0]
    wiersze = cialo.split("\n")
    # naglowek konczy sie pierwsza seria co najmniej dwoch pustych wierszy
    start = 0
    for i in range(len(wiersze) - 1):
        if wiersze[i].strip() == "" and wiersze[i + 1].strip() == "":
            start = i
            break
    out = [w.rstrip() for w in wiersze[start:]]
    while out and not out[0].strip():
        out.pop(0)
    while out and not out[-1].strip():
        out.pop()
    # przerwy miedzy zwrotkami: najwyzej jeden pusty wiersz
    zwarte = []
    for w in out:
        if w.strip() == "" and zwarte and zwarte[-1].strip() == "":
            continue
        zwarte.append(w)
    return "\n".join(zwarte) + "\n"


def run(cmd, **kw):
    return subprocess.run(cmd, check=True, **kw)


def vosk(model, wav16, katalog, sledzenie, tekst):
    slownik = os.path.join(katalog, "slownik.txt")
    with open(slownik, "w") as f:
        run([sledzenie, "--slownik", tekst], stdout=f)
    tu = os.path.dirname(os.path.abspath(__file__))
    run([sys.executable, os.path.join(tu, "vosk-hipotezy.py"), model, wav16, slownik,
         os.path.join(katalog, "wynik")], stdout=subprocess.DEVNULL)
    p = subprocess.run([sledzenie, "--prawda", tekst, os.path.join(katalog, "wynik.czasy")],
                       check=True, capture_output=True, text=True)
    return p.stdout, p.stderr.strip()


def jeden(slug, katalog, model, sledzenie):
    k = os.path.join(katalog, slug)
    os.makedirs(k, exist_ok=True)
    info = json.load(urllib.request.urlopen(API + slug + "/", timeout=60))
    mp3 = [m for m in info["media"] if m["type"] == "mp3"][0]
    pobierz(info["txt"], os.path.join(k, "oryginal.txt"))
    pobierz(mp3["url"], os.path.join(k, "oryginal.mp3"))

    tekst = os.path.join(k, "tekst.txt")
    with open(os.path.join(k, "oryginal.txt"), encoding="utf-8") as f:
        body = tekst_wiersza(f.read())
    with open(tekst, "w", encoding="utf-8") as f:
        f.write(body)

    # przebieg 1: cale nagranie — gdzie zaczyna sie i konczy tekst
    pelne16 = os.path.join(k, "pelne-16k.wav")
    run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", os.path.join(k, "oryginal.mp3"),
         "-ac", "1", "-ar", "16000", "-sample_fmt", "s16", pelne16])
    prawda, _ = vosk(model, pelne16, k, sledzenie, tekst)
    czasy = [(float(c[1]), float(c[2])) for c in (l.split("\t") for l in prawda.splitlines())
             if c and c[0] == "mowa"]
    if not czasy:
        raise RuntimeError("nie dopasowalem tekstu do nagrania")
    od = max(0.0, czasy[0][0] - 0.8)
    do = czasy[-1][1] + 1.5

    # przebieg 2: przyciete nagranie — wlasciwa prawda i hipotezy na zywo
    wav = os.path.join(k, "sesja.wav")
    wav16 = os.path.join(k, "sesja-16k.wav")
    run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", os.path.join(k, "oryginal.mp3"),
         "-ss", f"{od:.3f}", "-to", f"{do:.3f}", "-ac", "1", "-ar", "48000", "-c:a", "pcm_f32le", wav])
    run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", wav,
         "-ac", "1", "-ar", "16000", "-sample_fmt", "s16", wav16])
    prawda, dopasowanie = vosk(model, wav16, k, sledzenie, tekst)
    with open(os.path.join(k, "prawda.tsv"), "w", encoding="utf-8") as f:
        f.write(prawda)

    x, sr = sf.read(wav)
    wiersze = sum(1 for l in body.splitlines() if l.strip())
    print(f"{slug}: {len(x) / sr:.1f} s, {wiersze} wierszy, lektor {mp3['artist']}, {dopasowanie}")


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    katalog, model, sledzenie = sys.argv[1:4]
    slugi = sys.argv[4:] or DOMYSLNE
    os.makedirs(katalog, exist_ok=True)
    for s in slugi:
        try:
            jeden(s, katalog, model, os.path.abspath(sledzenie))
        except Exception as e:
            print(f"{s}: POMINIETY — {e}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
