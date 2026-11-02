# Actual GPS watch-face integration checks

Result: **1,441 assertions passed**, zero native build warnings. Run `sh run-ui.sh` after the GPS build generates its fonts. Receipt: `test-receipt.json`.

The harness compiles production `WatchFaceGps`, actual `GpsData` decoder/atomic store, Screen callback, theme and LVGL. DateTime, Settings, Bluetooth connection state and the NimBLE GATT wrapper are replaced at their boundaries. Test packets pass through the actual decoder/store; freshness and connection resets are not hand-set snapshots. A separate explicit boundary case models an incoherent concurrent publication.

Checks cover initial RTC-cache refresh; 12/24-hour time; six decimal coordinates and hemisphere/rounding; 3D/2D/unknown fix dimension; missing fields; fresh no-fix satellite/HDOP data; zero and extreme signed values; altitude metres/rounded feet through the existing WeatherFormat preference; the exact30-second stale boundary; unknown source age; Bluetooth loss and reconnection reset. Stale, disconnected, unknown-age and incoherent states hide all measurements. HDOP is a dimensionless ratio, never converted into an invented accuracy distance. Satellite values are used/in-view, with each unknown component shown as `--`.

Actual rendered states undergo full label bounds and pairwise non-overlap checks, including the longest coordinate/status/value forms. The clock face leaves gestures/button handling to DisplayApp. Unchanged data emits no display flush. Twelve lifetimes return LVGL allocation and task counts to the initial baseline.

Counted native LVGL allocation: 2,408 bytes initially/after cleanup, 7,168 bytes at peak, or 4,760 additional bytes for the exercised path. This does not model the watch's complete FreeRTOS heap, stacks or fragmentation.

All coordinates in screenshots are synthetic fixtures, not phone measurements. On the watch the face has no demonstration fallback and displays only received fresh values. No phone, Bluetooth, upload or firmware validation actions occur in these tests.
