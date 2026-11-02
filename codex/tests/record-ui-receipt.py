#!/usr/bin/env python3
"""Record fresh native UI results and portable input/output hashes."""
import hashlib
import json
import os
from pathlib import Path
import re
import sys

CODEX = Path(__file__).resolve().parent.parent
ROOT = CODEX.parent
SOURCE = ROOT / 'src'
BUILD = Path(os.environ.get('PINETIME_BUILD_DIR', ROOT / 'build'))
SUITES = {
    'gps-face': [
        'displayapp/screens/WatchFaceGps.h', 'displayapp/screens/WatchFaceGps.cpp',
        'components/ble/GpsData.h', 'components/ble/GpsService.h',
        'displayapp/screens/Screen.h', 'displayapp/screens/Screen.cpp',
        'displayapp/Controllers.h', 'displayapp/InfiniTimeTheme.h',
        'displayapp/InfiniTimeTheme.cpp', 'displayapp/apps/Apps.h.in', 'libs/lv_conf.h',
    ],
    'face-selection': [
        'components/settings/Settings.h', 'components/settings/Settings.cpp',
        'components/settings/WatchFaceSelection.h', 'displayapp/WatchFaceNavigation.h',
        'displayapp/screens/settings/SettingWatchFace.h',
        'displayapp/screens/settings/SettingWatchFace.cpp',
        'displayapp/screens/ScreenList.h', 'displayapp/widgets/PageIndicator.cpp',
        'displayapp/apps/Apps.h.in', 'displayapp/apps/CMakeLists.txt',
        'displayapp/InfiniTimeTheme.cpp', 'libs/lv_conf.h',
    ],
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    suite = sys.argv[1]
    test = CODEX / 'tests' / suite
    output = test / 'output'
    result = json.loads((output / 'test-result.json').read_text())
    assert result['result'] == 'PASS', result
    log = (test / 'build.log').read_text()
    warnings = len(re.findall(r'\bwarning:', log, re.IGNORECASE))
    assert not re.search(r'\berror:', log, re.IGNORECASE), 'native build error'
    assert warnings == 0, 'native build warning'
    inputs = [path for path in sorted(test.rglob('*')) if path.is_file()
              and path.suffix in {'.h', '.cpp', '.py', '.sh', '.md', '.txt'}
              and 'build' not in path.relative_to(test).parts
              and 'output' not in path.relative_to(test).parts
              and '__pycache__' not in path.relative_to(test).parts]
    receipt = dict(result)
    receipt.update({
        'source_sha256': {name: sha(SOURCE / name) for name in SUITES[suite]},
        'test_sha256': {str(path.relative_to(test)): sha(path) for path in inputs},
        'support_sha256': {str(path.relative_to(CODEX)): sha(path) for path in
                           [Path(__file__).resolve(), CODEX / 'host/FreeRTOS.h']},
        'render_sha256': {path.name: sha(path) for path in sorted(output.glob('*.png'))},
        'generated_font_input_sha256': {
            str(path.relative_to(BUILD / 'src')): sha(path)
            for path in sorted((BUILD / 'src/displayapp/fonts').glob('*.c'))
        },
        'generated_fonts_relative_to': 'PINETIME_BUILD_DIR/src',
        'firmware_version': '1.16.1-wf4',
        'warnings': warnings,
        'uploaded': False,
        'phone_actions': [],
    })
    if suite == 'gps-face':
        receipt['coordinates_in_previews'] = 'synthetic test fixtures'
        receipt['lvgl_peak_additional_bytes'] = result['lvgl_peak_bytes'] - result['lvgl_initial_bytes']
    (test / 'test-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__':
    main()
