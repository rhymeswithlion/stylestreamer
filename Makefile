# StyleStreamer — convenience build targets.

.PHONY: help ensure-weights-cache \
	juce-configure juce-refresh-icon juce-build juce-build-vst3 juce-run \
	juce-dist juce-dist-standalone juce-dist-vst3 \
	juce-dist-weights-check

REPO_ROOT := $(abspath .)

CMAKE_BUILD_TYPE ?= Release
JUCE_DIR := $(REPO_ROOT)/packages/magenta-rt-juce
JUCE_BUILD := $(JUCE_DIR)/build-mlx
JUCE_NPROCS := $(shell (command -v nproc >/dev/null 2>&1 && nproc) || (sysctl -n hw.ncpu 2>/dev/null) || echo 4)
STYLESTREAMER_APP  := $(JUCE_BUILD)/StyleStreamerJuce_artefacts/$(CMAKE_BUILD_TYPE)/Standalone/StyleStreamer.app
STYLESTREAMER_EXE  := $(JUCE_BUILD)/StyleStreamerJuce_artefacts/$(CMAKE_BUILD_TYPE)/Standalone/StyleStreamer
STYLESTREAMER_VST3 := $(JUCE_BUILD)/StyleStreamerJuce_artefacts/$(CMAKE_BUILD_TYPE)/VST3/StyleStreamer.vst3
STYLESTREAMER_USER_VST3 := $(HOME)/Library/Audio/Plug-Ins/VST3/StyleStreamer.vst3
STYLESTREAMER_VERSION := $(shell tr -d '\n' < $(JUCE_DIR)/VERSION 2>/dev/null || echo 0.1.0)
JUCE_PACKAGING_ICON   := $(JUCE_DIR)/packaging/icon.png
JUCE_WEIGHT_HELPER_DIR := $(JUCE_DIR)/packaging/weight-helper
JUCE_GENERATED_APPICON := $(JUCE_BUILD)/StyleStreamerJuce_artefacts/JuceLibraryCode/AppIcon.icns
JUCE_STANDALONE_APPICON := $(STYLESTREAMER_APP)/Contents/Resources/AppIcon.icns
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
JUCE_WEIGHTS_CACHE := $(REPO_ROOT)/.weights-cache

# Documentation files included in every distribution archive.
DIST_DOCS := $(REPO_ROOT)/README.md $(REPO_ROOT)/LICENSE $(REPO_ROOT)/NOTICE

# Distribution output paths.
JUCE_DIST_OUT_STANDALONE := $(REPO_ROOT)/dist/stylestreamer-$(STYLESTREAMER_VERSION)-macos-$(UNAME_M)
JUCE_DIST_ZIP_STANDALONE  := $(REPO_ROOT)/dist/stylestreamer-$(STYLESTREAMER_VERSION)-macos-$(UNAME_M).zip
JUCE_DIST_OUT_VST3 := $(REPO_ROOT)/dist/stylestreamer-vst3-$(STYLESTREAMER_VERSION)-macos-$(UNAME_M)
JUCE_DIST_ZIP_VST3 := $(REPO_ROOT)/dist/stylestreamer-vst3-$(STYLESTREAMER_VERSION)-macos-$(UNAME_M).zip

help:
	@echo "Targets:"
	@echo "  make ensure-weights-cache  Download C++ MLX .safetensors bundle from HF → .weights-cache/"
	@echo "  make juce-configure        Configure packages/magenta-rt-juce → build-mlx (Ninja, CMAKE_BUILD_TYPE)"
	@echo "  make juce-refresh-icon     Regenerate JUCE AppIcon.icns if packaging/icon.png changed"
	@echo "  make juce-build            Build StyleStreamerJuce_Standalone"
	@echo "  make juce-build-vst3       Build StyleStreamerJuce_VST3 and verify the local VST3 bundle"
	@echo "  make juce-run              juce-build then launch the Standalone app"
	@echo "  make juce-dist             Build both dist/standalone and dist/vst3 archives"
	@echo "  make juce-dist-standalone  dist/stylestreamer-<ver>-macos-<arch>.zip (app + dylibs + docs)"
	@echo "  make juce-dist-vst3        dist/stylestreamer-vst3-<ver>-macos-<arch>.zip (vst3 + dylibs + docs)"

ensure-weights-cache:
	python3 vendor/magenta-realtime-mlx-cpp/scripts/download_weights_from_hf.py --skip-if-complete

# ── Build targets ────────────────────────────────────────────────────────────

