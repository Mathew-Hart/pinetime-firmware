# Watch face captures

These are actual 240×240 LVGL software captures of the firmware faces, using synthetic test input. Their face and helper source matches the [custom wf4 firmware](https://github.com/Mathew-Hart/pinetime-firmware/tree/main). Weather, heart rate, alarm, clock and GPS values shown are examples.

| File | Face |
| --- | --- |
| `weather-focus.png` | Weather focus, including Paris and Tokyo clocks |
| `analog.png` | Analog with weather and heart rate |
| `elegant.png` | Elegant, with two hands and tick marks |
| `binary.png` | Binary time |
| `earth.png` | Earth with day/night lighting |
| `solar-morning.png` | Solar at 08:23 |
| `solar-afternoon.png` | Solar at 13:15 |
| `gps.png` | GPS with a synthetic 3D fix |

Earth uses NASA **Blue Marble 2002** imagery. Credit: NASA Goddard Space Flight Center; image by Reto Stöckli, enhancements by Robert Simmon. Its clouds are historical, not live weather. See the [source and attribution](../../assets/earth/README.md).

The Solar captures use nominal sunrise at **06:00** and sunset at **18:00**. The path is illustrative, not a calculated solar altitude. The GPS face is rendered with sample data; live phone-to-watch GPS integration remains unverified on hardware.

[manifest.json](manifest.json) records image hashes, the source commit and face/helper hashes, and original capture harness names. All eight PNGs match their corresponding raw LVGL PPM pixels. Seven images have matching hashes in the original local test receipts. Weather focus has no original image-hash receipt; its raw pixels and matching source were checked directly. The [GPS host harness](../../tests/gps-face) is bundled in this repository; the other named original harnesses are provenance references.
