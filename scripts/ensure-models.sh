#!/usr/bin/env bash
set -euo pipefail

# Download the audio pipeline's models, pinned by checksum in
# scripts/models-version.txt:
#   openwakeword/  the shared front end (melspectrogram, embedding) and the
#                  "hey jarvis" model, used until cookie_monster.onnx is trained
#                  (docs/wake-word-training.md)
#   whisper/       speech to text (small.en, 5-bit quantized)
#   piper/         the text-to-speech voice (en_US lessac, sherpa-onnx packaging)
#
# A trained openwakeword/cookie_monster.onnx is yours to drop in; this script
# leaves anything else in the folder alone.
#
# Usage: ensure-models.sh [models_dir]   (default: .cache/models)

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
pin_file="${repo_root}/scripts/models-version.txt"
models_dir="${1:-${ZOAL_BRIEFING_MODELS:-${repo_root}/.cache/models}}"

sum() {
  local out
  out="$(shasum -a 256 "$1" 2>/dev/null || sha256sum "$1")"
  echo "${out%% *}"
}

grep -v '^#' "${pin_file}" | tr -d '\r' | while read -r path checksum url; do
  [ -n "${path}" ] || continue
  dest="${models_dir}/${path}"
  case "${path}" in
    *.tar.bz2)
      # Unpacked into a folder of the archive's name; the archive is not kept.
      unpacked="${dest%.tar.bz2}"
      [ -d "${unpacked}" ] && continue
      ;;
    *)
      [ -f "${dest}" ] && continue
      ;;
  esac

  mkdir -p "$(dirname "${dest}")"
  tmp="${dest}.part"
  echo "Downloading ${path}..."
  curl -fsSL -o "${tmp}" "${url}"
  actual="$(sum "${tmp}")"
  if [ "${actual}" != "${checksum}" ]; then
    rm -f "${tmp}"
    echo "${path} checksum is ${actual}, want ${checksum}" >&2
    exit 1
  fi
  case "${path}" in
    *.tar.bz2)
      tar -xjf "${tmp}" -C "$(dirname "${dest}")"
      rm -f "${tmp}"
      ;;
    *) mv "${tmp}" "${dest}" ;;
  esac
done
echo "models ready in ${models_dir}"
