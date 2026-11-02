#pragma once
namespace Pinetime::Controllers {
  class Settings {
  public:
    enum class ClockType { H24, H12 };
    enum class WeatherFormat { Metric, Imperial };
    ClockType GetClockType() const { return clockType; }
    WeatherFormat GetWeatherFormat() const { return weatherFormat; }
    ClockType clockType = ClockType::H24;
    WeatherFormat weatherFormat = WeatherFormat::Metric;
  };
}
