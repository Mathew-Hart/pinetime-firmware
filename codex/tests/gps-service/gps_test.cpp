#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include "components/ble/GpsService.h"
#include <task.h>

namespace Data = Pinetime::Controllers::GpsData;
using Pinetime::Controllers::GpsService;
namespace {
  size_t assertions = 0;
  uint32_t clockTicks = 0;
  const ble_gatt_svc_def* registered = nullptr;
  int countCalls = 0;
  int registerCalls = 0;
  int lookupResult = 0;
  uint16_t lookupHandle = 128;
  int changedCalls = 0;
  uint16_t changedStart = 0;
  uint16_t changedEnd = 0;
  void Check(bool condition, const char* message) {
    ++assertions;
    if (!condition) {
      std::cerr << message << '\n';
      std::exit(1);
    }
  }

  // Independently encoded with Python struct '<BBBBBBHiihH'.
  // 37.3861,-122.0839; MSL12m; used8/view12; HDOP0.90; source age2s.
  constexpr Data::Packet golden {0x01, 0x03, 0x1f, 0x08, 0x0c, 0x00, 0x02, 0x00, 0x88, 0xaa,
                                 0x48, 0x16, 0xa8, 0x79, 0x3b, 0xb7, 0x0c, 0x00, 0x5a, 0x00};
  void Write16(Data::Packet& packet, size_t at, uint16_t value) {
    packet[at] = static_cast<uint8_t>(value);
    packet[at + 1] = static_cast<uint8_t>(value >> 8);
  }
  void Write32(Data::Packet& packet, size_t at, int32_t value) {
    const auto word = static_cast<uint32_t>(value);
    for (size_t byte = 0; byte < 4; ++byte) {
      packet[at + byte] = static_cast<uint8_t>(word >> (byte * 8));
    }
  }
  bool IsGolden(const Data::Measurement& value) {
    return value.fix == Data::Fix::ThreeD && value.flags == 31 && value.latitudeE7 == 373861000 &&
           value.longitudeE7 == -1220839000 && value.altitudeMeters == 12 && value.satellitesUsed == 8 &&
           value.satellitesInView == 12 && value.hdopCenti == 90 && value.sourceAgeSeconds == 2;
  }
  void Reject(const Data::Packet& packet) {
    Data::Measurement output;
    Check(Data::Decode(golden.data(), golden.size(), output), "golden packet accepted before rejection");
    Check(!Data::Decode(packet.data(), packet.size(), output), "malformed packet rejected");
    Check(IsGolden(output), "rejection never partially changes output");
  }
  void TestCodec() {
    Data::Measurement value;
    Check(Data::Decode(golden.data(), golden.size(), value), "literal golden decodes");
    Check(IsGolden(value), "literal golden has independently expected signed units");
    Check(value.Has(Data::Flag::Position) && value.Has(Data::Flag::Altitude) && value.Has(Data::Flag::Hdop), "validity flags visible");
    Check(!Data::Decode(nullptr, 20, value), "null input rejected");
    std::array<uint8_t, 40> longInput {};
    std::copy(golden.begin(), golden.end(), longInput.begin());
    for (size_t length = 0; length <= longInput.size(); ++length) {
      Check(Data::Decode(longInput.data(), length, value) == (length == 20), "only exact20 length accepted");
    }
    for (uint8_t version : {uint8_t {0}, uint8_t {2}, uint8_t {255}}) {
      auto packet = golden;
      packet[0] = version;
      Reject(packet);
    }
    for (uint8_t fix : {uint8_t {4}, uint8_t {255}}) {
      auto packet = golden;
      packet[1] = fix;
      Reject(packet);
    }
    for (uint8_t bit : {uint8_t {32}, uint8_t {64}, uint8_t {128}}) {
      auto packet = golden;
      packet[2] |= bit;
      Reject(packet);
    }
    for (int reserved = 1; reserved <= 255; ++reserved) {
      auto packet = golden;
      packet[5] = static_cast<uint8_t>(reserved);
      Reject(packet);
    }
    for (int32_t latitude : {-900000001, 900000001, INT32_MIN, INT32_MAX}) {
      auto packet = golden;
      Write32(packet, 8, latitude);
      Reject(packet);
    }
    for (int32_t longitude : {-1800000001, 1800000001, INT32_MIN, INT32_MAX}) {
      auto packet = golden;
      Write32(packet, 12, longitude);
      Reject(packet);
    }
    for (int32_t latitude : {-900000000, 0, 900000000}) {
      for (int32_t longitude : {-1800000000, 0, 1800000000}) {
        auto packet = golden;
        Write32(packet, 8, latitude);
        Write32(packet, 12, longitude);
        Check(Data::Decode(packet.data(), packet.size(), value), "all coordinate boundaries accepted");
        Check(value.latitudeE7 == latitude && value.longitudeE7 == longitude, "boundary coordinates exact");
      }
    }
    for (int32_t altitude : {-32768, -1, 0, 32767}) {
      auto packet = golden;
      Write16(packet, 16, static_cast<uint16_t>(altitude));
      Check(Data::Decode(packet.data(), packet.size(), value) && value.altitudeMeters == altitude, "signed MSL altitude exact");
    }
    for (size_t field : {size_t {3}, size_t {4}}) {
      auto packet = golden;
      packet[field] = 255;
      Reject(packet);
    }
    auto packet = golden;
    Write16(packet, 18, 65535);
    Reject(packet);
    for (uint8_t flag : {uint8_t {2}, uint8_t {4}, uint8_t {8}, uint8_t {16}}) {
      packet = golden;
      packet[2] &= static_cast<uint8_t>(~flag);
      Reject(packet);
    }
    packet = golden;
    packet[1] = 0;
    Reject(packet);
    packet = golden;
    packet[1] = 2;
    Reject(packet); // altitude must not imply a 3D fix.
    packet[2] &= static_cast<uint8_t>(~2U);
    Write16(packet, 16, 0);
    Check(Data::Decode(packet.data(), packet.size(), value) && value.fix == Data::Fix::TwoD, "2D without altitude valid");
    packet[1] = 1;
    Check(Data::Decode(packet.data(), packet.size(), value) && value.Has(Data::Flag::Position), "valid horizontal position may have unknown dimension");
    packet[1] = 0;
    packet[2] &= static_cast<uint8_t>(~1U);
    Write32(packet, 8, 0);
    Write32(packet, 12, 0);
    Check(Data::Decode(packet.data(), packet.size(), value) && value.fix == Data::Fix::None, "fresh nofix retains diagnostic satellite counts");
    packet[1] = 2;
    Reject(packet);
    packet[1] = 3;
    Reject(packet);
    packet[1] = 1;
    packet[2] = 0;
    packet[3] = packet[4] = 255;
    Write16(packet, 18, 65535);
    Write16(packet, 6, 65535);
    Check(Data::Decode(packet.data(), packet.size(), value) && value.flags == 0, "canonical source-off packet accepted");
    packet[8] = 1;
    Reject(packet);
  }

