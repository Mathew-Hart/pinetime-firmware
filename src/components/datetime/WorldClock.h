#pragma once
#include <cstdint>

namespace Pinetime::Controllers::WorldClock {
  struct Time {
    uint8_t hours;
    uint8_t minutes;
    int8_t dayOffset;
  };

  constexpr bool IsValidTimeZone(int8_t timezone, int8_t dst) {
    return timezone >= -48 && timezone <= 56 && (dst == 0 || dst == 2 || dst == 4 || dst == 8);
  }

  // Proleptic Gregorian civil arithmetic; no global libc timezone/static tm.
  // Based on Howard Hinnant's public-domain civil calendar algorithms.
  constexpr int64_t FloorDays(int64_t seconds) {
    return seconds / 86400 - (seconds % 86400 < 0);
  }

  constexpr int64_t DaysFromCivil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<int64_t>(era) * 146097 + doe - 719468;
  }

  constexpr int YearFromDays(int64_t days) {
    days += 719468;
    const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(days - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    return static_cast<int>(yoe + era * 400) + (mp >= 10);
  }

  constexpr int64_t ParisTransition(int year, unsigned month) {
    // Current Europe/Paris rules: last Sunday of March/October, 01:00UTC.
    const int64_t last = DaysFromCivil(year, month, 31);
    const int weekday = static_cast<int>(((last + 4) % 7 + 7) % 7);
    return (last - weekday) * 86400 + 3600;
  }

  constexpr int32_t ParisOffsetSeconds(int64_t utc) {
    const int year = YearFromDays(FloorDays(utc));
    return utc >= ParisTransition(year, 3) && utc < ParisTransition(year, 10) ? 7200 : 3600;
  }

  constexpr Time FromLocal(int64_t local, int8_t utcOffsetQuarters, int32_t cityOffsetSeconds) {
    const int64_t world = local - static_cast<int32_t>(utcOffsetQuarters) * 900 + cityOffsetSeconds;
    const int64_t day = FloorDays(world);
    const int64_t minute = (world - day * 86400) / 60;
    return {static_cast<uint8_t>(minute / 60), static_cast<uint8_t>(minute % 60),
            static_cast<int8_t>(day - FloorDays(local))};
  }

  constexpr Time ParisFromLocal(int64_t localEpochSeconds, int8_t utcOffsetQuarters) {
    const int64_t utc = localEpochSeconds - static_cast<int32_t>(utcOffsetQuarters) * 900;
    return FromLocal(localEpochSeconds, utcOffsetQuarters, ParisOffsetSeconds(utc));
  }

  constexpr Time TokyoFromLocal(int64_t localEpochSeconds, int8_t utcOffsetQuarters) {
    return FromLocal(localEpochSeconds, utcOffsetQuarters, 9 * 3600);
  }
}
