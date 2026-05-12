# StyleStreamer — convenience build targets.

.PHONY: help ensure-weights-cache \
	juce-configure juce-refresh-icon juce-build juce-build-vst3 juce-run juce-dist \
	juce-dist-weights-check juce-dist-macos

REPO_ROOT := $(abspath .)

CMAKE_BUILD_TYPE ?= Release
JUCE_DIR := $(REPO_ROOT)/packages/magenta-rt-juce
JUCE_BUILD := $(JUCE_DIR)/build-mlx
JUCE_NPROCS := $(shell (command -v nproc >/dev/null 2>&1 && nproc) || (sysctl -n hw.ncpu 2>/dev/null) || echo 4)
STYLESTREAMER_APP := $(JUCE_BUILD)/StyleStreamerJuce_artefacts/$(CMAKE_BUILD_TYPE)/Standalone/StyleStreamer.app
STYLESTREAMER_EXE := $(JUCE_BUILD)/StyleStreamerJuce_artefacts/$(CMAKE_BUILD_TYPE)/Standalone/StyleStreamer
STYLESTREAMER_VST3 := $(JUCE_BUILD)/StyleStreamerJuce_artefacts/$(CMAKE_BUILD_TYPE)/VST3/StyleStreamer.vst3
STYLESTREAMER_USER_VST3 := $(HOME)/Library/Audio/Plug-Ins/VST3/StyleStreamer.vst3
STYLESTREAMER_VERSION := $(shell tr -d '\n' < $(JUCE_DIR)/VERSION 2>/dev/null || echo 0.1.0)
JUCE_PACKAGING_ICON := $(JUCE_DIR)/packaging/icon.png
JUCE_WEIGHT_HELPER_DIR := $(JUCE_DIR)/packaging/weight-helper
JUCE_GENERATED_APPICON := $(JUCE_BUILD)/StyleStreamerJuce_artefacts/JuceLibraryCode/AppIcon.icns
JUCE_STANDALONE_APPICON := $(STYLESTREAMER_APP)/Contents/Resources/AppIcon.icns
UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
JUCE_WEIGHTS_CACHE := $(REPO_ROOT)/.weights-cache

help:
	@echo "Targets:"
	@echo "  make ensure-weights-cache  Download C++ MLX .safetensors bundle from HF → .weights-cache/"
	@echo "  make juce-configure      Configure packages/magenta-rt-juce → build-mlx (Ninja, CMAKE_BUILD_TYPE)"
	@echo "  make juce-refresh-icon   Regenerate JUCE AppIcon.icns if packaging/icon.png changed"
	@echo "  make juce-build          Build StyleStreamerJuce_Standalone (runs configure if build dir is new)"
	@echo "  make juce-build-vst3     Build StyleStreamerJuce_VST3 and verify the local VST3 bundle"
	@echo "  make juce-run            juce-build then launch the StyleStreamer Standalone app"
	@echo "  make juce-dist           juce-build → dist/*.zip (macOS): vendored MLX/native libs + HF weight helper"

ensure-weights-cache:
	python3 vendor/magenta-realtime-mlx-cpp/scripts/download_weights_from_hf.py --skip-if-complete

# StyleStreamer JUCE Standalone — requires MLX in the repo .venv (see packages/magenta-rt-juce/README.md).
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
	@test -f "$(STYLESTREAMER_VST3)/Contents/MacOS/StyleStreamer" || { echo "error: missing VST3 executable in $(STYLESTREAMER_VST3)" >&2; exit 1; }
	@echo "Built VST3: $(STYLESTREAMER_VST3)"
	@if test "$(UNAME_S)" = "Darwin"; then \
		if test -d "$(STYLESTREAMER_USER_VST3)"; then \
			echo "Copied VST3: $(STYLESTREAMER_USER_VST3)"; \
		else \
			echo "warning: JUCE did not copy the VST3 to $(STYLESTREAMER_USER_VST3)" >&2; \
		fi; \
	fi

juce-run: juce-build
	@if test "$(shell uname -s)" = "Darwin" && test -d "$(STYLESTREAMER_APP)"; then \
		open "$(STYLESTREAMER_APP)"; \
	elif test -x "$(STYLESTREAMER_EXE)"; then \
		exec "$(STYLESTREAMER_EXE)"; \
	else \
		echo "error: expected app $(STYLESTREAMER_APP) (macOS) or executable $(STYLESTREAMER_EXE)" >&2; \
		exit 1; \
	fi

# Distributable Standalone layout (gitignored ``dist/``): native libraries and the
# Hugging Face helper are bundled; model weights are downloaded into the standard
# HF cache on first use instead of copied into the archive.
JUCE_DIST_STAGING_ROOT := $(REPO_ROOT)/dist/staging-juce-dist
JUCE_DIST_STAGING_MACOS := $(JUCE_DIST_STAGING_ROOT)/macos/StyleStreamer.app
JUCE_DIST_OUT_MACOS := $(REPO_ROOT)/dist/stylestreamer-$(STYLESTREAMER_VERSION)-macos-$(UNAME_M)
JUCE_DIST_ZIP_MACOS := $(REPO_ROOT)/dist/stylestreamer-$(STYLESTREAMER_VERSION)-macos-$(UNAME_M).zip

