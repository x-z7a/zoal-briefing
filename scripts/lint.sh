#!/usr/bin/env bash
set -euo pipefail

# clang-tidy over the C++ files that differ from HEAD (tracked changes and new
# files), using each file's entry in a build's compile_commands.json. The old
# stub modules are not gated; LINT_ALL=1 lints every file a build compiles.
#
# Usage: scripts/lint.sh [build_dir...]   (default: build build-plugin build-audio)
# Exit status is clang-tidy's: non-zero when it reports a warning.

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

tidy="${CLANG_TIDY:-}"
if [ -z "${tidy}" ]; then
  for candidate in "$(command -v clang-tidy 2>/dev/null || true)" \
      /opt/homebrew/opt/llvm/bin/clang-tidy /usr/local/opt/llvm/bin/clang-tidy; do
    if [ -n "${candidate}" ] && [ -x "${candidate}" ]; then tidy="${candidate}"; break; fi
  done
fi
if [ -z "${tidy}" ]; then
  echo "clang-tidy not found (brew install llvm, or set CLANG_TIDY=/path/to/clang-tidy)" >&2
  exit 1
fi

build_dirs=("$@")
[ ${#build_dirs[@]} -gt 0 ] || build_dirs=(build build-plugin build-audio)
dbs=()
for dir in "${build_dirs[@]}"; do
  [ -f "${dir}/compile_commands.json" ] && dbs+=("${dir}")
done
if [ ${#dbs[@]} -eq 0 ]; then
  echo "no compile_commands.json in ${build_dirs[*]}; run make test (or make plugin) first" >&2
  exit 1
fi

# Homebrew's clang-tidy does not know where Apple's SDK is.
extra=()
if [ "$(uname -s)" = "Darwin" ] && command -v xcrun >/dev/null 2>&1; then
  extra+=("--extra-arg=-isysroot$(xcrun --show-sdk-path)")
fi

if [ "${LINT_ALL:-0}" = "1" ]; then
  files="$(git ls-files 'src/*.cpp' 'test/*.cpp' 'tests/*.cpp' 'tools/*.cpp')"
else
  files="$( { git diff --name-only HEAD -- 'src/*.cpp' 'test/*.cpp' 'tests/*.cpp' 'tools/*.cpp';
              git ls-files --others --exclude-standard -- 'src/*.cpp' 'test/*.cpp' 'tests/*.cpp' 'tools/*.cpp'; } | sort -u)"
fi

status=0
linted=0
for file in ${files}; do
  [ -f "${file}" ] || continue
  db=""
  for dir in "${dbs[@]}"; do
    if grep -q "\"file\": \"${repo_root}/${file}\"" "${dir}/compile_commands.json"; then db="${dir}"; break; fi
  done
  if [ -z "${db}" ]; then
    echo "skip ${file} (not compiled by ${dbs[*]})"
    continue
  fi
  echo "lint ${file}"
  linted=$((linted + 1))
  "${tidy}" -p "${db}" --quiet "${extra[@]}" "${file}" 2>/dev/null || status=1
done
[ "${linted}" -gt 0 ] || echo "nothing to lint (no changed .cpp files; LINT_ALL=1 for all)"
exit "${status}"
