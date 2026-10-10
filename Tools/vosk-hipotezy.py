#!/usr/bin/env python3
"""
vosk-hipotezy.py — co Vosk "slyszy" na nagraniu, chwila po chwili.

Pierwsza polowa powtarzalnego pomiaru promptera. Druga to Tools/zs-sledzenie.cpp.

    python3 Tools/vosk-hipotezy.py MODEL nagranie-16k.wav slownik.txt WYJSCIE

  MODEL         katalog vosk-model-small-pl-0.22
  nagranie      WAV 16 kHz mono 16 bit (ffmpeg -i x.mp3 -ac 1 -ar 16000 -sample_fmt s16 y.wav)
  slownik.txt   slowa skryptu, jedno na linie — robi je: zs-sledzenie --slownik skrypt.txt
  WYJSCIE       przedrostek plikow wynikowych:
                  WYJSCIE.hip   — hipotezy: "czas P|F slowo slowo ..."
                  WYJSCIE.czasy — czasy slow z drugiego przebiegu: "start koniec slowo"

PRZEBIEG 1 (.hip) udaje prace na zywo: audio idzie kawalkami po 0,25 s, a po
kazdym zapisujemy biezaca hipoteze (czesciowa P albo koncowa F) z czasem
AUDIO, w ktorym byla gotowa. Tak samo bedzie to widzial program — bez
opoznienia obliczen, ktore na szybkim komputerze jest male, a na wolnym nie.

PRZEBIEG 2 (.czasy) to czasy slow z wynikow koncowych (SetWords). Uwaga: to
nie jest niezalezny wzorzec — robi go ten sam model. Jesli sa czasy slow
zaznaczone recznie, lepiej uzyc ich.
"""

import json
import sys
import wave

from vosk import KaldiRecognizer, Model, SetLogLevel


def main():
    if len(sys.argv) != 5:
        print(__doc__)
        sys.exit(2)

    model_dir, wav_path, vocab_path, out = sys.argv[1:]
    SetLogLevel(-1)

    with open(vocab_path, encoding="utf-8") as f:
        vocab = [w.strip() for w in f if w.strip()]

    grammar = json.dumps(vocab + ["[unk]"], ensure_ascii=False)
    model = Model(model_dir)

    wf = wave.open(wav_path, "rb")
    if wf.getnchannels() != 1 or wf.getsampwidth() != 2:
        sys.exit("potrzebny WAV mono 16 bit")

    rate = wf.getframerate()
    chunk = int(rate * 0.25)

    # ---- przebieg 1: hipotezy co 0,25 s ----------------------------------------
    rec = KaldiRecognizer(model, rate, grammar)
    done = 0

    with open(out + ".hip", "w", encoding="utf-8") as hip:
        while True:
            data = wf.readframes(chunk)
            if not data:
                break

            done += len(data) // 2
            now = done / rate

            if rec.AcceptWaveform(data):
                text = json.loads(rec.Result()).get("text", "")
                hip.write(f"{now:.3f} F {text}\n")
            else:
                text = json.loads(rec.PartialResult()).get("partial", "")
                hip.write(f"{now:.3f} P {text}\n")

        text = json.loads(rec.FinalResult()).get("text", "")
        hip.write(f"{done / rate:.3f} F {text}\n")

    # ---- przebieg 2: czasy slow ---------------------------------------------------
    wf.rewind()
    rec = KaldiRecognizer(model, rate, grammar)
    rec.SetWords(True)
    words = []

    while True:
        data = wf.readframes(4000)
        if not data:
            break
        if rec.AcceptWaveform(data):
            words += json.loads(rec.Result()).get("result", [])

    words += json.loads(rec.FinalResult()).get("result", [])

    with open(out + ".czasy", "w", encoding="utf-8") as cz:
        for w in words:
            if w["word"] != "[unk]":
                cz.write(f"{w['start']:.3f} {w['end']:.3f} {w['word']}\n")

    print(f"{out}.hip, {out}.czasy: {len(words)} slow z czasami, {done / rate:.1f} s nagrania")


if __name__ == "__main__":
    main()
