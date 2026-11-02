# GPS face selection integration checks

Result: **180 assertions passed**, zero native build warnings. Receipt: `test-receipt.json`.

Run `sh run-ui.sh` locally after the GPS build generates its fonts. This compiles actual wf4 `SettingWatchFace.cpp`, `Settings.cpp`, the production cycle helper, ScreenList/PageIndicator and LVGL. Production CMake generates nine configured faces with stable IDs `[0,1,3,7,8,9,10,11,12]`; removed IDs `[2,4,5,6]` remain reserved.

An existing eight-face sidecar preserves previous choices and includes GPS by default. Actual Settings writes/reloads verify independent bit12 persistence, hidden removed-face bits, and saved current-face roundtrips for Elegant, Binary, Earth, Solar and GPS. Pointer clicks verify 4/4/1 pagination, exactly nine names, three inert blank rows on the GPS page, independent choices and no current-face changes. The production cycle helper visits GPS, wraps to Digital, skips unchecked/unavailable GPS, and remains empty with all nine unchecked even when removed faces remain enabled.

Only filesystem storage, brightness hardware and DisplayApp refresh requests are stubbed. Fonts come from the current GPS build. No phone, Bluetooth, uploads or watch firmware validation actions occur. Actual rendered picker pages are under `output`.
