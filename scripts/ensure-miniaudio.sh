#!/usr/bin/env bash
set -euo pipefail

# Provision miniaudio, the single-header library the plugin opens the
# microphone with (and the dev tool plays sound with). Pinned with the
# header's checksum in scripts/miniaudio-version.txt.

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
pin_file="${repo_root}/scripts/miniaudio-version.txt"
read -r version checksum < <(tr -d '\r' < "${pin_file}")
root="${ZOAL_BRIEFING_MINIAUDIO:-${repo_root}/.cache/miniaudio-${version}}"

if [ -f "${root}/miniaudio.h" ]; then
  exit 0
fi

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
echo "Downloading miniaudio ${version}..."
curl -fsSL -o "${work}/miniaudio.h" \
  "https://raw.githubusercontent.com/mackron/miniaudio/${version}/miniaudio.h"

actual="$(shasum -a 256 "${work}/miniaudio.h" 2>/dev/null || sha256sum "${work}/miniaudio.h")"
actual="${actual%% *}"
if [ "${actual}" != "${checksum}" ]; then
  echo "miniaudio ${version} checksum is ${actual}, want ${checksum}" >&2
  exit 1
fi

mkdir -p "${root}"
mv "${work}/miniaudio.h" "${root}/miniaudio.h"
echo "miniaudio ${version} at ${root}"
