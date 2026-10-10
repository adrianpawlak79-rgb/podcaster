#!/bin/bash
#
# zrzut-ekranu.sh — zdjecie ekranu programu, zrobione przy budowaniu.
#
# -----------------------------------------------------------------------------
# PO CO
#
# Program buduje sie na Linuksie, a chodzi na Windowsie. Dotad jedyne, co dalo
# sie o nim powiedziec przed wyslaniem, to "uruchomil sie i nie wywalil" (test
# pod Wine konczacy sie kodem 124). Jak WYGLADA, wiedzial dopiero czlowiek po
# drugiej stronie — i to on znajdowal rzeczy w rodzaju znikajacych znakow
# przestankowych w prompterze.
#
# -----------------------------------------------------------------------------
# OGRANICZENIE, KTORE TRZEBA ZNAC
#
# Pod Wine na wirtualnym ekranie okno maluje sie RAZ. Pierwsza klatka trafia na
# zrzut i na tym koniec: kazde pozniejsze repaint() nie daje juz nowej klatki.
# Sprawdzone wprost — trzy zrzuty nagrania trzyminutowego, co dwadziescia
# sekund, wyszly bajt w bajt identyczne, mimo ze miks w tym czasie przeszedl
# przez dwa etapy.
#
# Praktycznie znaczy to tyle: tym sposobem da sie zobaczyc STAN POCZATKOWY
# ekranu i nic poza nim. Dlatego ekran po nagraniu ma przelacznik
# --ocena-teraz, ktory liczy miks przed pokazaniem okna — zeby pierwsza klatka
# byla juz ta, o ktora chodzi.
#
# Czego tym sposobem NIE sprawdzisz: promptera jadacego za glosem, lampki
# zmieniajacej kolor, igly odsluchu. To nadal wychodzi dopiero na zywym
# Windowsie.
#
# -----------------------------------------------------------------------------
# UZYCIE
#
#   Tools/zrzut-ekranu.sh ocena nagranie.wav  [plik.png]
#   Tools/zrzut-ekranu.sh start               [plik.png]
#
set -e

HERE="$(cd "$(dirname "$0")/.." && pwd)"
EXE="$HERE/build-win/ZieloneSwiatlo_artefacts/Release/Zielone swiatlo.exe"

if [ ! -x "$EXE" ]; then
    echo "nie ma programu — zbuduj go najpierw: cd build-win && ninja"
    exit 1
fi

CO="${1:-start}"
DISP=":$(( 80 + RANDOM % 15 ))"

case "$CO" in
    ocena)
        WAV="$2"
        OUT="${3:-/tmp/zs-ocena.png}"

        if [ ! -f "$WAV" ]; then echo "nie ma pliku: $WAV"; exit 1; fi

        cp "$WAV" "$HERE/build-win/zrzut-wejscie.wav"
        ARGS=(--ocena-teraz zrzut-wejscie.wav)
        CZEKAJ=35
        ;;

    start)
        OUT="${2:-/tmp/zs-start.png}"
        ARGS=()
        CZEKAJ=25
        ;;

    *)
        echo "nie znam: $CO (umiem: ocena, start)"
        exit 1
        ;;
esac

export DISPLAY="$DISP"
Xvfb "$DISP" -screen 0 1280x800x24 >/dev/null 2>&1 &
XPID=$!
sleep 2

cd "$HERE/build-win"
timeout $(( CZEKAJ + 20 )) wine "$EXE" "${ARGS[@]}" > /tmp/zs-zrzut.log 2>&1 &

sleep "$CZEKAJ"
import -window root "$OUT" 2>/dev/null || true

wineserver -k 2>/dev/null || true
kill $XPID 2>/dev/null || true

if [ -f "$OUT" ]; then
    echo "zrzut: $OUT"
else
    echo "nie udalo sie zrobic zrzutu — patrz /tmp/zs-zrzut.log"
    exit 1
fi
