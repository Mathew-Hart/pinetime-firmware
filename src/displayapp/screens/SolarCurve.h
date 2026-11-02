#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Pinetime::Applications::Screens::SolarCurve {
  struct Point {
    int16_t x;
    int16_t y;
  };

  inline constexpr int16_t LeftX = 10;
  inline constexpr int16_t RightX = 230;
  inline constexpr int16_t HorizonY = 151;
  inline constexpr int16_t DayAmplitude = 68;
  inline constexpr int16_t NightAmplitude = 58;

  // A same-calendar-day interval with both a daylight and a night portion.
  // Missing/polar/overnight timestamps are a caller policy, not a valid curve.
  constexpr bool IsValidTimes(int16_t sunrise, int16_t sunset) {
    return sunrise >= 0 && sunrise < sunset && sunset < 1440;
  }

  // A visual daylight path, not an astronomical altitude. Minute0 and1440
  // share their vertical position but retain separate graph endpoints. The
  // optional fractional minute positions the sun between minute samples.
  // Invalid timing data returns a neutral horizon point defensively; callers
  // decide whether to substitute nominal sunrise/sunset or show no data.
  inline Point PointAt(double minutes, int16_t sunrise, int16_t sunset) {
    if (!std::isfinite(minutes)) {
      return {LeftX, HorizonY};
    }
    minutes = std::clamp(minutes, 0.0, 1440.0);
    const auto x = static_cast<int16_t>(std::lround(LeftX + (RightX - LeftX) * minutes / 1440.0));
    if (!IsValidTimes(sunrise, sunset)) {
      return {x, HorizonY};
    }

    constexpr double Pi = 3.14159265358979323846;
    double y;
    if (minutes >= sunrise && minutes <= sunset) {
      const double phase = (minutes - sunrise) / (sunset - sunrise);
      y = HorizonY - DayAmplitude * std::sin(Pi * phase);
    } else {
      const double elapsed = minutes < sunrise ? minutes + 1440.0 - sunset : minutes - sunset;
      const double phase = elapsed / (1440 - sunset + sunrise);
      y = HorizonY + NightAmplitude * std::sin(Pi * phase);
    }
    return {x, static_cast<int16_t>(std::lround(y))};
  }
}
