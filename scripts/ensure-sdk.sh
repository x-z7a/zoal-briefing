#!/usr/bin/env bash
# ensure-sdk.sh — Ensures the local X-Plane SDK is up-to-date.
# Downloads from the official X-Plane GitHub releases (not WordPress CDN):
#   https://github.com/x-plane/X-Plane-SDK/releases/tag/v4.4.0-b5
# Avoids git clone (which can fail with auth issues) by downloading
# release ZIP archives directly.
#
# This script is a convenience for developers who want to fetch/update
# the SDK without running CMake. The CMakeLists.txt uses cmake/XPlaneSDK.cmake
# which does the same thing automatically on first build.

set -euo pipefail

SDK_DIR="${1:-./sdk}"
VERSION="4.4.0-b5"
SDK_URL="https://github.com/x-plane/X-Plane-SDK/archive/refs/tags/v${VERSION}.zip"

echo ">>> Checking X-Plane SDK v${VERSION} upstream status..."

# Check if we already have a valid SDK (flattened structure)
if [ -f "${SDK_DIR}/XPLM/XPLMPlugin.h" ] || [ -f "${SDK_DIR}/CHeaders/XPLM/XPLMPlugin.h" ]; then
    echo "[SDK-CURRENT] SDK is present at ${SDK_DIR}"
else
    echo "[SDK-INIT] Downloading X-Plane SDK v${VERSION} from GitHub releases..."
    
    rm -rf "${SDK_DIR}"
    mkdir -p "${SDK_DIR}"
    
    TMPDIR=$(mktemp -d)
    curl -fsSL --connect-timeout 30 --max-time 300 \
        "${SDK_URL}" -o "${TMPDIR}/xpsdk.zip" || {
            echo "[SDK-WARN] Download failed — SDK may be unavailable."
            rm -rf "${TMPDIR}"
            exit 1
        }
    
    unzip -qo "${TMPDIR}/xpsdk.zip" -d "${TMPDIR}/src"
    
    # Flatten the extracted zip into sdk/ directory
    FLATTEN_DIR=$(find "${TMPDIR}/src" -maxdepth 1 -type d | head -1)
    cp -R "${FLATTEN_DIR}/"* "${SDK_DIR}/"
    
    rm -rf "${TMPDIR}"
    echo "[SDK-OK] SDK installed at ${SDK_DIR}"
fi

echo ">>> SDK check complete."
