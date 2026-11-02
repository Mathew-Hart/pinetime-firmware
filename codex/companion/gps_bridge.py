#!/usr/bin/env python3
"""Passively forward existing ModemManager GNSS data to the connected PineTime.

Never enables location/GNSS, activates gpsd, accesses modem ports, starts a
service, scans, pairs, connects, changes firmware, or records coordinates.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
import json
import math
import os
from pathlib import Path
import re
import struct
import tempfile
import time

FIRMWARE_UUID = '00002a26-0000-1000-8000-00805f9b34fb'
GPS_UUID = '00060001-78fc-48fe-8e23-433b3a1942d0'
SUPPORTED_FIRMWARE = ('1.16.1-wf4',)
COMPANION = 'pinetime-watchmate.service'
MM = 'org.freedesktop.ModemManager1'
LOCATION = MM + '.Modem.Location'
MAX_AGE = 30
POLL_SECONDS = 5
HEARTBEAT_SECONDS = 10
UTC = timezone.utc
PACKET = struct.Struct('<BBBBBBHiihH')
assert PACKET.size == 20


def configured_address(argument=None, environment=None):
    environment = os.environ if environment is None else environment
    value = argument if argument is not None else environment.get('PINETIME_ADDRESS')
    if not isinstance(value, str) or not value.strip():
        raise ValueError('Set --address or PINETIME_ADDRESS for your already-paired PineTime')
    value = value.strip().upper()
    if not re.fullmatch(r'(?:[0-9A-F]{2}:){5}[0-9A-F]{2}', value):
        raise ValueError('PineTime address must contain six colon-separated hexadecimal bytes')
    return value


def device_path(address):
    return '/org/bluez/hci0/dev_' + address.replace(':', '_')


@dataclass(frozen=True)
class Sample:
    # None=0; Unknown=1; TwoD=2; ThreeD=3, matching GpsData.h.
    fix: int = 1
    age: int = 65535
    latitude: float | None = None
    longitude: float | None = None
    altitude: float | None = None
    used: int | None = None
    in_view: int | None = None
    hdop: float | None = None
    reason: str = 'source_unavailable'


def number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float, str)):
        return None
    try:
        result = float(value)
        return result if math.isfinite(result) else None
    except (ValueError, OverflowError):
        return None


def integer(value, limit=254):
    value = number(value)
    return int(value) if value is not None and value.is_integer() and 0 <= value <= limit else None


def time_of_day(value):
    if not isinstance(value, str) or not re.fullmatch(r'[0-9]{6}(?:\.[0-9]{1,9})?', value):
        return None
    try:
        return datetime(2000, 1, 1, int(value[:2]), int(value[2:4]), int(value[4:6]),
                        int(((value.split('.')[1] if '.' in value else '') + '000000')[:6]), tzinfo=UTC).timetz()
    except ValueError:
        return None


def sentences(text):
    """Only bounded ASCII GNSS sentences with a valid required checksum."""
    if not isinstance(text, str) or len(text) > 65536:
        return []
    result = []
    for line in text.splitlines()[:256]:
        match = re.fullmatch(r'\$([A-Z]{2}(?:RMC|GGA|GSA|GSV),[^*\r\n]{0,480})\*([0-9A-Fa-f]{2})', line.strip())
        if not match:
            continue
        body, checksum = match.groups()
        try:
            actual = 0
            for character in body.encode('ascii'):
                actual ^= character
        except UnicodeEncodeError:
            continue
        if actual == int(checksum, 16):
            fields = body.split(',')
            if fields[0][:2] in ('GP', 'GN', 'GL', 'GA', 'GB', 'BD', 'GI', 'GQ', 'QZ'):
                result.append(fields)
    return result


def coordinate(value, hemisphere, latitude):
    digits = 4 if latitude else 5
    if not isinstance(value, str) or not re.fullmatch(r'[0-9]{%d}(?:\.[0-9]+)?' % digits, value):
        return None
    positive, negative, limit = ('N', 'S', 90) if latitude else ('E', 'W', 180)
    if hemisphere not in (positive, negative):
        return None
    degrees, minutes = divmod(float(value), 100)
    if minutes >= 60 or degrees + minutes / 60 > limit:
        return None
    return (degrees + minutes / 60) * (1 if hemisphere == positive else -1)


def rmc_records(lines):
    result = []
    for fields in lines:
        if not fields[0].endswith('RMC') or len(fields) < 10 or fields[2] not in ('A', 'V'):
            continue
        tod = time_of_day(fields[1])
        if tod is None or not re.fullmatch(r'[0-9]{6}', fields[9]):
            continue
        try:
            year = int(fields[9][4:])
            year += 1900 if year >= 80 else 2000
            stamp = datetime.combine(datetime(year, int(fields[9][2:4]), int(fields[9][:2])).date(), tod).timestamp()
        except ValueError:
            continue
        mode = fields[12] if len(fields) > 12 else ''
        valid = fields[2] == 'A' and mode in ('', 'A', 'D', 'P', 'R', 'F')
        lat = coordinate(fields[3], fields[4], True)
        lon = coordinate(fields[5], fields[6], False)
        result.append({'stamp': stamp, 'valid': valid, 'explicit_no_fix': not valid,
                       'latitude': lat, 'longitude': lon})
    return result


def raw_record(raw, records):
    if not isinstance(raw, dict):
        return None
    lat, lon = number(raw.get('latitude')), number(raw.get('longitude'))
    if lat is None or lon is None or abs(lat) > 90 or abs(lon) > 180:
        return None
    value = raw.get('utc-time')
    if not isinstance(value, str):
        return None
    tod = time_of_day(value)
    try:
        if tod:
            dates = {datetime.fromtimestamp(item['stamp'], UTC).date() for item in records if item['valid']}
            if len(dates) != 1:
                return None
            stamp = datetime.combine(next(iter(dates)), tod).timestamp()
        else:
            if not re.fullmatch(r'[0-9]{4}-[0-9]{2}-[0-9]{2}[Tt ][0-9]{2}:[0-9]{2}:[0-9]{2}(?:\.[0-9]+)?(?:[Zz]|[+-][0-9]{2}:[0-9]{2})?', value):
                return None
            parsed = datetime.fromisoformat(value.replace('z', '+00:00').replace('Z', '+00:00'))
            stamp = (parsed if parsed.tzinfo else parsed.replace(tzinfo=UTC)).timestamp()
    except (ValueError, OverflowError):
        return None
    return {'stamp': stamp, 'valid': True, 'explicit_no_fix': False, 'latitude': lat, 'longitude': lon}


def reported_in_view(lines):
    # Header counts repeat across numbered fragments and signals. Never sum
    # fragments or GN mixed-system totals with individual constellations.
    groups = {}
    aliases = {'GB': 'BD', 'GQ': 'QZ'}
    for fields in lines:
        if not fields[0].endswith('GSV') or len(fields) < 4:
            continue
        total, sequence, count = integer(fields[1], 64), integer(fields[2], 64), integer(fields[3])
        if total is None or sequence is None or count is None or not 1 <= sequence <= total or count > total * 4:
            continue
        talker = aliases.get(fields[0][:2], fields[0][:2])
        groups.setdefault(talker, set()).add(count)
    if 'GN' in groups:
        return next(iter(groups['GN'])) if len(groups['GN']) == 1 else None
    if not groups or any(len(values) != 1 for values in groups.values()):
        return None
    total = sum(next(iter(values)) for values in groups.values())
    return total if total <= 254 else None


def reported_modes(lines):
    modes = set()
    for fields in lines:
        if not fields[0].endswith('GSA') or len(fields) not in (18, 19) or fields[1] not in ('A', 'M'):
            continue
        mode = integer(fields[2], 3)
        if mode not in (1, 2, 3) or any(value and integer(value, 999) in (None, 0) for value in fields[3:15]):
            continue
        modes.add(mode)
    return modes


def parse_location(location, now=None):
    """Parse an MM a{uv} snapshot, without treating polling as a fresh fix."""
    now = time.time() if now is None else now
    if number(now) is None or not isinstance(location, dict):
        return Sample(reason='invalid_source')
    lines = sentences(location.get(4, ''))
    records = rmc_records(lines)
    newest = max(records, key=lambda item: (item['stamp'], item['explicit_no_fix']), default=None)
    raw = raw_record(location.get(2), records)
    # A near-concurrent explicit no-fix report wins over a retained raw fix.
    chosen = raw if raw and (newest is None or raw['stamp'] > newest['stamp'] + 2) else newest
    if chosen is None:
        return Sample(reason='no_dated_gnss_report')
    age = now - chosen['stamp']
    if age < -5 or age >= MAX_AGE:
        return Sample(reason='stale_or_future_gnss_report')
    age = math.ceil(max(0, age))
    if age >= MAX_AGE:
        return Sample(reason='stale_gnss_report')
    stamp = datetime.fromtimestamp(chosen['stamp'], UTC)
    ggas = []
    for fields in lines:
        if not fields[0].endswith('GGA') or len(fields) < 11:
            continue
        tod = time_of_day(fields[1])
        if tod is None:
            continue
        candidate = datetime.combine(stamp.date(), tod).timestamp()
        if abs(candidate - chosen['stamp']) <= 2:
            ggas.append(fields)
    # Multiple disagreeing same-epoch GGA reports cannot safely be merged.
    gga = ggas[0] if ggas and all(fields[6:11] == ggas[0][6:11] for fields in ggas) else None
    if gga and integer(gga[6], 8) not in (None, 0) and chosen['latitude'] is not None and chosen['longitude'] is not None:
        gga_lat, gga_lon = coordinate(gga[2], gga[3], True), coordinate(gga[4], gga[5], False)
        if (gga_lat is None or gga_lon is None or abs(gga_lat - chosen['latitude']) > .001 or
                abs(gga_lon - chosen['longitude']) > .001):
            gga = None
    used = integer(gga[7]) if gga else None
    hdop = number(gga[8]) if gga else None
    hdop = hdop if hdop is not None and 0 < hdop < 655.35 else None
    in_view = reported_in_view(lines)
    if used is not None and in_view is not None and used > in_view:
        in_view = None
    if chosen['explicit_no_fix'] or (gga and integer(gga[6], 8) == 0):
        return Sample(fix=0, age=age, used=used, in_view=in_view, hdop=hdop, reason='no_fix')
    if chosen['latitude'] is None or chosen['longitude'] is None:
        return Sample(reason='invalid_position')
    # GGA quality6/7/8 is estimated/manual/simulated, not a satellite fix.
    if gga and integer(gga[6], 8) not in (1, 2, 3, 4, 5):
        return Sample(reason='non_gnss_gga_quality')
    modes = reported_modes(lines)
    mode3 = modes == {3}
    altitude = number(gga[9]) if gga and gga[10] == 'M' and mode3 else None
    altitude = altitude if altitude is not None and -32768 <= altitude <= 32767 else None
    # Unknown dimension may still carry a valid horizontal position. An
    # altitude value alone never establishes a3D solution.
    fix = 3 if mode3 else 2 if modes == {2} else 1
    return Sample(fix=fix, age=age, latitude=chosen['latitude'], longitude=chosen['longitude'],
                  altitude=altitude, used=used, in_view=in_view, hdop=hdop, reason='fix')


def rounded(value):
    return math.floor(value + .5) if value >= 0 else math.ceil(value - .5)


def encode(sample):
    if (not isinstance(sample, Sample) or isinstance(sample.fix, bool) or not isinstance(sample.fix, int) or
            sample.fix not in (0, 1, 2, 3) or isinstance(sample.age, bool) or not isinstance(sample.age, int) or
            not 0 <= sample.age <= 65535):
        raise ValueError('Invalid GNSS sample')
    flags, lat, lon, altitude, used, view, hdop = 0, 0, 0, 0, 255, 255, 65535
    has_position = sample.latitude is not None or sample.longitude is not None
    if sample.fix in (2, 3) and not has_position:
        raise ValueError('Known GNSS dimension requires a position')
    if has_position:
        if (number(sample.latitude) is None or number(sample.longitude) is None or
                abs(sample.latitude) > 90 or abs(sample.longitude) > 180 or sample.age >= MAX_AGE or sample.fix == 0):
            raise ValueError('Invalid GNSS position')
        flags |= 1
        lat, lon = rounded(sample.latitude * 10**7), rounded(sample.longitude * 10**7)
    if sample.altitude is not None:
        if sample.fix != 3 or number(sample.altitude) is None or not -32768 <= rounded(sample.altitude) <= 32767:
            raise ValueError('Invalid GNSS altitude')
        flags |= 2
        altitude = rounded(sample.altitude)
    for bit, value in ((4, sample.used), (8, sample.in_view)):
        if value is not None:
            count = integer(value)
            if count is None:
                raise ValueError('Invalid GNSS satellite count')
            flags |= bit
            if bit == 4:
                used = count
            else:
                view = count
    if sample.hdop is not None:
        if number(sample.hdop) is None or not 0 < rounded(sample.hdop * 100) < 65535:
            raise ValueError('Invalid GNSS HDOP')
        flags |= 16
        hdop = rounded(sample.hdop * 100)
    return PACKET.pack(1, sample.fix, flags, used, view, 0, sample.age, lat, lon, altitude, hdop)


def unpack(value):
    while hasattr(value, 'unpack'):
        value = value.unpack()
    if isinstance(value, dict):
        return {key: unpack(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return type(value)(unpack(item) for item in value)
    return value


class RuntimeIO:
    def __init__(self):
        from gi.repository import Gio, GLib
        self.Gio, self.GLib = Gio, GLib
        self.system = Gio.bus_get_sync(Gio.BusType.SYSTEM, None)
        self.session = Gio.bus_get_sync(Gio.BusType.SESSION, None)
        self.settings = Gio.Settings.new('org.gnome.system.location')

    def call(self, bus, name, path, interface, method, arguments=None):
        # NO_AUTO_START and absence of ALLOW_INTERACTIVE_AUTHORIZATION are
        # deliberate: source reads must not activate a modem service or prompt.
        return unpack(bus.call_sync(name, path, interface, method, arguments, None,
                      self.Gio.DBusCallFlags.NO_AUTO_START, 3000, None))

    def owner(self, bus, name):
        try:
            return self.call(bus, 'org.freedesktop.DBus', '/org/freedesktop/DBus',
                             'org.freedesktop.DBus', 'GetNameOwner', self.GLib.Variant('(s)', (name,)))[0]
        except self.GLib.Error:
            return None

    def objects(self):
        return self.call(self.system, 'org.bluez', '/', 'org.freedesktop.DBus.ObjectManager', 'GetManagedObjects')[0]

    def firmware(self, path):
        return bytes(self.call(self.system, 'org.bluez', path, 'org.bluez.GattCharacteristic1',
                     'ReadValue', self.GLib.Variant('(a{sv})', ({},)))[0])

    def write(self, path, packet):
        self.call(self.system, 'org.bluez', path, 'org.bluez.GattCharacteristic1', 'WriteValue',
                  self.GLib.Variant('(aya{sv})', (packet, {'type': self.GLib.Variant('s', 'request')})))

    def companion_active(self):
        path = self.call(self.session, 'org.freedesktop.systemd1', '/org/freedesktop/systemd1',
                         'org.freedesktop.systemd1.Manager', 'GetUnit', self.GLib.Variant('(s)', (COMPANION,)))[0]
        return self.call(self.session, 'org.freedesktop.systemd1', path, 'org.freedesktop.DBus.Properties', 'Get',
                         self.GLib.Variant('(ss)', ('org.freedesktop.systemd1.Unit', 'ActiveState')))[0] == 'active'

    def location_enabled(self):
        return self.settings.get_boolean('enabled')

    def modem_snapshot(self):
        owner = self.owner(self.system, MM)
        if owner is None:
            return 'modem_unavailable', None
        objects = self.call(self.system, owner, '/org/freedesktop/ModemManager1',
                            'org.freedesktop.DBus.ObjectManager', 'GetManagedObjects')[0]
        for path, interfaces in sorted(objects.items()):
            properties = interfaces.get(LOCATION, {})
            if int(properties.get('Capabilities', 0)) & 6 and int(properties.get('Enabled', 0)) & 6:
                value = self.call(self.system, owner, path, LOCATION, 'GetLocation')[0]
                enabled = int(properties['Enabled'])
                return 'ready', {key: item for key, item in value.items() if key in (2, 4) and enabled & key}
        return 'gnss_disabled', None


def target(objects, address):
    path_prefix = device_path(address)
    device = objects.get(path_prefix, {}).get('org.bluez.Device1', {})
    if (str(device.get('Address', '')).upper() != address or not device.get('Connected') or not device.get('ServicesResolved') or
            not all(device.get(key) for key in ('Paired', 'Bonded', 'Trusted')) or device.get('Blocked', True)):
        return None
    found = {}
    for path, interfaces in objects.items():
        characteristic = interfaces.get('org.bluez.GattCharacteristic1')
        if path.startswith(path_prefix + '/') and characteristic:
            uuid = str(characteristic.get('UUID', '')).lower()
            if uuid in (GPS_UUID, FIRMWARE_UUID):
                if uuid in found:
                    return None
                found[uuid] = (path, characteristic)
    if GPS_UUID not in found or FIRMWARE_UUID not in found or 'write' not in found[GPS_UUID][1].get('Flags', []):
        return None
    return found[FIRMWARE_UUID][0], found[GPS_UUID][0]


class Bridge:
    def __init__(self, io=None, status_file=None, clock=None, address=None):
        self.address = configured_address(address)
        self.io = io if io is not None else RuntimeIO()
        self.status_file = status_file
        self.clock = clock if clock is not None else time.monotonic
        self.last_packet, self.last_sent = None, float('-inf')

    def status(self, status, **details):
        # No coordinates, altitude, raw NMEA or payload/hash in logs/cache.
        document = {'status': status, 'utc': datetime.now(UTC).isoformat(), **details}
        print(json.dumps(document, sort_keys=True), flush=True)
        if self.status_file:
            self.status_file.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile(mode='w', dir=self.status_file.parent, delete=False) as output:
                os.chmod(output.name, 0o600)
                json.dump(document, output)
                output.write('\n')
            os.replace(output.name, self.status_file)
        return document

    def attempt(self, now=None, monotonic=None):
        started = self.clock()
        now = time.time() if now is None else now
        monotonic = started if monotonic is None else monotonic
        try:
            paths = target(self.io.objects(), self.address)
            if paths is None or not self.io.companion_active():
                self.last_packet = None
                return self.status('skipped', reason='watch_not_ready')
            if self.io.firmware(paths[0]).decode(errors='replace') not in SUPPORTED_FIRMWARE:
                self.last_packet = None
                return self.status('skipped', reason='unsupported_firmware')
            if not self.io.location_enabled():
                state, snapshot = 'location_disabled', None
            else:
                state, snapshot = self.io.modem_snapshot()
            # Permission and peer identity may change while a source RPC waits.
            if target(self.io.objects(), self.address) != paths:
                self.last_packet = None
                return self.status('skipped', reason='watch_changed')
            if not self.io.location_enabled():
                state, snapshot = 'location_disabled', None
            # Age at send time, including every preceding synchronous RPC.
            # Re-reading an old MM snapshot never refreshes its GNSS timestamp.
            elapsed = max(0, self.clock() - started)
            sample = parse_location(snapshot, now + elapsed) if state == 'ready' else Sample(reason=state)
            monotonic += elapsed
            packet = encode(sample)
            if packet == self.last_packet and monotonic - self.last_sent < HEARTBEAT_SECONDS:
                return self.status('unchanged', reason=sample.reason)
            self.last_packet = None
            self.io.write(paths[1], packet)
            self.last_packet, self.last_sent = packet, monotonic
            return self.status('sent', reason=sample.reason, fix=sample.fix, source_age_seconds=sample.age, bytes=len(packet))
        except Exception:
            self.last_packet = None
            return self.status('skipped', reason='source_or_transport_error')


def main(argv=None, environment=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--once', action='store_true')
    parser.add_argument('--status-file', type=Path)
    parser.add_argument('--address', help='Already-paired PineTime address (or set PINETIME_ADDRESS)')
    arguments = parser.parse_args(argv)
    try:
        address = configured_address(arguments.address, environment)
    except ValueError as error:
        parser.error(str(error))
    bridge = Bridge(status_file=arguments.status_file, address=address)
    bridge.attempt()
    if not arguments.once:
        from gi.repository import GLib
        GLib.timeout_add_seconds(POLL_SECONDS, lambda: (bridge.attempt(), True)[1])
        GLib.MainLoop().run()


if __name__ == '__main__':
    main()
