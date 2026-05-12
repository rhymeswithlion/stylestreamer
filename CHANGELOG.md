# Changelog

All notable changes to StyleStreamer are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Changed

- Plays generated SpectroStream audio at 44.1 kHz without resampling, making
  playback slightly slower and lower in pitch than the native 48 kHz output.

- Starts with Downtempo enabled so generated playback begins at the slower
  32 kHz-style speed immediately on launch.

- Widened card-bank cards while keeping fixed 12 px gaps so the fifth column
  is partially exposed at startup as a scroll affordance.

- Updated the default live style cards and expanded the bank with a
  horizontally scrollable 7-row prompt layout, keeping editable `TODO:`
  placeholders isolated in the final bank column.

- Moved RTF, chunk, and frame-size stats into the style-card header; moved
  the status log into Advanced options; enlarged the card bank; tightened the
  main editor height around the remaining controls.

- Bank style cards now reuse the live-row soft gradient coloring instead of
  rendering as flat color blocks.

- Replaced the flat card-bank surface fill with the embedded green felt texture
  behind the bank cards.

- Removed the redundant top editor title/status rows and moved the version into
  the style-card dashboard eyebrow label.

- Removed the main-editor **Shuffle prompts** button; prompt changes now come
  from style-card edits or pasted prompt state.

### Fixed

- Live-card slider values now belong to the four top slots instead of individual
  cards, so replacing or swapping cards leaves the sliders in place; copy/paste
  persists those slot weights.

- Active-card weights now use always-draggable horizontal JUCE sliders below
  the live cards instead of decorative rotary knobs, with slider colors
  following each card's active/inactive state.

- Copy State / Paste State now use a v3 card-state payload that restores both
  the Live Style Mix row and the solitaire card bank positions instead of
  flattening all style cards into the active row.

- Empty active-card slots now render as drop placeholders, so banked cards can
  be dragged back into the top row after the active row drops below four cards.

- Active card weights now render as realistic rotary knobs whose values move
  with the card, while their tint and shading follow the card's active/inactive
  state.

- Cards in the solitaire bank now always stay active visually, including cards
  moved into the bank, while banked cards remain out of the effective prompt mix
  until moved into an active slot.

- Default style cards now avoid grey/low-saturation active colors, including
  newly added cards, and the solitaire bank seeds five visible columns with a
  3/3/3/3/1 card distribution.

- Transition dots now advance once per chunk through the four-chunk style
  ramp and stop at the end instead of looping.

- Expanded the style-card interactions with a visible `+` placeholder in the
  five-column card bank, inline card text editing, active-card percentage
  labels, greyed inactive active cards, and animated four-dot feedback while
  style transitions are ramping.

- Rearranged the main editor around a larger reference-style style-card
  dashboard with a taller active-card row, expanded solitaire bank area,
  cleaner flat background, and the `Advanced…` button preserved in the main
  control row.

- Dropping one active style card onto another active card now swaps their
  slots, and the default editor state preserves the solitaire bank cards
  instead of replacing the deck with legacy active-only prompt cards.

- Dropping an active style card onto an existing solitaire bank card now swaps
  the two cards instead of publishing unchanged state.

- The native style-card dashboard now seeds visible solitaire bank cards by
  default, and dragging a bank card onto an active card activates it instead of
  leaving it hidden in the bank.

- Style-card dragging now uses JUCE's built-in `DragAndDropContainer` /
  `DragAndDropTarget` APIs, adding a native drag image and drop-hover feedback
  for active slots and solitaire bank targets.

- The status log no longer rewrites its full text every timer tick while
  running, so users can scroll through the loaded-weights block without the
  text editor snapping back to the top.

- Prebuffering now matches a one-shot behaviour: audio stays silent until the
  target queue depth is reached, then continues draining instead of re-muting
  every callback as soon as the queue drops below the threshold. This fixes
  sawtooth playback/stutter near real-time throughput. The generated-audio ring
  buffer now also has capacity for the full 24-chunk max-queue slider range.

- `make juce-dist` (macOS) — Bundles `mlx.metallib` next to `libmlx.dylib` and
  adds symlinks at MLX's colocated/resource fallback paths, fixing runtime
  "Failed to load the default metallib. library not found" errors after the app
  starts.

- `make juce-dist` (macOS) — Archives now include `Contents/Frameworks/libmlx.dylib`
  (from the repo `.venv` / `MLX_ROOT`) plus Homebrew sentencepiece and portaudio,
  with `install_name_tool` rewrites so recipients are not tied to the builder's
  paths. Ad hoc `codesign --force --deep --sign -` is reapplied after the
  framework copy so dyld does not SIGKILL with "Code Signature Invalid".

- `make juce-dist` (macOS) — Archive creation uses `ditto -k --keepParent` so
  unpacking the zip produces `StyleStreamer.app/` instead of `Contents/` at the
  archive root.

### Changed (earlier)

- Reverted the experimental style-cards WebView UI back to the stable native
  prompt-row editor so the Web UI can be reimplemented with a less aggressive
  refresh model.

- The default generation chunk length is now 400 ms instead of one 40 ms codec
  frame, giving the local plugin a less aggressive startup default while keeping
  one-frame chunks available from Advanced options.

### Added

- Generated SVG style-card background assets and default card metadata for the
  native style-card deck.

- Redesigned the main editor around clean performance dashboard style cards with
  generated SVG backgrounds, native painted surfaces, drag/drop replacement,
  solitaire-style card-bank movement, active/inactive toggles, and ramped style
  embedding transitions.

