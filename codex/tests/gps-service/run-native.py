#!/usr/bin/env python3
"""Compile the actual GPS codec/store/service with sanitizer-checked NimBLE boundaries."""
import hashlib
import json
import os
import shlex
from pathlib import Path
import subprocess

TEST = Path(__file__).resolve().parent
CODEX = TEST.parent.parent
ROOT = CODEX.parent
SOURCE = ROOT / 'src'
BUILD = Path(os.environ.get('PINETIME_BUILD_DIR', ROOT / 'build')) / 'native-gps-service'
OUTPUT = TEST / 'output'
BUILD.mkdir(parents=True, exist_ok=True)
OUTPUT.mkdir(exist_ok=True)
binary = BUILD / 'gps-service-test'
command = shlex.split(os.environ.get('CXX', 'clang++')) + ['-std=c++20', '-Wall', '-Wextra', '-Werror', '-pedantic', '-O1', '-g',
           '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-pthread',
           '-I', str(TEST / 'stubs'), '-I', str(SOURCE), str(TEST / 'gps_test.cpp'),
           str(SOURCE / 'components/ble/GpsService.cpp'), '-o', str(binary)]
compiled = subprocess.run(command, capture_output=True, text=True)
(TEST / 'build.log').write_text(compiled.stdout + compiled.stderr)
compiled.check_returncode()
ran = subprocess.run([str(binary)], capture_output=True, text=True)
(OUTPUT / 'execution.log').write_text(ran.stdout + ran.stderr)
ran.check_returncode()
result = json.loads(ran.stdout)
(OUTPUT / 'test-result.json').write_text(json.dumps(result, indent=2) + '\n')
production = [SOURCE / 'components/ble' / name for name in ['GpsData.h', 'GpsService.h', 'GpsService.cpp']]
tests = [TEST / 'gps_test.cpp', TEST / 'run-native.py', TEST / 'README.md', *sorted((TEST / 'stubs').rglob('*.h'))]
receipt = {
    'result': result,
    'sanitizers': ['ASan', 'UBSan'],
    'boundary_stubs': ['NimBLE GATT registration and chained mbuf', 'FreeRTOS tick counter'],
    'source_sha256': {str(path.relative_to(SOURCE)): hashlib.sha256(path.read_bytes()).hexdigest() for path in production},
    'test_sha256': {str(path.relative_to(TEST)): hashlib.sha256(path.read_bytes()).hexdigest() for path in tests},
    'warnings_or_errors': False, 'uploaded': False, 'phone_actions': [],
}
(TEST / 'test-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps(receipt, indent=2))
