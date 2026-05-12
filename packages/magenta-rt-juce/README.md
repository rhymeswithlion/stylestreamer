# Magenta RT JUCE

Standalone-first JUCE/Pamplejuce host and local VST3 plugin for the optimized
C++ MLX Magenta RT runtime.

This package starts from the upstream
[Pamplejuce](https://github.com/sudara/pamplejuce) template and keeps its CMake,
JUCE, Catch2, assets, and module layout. The Magenta-specific layer lives under
`source/` and currently provides:

- `engine/MagentaRtJuceEngine.*` — wrapper around `magenta_realtime_mlx::System`
  with prompt selection, generation options, runtime load/reset, and background
  chunk generation.
- `engine/GeneratedAudioQueue.*` — fixed-capacity generated-audio queue drained
  by the JUCE audio callback.
- `engine/PromptPortfolio.*` — weighted prompt slots matching the original
  Magenta RT notebook controls.
- `PluginProcessor.*` and `PluginEditor.*` — Standalone/VST3 processor/editor wiring
  with prompt, seed, sampling, buffering, downtempo playback, weights, dtype, and
  MLXFN controls.

The **Downtempo** switch is playback-only: it leaves generation at 48 kHz, then
uses one JUCE **`WindowedSincInterpolator`** per channel to play the generated
stream at a **32/48** source-rate ratio into normal host-rate buffers. The result
is slower, lower-pitched playback that makes each generated chunk last **1.5x**
longer without changing the model runtime. Toggling the switch ramps the
effective speed ratio over about **0.4 s** using a sigmoid curve so the resampler
does not jump abruptly.

## Compatibility (macOS)

CMake sets **`CMAKE_OSX_DEPLOYMENT_TARGET` to 15.0** (macOS Sequoia) so release
and **`make juce-dist`** packages can run on **15.x** and newer. Reconfigure
after changing `cmake/PamplejuceMacOS.cmake`; pass
`-DCMAKE_OSX_DEPLOYMENT_TARGET=…` at configure time to override.

**Distributable Standalone** (`make juce-dist` from the monorepo root) runs
`scripts/bundle_juce_standalone_dylibs_macos.sh` on the packed `.app`: it copies
**`libmlx.dylib`** and **`mlx.metallib`** from the repo **`.venv`** (or **`MLX_ROOT`**) and
Homebrew **sentencepiece** / **portaudio** into **`Contents/Frameworks/`**. It also creates
symlinks at **`Contents/MacOS/mlx.metallib`**, **`Contents/MacOS/Resources/mlx.metallib`**, and
**`Contents/Resources/mlx.metallib`** because MLX searches colocated **`mlx.metallib`** paths
before falling back to its build-time path. The pack machine must have **`brew install
sentencepiece portaudio`**; recipients do not. **`bundle_juce_standalone_dylibs_macos.sh`**
ends with **ad hoc** **`codesign --force --deep --sign -`** because **`install_name_tool`**
strips prior signatures — without re-signing, **dyld** may terminate with **CODESIGNING /
Invalid Page** on load. Distribution
beyond ad hoc still requires your Apple ID / notarization as usual. Gatekeeper may still prompt
until notarized.

Weights are **not** copied into distributable archives. The Advanced window has
a **Download weights** button that runs the packaged
`packaging/weight-helper/download_hf_weights.py` helper. That helper uses
an isolated temporary Python venv for its dependencies, then calls
`huggingface_hub.snapshot_download()` without `local_dir`, so files land in the
standard Hugging Face cache (`HF_HUB_CACHE`, or `$HF_HOME/hub`, or
`~/.cache/huggingface/hub`) under
`datasets--rhymeswithlion--magenta-realtime-mlx-cpp/snapshots/<commit>/`. Set
`MRT_JUCE_WEIGHTS_DIR` to bypass discovery, `MRT_JUCE_PYTHON` to choose the
Python used to create the helper venv, or `HF_TOKEN` for private dataset access.

On **Linux**, the same **`make juce-dist`** path runs
**`scripts/bundle_juce_standalone_dylibs_linux.sh`**, placing **`./lib/libmlx.so*`** (+ other MLX-wheel **`.so`** files beside it) alongside **`sentencepiece`** / **portaudio** libraries resolved by **`ldd`**, then **`patchelf --set-rpath '$ORIGIN/lib'`** on the **`StyleStreamer`** ELF. The packager must install **`patchelf`** and the dev packages that provide those **`.so`** files; recipients only need a compatible **glibc**/**libstdc++** baseline.

## Build

Run from the repository root:

```bash
cmake -S packages/magenta-rt-juce -B packages/magenta-rt-juce/build-mlx -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build packages/magenta-rt-juce/build-mlx --target StyleStreamerJuce_Standalone
```

The CMake integration expects the repo `.venv` to contain the MLX wheel so it
can find C++ headers, `libmlx.dylib`, and `MLXConfig.cmake`. Run `uv sync` at the
repository root first if needed. Override with `-DMLX_ROOT=/path/to/mlx` for a
custom MLX install.

For the local VST3 development bundle:

```bash
make juce-build-vst3
```

That target builds `StyleStreamerJuce_VST3`, verifies the generated bundle under
`packages/magenta-rt-juce/build-mlx/StyleStreamerJuce_artefacts/Release/VST3/`,
and, on macOS, reports whether JUCE copied it to
`~/Library/Audio/Plug-Ins/VST3/StyleStreamer.vst3` for host scanning. Full
distribution packaging, notarization, and bundled-weight handling remain separate
from this local build path.

**App icon:** `juce_add_plugin` uses **`packaging/icon.png`** (1024×1024 **`ICON_BIG`**).
Hero poster **`stylestreamer.png`** at the monorepo root is the **source** (2475×1536 etc.): the
regenerator takes a **centred square** that keeps the **full height** on wide art (trims left/right
margins), then resamples to **1024²**. Regenerate with **`bash scripts/generate_juce_packaging_icon.sh`**
(macOS **`sips`**). JUCE caches the generated **`AppIcon.icns`** under **`build-mlx`**;
use **`make juce-build`** / **`make juce-refresh-icon`** so the cache is invalidated when
**`packaging/icon.png`** changes.

## Tests

```bash
cmake --build packages/magenta-rt-juce/build-mlx --target Tests
packages/magenta-rt-juce/build-mlx/Tests
```

The fast tests cover prompt selection, engine option mapping, background load
error handling, generated-audio queue behavior, and processor audio draining.

For a one-chunk generation smoke test with real weights:

```bash
MRT_JUCE_WEIGHTS_DIR=/path/to/.weights-cache packages/magenta-rt-juce/build-mlx/Tests "[smoke]"
```

The smoke test is hidden by default and requires a complete C++ MLX weights
snapshot containing `.safetensors`, `musiccoca_vocab.model`, and
`depthformer/depthformer_base.safetensors`. Use `MRT_JUCE_WEIGHTS_DIR` to point
at either a standard Hugging Face `snapshots/<commit>` directory or the repo
development `.weights-cache`.
