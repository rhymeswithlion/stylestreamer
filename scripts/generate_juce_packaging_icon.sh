#!/usr/bin/env bash
# Regenerate packages/magenta-rt-juce/packaging/icon.png (1024²) from repo-root ``stylestreamer.png``.
# macOS only (``sips``):
# - Landscape (wider than tall): square = full **height**, centred horizontally (trim side margins).
# - Portrait (taller than wide): square = full **width**, centred vertically (trim top/bottom).
set -euo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd)
SRC="${REPO}/stylestreamer.png"
OUT="${REPO}/packages/magenta-rt-juce/packaging/icon.png"
if [[ ! -f "$SRC" ]]; then
  echo "error: missing source $SRC" >&2
  exit 1
fi
W=$(sips -g pixelWidth "$SRC" | awk '/pixelWidth:/ {print $2}')
H=$(sips -g pixelHeight "$SRC" | awk '/pixelHeight:/ {print $2}')
if (( W >= H )); then
  SIDE=$H
  note="full-height ${SIDE}² (sides trimmed)"
else
  SIDE=$W
  note="full-width ${SIDE}² (top/bottom trimmed)"
fi
TMP=$(mktemp /tmp/gen-juce-packaging-icon.XXXXXX.png)
cleanup() { rm -f "$TMP"; }
trap cleanup EXIT
sips -c "$SIDE" "$SIDE" "$SRC" -o "$TMP" >/dev/null
sips -z 1024 1024 "$TMP" -o "$OUT" >/dev/null
echo "wrote ${OUT} (1024², ${note}, from ${W}x${H})"
