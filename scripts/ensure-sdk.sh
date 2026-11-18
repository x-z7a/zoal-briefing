#!/usr/bin/env bash
# ensure-sdk.sh — Ensures the local X-Plane SDK is up-to-date with upstream.
# Designed for CI compatibility: branch auto-detection, no-merge conflicts,
# and automatic synchronization of the `sdk/` directory.

set -euo pipefail

SDK_REPO="${1:-https://github.com/X-Plane/xpsdk.git}"
LOCAL_SDK_DIR="./sdk"

echo ">>> Checking X-Plane SDK upstream status..."

if [ ! -d "$LOCAL_SDK_DIR/.git" ]; then
    echo "[SDK-INIT] Cloning fresh X-Plane SDK to $LOCAL_SDK_DIR"
    git clone --depth 1 --single-branch \
        --branch master \
        "$SDK_REPO" "$LOCAL_SDK_DIR" || {
            echo "[SDK-WARN] Clone failed — SDK may be unavailable. Skipping update."
            exit 0
        }
else
    cd "$LOCAL_SDK_DIR"

    # Fetch latest refs without modifying working tree yet
    if ! git fetch origin >/dev/null 2>&1; then
        echo "[SDK-WARN] Fetch failed — SDK repo may be offline. Skipping update."
        exit 0
    fi

    # Detect the correct remote ref (master vs main)
    REMOTE_REF="origin/master"
    if ! git rev-parse "$REMOTE_REF" >/dev/null 2>&1; then
        echo "[SDK-WARN] origin/master not found. Trying origin/main..."
        REMOTE_REF="origin/main"
        if ! git rev-parse "$REMOTE_REF" >/dev/null 2>&1; then
            echo "[SDK-ERROR] Neither origin/master nor origin/main found."
            exit 0
        fi
    fi

    LOCAL_HEAD=$(git rev-parse HEAD)
    REMOTE_HEAD=$(git rev-parse "$REMOTE_REF")

    if [ "$LOCAL_HEAD" != "$REMOTE_HEAD" ]; then
        echo "[SDK-UPDATE] Upstream has changed! Syncing sdk/ directory..."

        # Force checkout to avoid merge conflicts in automated builds
        git config core.autocrlf false 2>/dev/null || true
        git checkout -f "$REMOTE_REF" -- >/dev/null 2>&1 && {
            echo "[SDK-OK] Forced sync complete."
        } || {
            # Last resort: re-clone the sdk/ directory
            cd ..
            rm -rf "$LOCAL_SDK_DIR"
            git clone --depth 1 --single-branch \
                --branch master \
                "$SDK_REPO" "$LOCAL_SDK_DIR" && \
                echo "[SDK-OK] Fresh clone as fallback." || \
                echo "[SDK-ERROR] Re-clone also failed. SDK unavailable."
        }
    else
        echo "[SDK-CURRENT] SDK is up to date."
    fi
fi

echo ">>> SDK check complete."
