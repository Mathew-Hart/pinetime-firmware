#!/bin/sh
# Actual firmware/LVGL tests with host hardware boundaries. No device access.
set -eu
test_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
codex_root=$(CDPATH= cd -- "$test_root/../.." && pwd)
repo_root=$(CDPATH= cd -- "$codex_root/.." && pwd)
deps_root=${PINETIME_DEPS_ROOT:-"$codex_root/deps"}
if [ -z "${CMAKE:-}" ]; then
  if [ -x "$deps_root/venv/bin/cmake" ]; then CMAKE="$deps_root/venv/bin/cmake"; else CMAKE=cmake; fi
fi
if [ -z "${PYTHON:-}" ]; then
  if [ -x "$repo_root/.venv/bin/python3" ]; then PYTHON="$repo_root/.venv/bin/python3"
  elif [ -x "$deps_root/venv/bin/python3" ]; then PYTHON="$deps_root/venv/bin/python3"
  else PYTHON=python3; fi
fi
build_root=${PINETIME_BUILD_DIR:-"$repo_root/build"}
"$CMAKE" -G Ninja -S "$test_root" -B "$test_root/build" -DPINETIME_BUILD_DIR="$build_root" > "$test_root/configure.log" 2>&1
"$CMAKE" --build "$test_root/build" --parallel "${BUILD_JOBS:-4}" > "$test_root/build.log" 2>&1
mkdir -p "$test_root/output"
"$test_root/build/face-selection-test" "$test_root/output" > "$test_root/output/interaction.log" 2>&1
"$PYTHON" "$test_root/render-png.py" "$test_root/output"
"$PYTHON" "$codex_root/tests/record-ui-receipt.py" face-selection
cat "$test_root/output/test-result.json"