  void TestStore() {
    Data::Store store;
    auto snapshot = store.GetSnapshot(0, 1024);
    Check(!snapshot.connected && !snapshot.received && snapshot.coherent && !snapshot.fresh, "initial no-data honest");
    Check(store.Update(golden.data(), golden.size(), 0) == Data::UpdateResult::Disconnected, "disconnected writes rejected");
    store.SetConnected(true);
    snapshot = store.GetSnapshot(0, 1024);
    Check(snapshot.connected && !snapshot.received && !snapshot.fresh, "connected does not imply GPS data");
    Check(store.Update(golden.data(), golden.size(), 1024) == Data::UpdateResult::Accepted, "connected write accepted");
    snapshot = store.GetSnapshot(1024, 1024);
    Check(snapshot.fresh && snapshot.received && IsGolden(snapshot) && snapshot.ageSeconds == 2, "store retains full atomic measurement");
    store.SetConnected(true);
    Check(store.GetSnapshot(2048, 1024).received, "duplicate same connected state preserves data");
    snapshot = store.GetSnapshot(1024 + 27 * 1024 + 1023, 1024);
    Check(snapshot.fresh && snapshot.ageSeconds == 29, "source plus local age just before expiry");
    snapshot = store.GetSnapshot(1024 + 28 * 1024, 1024);
    Check(!snapshot.fresh && snapshot.ageSeconds == 30, "exact30-second sample expires");
    Check(!store.GetSnapshot(1024, 0).fresh, "zero tick rate never fresh and never divides by zero");
    auto stale = golden;
    Write16(stale, 6, 45);
    Check(store.Update(stale.data(), stale.size(), 500000) == Data::UpdateResult::Accepted, "stale sample may describe last fix");
    snapshot = store.GetSnapshot(500000, 1024);
    Check(!snapshot.fresh && snapshot.ageSeconds == 45, "retransmission cannot refresh old provider sample");
    Write16(stale, 6, 65535);
    Check(store.Update(stale.data(), stale.size(), 500000) == Data::UpdateResult::Accepted, "unknown source age accepted");
    snapshot = store.GetSnapshot(500000, 1024);
    Check(!snapshot.fresh && snapshot.ageSeconds == Data::UnknownAge, "unknown provider age never fresh");
    Check(store.Update(golden.data(), golden.size(), UINT32_MAX - 1023) == Data::UpdateResult::Accepted, "pre-rollover sample accepted");
    snapshot = store.GetSnapshot(1024, 1024);
    Check(snapshot.fresh && snapshot.ageSeconds == 4, "unsigned tick subtraction crosses rollover");
    Check(store.Update(golden.data(), golden.size(), 0) == Data::UpdateResult::Accepted, "overflow fixture accepted");
    snapshot = store.GetSnapshot(UINT32_MAX, 1);
    Check(!snapshot.fresh && snapshot.ageSeconds == Data::UnknownAge, "age addition saturates instead of wrapping fresh");
    auto invalid = golden;
    invalid[0] = 2;
    Check(store.Update(invalid.data(), invalid.size(), 123) == Data::UpdateResult::Invalid, "invalid update rejected");
    Check(IsGolden(store.GetSnapshot(0, 1024)), "invalid update leaves previous whole fix unchanged");
    store.SetConnected(false);
    snapshot = store.GetSnapshot(123, 1024);
    Check(!snapshot.connected && !snapshot.received && !snapshot.fresh && snapshot.flags == 0 && snapshot.latitudeE7 == 0, "disconnect erases history");
    store.SetConnected(true);
    snapshot = store.GetSnapshot(123, 1024);
    Check(snapshot.connected && !snapshot.received && snapshot.flags == 0, "reconnect requires new packet");
  }

