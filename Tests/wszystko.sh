#!/bin/bash
# wszystko.sh — caly zestaw testow. Zaden nie potrzebuje mikrofonu ani karty.
set -e
cd "$(dirname "$0")"
CXX=${CXX:-g++}
FLAGS="-O2 -std=c++17 -I../Source"

$CXX $FLAGS lamp-test.cpp      -o lamp-test
$CXX $FLAGS dynamics-test.cpp  -o dynamics-test
$CXX $FLAGS loudness-test.cpp  -o loudness-test
$CXX $FLAGS restore-test.cpp   -o restore-test
$CXX $FLAGS prosody-test.cpp   -o prosody-test
$CXX $FLAGS setup-test.cpp     -o setup-test
$CXX $FLAGS montage-test.cpp   -o montage-test
$CXX $FLAGS deesser-test.cpp   -o deesser-test
$CXX $FLAGS tame-test.cpp      -o tame-test
$CXX $FLAGS monitor-test.cpp   -o monitor-test
$CXX $FLAGS kanal-test.cpp     -o kanal-test
$CXX $FLAGS decide-test.cpp    -o decide-test
$CXX $FLAGS prompter-test.cpp  -o prompter-test
$CXX $FLAGS wersy-test.cpp     -o wersy-test
$CXX $FLAGS start-test.cpp     -o start-test
$CXX $FLAGS odsluch-test.cpp   -o odsluch-test
$CXX $FLAGS powtorz-test.cpp   -o powtorz-test
$CXX $FLAGS wyglad-test.cpp    -o wyglad-test
$CXX $FLAGS sledzenie-test.cpp -o sledzenie-test

./lamp-test
./dynamics-test
./restore-test
./prosody-test 107 114
espeak-ng -v pl -s 150 -w /tmp/mowa.wav \
  "Dzisiaj wieczorem nagramy dwa zdania normalnym glosem. Sprawdzam poziom i odleglosc od mikrofonu."
./setup-test
./montage-test
./deesser-test
./tame-test
./monitor-test /tmp/mowa.wav
./kanal-test /tmp/mowa.wav
./decide-test /tmp/mowa.wav
# t2.wav nie jest w repo (*.wav w .gitignore) — syntezator robi go z t2.txt
[ -f t2.wav ] || espeak-ng -v pl -s 150 -w t2.wav -f t2.txt
./prompter-test t2.txt t2.wav
./wersy-test
./start-test
./odsluch-test /tmp/mowa.wav
./powtorz-test
./wyglad-test
./sledzenie-test

# Pamietanie ustawien — jedyny test, ktory potrzebuje JUCE i prawdziwego pliku,
# wiec idzie przez CMake i Wine. Gdy go nie zbudowano, mowimy to wprost zamiast
# po cichu przepuscic caly zestaw.
if [ -x ../build-win/UstawieniaTest_artefacts/Release/UstawieniaTest.exe ]; then
    (cd ../build-win && wine UstawieniaTest_artefacts/Release/UstawieniaTest.exe 2>/dev/null)
else
    echo "ustawienia-test: NIE ZBUDOWANY (ninja w build-win) — nie sprawdzono"
fi

./loudness-test /tmp/lufs > /tmp/lufs-nasze.txt
echo
echo "Glosnosc — porownanie z niezalezna implementacja"
echo "==============================================="
python3 loudness-check.py /tmp/lufs
