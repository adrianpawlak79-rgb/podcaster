#!/bin/bash
# ci.sh — te same testy co wszystko.sh, ale kazdy osobno i z podsumowaniem.
#
# wszystko.sh zatrzymuje sie na pierwszym bledzie (set -e). Na main dwa testy
# nie przechodza od dawna (Tests/znane-bledy.txt), wiec wszystko.sh nie
# dochodzi do konca i nie mowi, czy zmiana zepsula cos INNEGO. Ten skrypt
# puszcza wszystko, pisze tabele i konczy sie bledem tylko wtedy, gdy:
#   - nie przechodzi test spoza listy znanych bledow, albo
#   - test z listy znanych bledow zaczal przechodzic (wtedy trzeba go z niej
#     usunac — inaczej lista klamie).
#
# Uzywa go GitHub Actions (.github/workflows/testy.yml i gemini.yml).
# Potrzebuje: g++, espeak-ng, ffmpeg, python3 z soundfile i pyloudnorm.

cd "$(dirname "$0")"
CXX=${CXX:-g++}
FLAGS="-O2 -std=c++17 -I../Source"
OUT=${CI_PODSUMOWANIE:-/tmp/zs-podsumowanie.md}

# ---- dane testowe z syntezatora (zadne .wav nie jest w repo) ----------------
espeak-ng -v pl -s 150 -w /tmp/mowa.wav \
  "Dzisiaj wieczorem nagramy dwa zdania normalnym glosem. Sprawdzam poziom i odleglosc od mikrofonu."
[ -f t2.wav ] || espeak-ng -v pl -s 150 -w t2.wav -f t2.txt
mkdir -p /tmp/mowa
espeak-ng -v pl -s 140 -w /tmp/mowa/mowa-22k.wav \
  "Dzisiaj jest piatek i nagrywam pierwszy odcinek. Kot siedzi na plocie, a pies spi pod stolem. \
Nagranie musi byc czyste, zeby dalo sie go sluchac bez zmeczenia. Mikrofon stoi blisko ust, \
wiec slychac kazdy oddech. Czytam dalej rowno i spokojnie, az do konca strony. Potem robie \
przerwe, pije wode i wracam do tekstu. Najtrudniejsze sa dlugie zdania, w ktorych trzeba \
rozlozyc oddech na kilka fraz."
ffmpeg -hide_banner -loglevel error -y -i /tmp/mowa/mowa-22k.wav -ar 48000 /tmp/mowa/mowa-czysta.wav

# ---- testy: nazwa i argumenty ------------------------------------------------
TESTY=(
  "lamp-test"
  "dynamics-test"
  "restore-test"
  "prosody-test 107 114"
  "setup-test"
  "montage-test"
  "deesser-test"
  "tame-test"
  "monitor-test /tmp/mowa.wav"
  "kanal-test /tmp/mowa.wav"
  "decide-test /tmp/mowa.wav"
  "oddechy-test /tmp/mowa.wav"
  "prompter-test t2.txt t2.wav"
  "wersy-test"
  "start-test"
  "odsluch-test /tmp/mowa.wav"
  "powtorz-test"
  "wyglad-test"
  "sledzenie-test"
  "mowa16k-test"
  "wejscie-test"
  "czasy-wersow-test"
  "punkt-ciecia-test"
  "punch-test"
  "preroll-test"
  "znaczki-test"
  "lista-ciec-test"
  "zapis-test"
  "odzysk-test"
  "dziennik-test"
  "tekst-test"
  "tor-test"
  "profil-test /tmp/mowa.wav"
  "podklad-montaz-test"
)

ZNANE=$(grep -v '^#' znane-bledy.txt 2>/dev/null | awk '{print $1}')
zle=0

{
  echo "| test | wynik | sprawdzen OK | zle |"
  echo "|---|---|---|---|"
} > "$OUT"

for t in "${TESTY[@]}"; do
  name=${t%% *}
  args=${t#"$name"}

  # test z innej galezi (jeszcze nie scalony) — po prostu go tu nie ma
  if [ ! -f "$name.cpp" ]; then
    echo "| $name | brak na tej galezi | | |" >> "$OUT"; continue
  fi

  if ! $CXX $FLAGS "$name.cpp" -o "$name" 2> "/tmp/$name.kompilacja"; then
    echo "| $name | **nie kompiluje sie** | | |" >> "$OUT"; zle=1; continue
  fi

  # shellcheck disable=SC2086
  ./$name $args > "/tmp/$name.log" 2>&1
  kod=$?
  ok=$(grep -cE '^\s+OK\b' "/tmp/$name.log")
  bad=$(grep -cE '^\s+(ZLE|BLAD)\b' "/tmp/$name.log")

  znany=0
  for z in $ZNANE; do [ "$z" = "$name" ] && znany=1; done

  if [ $kod -eq 0 ] && [ $znany -eq 1 ]; then
    wynik="**przechodzi — usun z znane-bledy.txt**"; zle=1
  elif [ $kod -eq 0 ]; then
    wynik="przechodzi"
  elif [ $znany -eq 1 ]; then
    wynik="znany blad (jak na main)"
  else
    wynik="**NIE PRZECHODZI**"; zle=1
  fi

  echo "| $name | $wynik | $ok | $bad |" >> "$OUT"
done

# glosnosc wzgledem niezaleznej implementacji
# Swiezy katalog: stare pliki z poprzedniego przebiegu nie moga udawac
# wyniku tego (tak ukrywal sie brak kompilacji loudness-test ponizej).
rm -rf /tmp/lufs; : > /tmp/lufs-nasze.txt
$CXX $FLAGS loudness-test.cpp -o loudness-test 2> /tmp/loudness-test.kompilacja \
  || cat /tmp/loudness-test.kompilacja > /tmp/lufs-nasze.txt
./loudness-test /tmp/lufs >> /tmp/lufs-nasze.txt 2>&1 || true
if python3 loudness-check.py /tmp/lufs > /tmp/lufs.log 2>&1 && grep -q "^zgodne" /tmp/lufs.log; then
  echo "| loudness-check.py (pyloudnorm) | przechodzi | | |" >> "$OUT"
else
  echo "| loudness-check.py (pyloudnorm) | **NIE PRZECHODZI** | | |" >> "$OUT"; zle=1
fi

cat "$OUT"

if [ $zle -ne 0 ]; then
  if ! grep -q "^zgodne" /tmp/lufs.log 2>/dev/null; then
    echo; echo "== loudness-check.py"; tail -20 /tmp/lufs.log 2>/dev/null; tail -5 /tmp/lufs-nasze.txt 2>/dev/null
  fi
  echo; echo "Wyniki logow: /tmp/<test>.log"
  for f in /tmp/*-test.log; do
    n=$(basename "$f" .log)
    grep -qE '^\s+(ZLE|BLAD)\b' "$f" && { echo "== $n"; grep -E '^\s+(ZLE|BLAD)\b' "$f"; }
  done
fi

exit $zle
