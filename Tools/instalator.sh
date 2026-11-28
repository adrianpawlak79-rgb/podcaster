#!/bin/bash
# instalator.sh — sklada instalator Windows (NSIS) na Linuksie.
#
#   bash Tools/instalator.sh "KATALOG_Z_EXE" [WYNIK.exe]
#
# KATALOG_Z_EXE: tam, gdzie lezy "Zielone swiatlo.exe" (build mingw, np.
# build-win/ZieloneSwiatlo_artefacts/Release). Pobiera (raz, do ~/.cache/zs)
# biblioteke Vosk dla Windows i polski model, sprawdza ich sumy SHA-256,
# sklada wszystko w jednym katalogu i uruchamia makensis z
# Installer/zielone-swiatlo.nsi.
#
# Potrzebne: makensis (pakiet nsis), curl, unzip, sha256sum.

set -euo pipefail

EXE_DIR=${1:?podaj katalog z "Zielone swiatlo.exe"}
TU=$(cd "$(dirname "$0")/.." && pwd)
WERSJA=$(sed -n 's/^project(ZieloneSwiatlo VERSION \([0-9.]*\).*/\1/p' "$TU/CMakeLists.txt")
WYNIK=${2:-$TU/Zielone-swiatlo-$WERSJA-instalator.exe}

VOSK_ZIP=https://github.com/alphacep/vosk-api/releases/download/v0.3.45/vosk-win64-0.3.45.zip
MODEL_ZIP=https://alphacephei.com/vosk/models/vosk-model-small-pl-0.22.zip
# Sumy SHA-256 pobieranych paczek. Zmiana wersji Vosk/modelu = nowe sumy tutaj.
VOSK_SHA256=f1dcc9cca460630f81ea8f71794f69c80bed6556d2a4e6237b5785e1d2dff34b
MODEL_SHA256=c4cd16498ea544f446f9e9a55cbd602b71cfe5a2b6f2b0834d81e1b6fce15f0d
CACHE=${ZS_CACHE:-$HOME/.cache/zs}
mkdir -p "$CACHE"

sprawdz() {   # plik suma — sukces, gdy suma sie zgadza
  [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$2" ]
}
pobierz() {   # url plik suma
  if [ -s "$2" ] && sprawdz "$2" "$3"; then return 0; fi
  rm -f "$2"   # brak, uszkodzony albo podmieniony plik z pamieci podrecznej
  curl -fsSL --retry 4 -o "$2.tmp" "$1"
  if ! sprawdz "$2.tmp" "$3"; then
    rm -f "$2.tmp"
    echo "BLAD: suma SHA-256 pliku z $1 sie nie zgadza (oczekiwano $3)" >&2
    exit 1
  fi
  mv "$2.tmp" "$2"
}
pobierz "$VOSK_ZIP" "$CACHE/vosk-win64-0.3.45.zip" "$VOSK_SHA256"
pobierz "$MODEL_ZIP" "$CACHE/vosk-model-small-pl-0.22.zip" "$MODEL_SHA256"

ZRODLO=$(mktemp -d)
trap 'rm -rf "$ZRODLO"' EXIT

cp "$EXE_DIR/Zielone swiatlo.exe" "$ZRODLO/"
unzip -q -j "$CACHE/vosk-win64-0.3.45.zip" '*/libvosk.dll' '*/libgcc_s_seh-1.dll' '*/libstdc++-6.dll' '*/libwinpthread-1.dll' -d "$ZRODLO"
unzip -q "$CACHE/vosk-model-small-pl-0.22.zip" -d "$ZRODLO"
[ -f "$ZRODLO/vosk-model-small-pl-0.22/graph/Gr.fst" ] || { echo "model niekompletny" >&2; exit 1; }

# LICENCJE.txt: opis skladnikow + pelne teksty Apache 2.0 i SIL OFL 1.1
# (Apache 2.0 z Installer/licencje, OFL z Resources/Fonts — te same pliki,
# ktore leza obok czcionek w repozytorium).
LIC="$TU/Installer/licencje"
LINIA="====================================================================="
{
cat <<'KONIEC'
Zielone swiatlo - skladniki i licencje
======================================

Program "Zielone swiatlo" korzysta z ponizszych skladnikow. Pelne teksty
licencji Apache 2.0 i SIL OFL 1.1 sa ponizej.

1. JUCE 7 (https://juce.com)
   Program jest zbudowany na JUCE 7 w ramach darmowej licencji JUCE Personal
   (JUCE 7 End User Licence Agreement, https://juce.com/juce-7-licence).
   Zgodnie z nia program przy uruchomieniu pokazuje ekran "Made with JUCE".
   Moduly juce_core, juce_events, juce_audio_basics i juce_audio_devices
   sa na licencji ISC.

2. Vosk (libvosk.dll, https://alphacephei.com/vosk) - Apache License 2.0
   (pelny tekst nizej).

3. Model vosk-model-small-pl-0.22 (Alpha Cephei) - Apache License 2.0
   (pelny tekst nizej).

4. Biblioteki MinGW-w64 (libgcc_s_seh-1.dll, libstdc++-6.dll,
   libwinpthread-1.dll) - GPL z wyjatkiem GCC Runtime Library Exception
   oraz licencje MinGW-w64.

5. Czcionki Inter (Copyright (c) 2016 The Inter Project Authors) i
   JetBrains Mono (Copyright 2020 The JetBrains Mono Project Authors),
   wbudowane w program - SIL Open Font License 1.1 (pelny tekst nizej).

KONIEC
printf '\n%s\nApache License 2.0 (Vosk, model vosk-model-small-pl-0.22)\n%s\n\n' "$LINIA" "$LINIA"
cat "$LIC/Apache-2.0.txt"
printf '\n\n%s\nSIL Open Font License 1.1 - Inter\n%s\n\n' "$LINIA" "$LINIA"
cat "$TU/Resources/Fonts/OFL.txt"
printf '\n\n%s\nSIL Open Font License 1.1 - JetBrains Mono\n%s\n\n' "$LINIA" "$LINIA"
cat "$TU/Resources/Fonts/OFL-JetBrainsMono.txt"
} > "$ZRODLO/LICENCJE.txt"

makensis -V2 -INPUTCHARSET UTF8 -DZRODLO="$ZRODLO" -DWERSJA="$WERSJA" -DWYNIK="$WYNIK" "$TU/Installer/zielone-swiatlo.nsi"
echo "instalator: $WYNIK ($(du -h "$WYNIK" | cut -f1))"