juce-dist-weights-check:
	@test -f "$(JUCE_WEIGHTS_CACHE)/spectrostream_encoder.safetensors" \
		&& test -f "$(JUCE_WEIGHTS_CACHE)/depthformer/depthformer_base.safetensors" \
		&& test -d "$(JUCE_WEIGHTS_CACHE)/mlxfn" \
		|| { echo "error: incomplete $(JUCE_WEIGHTS_CACHE)/ (need full C++ bundle + mlxfn/). Run:" >&2; \
			echo "  make ensure-weights-cache" >&2; exit 1; }

juce-dist-macos: juce-build
	@test "$(UNAME_S)" = "Darwin" || { echo "error: juce-dist-macos expects macOS." >&2; exit 1; }
	@test -d "$(STYLESTREAMER_APP)" || { echo "error: missing $(STYLESTREAMER_APP)" >&2; exit 1; }
	rm -rf "$(JUCE_DIST_STAGING_ROOT)" "$(JUCE_DIST_ZIP_MACOS)"
	mkdir -p "$(JUCE_DIST_OUT_MACOS)"
	find "$(JUCE_DIST_OUT_MACOS)" -mindepth 1 -maxdepth 1 -exec rm -rf {} +
	mkdir -p "$(dir $(JUCE_DIST_STAGING_MACOS))"
	COPYFILE_DISABLE=1 ditto "$(STYLESTREAMER_APP)" "$(JUCE_DIST_STAGING_MACOS)"
	mkdir -p "$(JUCE_DIST_STAGING_MACOS)/Contents/Resources/weight-helper"
	rsync -a --delete --exclude '__pycache__/' --exclude '*.pyc' \
		"$(JUCE_WEIGHT_HELPER_DIR)/" "$(JUCE_DIST_STAGING_MACOS)/Contents/Resources/weight-helper/"
	COPYFILE_DISABLE=1 ditto "$(JUCE_DIST_STAGING_MACOS)" "$(JUCE_DIST_OUT_MACOS)/StyleStreamer.app"
	printf '%s\n' \
	  "StyleStreamer $(STYLESTREAMER_VERSION) ($(UNAME_M), $(CMAKE_BUILD_TYPE))." \
	  "" \
	  "Requires macOS 26.0 or newer (matches the bundled libmlx.dylib)." "" \
	  "MLX runtime bundled in Contents/Frameworks/: libmlx.dylib + mlx.metallib (from repo .venv or MLX_ROOT), sentencepiece & portaudio (from Homebrew at pack time);" \
	  "the pack script re-applies ad-hoc codesign --deep after install_name_tool (avoids dyld Code Signature Invalid kill)." "" \
	  "Weights are not bundled. Open Advanced... and click Download weights; the helper uses huggingface_hub snapshot_download and writes to the standard HF cache." \
	  "The app creates an isolated temporary Python venv for the helper dependencies; it does not install huggingface_hub into system Python." \
	  "Default HF cache: ~/.cache/huggingface/hub; override with HF_HOME or HF_HUB_CACHE. Private datasets use HF_TOKEN / huggingface-cli login." "" \
	  "After unzip, move StyleStreamer.app to Applications if you like;" \
	  "macOS Gatekeeper may require Control-click Open the first launch." "" \
	  "Optional: export MRT_JUCE_WEIGHTS_DIR=/path/to/snapshot to use different weights, MRT_JUCE_PYTHON=/path/to/python3 to choose the helper Python." "" \
	  "Model licence: CC-BY 4.0 (Google)." \
	  > "$(JUCE_DIST_OUT_MACOS)/README.txt"
	cp "$(JUCE_DIST_OUT_MACOS)/README.txt" \
	  "$(JUCE_DIST_OUT_MACOS)/StyleStreamer.app/Contents/Resources/DISTRIBUTION_README.txt"
	bash "$(REPO_ROOT)/scripts/bundle_juce_standalone_dylibs_macos.sh" "$(JUCE_DIST_OUT_MACOS)/StyleStreamer.app"
	cd "$(JUCE_DIST_OUT_MACOS)" && ditto -c -k --keepParent --norsrc --noextattr --noqtn StyleStreamer.app "$(JUCE_DIST_ZIP_MACOS)"
	rm -rf "$(JUCE_DIST_STAGING_ROOT)"
	@echo ""
	@echo "Output: folder $(JUCE_DIST_OUT_MACOS) and archive $(JUCE_DIST_ZIP_MACOS)"

juce-dist: juce-dist-macos
