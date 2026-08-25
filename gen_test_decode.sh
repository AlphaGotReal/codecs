#!/usr/bin/env bash
set -euo pipefail

GOP_SIZES=(250 100 60 50 30 20 10 3)
ROOT="$(cd "$(dirname "$0")" && pwd)"
BIN="$ROOT/build/test/random"

make -C "$ROOT/build" random

for G in "${GOP_SIZES[@]}"; do
  echo "=== GOP ${G} ==="
  "$BIN" "$ROOT/vids/test_decode_${G}.mp4" 640 480 30 1000 libx264 medium 23 "$G"
done
