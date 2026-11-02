#pragma once

#include <cstdint>
#include <optional>

namespace Pinetime::Controllers {
  // The analog face requests a bounded one-shot independently of the continuous
  // heart-rate app. Cadence belongs to the task, so replacing/recreating a face
  // or waking the display does not restart the ten-minute period.
  class AnalogHeartRateSchedule {
  public:
    enum class Event { None, Started, Canceled, TimedOut };

    explicit constexpr AnalogHeartRateSchedule(uint32_t tickRate) : interval {600 * tickRate}, limit {30 * tickRate} {
    }

    Event Update(bool isVisible, uint32_t now) {
      visible = isVisible;
      if (!visible) {
        if (active) {
          active = false;
          return Event::Canceled;
        }
        return Event::None;
      }
      if (active) {
        if (now - startedAt >= limit) {
          active = false;
          return Event::TimedOut;
        }
      } else if (!attempted || now - startedAt >= interval) {
        active = true;
        attempted = true;
        startedAt = now;
        return Event::Started;
      }
      return Event::None;
    }

    void Complete() {
      active = false;
    }

    bool Active() const {
      return active;
    }

    std::optional<uint32_t> Delay(uint32_t now) const {
      if (!visible) {
        return std::nullopt;
      }
      if (!attempted) {
        return 0;
      }
      const auto elapsed = now - startedAt;
      const auto due = active ? limit : interval;
      return elapsed >= due ? 0 : due - elapsed;
    }

  private:
    const uint32_t interval;
    const uint32_t limit;
    uint32_t startedAt = 0;
    bool visible = false;
    bool attempted = false;
    bool active = false;
  };
}
