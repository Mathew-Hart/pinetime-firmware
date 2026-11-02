#pragma once

#include <cmath>
#include <cstdint>

namespace Pinetime::Applications::Screens::EarthSolar {
  struct SunDirection {
    float x;
    float y;
    float z;
  };

  // Scope this watch visualization to 2000-01-01 through 2099-12-31 UTC.
  // Callers display an unshaded globe when UTC is unknown or unsupported.
  constexpr bool IsSupported(int64_t utcSeconds) {
    return utcSeconds >= 946684800LL && utcSeconds < 4102444800LL;
  }

  // NOAA's Meeus-based solar declination and equation-of-time calculations:
  // https://gml.noaa.gov/grad/solcalc/main.js
  // https://gml.noaa.gov/grad/solcalc/calcdetails.html
  // UTC seconds are POSIX seconds, with no local timezone or libc tm state.
  // The result is a unit vector in the fixed -100deg longitude/+20deg latitude
  // orthographic view: x is screen-right/east, y screen-up/north, z viewerward.
  // Unsupported epochs return the zero sentinel; check IsSupported first.
  inline SunDirection Direction(int64_t utcSeconds) {
    if (!IsSupported(utcSeconds)) {
      return {0.0f, 0.0f, 0.0f};
    }

    constexpr double Pi = 3.14159265358979323846;
    constexpr double RadiansPerDegree = Pi / 180.0;
    const auto radians = [](double degrees) { return degrees * RadiansPerDegree; };
    const auto normalize = [](double degrees) {
      const auto wrapped = std::fmod(degrees, 360.0);
      return wrapped < 0.0 ? wrapped + 360.0 : wrapped;
    };

    // Algebraically (JulianDay - 2451545.0)/36525; subtract the J2000 noon
    // epoch before division to preserve seconds without a large JD addition.
    const double centuries = (static_cast<double>(utcSeconds) - 946728000.0) / 3155760000.0;
    const double meanLongitude = normalize(280.46646 + centuries * (36000.76983 + centuries * 0.0003032));
    const double meanAnomaly = radians(normalize(357.52911 + centuries * (35999.05029 - 0.0001537 * centuries)));
    const double eccentricity = 0.016708634 - centuries * (0.000042037 + 0.0000001267 * centuries);
    const double sinAnomaly = std::sin(meanAnomaly);
    const double sinDoubleAnomaly = std::sin(2.0 * meanAnomaly);
    const double center = sinAnomaly * (1.914602 - centuries * (0.004817 + 0.000014 * centuries)) +
                          sinDoubleAnomaly * (0.019993 - 0.000101 * centuries) + std::sin(3.0 * meanAnomaly) * 0.000289;
    const double omega = radians(125.04 - 1934.136 * centuries);
    const double apparentLongitude = radians(meanLongitude + center - 0.00569 - 0.00478 * std::sin(omega));
    const double obliquitySeconds = 21.448 - centuries * (46.8150 + centuries * (0.00059 - centuries * 0.001813));
    const double obliquity = radians(23.0 + (26.0 + obliquitySeconds / 60.0) / 60.0 + 0.00256 * std::cos(omega));
    const double sinDeclination = std::sin(obliquity) * std::sin(apparentLongitude);
    const double cosDeclination = std::sqrt(1.0 - sinDeclination * sinDeclination);

    const double tangent = std::tan(obliquity / 2.0);
    const double y = tangent * tangent;
    const double longitudeRadians = radians(meanLongitude);
    const double equationOfTime =
      (y * std::sin(2.0 * longitudeRadians) - 2.0 * eccentricity * sinAnomaly +
       4.0 * eccentricity * y * sinAnomaly * std::cos(2.0 * longitudeRadians) - 0.5 * y * y * std::sin(4.0 * longitudeRadians) -
       1.25 * eccentricity * eccentricity * sinDoubleAnomaly) *
      (180.0 / Pi) * 4.0;

    const double utcMinutes = static_cast<double>(utcSeconds % 86400) / 60.0;
    // At solar noon, 720 = UTC_minutes + equationOfTime + 4*east_longitude.
    const double sunLongitude = 180.0 - (utcMinutes + equationOfTime) / 4.0;
    const double difference = radians(sunLongitude - (-100.0));
    constexpr double CenterLatitude = 20.0 * RadiansPerDegree;
    const double horizontal = cosDeclination * std::cos(difference);
    return {static_cast<float>(cosDeclination * std::sin(difference)),
            static_cast<float>(sinDeclination * std::cos(CenterLatitude) - horizontal * std::sin(CenterLatitude)),
            static_cast<float>(sinDeclination * std::sin(CenterLatitude) + horizontal * std::cos(CenterLatitude))};
  }
}
