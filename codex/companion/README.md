# Passive phone GPS companion

Forward an already-running ModemManager's already-enabled GNSS reports to an
already-connected PineTime running this firmware. The bridge respects GNOME's
location switch. It never enables GNSS/location, starts gpsd or ModemManager,
accesses modem ports, changes refresh rates, pairs, scans, connects or uploads
firmware. Disabled GNSS produces Unknown/no data, not an invented fix.

## Configuration

Requires Python 3.10+, PyGObject with Gio/GLib, the GNOME location settings schema,
BlueZ on adapter `hci0`, an existing ModemManager GNSS source, and the existing
`pinetime-watchmate.service` user unit. The watch must already be paired, bonded,
trusted, connected and exposing the encrypted GPS characteristic.

There is **no default device address**. Set `PINETIME_ADDRESS` to your selected
watch's existing Bluetooth address, or pass `--address`. Address validation runs
before any bus initialization; missing/invalid configuration exits clearly with
status 2. The command-line option takes precedence over the environment.

```sh
python3 gps_bridge.py --address "$PINETIME_ADDRESS" --once
```

For the optional user service, install the script as
`~/.local/libexec/pinetime-gps-bridge.py` and the supplied unit under
`~/.config/systemd/user/`. Create `~/.config/pinetime-gps-bridge.conf` containing
`PINETIME_ADDRESS=` followed by your watch's address. The service reads that file
and does not retry configuration errors. Enable it only after configuring its
existing phone/watch prerequisites. These files do not install or activate
themselves. Include this additional unit in any future firmware updater's
stop/restore handling.

## Data and freshness

MM supplies raw UTC/latitude/longitude/optional MSL altitude and NMEA traces.
The bridge reads only already-enabled sources, using a unique existing bus owner
and `NO_AUTO_START`, without interactive authorization. Lack of source access
leaves data unavailable. The GNOME location switch is checked before reading
and again immediately before sending.
[ModemManager Location API](https://www.freedesktop.org/software/ModemManager/doc/latest/ModemManager/gdbus-org.freedesktop.ModemManager1.Modem.Location.html),
[GIO call flags](https://gnome.pages.gitlab.gnome.org/gtk/gio/flags.DBusCallFlags.html)

Checksummed RMC supplies date/time, position and valid/no-fix state. Raw ISO UTC
is also accepted; raw time-only UTC requires a GNSS RMC date, never the host's
current date. Fresh coordinates remain usable with Unknown dimension when GSA
is absent. Reported GSA supplies 2D/3D; only reported 3D may carry altitude from
an epoch-matched GGA in MSL meters. GGA supplies used satellites and HDOP.
Estimated/manual/simulated reports are rejected. HDOP is dimensionless and is
not converted to a guessed meter accuracy.
[NMEA field reference](https://gpsd.io/NMEA.html)

MM can independently cache untimed GSA/GSV traces for at least 30 seconds.
Dimension and in-view counts are therefore **reported** values, not a guaranteed
instantaneous sky view. Repeated fragments/signals are not added, overall GN
totals are not combined with their individual constellations, and conflicting
totals become unknown. A current explicit no-fix report overrides cached 3D.
[MM cache semantics](https://www.freedesktop.org/software/ModemManager/doc/latest/ModemManager/gdbus-org.freedesktop.ModemManager1.Modem.Location.html#gdbus-property-org-freedesktop-ModemManager1-Modem-Location.Location)

The five-second loop reads GNSS only after the configured watch and service pass
their readiness checks. Source age includes synchronous RPC time before sending;
data expires after 30 seconds. Re-polling cached data cannot freshen it. A
ten-second heartbeat preserves fresh stationary fixes. Peer/permission
changes and failed writes cannot bypass the gates or poison deduplication.
Logs/status files contain state, age and byte count, never coordinates, altitude,
NMEA, payloads or location hashes. Optional status files use mode 0600.

## Tests and protocol

Run `python3 -m unittest -v test_gps_bridge.py` in this directory. All tests use
synthetic data and fake buses; the CLI configuration tests also verify failure
before GI/D-Bus initialization. The suite covers source validation, freshness,
permissions, peer selection, default-MTU writes, reconnect/heartbeat behavior,
failed writes, delayed RPCs, passive source calls and log privacy. A real GNSS
fix and physical watch behavior require separate hardware validation.

See [protocol.md](protocol.md) and [protocol-fixtures.json](protocol-fixtures.json)
for the exact 20-byte encrypted namespace 6 characteristic and synthetic shared
Python/C++ wire examples. Optional speed/course/meter accuracy are omitted from
v1. No personal device configuration or captured phone telemetry is included.
