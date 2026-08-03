#!/bin/bash
# STDL - Planar Display Library for Atari ST
# Copyright (C) 2026 Neil Rackett
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# On-target test driver: boots a .TOS program in Hatari with the
# console captured, then executes a ;-separated command script
# against the emulator's command fifo.
#
#   tests/hatari/run.sh NAME PROG.TOS BOOT_WAIT "cmd;cmd;..."
#
# Commands:
#   sleep N       wait N seconds (fractions ok)
#   waitfor STR   poll the console log for STR (stderr prints are
#                 immediate; stdout is line-buffered)
#   shot          screenshot -> tests/hatari/out/shots_NAME/
#   key VAL       press+release an ST scancode (0x01 = ESC) or a
#                 single alphanumeric character
#   keydown VAL / keyup VAL
#   click         double left mouse click
#   text STR      type a string
#   recsound      start recording emulated audio to $WAV
#   recstop       stop recording
#
# Console output lands in tests/hatari/out/NAME.log and is echoed
# at the end. Environment overrides:
#   HATARI  emulator binary   (default: mac app bundle path)
#   TOS     TOS/EmuTOS image  (required if no usable default)
#   FF      fast-forward on|off (default on; use off when taking
#           timed screenshots or recording sound)
#   MACHINE hatari machine type (default megaste; use st for the
#           8MHz correctness floor)
#   SOUND   sound frequency or off (default off; recsound needs it
#           on - 50066 matches the fastest STE DMA rate)
#   WAV     where recsound writes. Hatari takes this from the
#           szYMCaptureFileName config key, so setting it generates
#           an additional --configfile rather than a command line
#           option, which Hatari does not have.
#   EXTRA   extra hatari options
set -u
NAME=$1
PROG=$2
BOOT_WAIT=$3
SCRIPT=$4

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT=$HERE/out
HATARI=${HATARI:-/Applications/Hatari.app/Contents/MacOS/hatari}
TOS=${TOS:?set TOS to a TOS/EmuTOS image path}
FF=${FF:-on}
MACHINE=${MACHINE:-megaste}
SOUND=${SOUND:-off}
WAV=${WAV:-}
FIFO=$OUT/fifo_$NAME
SHOTDIR=$OUT/shots_$NAME
LOG=$OUT/$NAME.log

rm -rf "$SHOTDIR" "$FIFO" "$LOG"
mkdir -p "$SHOTDIR"

SNDCFG=
if [ -n "$WAV" ]; then
  rm -f "$WAV"
  SNDCFG=$OUT/sound_$NAME.cfg
  printf '[Sound]\nszYMCaptureFileName = %s\n' "$WAV" > "$SNDCFG"
  SNDCFG="--configfile $SNDCFG"
fi

"$HATARI" --tos "$TOS" --machine "$MACHINE" --fast-forward "$FF" \
  --fast-boot on ${EXTRA:-} --sound "$SOUND" $SNDCFG --statusbar off \
  --conout 2 --cmd-fifo "$FIFO" \
  --screenshot-dir "$SHOTDIR" --screenshot-format png \
  "$PROG" > "$LOG" 2>"$OUT/$NAME.err" &
HPID=$!

for i in $(seq 1 50); do
  [ -p "$FIFO" ] && break
  sleep 0.2
done

sleep "$BOOT_WAIT"

IFS=';' read -ra CMDS <<< "$SCRIPT"
for cmd in "${CMDS[@]}"; do
  set -- $cmd
  case $1 in
    sleep|wait) sleep "$2" ;;
    shot)       echo "hatari-shortcut screenshot" > "$FIFO" ;;
    key)        echo "hatari-event keypress $2" > "$FIFO" ;;
    keydown)    echo "hatari-event keydown $2" > "$FIFO" ;;
    keyup)      echo "hatari-event keyup $2" > "$FIFO" ;;
    text)       shift; echo "hatari-event text $*" > "$FIFO" ;;
    click)      echo "hatari-event doubleclick" > "$FIFO" ;;
    recsound)   echo "hatari-shortcut recsound" > "$FIFO" ;;
    recstop)    echo "hatari-shortcut recsound" > "$FIFO" ;;
    waitfor)    shift
                for i in $(seq 1 240); do
                  grep -q "$*" "$LOG" && break
                  sleep 0.5
                done ;;
    *)          echo "unknown cmd: $cmd" >&2 ;;
  esac
done

sleep 2
kill $HPID 2>/dev/null
wait $HPID 2>/dev/null
echo "=== $NAME console output ==="
cat "$LOG"
echo "=== screenshots ==="
ls "$SHOTDIR" 2>/dev/null
