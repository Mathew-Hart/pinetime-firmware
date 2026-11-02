#!/usr/bin/env python3
"""Fake-only GPS parser, packet, permission, transport and runtime-source tests."""
from contextlib import redirect_stdout, redirect_stderr
from dataclasses import replace
from datetime import datetime, timezone
import io
import json
import math
import os
import subprocess
import sys
from pathlib import Path
import tempfile
from types import SimpleNamespace
from unittest.mock import patch
import unittest

import gps_bridge as gps

TEST_ADDRESS = 'C2:00:00:00:00:01'  # Synthetic, locally administered test identity.
TEST_DEVICE = gps.device_path(TEST_ADDRESS)


def make_bridge(io=None, status_file=None, clock=None):
    return gps.Bridge(io, status_file, clock if clock is not None else lambda: 0, address=TEST_ADDRESS)


NOW = datetime(2026, 11, 1, 12, 0, 2, tzinfo=timezone.utc).timestamp()


def nmea(body):
    checksum = 0
    for byte in body.encode('ascii'):
        checksum ^= byte
    return '$' + body + '*%02X' % checksum


def rmc(utc='120000.00', status='A', day='011126', mode='A'):
    return nmea(f'GPRMC,{utc},{status},3723.1660,N,12205.0340,W,0.0,0.0,{day},,,{mode}')


def gga(utc='120000.00', quality='1', altitude='12.0', used='08', hdop='0.90'):
    return nmea(f'GPGGA,{utc},3723.1660,N,12205.0340,W,{quality},{used},{hdop},{altitude},M,-30.0,M,,')


def gsa(mode='3', talker='GP'):
    return nmea(','.join([talker+'GSA', 'A', mode] + ['01', '02', '03', '04', '05', '06', '07', '08', '', '', '', ''] + ['1.20', '.90', '.80']))


def gsv(talker='GP', count=12, total=3, sequence=1):
    return nmea(f'{talker}GSV,{total},{sequence},{count},01,45,123,35,02,25,234,30,03,50,345,40,04,35,045,25')


def source(*extra):
    return {4: '\r\n'.join((rmc(), gga(), gsa(), gsv(), *extra))}


def objects():
    return {TEST_DEVICE: {'org.bluez.Device1': {'Address': TEST_ADDRESS, 'Connected': True, 'ServicesResolved': True,
                'Paired': True, 'Bonded': True, 'Trusted': True, 'Blocked': False}},
            TEST_DEVICE+'/fw': {'org.bluez.GattCharacteristic1': {'UUID': gps.FIRMWARE_UUID, 'Flags': ['read']}},
            TEST_DEVICE+'/gps': {'org.bluez.GattCharacteristic1': {'UUID': gps.GPS_UUID, 'Flags': ['write'], 'MTU': 23}}}


class FakeIO:
    def __init__(self):
        self.tree = objects()
        self.active, self.enabled = True, True
        self.version = b'1.16.1-wf4'
        self.source = ('ready', source())
        self.source_reads = 0
        self.writes = []
        self.fail_write = False
        self.permission_after_read = None

    def objects(self): return self.tree
    def companion_active(self): return self.active
    def firmware(self, path): return self.version
    def location_enabled(self): return self.enabled
    def modem_snapshot(self):
        self.source_reads += 1
        if self.permission_after_read is not None:
            self.enabled = self.permission_after_read
        return self.source
    def write(self, path, payload):
        self.writes.append((path, payload))
        if self.fail_write: raise RuntimeError('synthetic write failure')


