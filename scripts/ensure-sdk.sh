#!/usr/bin/env bash
# ensure-sdk.sh
# Ensures the local X-Plane SDK is always up-to-date with upstream.
# Designed for CI compatibility: non-blocking, fast-fail on errors, 
# and automatic synchronization of the `sdk/` directory.

set -euo pipefail

SDK_REPO="${1:-https://github.com/X-Plane/xpsdk.git}"
LOCAL_SDK_DIR="./sdk"
REMOTE_REF="origin/main"

echo ">>> Checking X-Plane SDK upstream status for CI compatibility..."

if [ ! -d "$LOCAL_SDK_DIR/.git" ]; then
    echo "[SDK-INIT] Cloning fresh X-Plane SDK from $SDK_REPO to $LOCAL_SDK_DIR"
    git clone --depth 1 --single-branch "$SDK_REPO" "$LOCAL_SDK_DIR"
else
    cd "$LOCAL_SDK_DIR"
    
    # Fetch latest refs without modifying working tree yet
    git fetch origin >/dev/null 2>&1 || { echo "[SDK-ERROR] Failed to fetch from upstream."; exit 1; }
    
    LOCAL_HEAD=$(git rev-parse HEAD)
    REMOTE_HEAD=$(git rev-parse "$REMOTE_REF" 2>/dev/null || echo "")
    
    if [ -z "$REMOTE_HEAD" ]; then
        echo "[SDK-WARN] Remote ref $REMOTE_REF not found. Skipping update."
    elif [ "$LOCAL_HEAD" != "$REMOTE_HEAD" ]; then
        echo "[SDK-UPDATE] Upstream has changed! Syncing sdk/ directory for build..."
        
        # Fast-forward only to prevent merge commits in automated builds
        if git merge --ff-only "$REMOTE_REF" 2>/dev/null; then
            echo "[SDK-OK] Successfully fast-forwarded to latest upstream."
        else
            # If ff-only fails (diverged history), force update branch ref for CI safety
            echo "[SDK-FORCE] History diverged. Forcing checkout of remote HEAD..."
            git checkout -f "$REMOTE_REF"
            echo "[SDK-OK] Forced sync complete."
        fi
    else
        echo "[SDK-CURRENT] SDK is up to date with $REMOTE_REF."
    fi
fi

echo ">>> SDK check and sync complete. Proceeding with build."
