# GPS telemetry codec, store and service

Run `python3 run-native.py` to compile the actual production `GpsData.h` and
`GpsService.cpp`, using ASan/UBSan and strict compiler warnings. Only NimBLE GATT
registration/chained mbufs and the FreeRTOS tick counter are boundary stubs.
No phone, Bluetooth adapter, location source or watch is accessed.

The 20-byte version-1 value fits a single write request at the default ATT MTU.
The service is `00060000-78fc-48fe-8e23-433b3a1942d0`, with encrypted write-only
characteristic `00060001-78fc-48fe-8e23-433b3a1942d0`. The existing weather
service actually uses the 0005 family, so that family must not be reused.

The new service is appended after all old services. After the database starts
and stored bonds/CCCDs are restored, `AnnounceDatabaseChange()` marks only
`[GPS service start handle, 0xffff]` changed, once per boot. It never invalidates
existing service handles and is not invoked on reconnect. The pinned NimBLE
stack stores the pending update for subscribed bonded peers and sends it after
their encryption/bond restoration. BlueZ's
[Service Changed handler](https://raw.githubusercontent.com/bluez/bluez/master/src/shared/gatt-client.c)
discovers the indicated range. The live client's new characteristic still
requires verification after an eventual installation; a client without a
restored subscription is not guaranteed to receive an indication. No pairing
or client cache is removed by this helper.

All multibyte fields are little endian:

| Offset | Bytes | Field |
|---|---|---|
| 0 | 1 | Version, exactly 1 |
| 1 | 1 | Fix: 0 none, 1 unknown dimension/source, 2 2D, 3 3D |
| 2 | 1 | Flags: position 1, altitude 2, used 4, in-view 8, HDOP 16 |
| 3 | 1 | Satellites used; 255 when absent |
| 4 | 1 | Satellites in view, as reported; 255 when absent |
| 5 | 1 | Reserved, zero |
| 6 | 2 | Provider sample age in seconds; 65535 means unknown |
| 8 | 4 | Signed latitude, degrees times 10^7 |
| 12 | 4 | Signed longitude, degrees times 10^7 |
| 16 | 2 | Signed MSL altitude, whole meters |
| 18 | 2 | HDOP times 100; 65535 when absent |

Absent coordinates and altitude must be zero. Unknown fix dimension may carry a
valid position, but a known 2D/3D fix requires position, and explicit no-fix must
not carry position. Altitude is accepted only with explicit 3D. HDOP is a
dimensionless dilution value, not a fabricated accuracy in meters. Satellite
counts can remain diagnostic on a fresh no-fix sample. Unknown or stale
provider ages never become fresh merely because their packets are resent.

The synthetic golden packet is
`01031f080c00020088aa4816a8793bb70c005a00`: position
37.3861/-122.0839, altitude 12 m MSL, used 8/in-view 12, HDOP 0.90, source age 2
seconds, 3D, all five flags. These are synthetic test coordinates.

The store has one publisher, the NimBLE host task. Every payload word and its
publication state are atomic. A reader never waits for an interrupted writer;
it returns `coherent=false` until the next display poll. Disconnect and the
following reconnect erase all history. Freshness is source age plus unsigned
FreeRTOS tick elapsed, and expires at 30 seconds. No wall-clock time or timezone
is needed. Tick arithmetic supports a counter rollover between receipt and a
read; as with other tick-based timeouts, elapsed intervals must be shorter than
one full counter period.

Checks cover independent literal decoding and signed unit boundaries, strict
length/version/reserved/flag/canonical-value validation, rejection without
partial updates, unknown fix dimension, unknown age, source replay, expiry and
tick rollover, 50,000 concurrent publications with whole-snapshot reads,
actual GATT registration/encryption/UUIDs, every chained-mbuf split, operation
and copy errors, narrow/idempotent Service Changed signaling, and clearing
through the actual service on disconnect. Registration, handle lookup and
indication calls are boundary stubs, so this is not a live BlueZ round trip.
