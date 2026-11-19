SHELL := /bin/bash

# Nothing here is worth running in parallel at this level - cmake parallelises
# inside its own steps, and the release targets read build outputs that the
# targets above them write.
.NOTPARALLEL:

ROOT_DIR := $(CURDIR)
BUILD_DIR ?= $(ROOT_DIR)/build
PLUGIN_BUILD_DIR ?= $(ROOT_DIR)/build-plugin
DIST_DIR ?= $(ROOT_DIR)/dist
RELEASE_DIR ?= $(DIST_DIR)/zoal-briefing
BUILD_TYPE ?= Release

# Local X-Plane plugin folder (override for non-standard installs). The .xpl
# goes in its platform subfolder, as in zoal-atc: zoal-briefing/mac_x64/zoal-briefing.xpl.
LOCAL_XPLANE_PLUGIN_DIR ?= $(HOME)/X-Plane 12/Resources/plugins/zoal-briefing

# FreeType, which rasterizes the panel's text with hinting, and Inter, the
# typeface it is set in -- both as in zoal-atc, pinned with their archive
# checksums in scripts/*-version.txt and downloaded once into .cache.
FREETYPE_VERSION := $(shell cut -d' ' -f1 "$(ROOT_DIR)/scripts/freetype-version.txt")
ZOAL_BRIEFING_FREETYPE_SOURCE ?= $(ROOT_DIR)/.cache/freetype-$(FREETYPE_VERSION)
INTER_VERSION := $(shell cut -d' ' -f1 "$(ROOT_DIR)/scripts/inter-version.txt")
ZOAL_BRIEFING_INTER ?= $(ROOT_DIR)/.cache/inter-$(INTER_VERSION)
INTER_WEIGHTS := Inter-Regular.ttf Inter-SemiBold.ttf Inter-Bold.ttf

PLUGIN_XPL := $(PLUGIN_BUILD_DIR)/zoal-briefing.xpl

# The audio pipeline (openWakeWord, whisper.cpp, Piper via sherpa-onnx,
# miniaudio). On by default on macOS, where it is tested; AUDIO=1 elsewhere to
# try it, AUDIO=0 to build the plugin without it.
SHERPA_VERSION := $(shell awk '$$1 == "version" { print $$2 }' "$(ROOT_DIR)/scripts/sherpa-onnx-version.txt")
WHISPER_VERSION := $(shell cut -d' ' -f1 "$(ROOT_DIR)/scripts/whisper-version.txt")
MINIAUDIO_VERSION := $(shell cut -d' ' -f1 "$(ROOT_DIR)/scripts/miniaudio-version.txt")
ZOAL_BRIEFING_WHISPER_SOURCE ?= $(ROOT_DIR)/.cache/whisper.cpp-$(WHISPER_VERSION)
ZOAL_BRIEFING_MINIAUDIO ?= $(ROOT_DIR)/.cache/miniaudio-$(MINIAUDIO_VERSION)
ZOAL_BRIEFING_MODELS ?= $(ROOT_DIR)/.cache/models
AUDIO_BUILD_DIR ?= $(ROOT_DIR)/build-audio

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(OS),Windows_NT)
	PLATFORM := windows
	PLUGIN_PLATFORM_DIR := win_x64
else ifeq ($(UNAME_S),Darwin)
	PLATFORM := macos
	PLUGIN_PLATFORM_DIR := mac_x64
else
	PLATFORM := linux
	PLUGIN_PLATFORM_DIR := lin_x64
endif

ifeq ($(UNAME_M),arm64)
	ARCH := arm64
else ifeq ($(UNAME_M),aarch64)
	ARCH := arm64
else
	ARCH := x64
endif

ifeq ($(PLATFORM),macos)
	AUDIO ?= 1
	SHERPA_FLAVOUR := osx-arm64-static-lib
else ifeq ($(PLATFORM),linux)
	AUDIO ?= 0
	SHERPA_FLAVOUR := linux-x64-shared-lib