$(JUCE_BUILD)/build.ninja:
	cmake -S $(JUCE_DIR) -B $(JUCE_BUILD) -G Ninja -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

juce-configure:
	cmake -S $(JUCE_DIR) -B $(JUCE_BUILD) -G Ninja -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

juce-refresh-icon: $(JUCE_BUILD)/build.ninja
	@if test ! -f "$(JUCE_GENERATED_APPICON)" || test "$(JUCE_PACKAGING_ICON)" -nt "$(JUCE_GENERATED_APPICON)"; then \
		echo "Regenerating JUCE AppIcon.icns from $(JUCE_PACKAGING_ICON)"; \
		rm -f "$(JUCE_GENERATED_APPICON)" "$(JUCE_STANDALONE_APPICON)"; \
		cmake -S $(JUCE_DIR) -B $(JUCE_BUILD) -G Ninja -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE); \
	fi

juce-build: juce-refresh-icon
	cmake --build $(JUCE_BUILD) --target StyleStreamerJuce_Standalone -j $(JUCE_NPROCS)

juce-build-vst3: juce-refresh-icon
	cmake --build $(JUCE_BUILD) --target StyleStreamerJuce_VST3 -j $(JUCE_NPROCS)
	@test -d "$(STYLESTREAMER_VST3)" || { echo "error: missing $(STYLESTREAMER_VST3)" >&2; exit 1; }
	@test -f "$(STYLESTREAMER_VST3)/Contents/MacOS/StyleStreamer" \
		|| { echo "error: missing VST3 executable in $(STYLESTREAMER_VST3)" >&2; exit 1; }
	@echo "Built VST3: $(STYLESTREAMER_VST3)"
	@if test "$(UNAME_S)" = "Darwin" && test -d "$(STYLESTREAMER_USER_VST3)"; then \
		echo "Copied VST3: $(STYLESTREAMER_USER_VST3)"; \
	fi

juce-run: juce-build
	@if test "$(UNAME_S)" = "Darwin" && test -d "$(STYLESTREAMER_APP)"; then \
		open "$(STYLESTREAMER_APP)"; \
	elif test -x "$(STYLESTREAMER_EXE)"; then \
		exec "$(STYLESTREAMER_EXE)"; \
	else \
		echo "error: expected $(STYLESTREAMER_APP) (macOS) or $(STYLESTREAMER_EXE)" >&2; exit 1; \
	fi

# ── Distribution helpers ─────────────────────────────────────────────────────

juce-dist-weights-check:
	@test -f "$(JUCE_WEIGHTS_CACHE)/spectrostream_encoder.safetensors" \
		&& test -f "$(JUCE_WEIGHTS_CACHE)/depthformer/depthformer_base.safetensors" \
		&& test -d "$(JUCE_WEIGHTS_CACHE)/mlxfn" \
		|| { echo "error: incomplete $(JUCE_WEIGHTS_CACHE)/. Run: make ensure-weights-cache" >&2; exit 1; }

# Shared helper: write README.txt and copy docs into a dist output directory.
# Usage: $(call dist-write-docs,<out-dir>,<format-specific README.txt content>)
define STANDALONE_README_BODY
StyleStreamer $(STYLESTREAMER_VERSION) — Standalone app ($(UNAME_M), $(CMAKE_BUILD_TYPE)).

Requires macOS 26.0 or newer (matches the bundled libmlx.dylib).

MLX runtime, sentencepiece, and portaudio are bundled in Contents/Frameworks/.
Install: move StyleStreamer.app to /Applications.
First launch: macOS Gatekeeper may require Control-click → Open.

Weights are not bundled. Open Advanced... → Download weights.
Default HF cache: ~/.cache/huggingface/hub
Override: MRT_JUCE_WEIGHTS_DIR, HF_HOME, HF_HUB_CACHE, HF_TOKEN.

See README.md and NOTICE for full license and attribution details.
endef

define VST3_README_BODY
StyleStreamer $(STYLESTREAMER_VERSION) — VST3 plugin ($(UNAME_M), $(CMAKE_BUILD_TYPE)).

Requires macOS 26.0 or newer (matches the bundled libmlx.dylib).

Install: copy StyleStreamer.vst3 to ~/Library/Audio/Plug-Ins/VST3/ and
rescan plugins in your DAW.

MLX runtime, sentencepiece, and portaudio are bundled in
StyleStreamer.vst3/Contents/Frameworks/.

Weights are not bundled. Open Advanced... → Download weights in the plugin.
Default HF cache: ~/.cache/huggingface/hub
Override: MRT_JUCE_WEIGHTS_DIR, HF_HOME, HF_HUB_CACHE, HF_TOKEN.

