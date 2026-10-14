#!/bin/bash
# juce-testy.sh — testy, ktore potrzebuja JUCE (pliki, ustawienia, panel ciec,
# prawdziwy Recorder). ci.sh ich nie puszcza, bo buduje tylko g++ bez JUCE.
#
# Uzycie: bash Tests/juce-testy.sh <folder budowania CMake>
# Folder musi miec zbudowane cele: UstawieniaTest PunchPlikTest
# CieciaPanelTest RuntimeTest. Tabela trafia do $CI_PODSUMOWANIE (jesli jest).
# Konczy sie bledem, gdy ktorykolwiek test nie przechodzi.

BUILD=${1:?podaj folder budowania, np. build}
OUT=${CI_PODSUMOWANIE:-/tmp/zs-juce-podsumowanie.md}
TMP=$(mktemp -d)
zle=0

{
  echo "| test JUCE | wynik |"
  echo "|---|---|"
} > "$OUT"

for t in UstawieniaTest PunchPlikTest CieciaPanelTest RuntimeTest; do
  exe="$BUILD/${t}_artefacts/Release/$t"
  [ -x "$exe" ] || exe="$BUILD/${t}_artefacts/$t"
  if [ ! -x "$exe" ]; then
    echo "| $t | **nie zbudowany** |" >> "$OUT"; zle=1; continue
  fi
  args=()
  [ "$t" = RuntimeTest ] && { mkdir -p "$TMP/runtime"; args=("$TMP/runtime"); }

  echo "== $t"
  if timeout 600 "$exe" "${args[@]}" > "$TMP/$t.log" 2>&1; then
    echo "| $t | przechodzi |" >> "$OUT"
  else
    echo "| $t | **NIE PRZECHODZI** |" >> "$OUT"; zle=1
  fi
  cat "$TMP/$t.log"
done

rm -rf "$TMP"
cat "$OUT"
exit $zle
