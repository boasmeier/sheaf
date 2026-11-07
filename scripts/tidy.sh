#!/usr/bin/env bash
# Run clang-tidy on all project sources. Usage: scripts/tidy.sh [build-dir]
# CMake's compile database contains GCC-only module-scanning flags that clang-tidy rejects,
# so run it on a filtered copy.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="${1:-$root/build}"
filtered="$(mktemp -d)"
trap 'rm -rf "$filtered"' EXIT
sed -E 's/ -fdeps-format=[^ ]+//g; s/ -fmodules-ts//g; s/ -fmodule-mapper=[^ ]+//g; s/ -MD -MT [^ ]+ -MF [^ ]+//g' \
  "$build/compile_commands.json" > "$filtered/compile_commands.json"
cd "$root"
find src tests -name '*.cpp' -print0 | xargs -0 -n 1 -P "$(nproc)" clang-tidy -p "$filtered" --quiet