class ParserTests(unittest.TestCase):
    def test_three_d_golden(self):
        sample = gps.parse_location(source(), NOW)
        self.assertEqual((sample.fix, sample.age, sample.altitude, sample.used, sample.in_view, sample.hdop), (3, 2, 12, 8, 12, .9))
        self.assertEqual(gps.encode(sample).hex(), '01031f080c00020088aa4816a8793bb70c005a00')

    def test_unknown_dimension_keeps_horizontal_fix(self):
        sample = gps.parse_location({4: rmc()}, NOW)
        self.assertEqual(sample.fix, 1)
        self.assertAlmostEqual(sample.latitude, 37.3861)
        self.assertEqual(gps.encode(sample)[2], 1)

    def test_two_d_never_exposes_altitude(self):
        sample = gps.parse_location({4: '\n'.join((rmc(), gga(), gsa('2')))}, NOW)
        self.assertEqual(sample.fix, 2)
        self.assertIsNone(sample.altitude)

    def test_raw_iso_fix_and_time_only_date_requirement(self):
        raw = {'utc-time': '2026-11-01T12:00:00Z', 'latitude': 37.3861, 'longitude': -122.0839, 'altitude': 900}
        sample = gps.parse_location({2: raw}, NOW)
        self.assertEqual(sample.fix, 1)
        self.assertIsNone(sample.altitude)
        raw['utc-time'] = '120000.00'
        self.assertIsNone(gps.parse_location({2: raw}, NOW).latitude)
        self.assertIsNotNone(gps.parse_location({2: raw, 4: rmc()}, NOW).latitude)

    def test_stale_and_future_gnss_are_not_freshened_by_polling(self):
        for age in (30, 31, 86400, -6):
            with self.subTest(age=age):
                sample = gps.parse_location(source(), NOW - 2 + age)
                self.assertEqual((sample.fix, sample.age, sample.latitude), (1, 65535, None))
        self.assertEqual(gps.parse_location(source(), NOW-7).age, 0)  # five seconds future
        self.assertEqual(gps.parse_location(source(), NOW+27.1).age, 65535)  # ceil to30

    def test_active_rmc_invalidates_old_fix_without_inventing_date(self):
        data = source(rmc('120001.00', 'V'))
        sample = gps.parse_location(data, NOW)
        self.assertEqual(sample.fix, 0)
        self.assertIsNone(sample.latitude)
        self.assertIsNone(sample.altitude)
        raw = {'utc-time': '120000', 'latitude': 1, 'longitude': 2}
        self.assertIsNone(gps.parse_location({2: raw}, NOW).latitude)

    def test_gga_no_fix_overrides_cached_three_d(self):
        sample = gps.parse_location({4: '\n'.join((rmc(), gga(quality='0', used='00'), gsa()))}, NOW)
        self.assertEqual((sample.fix, sample.used), (0, 0))
        self.assertIsNone(sample.latitude)

    def test_estimated_manual_simulated_reports_are_not_gnss_fixes(self):
        for mode in ('E', 'M', 'S', 'N', 'X'):
            with self.subTest(mode=mode):
                self.assertIsNone(gps.parse_location({4: rmc(mode=mode)}, NOW).latitude)
        for quality in ('6', '7', '8'):
            with self.subTest(quality=quality):
                self.assertIsNone(gps.parse_location({4: '\n'.join((rmc(), gga(quality=quality), gsa()))}, NOW).latitude)

    def test_bad_checksum_or_checksum_free_nmea_is_rejected(self):
        for text in (rmc()[:-2]+'00', rmc().split('*')[0], rmc().replace('$', '!'), rmc()+'trailing'):
            self.assertIsNone(gps.parse_location({4: text}, NOW).latitude)

    def test_old_or_mismatched_gga_does_not_supply_auxiliary_data(self):
        data = {4: '\n'.join((rmc(), gga('115900.00'), gsa()))}
        sample = gps.parse_location(data, NOW)
        self.assertEqual(sample.fix, 3)
        self.assertIsNone(sample.altitude)
        self.assertIsNone(sample.used)
        wrong = gga().split('*')[0][1:].replace('3723.1660', '1023.1660')
        sample = gps.parse_location({4: '\n'.join((rmc(), nmea(wrong), gsa()))}, NOW)
        self.assertIsNone(sample.altitude)

    def test_gsv_fragments_signals_and_mixed_totals_are_not_double_counted(self):
        lines = gps.sentences('\n'.join((gsv(), gsv(sequence=2), gsv(sequence=3), gsv('GL', 8, 2), gsv('GN', 20, 5))))
        self.assertEqual(gps.reported_in_view(lines), 20)
        self.assertEqual(gps.reported_in_view(gps.sentences('\n'.join((gsv(), gsv('GL', 8, 2))))), 20)
        self.assertEqual(gps.reported_in_view(gps.sentences('\n'.join((gsv('BD', 8, 2), gsv('GB', 8, 2))))), 8)
        self.assertIsNone(gps.reported_in_view(gps.sentences('\n'.join((gsv(), gsv(count=11))))))

    def test_truncated_gsa_or_conflicting_modes_do_not_claim_three_d(self):
        for tail in (nmea('GPGSA,A,3'), gsa('2')+'\n'+gsa('3')):
            sample = gps.parse_location({4: '\n'.join((rmc(), gga(), tail))}, NOW)
            self.assertEqual(sample.fix, 1)
            self.assertIsNone(sample.altitude)

    def test_altitude_units_hdop_and_satellite_ranges(self):
        sample = gps.parse_location({4: '\n'.join((rmc(), gga(altitude='40000', used='999', hdop='nan'), gsa()))}, NOW)
        self.assertIsNone(sample.altitude)
        self.assertIsNone(sample.used)
        self.assertIsNone(sample.hdop)

    def test_invalid_dates_coordinates_and_bounded_input(self):
        for text in (rmc(day='321126'), rmc(utc='126099'), 'x'*65537):
            self.assertIsNone(gps.parse_location({4: text}, NOW).latitude)
        self.assertIsNone(gps.coordinate('3760.000', 'N', True))
        self.assertIsNone(gps.coordinate('9100.000', 'N', True))
        self.assertIsNone(gps.coordinate('18100.000', 'E', False))
        self.assertEqual(gps.coordinate('0000.000', 'S', True), 0)