- Runtime chunk-length control in Advanced options wired into the C++ MLX
  `SystemConfig`, so the plugin can start the runtime at subsecond chunk sizes
  down to one 25 Hz codec frame (40 ms).

- VST3 bundle alongside the Standalone target. `make juce-build-vst3` builds
  `StyleStreamerJuce_VST3`, verifies the generated bundle, and reports the
  copied macOS user plugin path for DAW scanning.

- The status log and terminal stderr now list the resolved SpectroStream,
  MusicCoCa, Depthformer, and MLXFN weight paths after runtime load, making
  fine-tuned Depthformer overrides visible at startup.

- Fine-tune runs export the final Depthformer checkpoint as adjacent
  `.safetensors` for the C++/MLX runtime, and the Advanced options panel
  includes an optional fine-tuned Depthformer path field. When set,
  SpectroStream, MusicCoCa, vocab, and MLXFN assets still resolve from the
  normal weights directory.

- **Copy state** / **Paste state** controls for the four-prompt mix and live
  generation settings. State is stored as base64-encoded versioned JSON so
  prompt setups can be saved externally and restored later.

- **Downtempo** switch that plays generated audio as 32 kHz source material
  through JUCE's high-quality `WindowedSincInterpolator`, stretching playback to
  1.5× duration with lower pitch. Switching ramps the effective playback speed
  over about 0.4 s with a sigmoid curve to avoid crackles from abrupt
  resampler-ratio changes.

- `make juce-dist` — Assembles `dist/stylestreamer-<VERSION>-<os>-<arch>.zip`
  (plus an unpacked sibling folder): release Standalone with a packaged Hugging
  Face weight helper and no bundled model weights. Weights resolution prefers
  `MRT_JUCE_WEIGHTS_DIR`, then bundled/manual paths, then the standard
  `huggingface_hub` cache snapshot, then repo `.weights-cache`.

- Advanced options now include **Download weights**, which runs
  `huggingface_hub.snapshot_download()` through the packaged helper and fills
  the weights field with the resolved standard cache `snapshots/<commit>`
  directory. The app bootstraps an isolated temporary Python venv for helper
  dependencies instead of installing into system Python.

- `packaging/icon.png` is generated from the hero artwork: on wide art the crop
  keeps full height and trims left/right, then resamples to 1024² (`ICON_BIG`).
  `make juce-build` runs `juce-refresh-icon` to invalidate JUCE's generated
  `AppIcon.icns` cache when `packaging/icon.png` changes.

- Default macOS deployment target **15.0** (Sequoia) so distributable archives
  and local Standalone builds declare compatibility with macOS 15.x and newer.

- When MLXFN fast paths are enabled (default), the engine sets
  `MRT_DEPTHFORMER_*` from `<weights>/mlxfn` and fails to load if that
  directory or a full encode + depth + temporal bundle for the current tag/dtype
  is missing. Disabling MLXFN in Advanced options clears those env vars.

- Control chrome (sliders, text fields, combo, buttons, popup menus, status log)
  uses 90% opacity (`widgetFillAlpha`) over the hero for a consistent glass
  look; label and body text stay fully opaque.

- Live style controls — `MagentaRtJuceEngine` keeps `RuntimeSettings` vs
  `LiveControlSnapshot`; `setLiveControls` and `setPromptPortfolio` update the
  worker without restarting; generation refreshes `GenerateChunkOptions` per
  chunk and caches weighted style tokens by portfolio signature.
  `PluginProcessor::processBlock` withholds playback until
  `prebufferTargetFrames` are queued.

- Embedded MusicCoCa text+audio distill prompt pool
  (`assets/data/musiccoca_textaudio100_prompts.json`), startup random mix of
  four distinct prompts with descending blend weights, and **Shuffle prompts**
  to re-roll prompts and percentages live.

- JUCE UI snapshot test — Catch test `[ui]` renders `PluginEditor` off-screen
  and optionally writes `plugin-editor.png` when `MRT_JUCE_UI_SNAPSHOT=1`
  (default path `output/<date>-juce-editor-snapshot/` at the repo root; override
  with `MRT_JUCE_UI_SNAPSHOT_DIR`).

- Makefile `juce-configure`, `juce-build`, and `juce-run` targets
  (`build-mlx`, Ninja, `StyleStreamerJuce_Standalone`). `juce-run` launches the
  Standalone app on macOS (`open`) or the binary on Linux. Override
  `CMAKE_BUILD_TYPE` (default `Release`) as needed.

- **Advanced…** floating window for weights path, engine, sampling, and debug
  controls (DType, MLXFN fast paths, seed, temperature, top-k, CFG weight,
  prebuffer, max queue, Inspect UI, Copy log); main editor keeps prompts,
  transport, RTF, and status log.

- Initial StyleStreamer JUCE plugin on Pamplejuce scaffold — Standalone + VST3
  build linking the optimized C++ MLX runtime
  (`vendor/magenta-realtime-mlx-cpp`), background generation worker, realtime-
  safe audio queue, weighted-prompt UI, display name **StyleStreamer**, bundle
  id `com.rhymeswithlion.stylestreamer`, neon cyan/magenta/yellow/orange
  accents, full-window hero background.

### Fixed (earlier)

- JUCE Standalone silent output — weights field now defaults by scanning parents
  of the app bundle for `.weights-cache`, then `MRT_JUCE_WEIGHTS_DIR`, then CWD;
  **Start** refuses a missing folder; a timer shows prebuffer progress, playback
  queue depth, and MLX load / generation errors.

- JUCE status panel — replaced single-line label with a scrollable multi-line
  status area so MLX bundle errors are fully visible; monospace font, **Copy
  log** copies the full message to the clipboard.
