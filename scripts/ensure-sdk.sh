#!/bin/bash
# Ensure the X-Plane SDK is available and up-to-date with upstream.
# Usage: ./scripts/ensure-sdk.sh [path_to_xplane_sdk]
#
# If the given path is an existing git repository, it will fetch HEAD from
# upstream (default remote "origin") and pull only when the local branch is
# behind.  This makes CI safe to run on every PR without stalling — if no
# update is needed the script exits cleanly; if a new commit exists it pulls
# it before the build proceeds.
#
# If the path does not exist, the script prints clone instructions and fails.

set -euo pipefail

XPLANE_SDK="${1:-$HOME/git/xplane-sdk}"
DEFAULT_REMOTE="origin"
DEFAULT_BRANCH="main"   # LaminarResearch uses "main" for SDK v3

if [ ! -d "$XPLANE_SDK" ]; then
    echo "ERROR: X-Plane SDK not found at $XPLANE_SDK"
    echo "Clone the SDK and re-run:"
    echo "  git clone https://github.com/LaminarResearch/X-Plane-SDK.git $XPLANE_SDK"
    exit 1
fi

# Verify it's a valid SDK checkout.
if [ ! -f "$XPLANE_SDK/include/XPLMPlugin.h" ]; then
    echo "ERROR: $XPLANE_SDK does not look like an X-Plane SDK (missing XPLMPlugin.h)"
    exit 1
fi

cd "$XPLANE_SDK"

# Check if it's a git repository.
if [ ! -d ".git" ]; then
    echo "WARNING: $XPLANE_SDK is not a git repository — cannot check for upstream updates."
    echo "        For CI compatibility, clone it as a git repo:"
    echo "          cd $(dirname "$XPLANE_SDK") && rm -rf $(basename "$XPLANE_SDK")"
    echo "          git clone https://github.com/LaminarResearch/X-Plane-SDK.git $(basename "$XPLANE_SDK")"
    echo "        Then re-run this script."
    exit 0   # Non-fatal; the SDK exists and has headers.
fi

# Determine remote URL to check. Use explicit --remote if set, otherwise
# default to LaminarResearch's upstream.
REMOTE="${DEFAULT_REMOTE}"
# Allow override via environment variable.
if [ -n "${XPLANE_SDK_REMOTE:-}" ]; then
    REMOTE="$XPLANE_SDK_REMOTE"
fi

echo "Checking $REMOTE for X-Plane SDK updates..."

# Fetch only (fast, no merge conflict risk in CI).
git fetch --quiet "$REMOTE" 2>/dev/null || {
    echo "WARNING: Could not fetch from $REMOTE — continuing with local HEAD."
    exit 0
}

LOCAL_HEAD=$(git rev-parse HEAD)
UPSTREAM_HEAD=$(git rev-parse "$REMOTE/$DEFAULT_BRANCH" 2>/dev/null || git rev-parse "$REMOTE/main")

if [ "$LOCAL_HEAD" = "$UPSTREAM_HEAD" ]; then
    echo "X-Plane SDK is up to date with $REMOTE/$DEFAULT_BRANCH ($LOCAL_HEAD)"
else
    LOCAL_COUNT=$(git rev-list --count HEAD.."$UPSTREAM_HEAD" 2>/dev/null || echo "?")
    echo "Update available: $LOCAL_COUNT new commit(s) on $REMOTE/$DEFAULT_BRANCH"
    echo "Local: $LOCAL_HEAD"
    echo "Upstream: $UPSTREAM_HEAD"

    # In CI (or when --no-pull flag is passed), just warn.
    if [ "${1:-}" != "--no-pull" ] && [ -z "${CI_FORCE_CHECK_ONLY:-}" ]; then
        read -p "Pull updates now? [y/N] " answer
        case "$answer" in
            [yY]*)
                git pull --ff-only 2>/dev/null || {
                    echo "WARNING: Fast-forward failed; there may be merge conflicts."
                    echo "Manual intervention required or run: git pull --rebase"
                }
                ;;
            *)
                echo "Skipped — SDK left at local HEAD $LOCAL_HEAD"
                ;;
        esac
    else
        echo "NOTE: CI_CHECK_ONLY set — skipping automatic pull (reporting only)."
    fi
fi

echo "X-Plane SDK headers verified at $XPLANE_SDK"