else
	AUDIO ?= 0
	SHERPA_FLAVOUR := win-x64-shared-MT-Release-lib
endif
ZOAL_BRIEFING_SHERPA_ONNX ?= $(ROOT_DIR)/.cache/sherpa-onnx-$(SHERPA_VERSION)-$(SHERPA_FLAVOUR)

ifeq ($(AUDIO),1)
AUDIO_DEPS := audio-deps
AUDIO_CMAKE_FLAGS := -DZoalBriefing_WITH_AUDIO=ON \
	-DZoalBriefing_WHISPER_SOURCE="$(ZOAL_BRIEFING_WHISPER_SOURCE)" \
	-DZoalBriefing_SHERPA_ONNX="$(ZOAL_BRIEFING_SHERPA_ONNX)" \
	-DZoalBriefing_MINIAUDIO="$(ZOAL_BRIEFING_MINIAUDIO)" \
	-DZoalBriefing_MODELS_DIR="$(ZOAL_BRIEFING_MODELS)"
else
AUDIO_DEPS :=
AUDIO_CMAKE_FLAGS := -DZoalBriefing_WITH_AUDIO=OFF
endif

# On Windows the default CMake generator is Visual Studio (multi-config), which
# ignores -DCMAKE_BUILD_TYPE and places outputs in a <Config>/ subdirectory.
# Force MinGW Makefiles (a single-config generator) so the plugin lands at
# build-plugin/zoal-briefing.xpl, matching the packaging step below.
ifeq ($(OS),Windows_NT)
CMAKE_GENERATOR := -G "MinGW Makefiles"
else
CMAKE_GENERATOR :=
endif

.PHONY: help test lint plugin freetype fonts audio-deps models voice-check voice-live install install-plugin release clean

.DEFAULT_GOAL := help

help:
	@echo "zoal-briefing — X-Plane voice briefing plugin"
	@echo
	@echo "  test            build and run the SDK-free C++ core tests"
	@echo "  lint            clang-tidy the changed .cpp files (LINT_ALL=1 for all)"
	@echo "  plugin          build the .xpl (fetches the X-Plane SDK from GitHub releases)"
	@echo "  models          download the audio models into $(ZOAL_BRIEFING_MODELS)"
	@echo "  voice-check     self-test the audio pipeline with the real models (no mic)"
	@echo "  voice-live      run the wake word -> listen -> echo loop on mic + speakers"
	@echo "  install         build and install into X-Plane ($(LOCAL_XPLANE_PLUGIN_DIR))"
	@echo "  install-plugin  same as install"
	@echo "  release         assemble the release tree in dist/zoal-briefing"
	@echo "  clean           remove build outputs and dist/"

# --- build -------------------------------------------------------------------

# Build tests only (SDK-free, runs on all platforms)
test:
	cmake -S . -B "$(BUILD_DIR)" $(CMAKE_GENERATOR) \
		-DZoalBriefing_BUILD_TESTS=ON \
		-DZoalBriefing_BUILD_PLUGIN=OFF \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
ifeq ($(OS),Windows_NT)
	cmake --build "$(BUILD_DIR)" --config $(BUILD_TYPE)
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure -C $(BUILD_TYPE)
else
	cmake --build "$(BUILD_DIR)"
	ctest --test-dir "$(BUILD_DIR)" --output-on-failure
endif

# clang-tidy (see .clang-tidy) over changed files, with each build's
# compile_commands.json.
# Builds (does not run) the tests, so a failing test does not block linting.
lint:
	cmake -S . -B "$(BUILD_DIR)" $(CMAKE_GENERATOR) \
		-DZoalBriefing_BUILD_TESTS=ON \
		-DZoalBriefing_BUILD_PLUGIN=OFF \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build "$(BUILD_DIR)"
	LINT_ALL="$(LINT_ALL)" "$(ROOT_DIR)/scripts/lint.sh"