class PacketTests(unittest.TestCase):
    def test_shared_protocol_fixtures_remain_exact(self):
        fixture = json.loads(Path(__file__).with_name('protocol-fixtures.json').read_text())
        self.assertTrue(fixture['all_examples_synthetic'])
        for item in fixture['cases']:
            with self.subTest(name=item['name']):
                self.assertEqual(gps.encode(gps.Sample(**item['sample'])).hex(), item['hex'])


class PacketValidationTests(unittest.TestCase):
    def test_unknown_packet_is_canonical_and_atomic_twenty_bytes(self):
        data = gps.encode(gps.Sample())
        self.assertEqual(data.hex(), '010100ffff00ffff00000000000000000000ffff')
        self.assertEqual(len(data), 20)

    def test_unknown_dimension_with_position_is_valid(self):
        data = gps.encode(gps.Sample(fix=1, age=0, latitude=0, longitude=0))
        self.assertEqual(data[2], 1)

    def test_invalid_packet_inputs_do_not_get_silently_clamped(self):
        good = gps.Sample(fix=3, age=1, latitude=1, longitude=2, altitude=3)
        bad = [replace(good, fix=True), replace(good, age=True), replace(good, age=1.5), replace(good, latitude=91),
               replace(good, longitude=float('nan')), replace(good, altitude=40000), replace(good, age=30),
               replace(good, fix=0), replace(good, used=255), replace(good, used=True), replace(good, hdop=0)]
        for value in bad:
            with self.subTest(value=value), self.assertRaises(ValueError): gps.encode(value)


