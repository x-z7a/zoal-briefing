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

# Local X-Plane plugin installation directory (override for testing on non-standard installs).
LOCAL_XPLANE_PLUGIN_DIR ?= $(HOME)/X-Plane 12/Resources/plugins/zoal-briefing

PLUGIN_XPL := $(PLUGIN_BUILD_DIR)/zoal-briefing.xpl

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

# On Windows the default CMake generator is Visual Studio (multi-config), which
# ignores -DCMAKE_BUILD_TYPE and places outputs in a <Config>/ subdirectory.
# Force MinGW Makefiles (a single-config generator) so the plugin lands at
# build-plugin/zoal-briefing.xpl, matching the packaging step below.
ifeq ($(OS),Windows_NT)
CMAKE_GENERATOR := -G "MinGW Makefiles"
else
CMAKE_GENERATOR :=
endif

.PHONY: help test plugin install-plugin release clean

.DEFAULT_GOAL := help

help:
	@echo "zoal-briefing — X-Plane voice briefing plugin"
	@echo
	@echo "  test            build and run the SDK-free C++ core tests"
	@echo "  plugin          build the .xpl (fetches the X-Plane SDK from GitHub releases)"
	@echo "  install-plugin  build and install into X-Plane ($(LOCAL_XPLANE_PLUGIN_DIR))"
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
else
	cmake --build "$(BUILD_DIR)"
endif

# Build plugin (fetches X-Plane SDK from GitHub releases automatically)
plugin:
	@echo ">>> Building zoal-briefing .xpl plugin..."
	cmake -S . -B "$(PLUGIN_BUILD_DIR)" $(CMAKE_GENERATOR) \
		-DZoalBriefing_BUILD_TESTS=OFF \
		-DZoalBriefing_BUILD_PLUGIN=ON \
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

install-plugin: plugin
	@plugins_dir="$$(dirname "$(LOCAL_XPLANE_PLUGIN_DIR)")"; \
	if [ ! -d "$$plugins_dir" ]; then \
		echo "No X-Plane plugins folder at $$plugins_dir" >&2; \
		echo "Pass LOCAL_XPLANE_PLUGIN_DIR=/path/to/X-Plane/Resources/plugins/zoal-briefing" >&2; \
		exit 1; \
	fi; \
	rm -rf "$(LOCAL_XPLANE_PLUGIN_DIR)"; \
	mkdir -p "$(LOCAL_XPLANE_PLUGIN_DIR)"; \
	cp "$(PLUGIN_XPL)" "$(LOCAL_XPLANE_PLUGIN_DIR)/"; \
	echo "Installed zoal-briefing.xpl into $(LOCAL_XPLANE_PLUGIN_DIR)"

# Assemble release tree in dist/zoal-briefing (includes README).
release: plugin
	rm -rf "$(RELEASE_DIR)"
	mkdir -p "$(RELEASE_DIR)"
	cp "$(PLUGIN_XPL)" "$(RELEASE_DIR)/"
	printf '%s\n' \
	  'zoal-briefing — voice briefing plugin for X-Plane' '' \
	  '1. Copy zoal-briefing.xpl into X-Plane 12/Resources/plugins/' '' \
	  '   Requires: X-Plane 12.4+ SDK (v4.4.0-b5)' > "$(RELEASE_DIR)/README.txt"
	@echo "Release assembled in $(RELEASE_DIR)"

# --- cleanup -----------------------------------------------------------------

clean:
	cmake -E rm -rf "$(BUILD_DIR)" "$(PLUGIN_BUILD_DIR)" "$(DIST_DIR)"
