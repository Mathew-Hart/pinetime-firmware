#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace Pinetime::Controllers {
  class WatchFaceSelection {
  public:
    using Record = std::array<uint8_t, 12>;

    explicit constexpr WatchFaceSelection(uint32_t disabled = 0) : disabled {disabled} {
    }

    constexpr bool IsEnabled(uint8_t id) const {
      return id < 32 && (disabled & (uint32_t {1} << id)) == 0;
    }

    bool SetEnabled(uint8_t id, bool enabled) {
      if (id >= 32) {
        return false;
      }
      const auto before = disabled;
      const auto bit = uint32_t {1} << id;
      disabled = enabled ? disabled & ~bit : disabled | bit;
      return before != disabled;
    }

    constexpr Record Encode() const {
      Record record {'W', 'F', 'C', 1};
      for (uint8_t i = 0; i < 4; ++i) {
        record[4 + i] = static_cast<uint8_t>(disabled >> (8 * i));
        record[8 + i] = static_cast<uint8_t>(~disabled >> (8 * i));
      }
      return record;
    }

    static constexpr std::optional<WatchFaceSelection> Decode(const Record& record) {
      if (record[0] != 'W' || record[1] != 'F' || record[2] != 'C' || record[3] != 1) {
        return std::nullopt;
      }
      uint32_t disabled = 0;
      uint32_t complement = 0;
      for (uint8_t i = 0; i < 4; ++i) {
        disabled |= uint32_t {record[4 + i]} << (8 * i);
        complement |= uint32_t {record[8 + i]} << (8 * i);
      }
      if (disabled != ~complement) {
        return std::nullopt;
      }
      return WatchFaceSelection {disabled};
    }

  private:
    uint32_t disabled;
  };
}