freetype:
	ZOAL_BRIEFING_FREETYPE_SOURCE="$(ZOAL_BRIEFING_FREETYPE_SOURCE)" "$(ROOT_DIR)/scripts/ensure-freetype.sh"

fonts:
	ZOAL_BRIEFING_INTER="$(ZOAL_BRIEFING_INTER)" "$(ROOT_DIR)/scripts/ensure-inter.sh"

audio-deps:
	ZOAL_BRIEFING_WHISPER_SOURCE="$(ZOAL_BRIEFING_WHISPER_SOURCE)" "$(ROOT_DIR)/scripts/ensure-whisper-src.sh"
	ZOAL_BRIEFING_SHERPA_ONNX="$(ZOAL_BRIEFING_SHERPA_ONNX)" "$(ROOT_DIR)/scripts/ensure-sherpa-onnx.sh" $(SHERPA_FLAVOUR)
	ZOAL_BRIEFING_MINIAUDIO="$(ZOAL_BRIEFING_MINIAUDIO)" "$(ROOT_DIR)/scripts/ensure-miniaudio.sh"

models:
	"$(ROOT_DIR)/scripts/ensure-models.sh" "$(ZOAL_BRIEFING_MODELS)"

# The audio pipeline outside X-Plane: the dev tool and the audio tests.
$(AUDIO_BUILD_DIR)/voice_check: audio-deps models
	cmake -S . -B "$(AUDIO_BUILD_DIR)" $(CMAKE_GENERATOR) \
		-DZoalBriefing_BUILD_TESTS=ON -DZoalBriefing_BUILD_PLUGIN=OFF \
		-DZoalBriefing_WITH_AUDIO=ON \
		-DZoalBriefing_WHISPER_SOURCE="$(ZOAL_BRIEFING_WHISPER_SOURCE)" \
		-DZoalBriefing_SHERPA_ONNX="$(ZOAL_BRIEFING_SHERPA_ONNX)" \
		-DZoalBriefing_MINIAUDIO="$(ZOAL_BRIEFING_MINIAUDIO)" \
		-DZoalBriefing_MODELS_DIR="$(ZOAL_BRIEFING_MODELS)" \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	cmake --build "$(AUDIO_BUILD_DIR)" --target voice_check

voice-check: $(AUDIO_BUILD_DIR)/voice_check
	"$(AUDIO_BUILD_DIR)/voice_check" selftest "$(ZOAL_BRIEFING_MODELS)"

voice-live: $(AUDIO_BUILD_DIR)/voice_check
	"$(AUDIO_BUILD_DIR)/voice_check" live "$(ZOAL_BRIEFING_MODELS)" $(CONFIG)

# Build plugin (fetches X-Plane SDK from GitHub releases automatically)
plugin: freetype $(AUDIO_DEPS)
	@echo ">>> Building zoal-briefing .xpl plugin..."
	cmake -S . -B "$(PLUGIN_BUILD_DIR)" $(CMAKE_GENERATOR) \
		-DZoalBriefing_BUILD_TESTS=OFF \
		-DZoalBriefing_BUILD_PLUGIN=ON \
		-DZoalBriefing_FREETYPE_SOURCE="$(ZOAL_BRIEFING_FREETYPE_SOURCE)" \
		$(AUDIO_CMAKE_FLAGS) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
ifeq ($(OS),Windows_NT)
	cmake --build "$(PLUGIN_BUILD_DIR)" --config $(BUILD_TYPE)
else
	cmake --build "$(PLUGIN_BUILD_DIR)"
endif
	@echo ">>> Plugin built: $(PLUGIN_XPL)"

# --- install & release -------------------------------------------------------
#
# Install plugin into local X-Plane plugins directory.
# Mirrors zoal-atc pattern: build the plugin, then copy it into X-Plane's Plugins folder.

install: install-plugin

ifeq ($(AUDIO),1)
INSTALL_MODELS := models
else
INSTALL_MODELS :=
endif

