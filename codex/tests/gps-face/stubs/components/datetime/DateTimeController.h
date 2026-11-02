#pragma once
#include <chrono>
#include <cstdint>
#include <ctime>

namespace Pinetime::Controllers {
  class DateTime {
  public:
    enum class Months : uint8_t { Unknown, January, February, March, April, May, June, July, August, September, October, November, December };
    DateTime() { SetLocal(2026, 10, 31, 10, 8, 0); }
    void SetLocal(int year, int month, int day, int hour, int minute, int second) {
      std::tm value {}; value.tm_year = year - 1900; value.tm_mon = month - 1; value.tm_mday = day;
      value.tm_hour = hour; value.tm_min = minute; value.tm_sec = second;
      pending = std::chrono::seconds {timegm(&value)};
    }
    auto CurrentDateTime() {
      ++updates;
      const auto seconds = static_cast<std::time_t>(pending.count());
      current = *std::gmtime(&seconds);
      return std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> {pending};
    }
    auto UTCDateTime() { return CurrentDateTime() - std::chrono::seconds {static_cast<int>(offsetQuarters) * 900}; }
    uint16_t Year() const { return current.tm_year + 1900; }
    Months Month() const { return static_cast<Months>(current.tm_mon + 1); }
    uint8_t Day() const { return current.tm_mday; }
    uint8_t Hours() const { return current.tm_hour; }
    uint8_t Minutes() const { return current.tm_min; }
    uint8_t Seconds() const { return current.tm_sec; }
    const char* DayOfWeekShortToString() const {
      constexpr const char* names[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
      return names[current.tm_wday];
    }
    bool HasTimeZone() const { return hasTimeZone; }
    int8_t UtcOffset() const { return offsetQuarters; }
    bool hasTimeZone = true;
    int8_t offsetQuarters = -28;
    std::chrono::seconds pending {};
    unsigned updates = 0;
  private:
    std::tm current {};
  };
}
