# PineTime GPS v1 wire protocol

Service: `00060000-78fc-48fe-8e23-433b3a1942d0`.
Writable characteristic: `00060001-78fc-48fe-8e23-433b3a1942d0`.
Namespace `00050000/00050001` already belongs to Weather and is not reused.

Every update is one **20-byte** value, little endian, so it fits the default ATT MTU 23 without fragments or a long write. The phone uses an acknowledged `WriteValue` request. Firmware requires an encrypted link; the bridge additionally requires this watch's existing pairing, bond, trust and resolved connection, active WatchMate, exact supported firmware and GPS characteristic. It never establishes that connection itself.

| Offset | Type | Field |
| --- | --- | --- |
| 0 | u8 | Version: 1 |
| 1 | u8 | Dimension: None 0, Unknown 1, TwoD 2, ThreeD 3 |
| 2 | u8 | Flags: Position 1, Altitude 2, Used 4, InView 8, HDOP 16; upper bits zero |
| 3 | u8 | Satellites used, or 255 when absent |
| 4 | u8 | Satellites in view, or 255 when absent |
| 5 | u8 | Reserved: 0 |
| 6 | u16 | GNSS source age in seconds, or 65535 unknown |
| 8 | i32 | Latitude in degrees×10⁷; zero when absent |
| 12 | i32 | Longitude in degrees×10⁷; zero when absent |
| 16 | i16 | Altitude above mean sea level, whole meters; zero when absent |
| 18 | u16 | Dimensionless HDOP×100; 65535 when absent |

Unknown dimension may carry a valid Position flag. None forbids Position; TwoD/ThreeD require it. Altitude requires ThreeD. Missing-field flags and canonical values must agree, including real zero coordinates/altitude being distinguished by flags. There is no guessed meter accuracy, speed or course in v1. HDOP is not an accuracy distance.

Age is calculated from the receiver's UTC report, including synchronous RPC time before sending. It is rounded upward; up to five seconds of source clock lead is tolerated and encoded as zero age. At 30 seconds it becomes unavailable. The watch adds local monotonic elapsed time to the transmitted age, expires at 30 seconds, and clears its data on disconnect/reconnect. Repeatedly polling or retransmitting a cached MM value cannot turn its old GNSS UTC into a new fix. The ten-second stationary heartbeat only keeps truly fresh, unchanged fixes alive.

None means a recent explicit receiver no-fix report, not disabled GNSS. When the phone's location switch is off, GNSS sources are disabled, the modem is unavailable, or no dated fresh report exists, the bridge sends Unknown with no fields and unknown age. Coordinates are then absent from the packet. Fresh horizontal coordinates remain usable if GSA dimension is unavailable. GSA dimension and GSV counts are **reported** values: their untimed NMEA sentences may be cached independently by ModemManager. A near-concurrent RMC invalid/no-fix or GGA quality 0 overrides a retained 3D report. No 3D dimension is inferred from an altitude number.

Synthetic shared golden fixture: 3D, latitude 37.3861°, longitude −122.0839°, altitude 12 m MSL, 8 used/12 in view, HDOP 0.90, age 2 seconds:

```text
01031f080c00020088aa4816a8793bb70c005a00
```

This is a fixed test example, not captured phone telemetry. `protocol-fixtures.json` contains additional canonical packets shared with native tests; `GpsData.h` defines the firmware codec.
