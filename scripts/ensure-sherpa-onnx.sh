#!/usr/bin/env bash
set -euo pipefail

# Provision sherpa-onnx and the ONNX Runtime headers.
#
# sherpa-onnx is the runtime for two parts of the audio pipeline:
#   - text-to-speech: Piper (VITS) voices through sherpa-onnx's C API;
#   - wake word: openWakeWord's ONNX models, run directly on the ONNX Runtime
#     that ships inside the sherpa-onnx build.
# The release archives carry ONNX Runtime's libraries but not its headers, so
# the matching C API headers are fetched separately. Everything is pinned by
# checksum in scripts/sherpa-onnx-version.txt.
#
# macOS uses the static build (linked into the .xpl). Windows and Linux use
# the shared builds, whose libraries ship beside the .xpl.
#
# Usage: ensure-sherpa-onnx.sh [flavour]   (default: this machine's)

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
pin_file="${repo_root}/scripts/sherpa-onnx-version.txt"

pin() { tr -d '\r' < "${pin_file}" | awk -v key="$1" '$1 == key { print $2; exit }'; }

version="$(pin version)"
header_sum="$(pin header)"
ort_version="$(pin ort-version)"
flavour="${1:-${ZOAL_BRIEFING_SHERPA_FLAVOUR:-}}"
if [ -z "${flavour}" ]; then
  case "$(uname -s)-$(uname -m)" in
    Darwin-arm64) flavour=osx-arm64-static-lib ;;
    Linux-x86_64) flavour=linux-x64-shared-lib ;;
    MINGW*|MSYS*|CYGWIN*) flavour=win-x64-shared-MT-Release-lib ;;
    *) echo "no pinned sherpa-onnx build for $(uname -s)-$(uname -m)" >&2; exit 1 ;;
  esac
fi
archive_sum="$(pin "${flavour}")"
if [ -z "${archive_sum}" ]; then
  echo "no pin for sherpa-onnx flavour ${flavour} in ${pin_file}" >&2
  exit 1
fi
root="${ZOAL_BRIEFING_SHERPA_ONNX:-${repo_root}/.cache/sherpa-onnx-${version}-${flavour}}"

if [ -f "${root}/include/sherpa-onnx/c-api/c-api.h" ] \
   && [ -f "${root}/include/onnxruntime/onnxruntime_c_api.h" ] && [ -d "${root}/lib" ]; then
  exit 0
fi

sum() {
  local out
  out="$(shasum -a 256 "$1" 2>/dev/null || sha256sum "$1")"
  echo "${out%% *}"
}

fetch() { # url dest want_sum
  curl -fsSL -o "$2" "$1"
  local got
  got="$(sum "$2")"
  if [ "${got}" != "$3" ]; then
    echo "$(basename "$2") checksum is ${got}, want $3" >&2
    exit 1
  fi
}

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
name="sherpa-onnx-${version}-${flavour}"
echo "Downloading ${name}..."
fetch "https://github.com/k2-fsa/sherpa-onnx/releases/download/${version}/${name}.tar.bz2" \
  "${work}/${name}.tar.bz2" "${archive_sum}"
fetch "https://raw.githubusercontent.com/k2-fsa/sherpa-onnx/${version}/sherpa-onnx/c-api/c-api.h" \
  "${work}/c-api.h" "${header_sum}"

mkdir -p "${work}/src"
tar -xjf "${work}/${name}.tar.bz2" -C "${work}/src"
mkdir -p "${work}/src/${name}/include/sherpa-onnx/c-api" "${work}/src/${name}/include/onnxruntime"
mv "${work}/c-api.h" "${work}/src/${name}/include/sherpa-onnx/c-api/c-api.h"

echo "Downloading ONNX Runtime ${ort_version} headers..."
tr -d '\r' < "${pin_file}" | awk '$1 == "ort" { print $2, $3 }' | while read -r file want; do
  fetch "https://cdn.jsdelivr.net/gh/microsoft/onnxruntime@${ort_version}/include/onnxruntime/core/session/${file}" \
    "${work}/src/${name}/include/onnxruntime/${file}" "${want}"
done

mkdir -p "$(dirname "${root}")"
rm -rf "${root}"
mv "${work}/src/${name}" "${root}"
echo "sherpa-onnx ${version} ${flavour} at ${root}"