See README.md and NOTICE for full license and attribution details.
endef

export STANDALONE_README_BODY
export VST3_README_BODY

# ── Standalone distribution ──────────────────────────────────────────────────

juce-dist-standalone: juce-build
	@test "$(UNAME_S)" = "Darwin" || { echo "error: macOS only." >&2; exit 1; }
	@test -d "$(STYLESTREAMER_APP)" || { echo "error: missing $(STYLESTREAMER_APP)" >&2; exit 1; }
	rm -rf "$(JUCE_DIST_OUT_STANDALONE)" "$(JUCE_DIST_ZIP_STANDALONE)"
	mkdir -p "$(JUCE_DIST_OUT_STANDALONE)"
	COPYFILE_DISABLE=1 ditto "$(STYLESTREAMER_APP)" "$(JUCE_DIST_OUT_STANDALONE)/StyleStreamer.app"
	mkdir -p "$(JUCE_DIST_OUT_STANDALONE)/StyleStreamer.app/Contents/Resources/weight-helper"
	rsync -a --delete --exclude '__pycache__/' --exclude '*.pyc' \
		"$(JUCE_WEIGHT_HELPER_DIR)/" \
		"$(JUCE_DIST_OUT_STANDALONE)/StyleStreamer.app/Contents/Resources/weight-helper/"
	bash "$(REPO_ROOT)/scripts/bundle_juce_standalone_dylibs_macos.sh" \
		"$(JUCE_DIST_OUT_STANDALONE)/StyleStreamer.app"
	cp $(DIST_DOCS) "$(JUCE_DIST_OUT_STANDALONE)/"
	printf '%s\n' "$$STANDALONE_README_BODY" > "$(JUCE_DIST_OUT_STANDALONE)/README.txt"
	cp "$(JUCE_DIST_OUT_STANDALONE)/README.txt" \
		"$(JUCE_DIST_OUT_STANDALONE)/StyleStreamer.app/Contents/Resources/DISTRIBUTION_README.txt"
	cd "$(JUCE_DIST_OUT_STANDALONE)" && \
		COPYFILE_DISABLE=1 zip -r --symlinks "$(JUCE_DIST_ZIP_STANDALONE)" \
		StyleStreamer.app README.md README.txt LICENSE NOTICE
	@echo ""
	@echo "Standalone: $(JUCE_DIST_ZIP_STANDALONE)"

# ── VST3 distribution ────────────────────────────────────────────────────────

juce-dist-vst3: juce-build-vst3
	@test "$(UNAME_S)" = "Darwin" || { echo "error: macOS only." >&2; exit 1; }
	@test -d "$(STYLESTREAMER_VST3)" || { echo "error: missing $(STYLESTREAMER_VST3)" >&2; exit 1; }
	rm -rf "$(JUCE_DIST_OUT_VST3)" "$(JUCE_DIST_ZIP_VST3)"
	mkdir -p "$(JUCE_DIST_OUT_VST3)"
	COPYFILE_DISABLE=1 ditto "$(STYLESTREAMER_VST3)" "$(JUCE_DIST_OUT_VST3)/StyleStreamer.vst3"
	mkdir -p "$(JUCE_DIST_OUT_VST3)/StyleStreamer.vst3/Contents/Resources/weight-helper"
	rsync -a --delete --exclude '__pycache__/' --exclude '*.pyc' \
		"$(JUCE_WEIGHT_HELPER_DIR)/" \
		"$(JUCE_DIST_OUT_VST3)/StyleStreamer.vst3/Contents/Resources/weight-helper/"
	bash "$(REPO_ROOT)/scripts/bundle_juce_standalone_dylibs_macos.sh" \
		"$(JUCE_DIST_OUT_VST3)/StyleStreamer.vst3"
	cp $(DIST_DOCS) "$(JUCE_DIST_OUT_VST3)/"
	printf '%s\n' "$$VST3_README_BODY" > "$(JUCE_DIST_OUT_VST3)/README.txt"
	cd "$(JUCE_DIST_OUT_VST3)" && \
		COPYFILE_DISABLE=1 zip -r --symlinks "$(JUCE_DIST_ZIP_VST3)" \
		StyleStreamer.vst3 README.md README.txt LICENSE NOTICE
	@echo ""
	@echo "VST3: $(JUCE_DIST_ZIP_VST3)"

# ── Combined ─────────────────────────────────────────────────────────────────

juce-dist: juce-dist-standalone juce-dist-vst3
