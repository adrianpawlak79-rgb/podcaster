#!/bin/bash
# instalator.sh — sklada instalator Windows (NSIS) na Linuksie.
#
#   bash Tools/instalator.sh "KATALOG_Z_EXE" [WYNIK.exe]
#
# KATALOG_Z_EXE: tam, gdzie lezy "Zielone swiatlo.exe" (build mingw, np.
# build-win/ZieloneSwiatlo_artefacts/Release). Pobiera (raz, do ~/.cache/zs)
# biblioteke Vosk dla Windows i polski model, sklada wszystko w jednym
# katalogu i uruchamia makensis z Installer/zielone-swiatlo.nsi.
#
# Potrzebne: makensis (pakiet nsis), curl, unzip.

set -euo pipefail

EXE_DIR=${1:?podaj katalog z "Zielone swiatlo.exe"}
TU=$(cd "$(dirname "$0")/.." && pwd)
WERSJA=$(sed -n 's/^project(ZieloneSwiatlo VERSION \([0-9.]*\).*/\1/p' "$TU/CMakeLists.txt")
WYNIK=${2:-$TU/Zielone-swiatlo-$WERSJA-instalator.exe}

VOSK_ZIP=https://github.com/alphacep/vosk-api/releases/download/v0.3.45/vosk-win64-0.3.45.zip
MODEL_ZIP=https://alphacephei.com/vosk/models/vosk-model-small-pl-0.22.zip
CACHE=${ZS_CACHE:-$HOME/.cache/zs}
mkdir -p "$CACHE"

pobierz() {   # url plik
  [ -s "$2" ] && return 0
  curl -fsSL --retry 4 -o "$2.tmp" "$1"
  mv "$2.tmp" "$2"
}
pobierz "$VOSK_ZIP" "$CACHE/vosk-win64-0.3.45.zip"
pobierz "$MODEL_ZIP" "$CACHE/vosk-model-small-pl-0.22.zip"

ZRODLO=$(mktemp -d)
trap 'rm -rf "$ZRODLO"' EXIT

cp "$EXE_DIR/Zielone swiatlo.exe" "$ZRODLO/"
unzip -q -j "$CACHE/vosk-win64-0.3.45.zip" '*/libvosk.dll' '*/libgcc_s_seh-1.dll' '*/libstdc++-6.dll' '*/libwinpthread-1.dll' -d "$ZRODLO"
unzip -q "$CACHE/vosk-model-small-pl-0.22.zip" -d "$ZRODLO"
[ -f "$ZRODLO/vosk-model-small-pl-0.22/graph/Gr.fst" ] || { echo "model niekompletny" >&2; exit 1; }

cat > "$ZRODLO/LICENCJE.txt" <<'EOF'
Zielone swiatlo - skladniki
===========================

JUCE 7 (https://juce.com) - licencja JUCE / GPLv3.
Vosk (libvosk.dll, https://alphacephei.com/vosk) - Apache License 2.0.
Model vosk-model-small-pl-0.22 (Alpha Cephei) - Apache License 2.0.
Biblioteki MinGW-w64 (libgcc, libstdc++, winpthread) - GPL z wyjatkiem
GCC Runtime Library Exception / licencje MinGW-w64.
EOF

makensis -V2 -INPUTCHARSET UTF8 -DZRODLO="$ZRODLO" -DWERSJA="$WERSJA" -DWYNIK="$WYNIK" "$TU/Installer/zielone-swiatlo.nsi"
echo "instalator: $WYNIK ($(du -h "$WYNIK" | cut -f1))"
