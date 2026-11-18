#!/bin/bash
# Ensure the X-Plane SDK is available and up-to-date with upstream.
#
# Usage: ./scripts/ensure-sdk.sh [path_to_xplane_sdk] [--no-pull]
#
# If the given path is an existing git repository, it will fetch HEAD from
# upstream (default remote "origin") and compare against the local branch.
# When commits are available upstream, it offers to pull — in CI environments
# the automatic pull is suppressed unless --no-pull is not passed (in which
# case it pulls silently).  This keeps CI always building against the latest
# compatible SDK without manual intervention.
#
# Environment variables:
#   XPLANE_SDK_REMOTE    Override the remote name to check (default: origin)
#   CI_FORCE_CHECK_ONLY  Set to "1" to skip pull prompts in non-interactive mode
#
# Exit codes:
#   0  SDK found and healthy
#   1  SDK missing or invalid
#   2  Git error (non-fatal, reported but continues)

set -euo pipefail

XPLANE_SDK="${1:-$HOME/git/xplane-sdk}"
PULL_MODE="auto"  # auto, always, or never

# Parse --no-pull flag.
shift || true
for arg in "$@"; do
    case "$arg" in
        --no-pull) PULL_MODE="never" ;;
    esac
done

DEFAULT_REMOTE="${XPLANE_SDK_REMOTE:-origin}"
DEFAULT_BRANCH="main"   # LaminarResearch uses "main" for SDK v3

# ── Early validation ────────────────────────────────────────────────────────
if [ ! -d "$XPLANE_SDK" ]; then
    echo "ERROR: X-Plane SDK not found at $XPLANE_SDK"
    echo "Clone the SDK and re-run:"
    echo "  git clone https://github.com/LaminarResearch/X-Plane-SDK.git $XPLANE_SDK"
    exit 1
fi

if [ ! -f "$XPLANE_SDK/include/XPLMPlugin.h" ]; then
    echo "ERROR: $XPLANE_SDK does not look like an X-Plane SDK (missing XPLMPlugin.h)"
    echo "       Expected headers at: $XPLANE_SDK/include/"
    exit 1
fi

cd "$XPLANE_SDK"

# ── Determine pull behavior ─────────────────────────────────────────────────
is_ci() {
    # Detect common CI environments.
    local ci_vars=("CI" "GITHUB_ACTIONS" "GITLAB_CI" "TRAVIS" "CIRCLECI"
                   "JENKINS_URL" "BUILD_NUMBER" "DRONE")
    for var in "${ci_vars[@]}"; do
        if [ -n "${!var:-}" ]; then return 0; fi
    done
    # tty check as fallback: non-interactive = CI.
    [ ! -t 0 ] && return 0
    return 1
}

auto_pull() {
    # Decide whether to auto-pull in this environment.
    if [ "$PULL_MODE" = "always" ]; then return 0; fi
    if [ "$PULL_MODE" = "never" ]; then return 1; fi
    # In CI (non-interactive), always pull silently.
    is_ci && return 0 || return 1
}

# ── Git operations ──────────────────────────────────────────────────────────
if [ ! -d ".git" ]; then
    echo "WARNING: $XPLANE_SDK is not a git repository — cannot check for upstream updates."
    echo "        For CI compatibility, clone it as a git repo:"
    echo "          cd $(dirname "$XPLANE_SDK") && rm -rf $(basename "$XPLANE_SDK")"
    echo "          git clone https://github.com/LaminarResearch/X-Plane-SDK.git $(basename "$XPLANE_SDK")"
    echo "        Then re-run this script."
    exit 0   # Non-fatal; the SDK exists and has headers.
fi

echo "Checking $DEFAULT_REMOTE for X-Plane SDK updates..."

# Fetch — quiet in CI to avoid log noise, verbose otherwise.
if is_ci; then
    git fetch --quiet "$DEFAULT_REMOTE" 2>/dev/null || {
        echo "WARNING: Could not fetch from $DEFAULT_REMOTE (continuing with local HEAD)."
        exit 0
    }
else
    git fetch "$DEFAULT_REMOTE" 2>/dev/null || {
        echo "WARNING: Could not fetch from $DEFAULT_REMOTE — continuing with local HEAD."
        exit 0
    }
fi

LOCAL_HEAD=$(git rev-parse HEAD)
UPSTREAM_HEAD=$(git rev-parse "$DEFAULT_REMOTE/$DEFAULT_BRANCH" 2>/dev/null \
                    || git rev-parse "$DEFAULT_REMOTE/main" \
                    || git rev-parse "$DEFAULT_REMOTE/master")

if [ "$LOCAL_HEAD" = "$UPSTREAM_HEAD" ]; then
    echo "X-Plane SDK is up to date with $DEFAULT_REMOTE/$DEFAULT_BRANCH ($LOCAL_HEAD)"
else
    LOCAL_COUNT=$(git rev-list --count HEAD.."$UPSTREAM_HEAD" 2>/dev/null || echo "?")
    UPSTREAM_DESC=$(git log -1 --oneline "$UPSTREAM_HEAD" 2>/dev/null || echo "unknown")
    
    echo "Update available: $LOCAL_COUNT new commit(s) on $DEFAULT_REMOTE/$DEFAULT_BRANCH"
    echo "  Local:         $LOCAL_HEAD"
    echo "  Upstream head: $UPSTREAM_HEAD ($UPSTREAM_DESC)"

    if auto_pull; then
        echo "Auto-pulling in CI / non-interactive mode..."
        if git pull --ff-only 2>/dev/null; then
            echo "Pull succeeded. Continuing build with updated SDK."
        else
            echo "WARNING: Fast-forward failed — merge conflicts may exist."
            echo "Run 'git pull --rebase' manually, then re-run this script."
        fi
    else
        echo "Skipping automatic pull (interactive mode)."
        read -p "Pull updates now? [y/N] " answer
        case "$answer" in
            [yY]*)
                git pull --ff-only 2>/dev/null || {
                    echo "WARNING: Fast-forward failed — run 'git pull --rebase' manually."
                }
                ;;
            *) echo "Skipped. SDK left at local HEAD $LOCAL_HEAD";;
        esac
    fi
fi

echo "X-Plane SDK ready at $XPLANE_SDK"