class ConfigurationTests(unittest.TestCase):
    def test_no_address_is_a_clear_error(self):
        for environment in ({}, {'PINETIME_ADDRESS': ''}, {'PINETIME_ADDRESS': '   '}):
            with self.subTest(environment=environment), self.assertRaisesRegex(ValueError, 'Set --address or PINETIME_ADDRESS'):
                gps.configured_address(environment=environment)

    def test_environment_address_is_normalized(self):
        self.assertEqual(gps.configured_address(environment={'PINETIME_ADDRESS': ' c2:00:00:00:00:01 '}), TEST_ADDRESS)

    def test_explicit_address_overrides_environment(self):
        self.assertEqual(gps.configured_address(TEST_ADDRESS, {'PINETIME_ADDRESS': 'invalid'}), TEST_ADDRESS)

    def test_invalid_address_is_rejected_without_echoing_input(self):
        for address in ('not-an-address', '00:11:22:33:44', '00:11:22:33:44:ZZ', 'C2_00_00_00_00_01', TEST_ADDRESS+'junk'):
            with self.subTest(address=address):
                with self.assertRaises(ValueError) as caught:
                    gps.configured_address(address, {})
                self.assertNotIn(address, str(caught.exception))

    def test_missing_config_is_rejected_before_bus_initialization(self):
        with patch.dict(os.environ, {}, clear=True), patch.object(gps, 'RuntimeIO') as runtime:
            with self.assertRaises(ValueError): gps.Bridge()
            runtime.assert_not_called()

    def test_cli_without_configuration_exits_two_before_io(self):
        output = io.StringIO()
        with redirect_stderr(output), patch.object(gps, 'RuntimeIO') as runtime:
            with self.assertRaises(SystemExit) as caught: gps.main(['--once'], {})
            self.assertEqual(caught.exception.code, 2)
            self.assertIn('Set --address or PINETIME_ADDRESS', output.getvalue())
            runtime.assert_not_called()

    def test_actual_cli_has_no_hidden_default_device(self):
        environment = os.environ.copy(); environment.pop('PINETIME_ADDRESS', None)
        result = subprocess.run([sys.executable, str(Path(gps.__file__)), '--once'], env=environment, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn('Set --address or PINETIME_ADDRESS', result.stderr)
        self.assertNotIn('gi.repository', result.stderr)

    def test_address_selection_does_not_accept_a_different_connected_peer(self):
        fake = FakeIO()
        other_address = 'C2:00:00:00:00:02'
        fake.tree[TEST_DEVICE]['org.bluez.Device1']['Address'] = other_address
        with redirect_stdout(io.StringIO()): gps.Bridge(fake, address=TEST_ADDRESS).attempt(NOW, 0)
        self.assertEqual((fake.source_reads, fake.writes), (0, []))

class BridgeTests(unittest.TestCase):
    def run_attempt(self, bridge, stamp=0):
        with redirect_stdout(io.StringIO()):
            return bridge.attempt(NOW, stamp)

    def test_success_is_one_request_and_logs_no_location(self):
        fake = FakeIO()
        with tempfile.TemporaryDirectory() as folder:
            status = Path(folder)/'status.json'
            bridge = make_bridge(fake, status, clock=lambda: 0)
            log = io.StringIO()
            with redirect_stdout(log): result = bridge.attempt(NOW, 0)
            self.assertEqual(result['status'], 'sent')
            self.assertEqual(len(fake.writes), 1)
            self.assertEqual(fake.writes[0][1].hex(), '01031f080c00020088aa4816a8793bb70c005a00')
            self.assertEqual(status.stat().st_mode & 0o777, 0o600)
            for private in ('37.3861', '-122.0839', '$GP', '01031f', 'latitude', 'longitude'):
                self.assertNotIn(private, log.getvalue()+status.read_text())

    def test_all_peer_gates_precede_gnss_reads(self):
        for key, value in (('Connected', False), ('ServicesResolved', False), ('Paired', False), ('Bonded', False),
                           ('Trusted', False), ('Blocked', True), ('Address', '00:00:00:00:00:00')):
            fake = FakeIO(); fake.tree[TEST_DEVICE]['org.bluez.Device1'][key] = value
            self.run_attempt(make_bridge(fake)); self.assertEqual((fake.source_reads, fake.writes), (0, []))
        for kind in ('inactive', 'wrong_firmware', 'missing_characteristic'):
            fake = FakeIO()
            if kind == 'inactive': fake.active = False
            elif kind == 'wrong_firmware': fake.version = b'1.16.1-wf40'
            else: del fake.tree[TEST_DEVICE+'/gps']
            self.run_attempt(make_bridge(fake)); self.assertEqual((fake.source_reads, fake.writes), (0, []))

    def test_location_off_or_disabled_gnss_sends_unknown_not_no_fix(self):
        for kind in ('location', 'gnss', 'modem'):
            fake = FakeIO()
            if kind == 'location': fake.enabled = False
            else: fake.source = ('gnss_disabled' if kind == 'gnss' else 'modem_unavailable', None)
            result = self.run_attempt(make_bridge(fake))
            self.assertEqual(result['fix'], 1)
            self.assertEqual(fake.writes[0][1], gps.encode(gps.Sample()))
            if kind == 'location': self.assertEqual(fake.source_reads, 0)

    def test_permission_revocation_after_source_read_clears_position(self):
        fake = FakeIO(); fake.permission_after_read = False
        self.run_attempt(make_bridge(fake))
        self.assertEqual(fake.writes[0][1], gps.encode(gps.Sample()))

    def test_stationary_heartbeat_and_reconnect_resend(self):
        fake = FakeIO(); bridge = make_bridge(fake)
        self.run_attempt(bridge, 0); self.run_attempt(bridge, 5)
        self.assertEqual(len(fake.writes), 1)
        self.run_attempt(bridge, 10); self.assertEqual(len(fake.writes), 2)
        fake.tree[TEST_DEVICE]['org.bluez.Device1']['Connected'] = False
        self.run_attempt(bridge, 11)
        fake.tree[TEST_DEVICE]['org.bluez.Device1']['Connected'] = True
        self.run_attempt(bridge, 12); self.assertEqual(len(fake.writes), 3)

    def test_failed_write_never_poison_dedup_cache(self):
        fake = FakeIO(); bridge = make_bridge(fake); fake.fail_write = True
        self.run_attempt(bridge, 0); self.assertIsNone(bridge.last_packet)
        fake.fail_write = False
        self.assertEqual(self.run_attempt(bridge, 1)['status'], 'sent')

    def test_slow_rpcs_crossing_expiry_send_unknown_not_a_fresh_fix(self):
        fake = FakeIO(); clock = [100]
        original = fake.modem_snapshot
        def delayed():
            clock[0] += 30
            return original()
        fake.modem_snapshot = delayed
        bridge = make_bridge(fake, clock=lambda: clock[0])
        result = self.run_attempt(bridge)
        self.assertEqual(result['fix'], 1)
        self.assertEqual(fake.writes[0][1], gps.encode(gps.Sample()))

    def test_new_fix_after_slow_rpc_is_not_mistaken_for_future_data(self):
        fake = FakeIO(); clock = [100]
        def delayed():
            clock[0] += 25
            return 'ready', {4: rmc('120022.00')}
        fake.modem_snapshot = delayed
        bridge = make_bridge(fake, clock=lambda: clock[0])
        result = self.run_attempt(bridge)
        self.assertEqual(result['source_age_seconds'], 5)
        self.assertEqual(gps.PACKET.unpack(fake.writes[0][1])[2], 1)


class RuntimeSourceTests(unittest.TestCase):
    def test_source_off_never_calls_get_location_or_setup(self):
        runtime = gps.RuntimeIO.__new__(gps.RuntimeIO)
        runtime.system = object(); runtime.owner = lambda *args: ':1.99'
        calls = []
        def call(*args):
            calls.append(args[4])
            return ({'/modem': {gps.LOCATION: {'Capabilities': 6, 'Enabled': 1}}},)
        runtime.call = call
        self.assertEqual(runtime.modem_snapshot(), ('gnss_disabled', None))
        self.assertEqual(calls, ['GetManagedObjects'])

    def test_enabled_source_reads_only_unique_owner_existing_state(self):
        runtime = gps.RuntimeIO.__new__(gps.RuntimeIO)
        runtime.system = object(); runtime.owner = lambda *args: ':1.99'
        calls = []
        def call(*args):
            calls.append((args[1], args[4]))
            return ({'/modem': {gps.LOCATION: {'Capabilities': 6, 'Enabled': 6}}},) if args[4] == 'GetManagedObjects' else (source(),)
        runtime.call = call
        self.assertEqual(runtime.modem_snapshot()[0], 'ready')
        self.assertEqual(calls, [(':1.99', 'GetManagedObjects'), (':1.99', 'GetLocation')])

    def test_no_auto_start_and_no_interactive_auth_flags(self):
        runtime = gps.RuntimeIO.__new__(gps.RuntimeIO)
        runtime.Gio = SimpleNamespace(DBusCallFlags=SimpleNamespace(NO_AUTO_START=4))
        observed = []
        class Variant:
            def unpack(self): return ('ok',)
        class Bus:
            def call_sync(self, *args): observed.append(args); return Variant()
        self.assertEqual(runtime.call(Bus(), ':1.99', '/modem', gps.LOCATION, 'GetLocation'), ('ok',))
        self.assertEqual(observed[0][6:8], (4, 3000))

    def test_disabled_nmea_cache_is_not_reused_by_raw_only_source(self):
        runtime = gps.RuntimeIO.__new__(gps.RuntimeIO)
        runtime.system = object(); runtime.owner = lambda *args: ':1.99'
        def call(*args):
            return ({'/modem': {gps.LOCATION: {'Capabilities': 6, 'Enabled': 2}}},) if args[4] == 'GetManagedObjects' else ({2: {}, 4: rmc()},)
        runtime.call = call
        self.assertEqual(runtime.modem_snapshot(), ('ready', {2: {}}))


if __name__ == '__main__':
    unittest.main()
