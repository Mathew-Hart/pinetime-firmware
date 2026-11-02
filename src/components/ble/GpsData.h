#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Pinetime::Controllers::GpsData {
  // Version 1 is one 20-byte ATT value, including with the default MTU of 23.
  // Integers are little endian; altitude is MSL, and HDOP is dimensionless.
  // Source age must describe the provider sample, not the last retransmission.
  constexpr size_t PacketSize = 20;
  constexpr uint16_t UnknownSourceAge = 65535;
  constexpr uint32_t UnknownAge = std::numeric_limits<uint32_t>::max();
  constexpr uint32_t FreshSeconds = 30;
  using Packet = std::array<uint8_t, PacketSize>;

  enum class Fix : uint8_t { None = 0, Unknown = 1, TwoD = 2, ThreeD = 3 };
  enum class Flag : uint8_t { Position = 1, Altitude = 2, SatellitesUsed = 4, SatellitesInView = 8, Hdop = 16 };

  struct Measurement {
    Fix fix = Fix::Unknown;
    uint8_t flags = 0;
    uint8_t satellitesUsed = 255;
    uint8_t satellitesInView = 255;
    uint16_t sourceAgeSeconds = UnknownSourceAge;
    int32_t latitudeE7 = 0;
    int32_t longitudeE7 = 0;
    int16_t altitudeMeters = 0;
    uint16_t hdopCenti = 65535;

    bool Has(Flag flag) const {
      return (flags & static_cast<uint8_t>(flag)) != 0;
    }
  };

  struct Snapshot : Measurement {
    bool connected = false;
    bool received = false;
    bool coherent = true;
    bool fresh = false;
    uint32_t ageSeconds = UnknownAge;
  };

  inline uint16_t Read16(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0]) | static_cast<uint16_t>(bytes[1]) << 8U;
  }

  inline uint32_t Read32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) | static_cast<uint32_t>(bytes[1]) << 8U |
           static_cast<uint32_t>(bytes[2]) << 16U | static_cast<uint32_t>(bytes[3]) << 24U;
  }

  inline int32_t Signed32(uint32_t value) {
    return value <= 0x7fffffffU ? static_cast<int32_t>(value) : -1 - static_cast<int32_t>(~value);
  }

  inline int16_t Signed16(uint16_t value) {
    return value <= 0x7fffU ? static_cast<int16_t>(value) : static_cast<int16_t>(-1 - static_cast<int32_t>(static_cast<uint16_t>(~value)));
  }

  // Decode into a temporary: rejected values never partially replace a fix.
  inline bool Decode(const uint8_t* bytes, size_t length, Measurement& output) {
    if (bytes == nullptr || length != PacketSize || bytes[0] != 1 || bytes[1] > 3 || (bytes[2] & 0xe0U) != 0 || bytes[5] != 0) {
      return false;
    }
    Measurement value;
    value.fix = static_cast<Fix>(bytes[1]);
    value.flags = bytes[2];
    value.satellitesUsed = bytes[3];
    value.satellitesInView = bytes[4];
    value.sourceAgeSeconds = Read16(bytes + 6);
    value.latitudeE7 = Signed32(Read32(bytes + 8));
    value.longitudeE7 = Signed32(Read32(bytes + 12));
    value.altitudeMeters = Signed16(Read16(bytes + 16));
    value.hdopCenti = Read16(bytes + 18);

    const bool hasFix = value.fix == Fix::TwoD || value.fix == Fix::ThreeD;
    if ((hasFix && !value.Has(Flag::Position)) || (value.fix == Fix::None && value.Has(Flag::Position)) ||
        (value.Has(Flag::Position) && (value.latitudeE7 < -900000000 || value.latitudeE7 > 900000000 ||
                                      value.longitudeE7 < -1800000000 || value.longitudeE7 > 1800000000)) ||
        (!value.Has(Flag::Position) && (value.latitudeE7 != 0 || value.longitudeE7 != 0)) ||
        (value.Has(Flag::Altitude) && value.fix != Fix::ThreeD) || (!value.Has(Flag::Altitude) && value.altitudeMeters != 0) ||
        (value.Has(Flag::SatellitesUsed) == (value.satellitesUsed == 255)) ||
        (value.Has(Flag::SatellitesInView) == (value.satellitesInView == 255)) ||
        (value.Has(Flag::Hdop) == (value.hdopCenti == 65535))) {
      return false;
    }
    output = value;
    return true;
  }

  enum class UpdateResult { Accepted, Invalid, Disconnected };

  // One publisher: all calls to SetConnected/Update run on the NimBLE host task.
  // Readers may preempt it. Every word is atomic, and the reader never spins:
  // a concurrent write returns an unavailable snapshot for the next UI poll.
  class Store {
  public:
    void SetConnected(bool value) {
      if (connected.load() == value) {
        return;
      }
      version.fetch_add(1);
      connected.store(value);
      received.store(false);
      for (auto& word : words) {
        word.store(0);
      }
      receivedTicks.store(0);
      version.fetch_add(1);
    }

    UpdateResult Update(const uint8_t* bytes, size_t length, uint32_t nowTicks) {
      Measurement value;
      if (!Decode(bytes, length, value)) {
        return UpdateResult::Invalid;
      }
      if (!connected.load()) {
        return UpdateResult::Disconnected;
      }
      version.fetch_add(1);
      for (size_t i = 0; i < words.size(); ++i) {
        words[i].store(Read32(bytes + i * 4));
      }
      receivedTicks.store(nowTicks);
      received.store(true);
      version.fetch_add(1);
      return UpdateResult::Accepted;
    }

    Snapshot GetSnapshot(uint32_t nowTicks, uint32_t ticksPerSecond) const {
      Snapshot result;
      const auto before = version.load();
      const bool isConnected = connected.load();
      const bool wasReceived = received.load();
      const uint32_t ticks = receivedTicks.load();
      Packet bytes {};
      for (size_t i = 0; i < words.size(); ++i) {
        const uint32_t word = words[i].load();
        for (size_t byte = 0; byte < 4; ++byte) {
          bytes[i * 4 + byte] = static_cast<uint8_t>(word >> (byte * 8));
        }
      }
      const auto after = version.load();
      result.connected = isConnected;
      if ((before & 1U) != 0 || before != after) {
        result.coherent = false;
        return result;
      }
      if (!isConnected || !wasReceived) {
        return result;
      }
      Measurement value;
      if (!Decode(bytes.data(), bytes.size(), value)) {
        result.coherent = false;
        return result;
      }
      static_cast<Measurement&>(result) = value;
      result.received = true;
      if (ticksPerSecond != 0 && value.sourceAgeSeconds != UnknownSourceAge) {
        // Unsigned subtraction also handles one FreeRTOS tick-counter rollover.
        const uint32_t elapsed = (nowTicks - ticks) / ticksPerSecond;
        result.ageSeconds = elapsed > UnknownAge - value.sourceAgeSeconds ? UnknownAge : elapsed + value.sourceAgeSeconds;
        result.fresh = result.ageSeconds < FreshSeconds;
      }
      return result;
    }

  private:
    static_assert(std::atomic<uint32_t>::is_always_lock_free, "GPS snapshots require lock-free 32-bit atomics");
    std::array<std::atomic<uint32_t>, PacketSize / 4> words {};
    std::atomic<uint32_t> version {0};
    std::atomic<uint32_t> receivedTicks {0};
    std::atomic<bool> connected {false};
    std::atomic<bool> received {false};
  };
}
