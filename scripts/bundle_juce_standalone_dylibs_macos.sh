#!/usr/bin/env bash
# Bundle third-party dylibs into a StyleStreamer Standalone .app for distribution.
# Run on the *build* machine (needs Homebrew sentencepiece + portaudio; MLX from .venv or MLX_ROOT).
#
# ``install_name_tool`` invalidates embedded signatures; without re-signing, dyld exits with
# CODESIGNING / Invalid Page on recent macOS. We apply **ad hoc** signing to the bundle when
# ``codesign`` is available. Set ``MRT_SKIP_CODESIGN=1`` to skip (debug only).
set -euo pipefail

usage() {
  echo "usage: $0 /path/to/StyleStreamer.app" >&2
  exit 2
}

[[ "${1:-}" ]] || usage
APP=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
EXE="$APP/Contents/MacOS/StyleStreamer"
FW="$APP/Contents/Frameworks"
REL="@executable_path/../Frameworks"

if [[ ! -d "$APP" || ! -x "$EXE" ]]; then
  echo "error: expected app with executable at $EXE" >&2
  exit 1
fi

REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)

MLX_DYLIB=""
if [[ -n "${MLX_ROOT:-}" && -f "${MLX_ROOT}/lib/libmlx.dylib" ]]; then
  MLX_DYLIB="${MLX_ROOT}/lib/libmlx.dylib"
else
  # shellcheck disable=SC2012
  MLX_DYLIB=$(ls -1 "$REPO_ROOT"/.venv/lib/python*/site-packages/mlx/lib/libmlx.dylib 2>/dev/null | head -1 || true)
fi
if [[ -z "$MLX_DYLIB" || ! -f "$MLX_DYLIB" ]]; then
  echo "error: libmlx.dylib not found (uv sync and MLX wheel, or set MLX_ROOT)" >&2
  exit 1
fi
MLX_LIB_DIR=$(cd "$(dirname "$MLX_DYLIB")" && pwd)
MLX_METALLIB="$MLX_LIB_DIR/mlx.metallib"
if [[ ! -f "$MLX_METALLIB" ]]; then
  echo "error: mlx.metallib not found next to libmlx.dylib at $MLX_LIB_DIR" >&2
  exit 1
fi

if ! command -v brew >/dev/null 2>&1; then
  echo "error: Homebrew not found; need brew install sentencepiece portaudio on this Mac" >&2
  exit 1
fi
SP_LIB="$(brew --prefix sentencepiece)/lib/libsentencepiece.0.dylib"
PA_LIB="$(brew --prefix portaudio)/lib/libportaudio.2.dylib"
for f in "$SP_LIB" "$PA_LIB"; do
  if [[ ! -f "$f" ]]; then
    echo "error: missing $f (brew install sentencepiece portaudio)" >&2
    exit 1
  fi
done

mkdir -p "$FW"
cp -f "$MLX_DYLIB" "$FW/libmlx.dylib"
cp -f "$SP_LIB" "$FW/libsentencepiece.0.dylib"
cp -f "$PA_LIB" "$FW/libportaudio.2.dylib"
cp -f "$MLX_METALLIB" "$FW/mlx.metallib"

# MLX first tries ``mlx.metallib`` next to ``current_binary_dir()``. In wheels that is beside
# ``libmlx.dylib``; in app bundles it may resolve to the executable directory depending on how
# the loader reports the current image. Keep one real copy in Frameworks and expose symlinks for
# both colocated app/executable lookup and the ``Resources/mlx.metallib`` fallback.
ln -sf "../Frameworks/mlx.metallib" "$APP/Contents/MacOS/mlx.metallib"
mkdir -p "$APP/Contents/MacOS/Resources" "$APP/Contents/Resources"
ln -sf "../../Frameworks/mlx.metallib" "$APP/Contents/MacOS/Resources/mlx.metallib"
ln -sf "../Frameworks/mlx.metallib" "$APP/Contents/Resources/mlx.metallib"

install_name_tool -id "@loader_path/libmlx.dylib" "$FW/libmlx.dylib"
install_name_tool -id "@loader_path/libsentencepiece.0.dylib" "$FW/libsentencepiece.0.dylib"
install_name_tool -id "@loader_path/libportaudio.2.dylib" "$FW/libportaudio.2.dylib"

install_name_tool -change "@rpath/libmlx.dylib" "${REL}/libmlx.dylib" "$EXE"

while IFS= read -r line; do
  old=$(echo "$line" | awk '{gsub(/^ +/,""); gsub(/,$/,"",$1); print $1}')
  [[ -z "$old" ]] && continue
  case "$old" in
    /System/*|/usr/lib/*|@executable_path/*|@loader_path/*|@rpath/*)
      continue
      ;;
    *libsentencepiece*.dylib)
      install_name_tool -change "$old" "${REL}/libsentencepiece.0.dylib" "$EXE"
      ;;
    *libportaudio*.dylib)
      install_name_tool -change "$old" "${REL}/libportaudio.2.dylib" "$EXE"
      ;;
  esac
done < <(otool -L "$EXE" | grep $'^\t')

# Strip LC_RPATH entries pointing at the build machine (.venv MLX, etc.).
while IFS= read -r rp; do
  [[ -z "$rp" ]] && continue
  [[ "$rp" == @* ]] && continue
  install_name_tool -delete_rpath "$rp" "$EXE" 2>/dev/null || true
done < <(otool -l "$EXE" | grep -A2 LC_RPATH | sed -n 's/.*path \(.*\) (offset.*)/\1/p')

echo "bundled MLX + Homebrew dylibs/metallib → $FW"

if [[ "${MRT_SKIP_CODESIGN:-0}" == "1" ]]; then
  echo "MRT_SKIP_CODESIGN=1: skipping codesign (app may crash at launch — not for redistribution)" >&2
elif command -v codesign >/dev/null 2>&1; then
  # Ad hoc identity ``-``: satisfies library validation after mutating Mach-O headers.
  codesign --force --deep --sign - "$APP"
  codesign --verify --verbose=2 "$APP" >/dev/null
  echo "ad-hoc codesign --deep verified for $APP"
else
  echo "warning: codesign not found (install Xcode Command Line Tools). Unsigned bundle may SIGKILL under CODESIGNING." >&2
fi
