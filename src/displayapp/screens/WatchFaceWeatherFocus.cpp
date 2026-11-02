#include "displayapp/screens/WatchFaceWeatherFocus.h"

#include <chrono>
#include <cstdio>
#include "components/alarm/AlarmController.h"
#include "components/battery/BatteryController.h"
#include "components/ble/BleController.h"
#include "components/ble/NotificationManager.h"
#include "components/ble/SimpleWeatherService.h"
#include "components/datetime/DateTimeController.h"
#include "components/datetime/WorldClock.h"
#include "components/settings/Settings.h"
#include "displayapp/screens/Symbols.h"
#include "displayapp/screens/WeatherSymbols.h"

using namespace Pinetime::Applications::Screens;

namespace {
  const char* Condition(Pinetime::Controllers::SimpleWeatherService::Icons icon) {
    using Icons = Pinetime::Controllers::SimpleWeatherService::Icons;
    switch (icon) {
      case Icons::Sun: return "CLEAR";
      case Icons::CloudsSun: return "PART CLOUD";
      case Icons::Clouds: return "CLOUDY";
      case Icons::BrokenClouds: return "OVERCAST";
      case Icons::Thunderstorm: return "THUNDER";
      case Icons::Snow: return "SNOW";
      case Icons::CloudShowerHeavy: return "SHOWERS";
      case Icons::CloudSunRain: return "RAIN";
      case Icons::Smog: return "MIST";
      default: return "NO DATA";
    }
  }
}

WatchFaceWeatherFocus::WatchFaceWeatherFocus(Pinetime::Applications::AppControllers& controllers)
  : controllers {controllers}, view {lv_scr_act()} {
  Refresh();
  refreshTask = lv_task_create(RefreshTaskCallback, 1000, LV_TASK_PRIO_MID, this);
}

WatchFaceWeatherFocus::~WatchFaceWeatherFocus() {
  lv_task_del(refreshTask);
}

void WatchFaceWeatherFocus::Refresh() {
  using Field = WeatherFocusView::Field;
  auto& dateTime = controllers.dateTimeController;
  const auto localEpoch = std::chrono::duration_cast<std::chrono::seconds>(dateTime.CurrentDateTime().time_since_epoch()).count();
  const bool twelveHour = controllers.settingsController.GetClockType() == Pinetime::Controllers::Settings::ClockType::H12;
  char text[64];

  std::snprintf(text, sizeof(text), "%s %02d-%02d", dateTime.DayOfWeekShortToString(), static_cast<int>(dateTime.Month()), dateTime.Day());
  view.Set(Field::Date, text);
  auto hour = dateTime.Hours();
  if (twelveHour) {
    view.Set(Field::Period, hour < 12 ? "AM" : "PM");
    hour = hour % 12;
    if (hour == 0) hour = 12;
  } else {
    view.Set(Field::Period, "");
  }
  std::snprintf(text, sizeof(text), "%02d:%02d", hour, dateTime.Minutes());
  view.Set(Field::Time, text);

  const auto battery = controllers.batteryController.PercentRemaining();
  const char* batteryGlyph = battery >= 80 ? "\xEF\x89\x80" : battery >= 60 ? "\xEF\x89\x81" :
                             battery >= 40 ? "\xEF\x89\x82" : battery >= 20 ? "\xEF\x89\x83" : "\xEF\x89\x84";
  std::snprintf(text, sizeof(text), "%s%s%s%s %d%%",
                controllers.bleController.IsConnected() ? Symbols::bluetooth : "",
                controllers.notificationManager.AreNewNotificationsAvailable() ? Symbols::bell : "",
                controllers.batteryController.IsPowerPresent() ? Symbols::plug : "",
                batteryGlyph, battery);
  view.Set(Field::Status, text);
  const auto& alarm = controllers.alarmController;
  if (alarm.IsEnabled()) {
    std::snprintf(text, sizeof(text), "%s ALM %02d:%02d", Symbols::bell, alarm.Hours(), alarm.Minutes());
  } else {
    std::snprintf(text, sizeof(text), "%s ALM OFF", Symbols::bell);
  }
  view.Set(Field::Alarm, text);
  view.SetAlarmEnabled(alarm.IsEnabled());

  if (dateTime.HasTimeZone()) {
    const int offset = dateTime.UtcOffset() * 15;
    const int magnitude = offset < 0 ? -offset : offset;
    if (magnitude % 60 == 0) {
      std::snprintf(text, sizeof(text), "UTC%c%02d", offset < 0 ? '-' : '+', magnitude / 60);
    } else {
      std::snprintf(text, sizeof(text), "UTC%c%02d:%02d", offset < 0 ? '-' : '+', magnitude / 60, magnitude % 60);
    }
    view.Set(Field::Zone, text);
    const auto paris = Pinetime::Controllers::WorldClock::ParisFromLocal(localEpoch, dateTime.UtcOffset());
    const auto tokyo = Pinetime::Controllers::WorldClock::TokyoFromLocal(localEpoch, dateTime.UtcOffset());
    std::snprintf(text, sizeof(text), "%02d:%02d", paris.hours, paris.minutes);
    view.Set(Field::ParisTime, text);
    if (paris.dayOffset != 0) std::snprintf(text, sizeof(text), "%+d", paris.dayOffset);
    else text[0] = '\0';
    view.Set(Field::ParisDay, text);
    std::snprintf(text, sizeof(text), "%02d:%02d", tokyo.hours, tokyo.minutes);
    view.Set(Field::TokyoTime, text);
    if (tokyo.dayOffset != 0) std::snprintf(text, sizeof(text), "%+d", tokyo.dayOffset);
    else text[0] = '\0';
    view.Set(Field::TokyoDay, text);
  } else {
    view.Set(Field::Zone, "LOCAL");
    view.Set(Field::ParisTime, "--:--");
    view.Set(Field::TokyoTime, "--:--");
    view.Set(Field::ParisDay, "");
    view.Set(Field::TokyoDay, "");
  }

  const auto weather = controllers.weatherController->Current();
  if (weather) {
    const bool imperial = controllers.settingsController.GetWeatherFormat() == Pinetime::Controllers::Settings::WeatherFormat::Imperial;
    const auto convert = [imperial](const auto& temperature) { return imperial ? temperature.Fahrenheit() : temperature.Celsius(); };
    std::snprintf(text, sizeof(text), "%d", convert(weather->temperature));
    view.Set(Field::Temperature, text);
    view.Set(Field::Unit, imperial ? "°F" : "°C");
    view.Set(Field::Condition, Condition(weather->iconId));
    view.Set(Field::WeatherIcon, Symbols::GetSymbol(weather->iconId, controllers.weatherController->IsNight()));
    std::snprintf(text, sizeof(text), "H %d° L %d°", convert(weather->maxTemperature), convert(weather->minTemperature));
    view.Set(Field::Range, text);
  } else {
    view.Set(Field::Temperature, "--");
    view.Set(Field::Unit, "");
    view.Set(Field::Condition, "NO DATA");
    view.Set(Field::WeatherIcon, Symbols::cloud);
    view.Set(Field::Range, "");
  }
}