  void TestConcurrency() {
    Data::Store store;
    store.SetConnected(true);
    auto other = golden;
    Write32(other, 8, -338688000);
    Write32(other, 12, 1512093000);
    Write16(other, 16, static_cast<uint16_t>(-5));
    Write16(other, 6, 7);
    other[3] = 6;
    other[4] = 10;
    Write16(other, 18, 120);
    std::atomic<bool> start {false};
    std::atomic<bool> done {false};
    std::atomic<bool> writesOk {true};
    std::thread writer([&] {
      while (!start.load()) {}
      for (uint32_t i = 0; i < 50000; ++i) {
        const auto& packet = (i & 1U) == 0 ? golden : other;
        if (store.Update(packet.data(), packet.size(), i) != Data::UpdateResult::Accepted) {
          writesOk.store(false);
        }
      }
      done.store(true);
    });
    start.store(true);
    bool allWhole = true;
    size_t observations = 0;
    do {
      const auto snapshot = store.GetSnapshot(50000, 1024);
      if (snapshot.coherent && snapshot.received) {
        ++observations;
        const bool isOther = snapshot.latitudeE7 == -338688000 && snapshot.longitudeE7 == 1512093000 &&
                             snapshot.altitudeMeters == -5 && snapshot.satellitesUsed == 6 && snapshot.satellitesInView == 10 &&
                             snapshot.sourceAgeSeconds == 7 && snapshot.hdopCenti == 120 && snapshot.flags == 31 &&
                             snapshot.fix == Data::Fix::ThreeD;
        allWhole = allWhole && (IsGolden(snapshot) || isOther);
      }
    } while (!done.load());
    writer.join();
    Check(writesOk.load(), "concurrent50k publisher writes valid");
    Check(allWhole, "concurrent readers see whole source measurement or transient unavailable");
    Check(observations != 0 || store.GetSnapshot(50000, 1024).received, "concurrent test observed published data");
  }

