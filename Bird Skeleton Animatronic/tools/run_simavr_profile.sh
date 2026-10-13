#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH="$ROOT/firmware/BirdSkeletonFixture"
ELF="$SKETCH/build/MiniCore.avr.8/BirdSkeletonFixture.ino.elf"
HEX="$SKETCH/build/MiniCore.avr.8/BirdSkeletonFixture.ino.hex"
PROFILER="${TMPDIR:-/tmp}/bird-simavr-profile"

arduino-cli compile \
  --fqbn 'MiniCore:avr:8:clock=16MHz_external,BOD=2v7,eeprom=keep,LTO=Os_flto,bootloader=uart0,baudrate=default' \
  --libraries /home/aidancrowther/Arduino/libraries \
  --export-binaries \
  "$SKETCH"

cc -O2 -Wall -Wextra \
  $(pkg-config --cflags simavr) \
  "$ROOT/tools/simavr_profile.c" \
  -o "$PROFILER" \
  $(pkg-config --libs simavr)

echo "=== cooperative DMX source ==="
"$PROFILER" "$HEX" --cooperative

echo
echo "=== fixed-rate DMX source ==="
echo "simavr UART overrun diagnostics are expected in this mode."
"$PROFILER" "$HEX" --fixed