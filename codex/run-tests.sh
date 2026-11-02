#!/bin/sh
# Native tests and fake-only companion checks. No hardware access.
set -eu
codex_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$codex_root/.." && pwd)
deps_root=${PINETIME_DEPS_ROOT:-"$codex_root/deps"}
PINETIME_BUILD_DIR=${PINETIME_BUILD_DIR:-"$repo_root/build"}
if [ -z "${CMAKE:-}" ]; then
  if [ -x "$deps_root/venv/bin/cmake" ]; then CMAKE="$deps_root/venv/bin/cmake"; else CMAKE=cmake; fi
fi
if [ -z "${PYTHON:-}" ]; then
  if [ -x "$repo_root/.venv/bin/python3" ]; then PYTHON="$repo_root/.venv/bin/python3"
  elif [ -x "$deps_root/venv/bin/python3" ]; then PYTHON="$deps_root/venv/bin/python3"
  else PYTHON=python3; fi
fi
export CMAKE PYTHON PINETIME_BUILD_DIR
export CC=${CC:-clang} CXX=${CXX:-clang++}
[ -f "$PINETIME_BUILD_DIR/src/displayapp/fonts/solar_time_42.c" ] || {
  printf '%s\n' 'Generate the firmware fonts first: sh codex/build-local.sh' >&2
  exit 1
}
"$PYTHON" "$codex_root/tests/gps-service/run-native.py"
sh "$codex_root/tests/gps-face/run-ui.sh"
sh "$codex_root/tests/face-selection/run-ui.sh"
"$PYTHON" -m unittest discover -s "$codex_root/companion" -p 'test_*.py' -v
"$PYTHON" "$codex_root/tests/protocol-roundtrip/verify.py"
