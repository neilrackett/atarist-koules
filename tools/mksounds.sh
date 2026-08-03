#!/bin/sh
# Build the ST sample set from upstream's headerless .raw sounds.
#
#   tools/mksounds.sh [STDLCONV]
#
# Upstream ships sounds/*.raw: headerless unsigned 8-bit mono PCM at
# 8000 Hz (sounds/auhead is a RIFF header template for creator1, the
# only clue to the format).  STDL's converter wants a real WAV, so
# each payload gets a 44-byte canonical header prepended and is then
# resampled to 6258 Hz - the lowest exact STE DMA rate, chosen
# because it is the only one that shrinks the set (157K -> 120K) and
# 8000 Hz source has nothing above 4 kHz to lose anyway.
#
# Output is tracked in assets/ and copied into dist/ by the Makefile.
set -e

HERE=$(dirname "$0")
ROOT=$(cd "$HERE/.." && pwd)
STDLCONV=${1:-$ROOT/../atarist-stdl/tools/stdlconv/stdlconv.py}
RATE=6258
SRC=$ROOT/sounds
OUT=$ROOT/assets
TMP=${TMPDIR:-/tmp}/koules-mksounds.$$

[ -f "$STDLCONV" ] || { echo "no stdlconv at $STDLCONV" >&2; exit 1; }
mkdir -p "$OUT" "$TMP"
trap 'rm -rf "$TMP"' EXIT

# raw name -> GEMDOS 8.3 upper-case output name.  The sound ids in
# koules.h (S_START..S_CREATOR2) index this list in order, matching
# FILENAME[] in the upstream sound servers.
set -- \
    start:START       end:END           colize:COLIZE \
    destroy1:DESTROY1 destroy2:DESTROY2 \
    creator1:CREATOR1 creator2:CREATOR2

for pair in "$@"; do
    raw=${pair%%:*}
    dos=${pair##*:}
    python3 -c '
import struct, sys
data = open(sys.argv[1], "rb").read()
with open(sys.argv[2], "wb") as f:
    f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE")
    f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, 8000, 8000, 1, 8))
    f.write(b"data" + struct.pack("<I", len(data)) + data)
' "$SRC/$raw.raw" "$TMP/$raw.wav"
    python3 "$STDLCONV" wav "$TMP/$raw.wav" "$OUT/$dos.WAV" --rate $RATE
done

echo "--- assets/ ---"
ls -l "$OUT"
