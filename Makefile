# ============================================================================
# BriefingMaster — Makefile for local development & CI
# Inspired by zoal-atc: install-plugin copies .xpl to X-Plane plugins dir,
# plugin target builds + installs in one step.
# ============================================================================
SHELL := /bin/bash
.NOTPARALLEL:

XPLANE_DEV_PLUGINS ?= $(HOME)/X-Plane 12/Resources/Plugins/zoal_briefing/mac_x64
CMAKE = cmake
BUILD_TYPE ?= Debug
VERBOSE ?= -v

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  HOST_OS := macos
else ifeq ($(UNAME_S),Linux)
  HOST_OS := linux
else ifeq ($(OS),Windows_NT)
  HOST_OS := windows
endif

.PHONY: help build-tests build-core build-plugin install-plugin \
        test test-verbose sdk-update sdk-reset clean distclean plugin

.DEFAULT_GOAL := help

# ============================================================================
# Help
# ============================================================================
help: ## Show this help message
	@grep -E '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | \
	 awk 'BEGIN {FS = ":.*## "}; {printf "\033[36m%-22s\033[0m %s\n", $$1, $$2}'

# ============================================================================
# Core targets — build the core library (no X-Plane SDK needed for tests)
# ============================================================================
build-tests: ## Build core library + unit tests (no SDK required)
	$(CMAKE) -B build-test \
		-DZoalBriefing_BUILD_TESTS=ON \
		-DZoalBriefing_BUILD_PLUGIN=OFF \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	$(CMAKE) --build build-test --config $(BUILD_TYPE) $(VERBOSE)

build-core: ## Build only the core library (static)
	$(CMAKE) -B build-core \
		-DZoalBriefing_BUILD_TESTS=OFF \
		-DZoalBriefing_BUILD_PLUGIN=OFF \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	$(CMAKE) --build build-core --config $(BUILD_TYPE) $(VERBOSE)

# ============================================================================
# Plugin — builds the X-Plane plugin module (.xpl)
# ============================================================================
build-plugin: ## Build the X-Plane plugin (requires SDK headers)
	$(CMAKE) -B build-plugin \
		-DZoalBriefing_BUILD_TESTS=ON \
		-DZoalBriefing_BUILD_PLUGIN=ON \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	$(CMAKE) --build build-plugin --config $(BUILD_TYPE) $(VERBOSE)

install-plugin: build-plugin ## Build + install .xpl to X-Plane Developer/Plugins
ifeq ($(HOST_OS),macos)
	@echo "Installing zoal_briefing.xpl → $(XPLANE_DEV_PLUGINS)"
	@mkdir -p "$(XPLANE_DEV_PLUGINS)" && \
	if [ -f build-plugin/zoal_briefing.xpl ]; then \
		cp build-plugin/zoal_briefing.xpl "$(XPLANE_DEV_PLUGINS)/"; \
	elif ls build-plugin/*zoal_briefing*.xpl 1>/dev/null 2>&1; then \
		cp build-plugin/*zoal_briefing*.xpl "$(XPLANE_DEV_PLUGINS)/"; \
	else \
		echo "ERROR: zoal_briefing.xpl not found in build-plugin/"; \
		exit 1; \
	fi
	@echo "Installed. Check $(XPLANE_DEV_PLUGINS)/zoal_briefing.xpl"
else ifeq ($(HOST_OS),windows)
	@echo "Installing to C:/X-Plane/Developer/Plugins/"
	@if exist build-plugin\zoal_briefing.xpl ( \
		mkdir "C:\X-Plane\Developer\Plugins" 2>nul && \
		copy /Y build-plugin\zoal_briefing.xpl "C:\X-Plane\Developer\Plugins\" \
	) else ( \
		echo "ERROR: zoal_briefing.xpl not found — run 'make build-plugin' first"; \
		exit 1 \
	)
else
	@echo "$(HOST_OS) install — manually place zoal_briefing.xpl in X-Plane/Plugins"
endif

# Quick one-command build + install for local testing
plugin: build-plugin install-plugin ## Build + install plugin
	@echo "Done — open X-Plane and check Developer/Logs/xplane.log."

# ============================================================================
# Test targets
# ============================================================================
test: build-tests ## Run unit tests
	cd build-test && ctest --output-on-failure -C $(BUILD_TYPE)

test-verbose: build-tests ## Run tests with verbose output
	cd build-test && ctest --output-on-failure -C $(BUILD_TYPE) --verbose

# ============================================================================
# SDK helpers
# ============================================================================
sdk-update: ## Fetch latest X-Plane SDK upstream changes
	bash scripts/ensure-sdk.sh

sdk-reset: ## Re-clone SDK from scratch
	rm -rf sdk && bash scripts/ensure-sdk.sh

# ============================================================================
# Cleanup
# ============================================================================
clean: ## Remove all build artifacts (keeps sdk/)
	rm -rf build-test build-core build-plugin
	@echo "Cleaned build dirs."

distclean: clean sdk-reset ## Full reset (including SDK re-clone)
	@echo "Distclean complete."
