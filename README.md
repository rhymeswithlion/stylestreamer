# StyleStreamer

A real-time music generation plugin for Apple Silicon Macs, built on the
[Magenta RealTime](https://github.com/magenta/magenta-realtime) model stack
(SpectroStream + MusicCoCa + Depthformer) and an optimized C++ MLX runtime.

https://github.com/user-attachments/assets/11d24e52-224c-493e-92c3-2ac8bf91f79a

**Minimum macOS: 26.0** (matches the bundled `libmlx.dylib`)

---

## What it does

StyleStreamer generates continuous music audio in configurable chunks conditioned
on up to four text style prompts, blended by weight. Chunk length is adjustable
from 40 ms (one codec frame) up to 2 seconds; the default is 400 ms, balancing
generation overhead against playback latency. You drag style cards
between a live mixing row and a scrollable card bank, edit prompts inline, and
copy/paste the full mix state as base64 JSON for external storage. A Downtempo
toggle plays generated audio at a reduced effective rate (32 kHz source at 44.1 kHz
playback), producing a slower, lower-pitched texture with a smooth sigmoid ramp
on toggle to avoid crackles.

---

## Model architecture

| Component | Purpose | In / Out |
|---|---|---|
| **SpectroStream** | Audio codec | Stereo audio ↔ 64-RVQ discrete tokens at 25 Hz |
| **MusicCoCa** | Style embedding | Text prompts → 768-dim embedding (12 RVQ) |
| **Depthformer** | Token generation | Context + style tokens → next-chunk tokens |

The generation loop produces audio chunks (40 ms – 2 s, default 400 ms)
conditioned on 10 seconds of context. The C++ MLX runtime runs all three components natively on Apple Silicon
via Metal.

---

## Repository structure

```
magenta-rt-rewrite/
  vendor/
    magenta-realtime-mlx-cpp/   # Optimized C++ MLX runtime (submodule)
  packages/
    magenta-rt-juce/            # StyleStreamer JUCE plugin
      source/                   # PluginProcessor, PluginEditor, engine, UI
      tests/                    # Catch2 unit + snapshot tests
      assets/                   # SVG card backgrounds, images, data
      cmake/                    # Pamplejuce CMake helpers
      JUCE/                     # JUCE framework (submodule)
      modules/                  # JUCE modules (melatonin-inspector, CLAP, etc.)
  scripts/
    bundle_juce_standalone_dylibs_macos.sh
    generate_juce_packaging_icon.sh
  Makefile
  CHANGELOG.md
```

---

## Build

### Prerequisites

- macOS 15.0+ (Sequoia), Apple Silicon
- Xcode command-line tools (`xcode-select --install`)
- CMake ≥ 3.25 and Ninja (`brew install cmake ninja`)
- Homebrew sentencepiece and portaudio (runtime dylibs)

```bash
brew install cmake ninja sentencepiece portaudio
```

### Clone

```bash
git clone --recurse-submodules <repo-url>
cd magenta-rt-rewrite
```

### Configure and build

```bash
# Build standalone app (Release)
make juce-build

# Build and launch
make juce-run
```

Or manually via CMake:

```bash
cmake -S packages/magenta-rt-juce -B packages/magenta-rt-juce/build-mlx \
      -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build packages/magenta-rt-juce/build-mlx --target StyleStreamerJuce_Standalone
```

Use `CMAKE_BUILD_TYPE=Debug make juce-build` for a debug build.

---

## Weights

StyleStreamer downloads model weights from Hugging Face on first use. Click
**Download weights** in the **Advanced…** window — the app installs
`huggingface_hub` into an isolated temporary Python venv (nothing touches system
Python), downloads the snapshot, and fills the weights field with the resolved
path.

Weight search order: `MRT_JUCE_WEIGHTS_DIR` env var → bundled/manual
`model-weights` folders → standard `~/.cache/huggingface/hub` snapshot →
repo `.weights-cache/` fallback.

To pre-populate the cache from the command line:

```bash
make ensure-weights-cache
```

---

## Distribution archive

```bash
make juce-dist
```

Produces `dist/stylestreamer-<VERSION>-macos-arm64.zip`. The archive unpacks to
`StyleStreamer.app/` at the archive root. The bundle includes:

- `Contents/Frameworks/libmlx.dylib` + `mlx.metallib` (from the repo `.venv` or `MLX_ROOT`)
- Homebrew sentencepiece and portaudio dylibs
- `install_name_tool` rewrites and ad-hoc codesign so dyld does not halt with
  "Code Signature Invalid" on the recipient's machine
- A packaged Hugging Face weight helper under `Contents/Resources/weight-helper/`

---

## Tests

```bash
# Build and run all unit tests
cmake --build packages/magenta-rt-juce/build-mlx --target Tests
./packages/magenta-rt-juce/build-mlx/Tests/Tests

# Run a specific tag
./packages/magenta-rt-juce/build-mlx/Tests/Tests "[processor-audio]"

# Capture a PNG snapshot of the plugin editor (requires MRT_JUCE_UI_SNAPSHOT=1)
MRT_JUCE_UI_SNAPSHOT=1 ./packages/magenta-rt-juce/build-mlx/Tests/Tests "[ui]"
```

Snapshot images are written to `output/<date>-juce-editor-snapshot/plugin-editor.png`
(default 2× scale; set `MRT_JUCE_UI_SNAPSHOT_SCALE=1` for 900×900).

---

## Runtime optimizations

The C++ MLX runtime (`vendor/magenta-realtime-mlx-cpp`) employs several
optimizations to approach real-time throughput on Apple Silicon:

- **Fused Metal attention** — `mx::fast::scaled_dot_product_attention` fuses
  softmax and QKV matmuls into a single Metal kernel across all three transformer
  stacks (SpectroStream encoder, Depthformer encoder, temporal and depth decoders).
- **`mx::compile` on fixed-shape paths** — the Depthformer encoder and selected
  decode steps are compiled via `mx::compile`, reducing Metal kernel dispatch
  overhead significantly for the 800-step autoregressive loop.
- **MLXFN precompiled function bundles** — when enabled (default), the runtime
  loads a precompiled `mlxfn/` bundle (encode + depth + temporal) that bypasses
  Python-level graph tracing on every chunk, giving the largest single latency win.
  The **Advanced…** window reports an error on load if the bundle is missing for
  the chosen tag/dtype.
- **Speculative depth decoding** — for chunks after the first, draft tokens are
  built from the previous chunk's tokens at the same frame position (exploiting
  temporal continuity in music), verified in a single causal forward pass, and
  accepted up to the first mismatch. Expected acceptance rate 50–75% per depth
  token, reducing sequential depth steps per frame.
- **Vocab masks pre-evaluated** — per-RVQ-level codec token masks are computed
  once before the generation loop and held as MLX arrays, avoiding lazy-graph
  bloat during autoregressive decode.
- **Minimized `mx::eval` calls** — eval boundaries are placed only where a
  concrete value is required for the next input (e.g., sampled token IDs), not
  after every operation.

---

## Author

Brian Cruz ([@rhymeswithlion](https://github.com/rhymeswithlion))

## License

StyleStreamer source code is licensed under the [Apache License, Version 2.0](LICENSE).

This project incorporates or links against several third-party components with
their own licenses. See [NOTICE](NOTICE) for the full list, including:

- **Magenta RealTime** model architecture and weights — Apache 2.0 / CC-BY 4.0 (Google)
- **JUCE framework** — AGPL v3 (open source) or JUCE 8 Commercial Licence
- **Apple MLX** (`libmlx.dylib`) — MIT License (Apple Inc.)
- **SentencePiece** — Apache 2.0 (Google)
- **PortAudio** — MIT License