install-plugin: plugin fonts $(INSTALL_MODELS)
	@plugins_dir="$$(dirname "$(LOCAL_XPLANE_PLUGIN_DIR)")"; \
	if [ ! -d "$$plugins_dir" ]; then \
		echo "no X-Plane plugins folder at $$plugins_dir" >&2; \
		echo "pass LOCAL_XPLANE_PLUGIN_DIR=/path/to/X-Plane/Resources/plugins/zoal-briefing" >&2; \
		exit 1; \
	fi
	rm -rf "$(LOCAL_XPLANE_PLUGIN_DIR)"
	mkdir -p "$(LOCAL_XPLANE_PLUGIN_DIR)/$(PLUGIN_PLATFORM_DIR)"
	cp "$(PLUGIN_XPL)" "$(LOCAL_XPLANE_PLUGIN_DIR)/$(PLUGIN_PLATFORM_DIR)/zoal-briefing.xpl"
	mkdir -p "$(LOCAL_XPLANE_PLUGIN_DIR)/fonts" "$(LOCAL_XPLANE_PLUGIN_DIR)/licenses/inter"
	for weight in $(INTER_WEIGHTS); do cp "$(ZOAL_BRIEFING_INTER)/$$weight" "$(LOCAL_XPLANE_PLUGIN_DIR)/fonts/"; done
	cp "$(ZOAL_BRIEFING_INTER)/LICENSE.txt" "$(LOCAL_XPLANE_PLUGIN_DIR)/licenses/inter/LICENSE.txt"
ifeq ($(AUDIO),1)
	@# Models beside the plugin (<plugin>/models), and any shared runtime
	@# libraries (Windows/Linux) beside the .xpl.
	mkdir -p "$(LOCAL_XPLANE_PLUGIN_DIR)/models"
	cp -R "$(ZOAL_BRIEFING_MODELS)/." "$(LOCAL_XPLANE_PLUGIN_DIR)/models/"
	for lib in "$(PLUGIN_BUILD_DIR)"/*.dll "$(PLUGIN_BUILD_DIR)"/*.so; do \
		[ -f "$$lib" ] && cp "$$lib" "$(LOCAL_XPLANE_PLUGIN_DIR)/$(PLUGIN_PLATFORM_DIR)/"; \
	done; true
endif
	@echo "installed into $(LOCAL_XPLANE_PLUGIN_DIR)/$(PLUGIN_PLATFORM_DIR)"

# Assemble release tree in dist/zoal-briefing (includes README).
release: plugin fonts
	rm -rf "$(RELEASE_DIR)"
	mkdir -p "$(RELEASE_DIR)/$(PLUGIN_PLATFORM_DIR)" "$(RELEASE_DIR)/fonts" "$(RELEASE_DIR)/licenses/inter"
	cp "$(PLUGIN_XPL)" "$(RELEASE_DIR)/$(PLUGIN_PLATFORM_DIR)/zoal-briefing.xpl"
	for weight in $(INTER_WEIGHTS); do cp "$(ZOAL_BRIEFING_INTER)/$$weight" "$(RELEASE_DIR)/fonts/"; done
	cp "$(ZOAL_BRIEFING_INTER)/LICENSE.txt" "$(RELEASE_DIR)/licenses/inter/LICENSE.txt"
	printf '%s\n' \
	  'zoal-briefing — voice briefing plugin for X-Plane' '' \
	  '1. Drop this zoal-briefing folder into X-Plane 12/Resources/plugins/' '' \
	  'Requires: X-Plane 12.4+ SDK (xpsdk_4.4.0-r2)' > "$(RELEASE_DIR)/README.txt"
	@echo "Release assembled in $(RELEASE_DIR)"

# --- cleanup -----------------------------------------------------------------

clean:
	cmake -E rm -rf "$(BUILD_DIR)" "$(PLUGIN_BUILD_DIR)" "$(AUDIO_BUILD_DIR)" "$(DIST_DIR)"
