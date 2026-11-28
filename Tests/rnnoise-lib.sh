#!/bin/bash
# rnnoise-lib.sh <katalog wyjscia> — buduje ThirdParty/rnnoise jako librnnoise.a
# dla testow (wszystko.sh, ci.sh). Program buduje to samo przez CMakeLists.txt.
# Wypisuje flagi do dopisania przy kompilacji testu:
#   -DZS_RNNOISE=1 <katalog>/librnnoise.a
set -e
OUTDIR=${1:-/tmp/zs-rnnoise}
CC=${CC:-gcc}
AR=${AR:-ar}
TP="$(cd "$(dirname "$0")/../ThirdParty/rnnoise" && pwd)"
mkdir -p "$OUTDIR"
CF="-O2 -DUSE_WEIGHTS_FILE -DDISABLE_DEBUG_FLOAT=1 -I$TP/include -I$TP/src"
objs=()
for f in denoise rnn pitch kiss_fft celt_lpc nnet nnet_default parse_lpcnet_weights rnnoise_tables rnnoise_data; do
  $CC $CF -c "$TP/src/$f.c" -o "$OUTDIR/$f.o"
  objs+=("$OUTDIR/$f.o")
done
$CC $CF "-DZS_RNNOISE_BLOB=\"$TP/rnnoise_weights.bin\"" -c "$TP/src/zs_model.c" -o "$OUTDIR/zs_model.o"
objs+=("$OUTDIR/zs_model.o")
rm -f "$OUTDIR/librnnoise.a"
$AR rcs "$OUTDIR/librnnoise.a" "${objs[@]}"
echo "-DZS_RNNOISE=1 $OUTDIR/librnnoise.a"
