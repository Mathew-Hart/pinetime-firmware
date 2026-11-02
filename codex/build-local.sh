#!/bin/sh
# Build the application only. No phone or watch access.
set -eu
codex_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$codex_root/.." && pwd)
deps_root=${PINETIME_DEPS_ROOT:-"$codex_root/deps"}
build_root=${PINETIME_BUILD_DIR:-"$repo_root/build"}
arm_root=${ARM_NONE_EABI_TOOLCHAIN_PATH:-"$deps_root/gcc-arm-none-eabi-10.3-2021.10"}
sdk_root=${NRF5_SDK_PATH:-"$deps_root/nRF5_SDK_15.3.0_59ac345"}

# Prefer explicit tool overrides, then a local dependency environment, then PATH.
if [ -z "${CMAKE:-}" ]; then
  if [ -x "$deps_root/venv/bin/cmake" ]; then CMAKE="$deps_root/venv/bin/cmake"; else CMAKE=cmake; fi
fi
if [ -z "${PYTHON:-}" ]; then
  if [ -x "$repo_root/.venv/bin/python3" ]; then PYTHON="$repo_root/.venv/bin/python3"
  elif [ -x "$deps_root/venv/bin/python3" ]; then PYTHON="$deps_root/venv/bin/python3"
  else PYTHON=python3; fi
fi
PYTHON=$(command -v "$PYTHON")
if [ -n "${NODE_BIN_DIR:-}" ]; then
  PATH="$NODE_BIN_DIR:$PATH"
else
  for node_bin in "$deps_root"/node-v*/bin; do
    if [ -x "$node_bin/node" ]; then PATH="$node_bin:$PATH"; break; fi
  done
fi
export PATH="$arm_root/bin:$(dirname -- "$PYTHON"):$codex_root/tooling/node_modules/.bin:$deps_root/npm/node_modules/.bin:$PATH"

[ -x "$arm_root/bin/arm-none-eabi-gcc" ] || { printf '%s\n' 'Set ARM_NONE_EABI_TOOLCHAIN_PATH to GNU Arm 10.3-2021.10.' >&2; exit 1; }
[ -d "$sdk_root/components" ] || { printf '%s\n' 'Set NRF5_SDK_PATH to nRF5 SDK 15.3.0_59ac345.' >&2; exit 1; }
"$CMAKE" -G Ninja -S "$repo_root" -B "$build_root" \
  -U WATCHFACE_TYPES -U USERAPP_TYPES \
  -DCMAKE_TOOLCHAIN_FILE="$repo_root/cmake-nRF5x/arm-gcc-toolchain.cmake" \
  -DARM_NONE_EABI_TOOLCHAIN_PATH="$arm_root" \
  -DARM_NONE_EABI_TOOLCHAIN_BIN_PATH="$arm_root/bin" \
  -DNRF5_SDK_PATH="$sdk_root" \
  -DPython3_EXECUTABLE="$PYTHON" \
  -DCMAKE_BUILD_TYPE=Release -DTARGET_DEVICE=PINETIME \
  -DBUILD_DFU=1 -DBUILD_RESOURCES=0
"$CMAKE" --build "$build_root" --target pinetime-mcuboot-app --parallel "${BUILD_JOBS:-8}"