  void TestService() {
    GpsService service;
    service.Init();
    Check(countCalls == 1 && registerCalls == 1 && registered != nullptr, "actual service registers exactly once");
    Check(registered[0].type == BLE_GATT_SVC_TYPE_PRIMARY && registered[1].uuid == nullptr, "service list terminates");
    const auto* uuid = reinterpret_cast<const ble_uuid128_t*>(registered[0].uuid);
    constexpr std::array<uint8_t, 16> expectedService {0xd0, 0x42, 0x19, 0x3a, 0x3b, 0x43, 0x23, 0x8e, 0xfe, 0x48, 0xfc, 0x78, 0, 0, 6, 0};
    Check(uuid->u.type == 128 && std::equal(expectedService.begin(), expectedService.end(), uuid->value), "GPS namespace6 avoids weather collision");
    const auto& characteristic = registered[0].characteristics[0];
    const auto* characteristicUuid = reinterpret_cast<const ble_uuid128_t*>(characteristic.uuid);
    auto expectedChar = expectedService;
    expectedChar[12] = 1;
    Check(std::equal(expectedChar.begin(), expectedChar.end(), characteristicUuid->value), "GPS characteristic UUID byte ordering");
    Check(characteristic.flags == (BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC), "encrypted write-only no remote coordinate read");
    Check(registered[0].characteristics[1].uuid == nullptr && characteristic.arg == &service, "characteristic lifetime and callback context");
    lookupResult = 1;
    Check(!service.AnnounceDatabaseChange() && changedCalls == 0, "missing service never sends invalid service-changed range");
    lookupResult = 0;
    lookupHandle = 0;
    Check(!service.AnnounceDatabaseChange() && changedCalls == 0, "zero start handle never invalidates all old handles");
    lookupHandle = 128;
    Check(service.AnnounceDatabaseChange() && changedCalls == 1, "new appended service range announced");
    Check(changedStart == 128 && changedEnd == 65535, "service-changed indication excludes all prior handles");
    Check(service.AnnounceDatabaseChange() && changedCalls == 1, "service-changed is idempotent once per boot");
    Check(service.OnCommand(nullptr) == BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN, "null GATT context rejected");
    os_mbuf full {golden.data(), 20, nullptr, 20, 0};
    ble_gatt_access_ctxt context {BLE_GATT_ACCESS_OP_WRITE_CHR, &full, &characteristic};
    Check(characteristic.access_cb(1, 2, &context, characteristic.arg) == BLE_ATT_ERR_INSUFFICIENT_AUTHOR, "actual callback refuses disconnected update");
    service.SetConnected(true);
    for (uint16_t split = 0; split <= 20; ++split) {
      os_mbuf tail {golden.data() + split, static_cast<uint16_t>(20 - split), nullptr, static_cast<uint16_t>(20 - split), 0};
      os_mbuf head {golden.data(), split, &tail, 20, 0};
      context.om = &head;
      clockTicks = 1000 + split;
      Check(characteristic.access_cb(1, 2, &context, characteristic.arg) == 0, "chained mbuf split accepted atomically");
      Check(IsGolden(service.GetSnapshot()) && service.GetSnapshot().fresh, "all chained mbuf boundaries decode exact signed fields");
    }
    context.om = &full;
    context.op = BLE_GATT_ACCESS_OP_READ_CHR;
    Check(service.OnCommand(&context) == BLE_ATT_ERR_WRITE_NOT_PERMITTED, "nonwrite GATT access rejected");
    context.op = BLE_GATT_ACCESS_OP_WRITE_CHR;
    context.om = nullptr;
    Check(service.OnCommand(&context) == BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN, "null mbuf rejected");
    context.om = &full;
    full.packetLength = 19;
    Check(service.OnCommand(&context) == BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN, "short GATT value rejected");
    full.packetLength = 21;
    Check(service.OnCommand(&context) == BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN, "long GATT value rejected");
    full.packetLength = 20;
    full.forcedCopyResult = -1;
    Check(service.OnCommand(&context) == BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN, "mbuf copy failure rejected");
    full.forcedCopyResult = 0;
    auto bad = golden;
    bad[5] = 1;
    full.bytes = bad.data();
    Check(service.OnCommand(&context) == BLE_ATT_ERR_UNLIKELY, "GATT semantic failure rejected");
    Check(IsGolden(service.GetSnapshot()), "bad GATT write never replaces last good measurement");
    service.SetConnected(false);
    Check(!service.GetSnapshot().received && service.GetSnapshot().flags == 0, "actual service clears values on disconnect");
    service.SetConnected(true);
    Check(!service.GetSnapshot().received && !service.GetSnapshot().fresh, "actual service reconnect waits for new packet");
    Check(changedCalls == 1, "reconnect never triggers a service-changed indication");
  }
}

uint32_t xTaskGetTickCount() { return clockTicks; }
int ble_gatts_count_cfg(const ble_gatt_svc_def*) { ++countCalls; return 0; }
int ble_gatts_add_svcs(const ble_gatt_svc_def* services) { ++registerCalls; registered = services; return 0; }
int ble_gatts_find_svc(const ble_uuid_t* uuid, uint16_t* handle) {
  const auto* value = reinterpret_cast<const ble_uuid128_t*>(uuid);
  Check(value->value[12] == 0 && value->value[14] == 6, "service-changed lookup uses own GPS UUID");
  *handle = lookupHandle;
  return lookupResult;
}
void ble_svc_gatt_changed(uint16_t startHandle, uint16_t endHandle) {
  ++changedCalls;
  changedStart = startHandle;
  changedEnd = endHandle;
}
int os_mbuf_copydata(const os_mbuf* buffer, int offset, int length, void* output) {
  if (buffer->forcedCopyResult != 0) { return buffer->forcedCopyResult; }
  auto* target = static_cast<uint8_t*>(output);
  for (const os_mbuf* part = buffer; part != nullptr && length != 0; part = part->next) {
    const int skip = std::min(offset, static_cast<int>(part->length));
    offset -= skip;
    const int count = std::min(length, static_cast<int>(part->length) - skip);
    std::memcpy(target, part->bytes + skip, static_cast<size_t>(count));
    target += count;
    length -= count;
  }
  return length == 0 ? 0 : -1;
}
int main() {
  TestCodec();
  TestStore();
  TestConcurrency();
  TestService();
  std::cout << "{\"result\":\"PASS\",\"assertions\":" << assertions
            << ",\"actual_gps_codec_store_and_service\":true,\"concurrent_publish_count\":50000,\"chained_mbuf_splits\":21,\"phone_actions\":[]}\n";
}
