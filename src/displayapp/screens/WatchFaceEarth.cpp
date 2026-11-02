#include "displayapp/screens/WatchFaceEarth.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include "components/ble/SimpleWeatherService.h"
#include "components/datetime/DateTimeController.h"
#include "components/settings/Settings.h"
#include "displayapp/screens/EarthSolarPosition.h"

using namespace Pinetime::Applications::Screens;

namespace {
  constexpr int16_t RightEdge = 234;

  lv_obj_t* CreateLabel(int16_t y, const lv_font_t* font, lv_color_t color) {
    auto* label = lv_label_create(lv_scr_act(), nullptr);
    lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font);
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
    lv_obj_set_style_local_text_letter_space(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_bg_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_set_style_local_bg_opa(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_60);
    lv_obj_set_style_local_radius(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 4);
    lv_obj_set_style_local_pad_left(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 3);
    lv_obj_set_style_local_pad_right(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 3);
    lv_obj_set_style_local_pad_top(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_style_local_pad_bottom(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_label_set_text_static(label, "");
    lv_label_set_long_mode(label, LV_LABEL_LONG_EXPAND);
    lv_label_set_align(label, LV_LABEL_ALIGN_RIGHT);
    lv_obj_set_pos(label, RightEdge - lv_obj_get_width(label), y);
    lv_obj_set_click(label, false);
    lv_obj_set_hidden(label, true);
    return label;
  }

  void SetText(lv_obj_t* label, const char* text) {
    if (std::strcmp(lv_label_get_text(label), text) != 0) {
      lv_label_set_text(label, text);
      lv_obj_set_x(label, RightEdge - lv_obj_get_width(label));
      lv_obj_set_hidden(label, text[0] == '\0');
    }
  }

  const char* Condition(Pinetime::Controllers::SimpleWeatherService::Icons icon) {
    using Icons = Pinetime::Controllers::SimpleWeatherService::Icons;
    switch (icon) {
      case Icons::Sun: return "CLEAR";
      case Icons::CloudsSun: return "PART CLOUD";
      case Icons::Clouds: return "CLOUDY";
      case Icons::BrokenClouds: return "OVERCAST";
      case Icons::Thunderstorm: return "THUNDERSTORM";
      case Icons::Snow: return "SNOW";
      case Icons::CloudShowerHeavy: return "SHOWERS";
      case Icons::CloudSunRain: return "RAIN";
      case Icons::Smog: return "MIST";
      default: return "NO WEATHER";
    }
  }
}

WatchFaceEarth::WatchFaceEarth(Controllers::DateTime& dateTimeController,
                             Controllers::Settings& settingsController,
                             Controllers::SimpleWeatherService* weatherService)
  : dateTimeController {dateTimeController}, settingsController {settingsController}, weatherService {weatherService}, globe {lv_scr_act()} {
  lv_obj_set_pos(globe.GetObject(), -8, 26);
  dateLabel = CreateLabel(2, &jetbrains_mono_bold_20, LV_COLOR_WHITE);
  timeLabel = CreateLabel(27, &jetbrains_mono_42, LV_COLOR_WHITE);
  periodLabel = CreateLabel(79, &jetbrains_mono_bold_20, LV_COLOR_SILVER);
  temperatureLabel = CreateLabel(183, &jetbrains_mono_bold_20, LV_COLOR_WHITE);
  conditionLabel = CreateLabel(212, &jetbrains_mono_bold_20, LV_COLOR_WHITE);
  Refresh();
  refreshTask = lv_task_create(RefreshTaskCallback, 1000, LV_TASK_PRIO_LOW, this);
}

WatchFaceEarth::~WatchFaceEarth() {
  lv_task_del(refreshTask);
  lv_obj_del(dateLabel);
  lv_obj_del(timeLabel);
  lv_obj_del(periodLabel);
  lv_obj_del(temperatureLabel);
  lv_obj_del(conditionLabel);
  // EarthGlobe owns its image and decoder, including invalidation of cached data.
}

void WatchFaceEarth::Refresh() {
  const auto localEpoch = std::chrono::duration_cast<std::chrono::seconds>(dateTimeController.CurrentDateTime().time_since_epoch()).count();
  const auto utcEpoch = localEpoch - static_cast<int64_t>(dateTimeController.UtcOffset()) * 900;
  const bool known = dateTimeController.HasTimeZone() && EarthSolar::IsSupported(utcEpoch);
  const auto bucket = utcEpoch / 300;
  if (!sunInitialized || known != sunKnown || (known && bucket != sunBucket)) {
    const auto sun = known ? EarthSolar::Direction(utcEpoch) : EarthSolar::SunDirection {0.0f, 0.0f, 0.0f};
    globe.SetSun(sun.x, sun.y, sun.z, known);
    sunBucket = bucket;
    sunKnown = known;
    sunInitialized = true;
  }

  char text[32];
  std::snprintf(text, sizeof(text), "%s %u", dateTimeController.DayOfWeekShortToString(), static_cast<unsigned>(dateTimeController.Day()));
  SetText(dateLabel, text);
  const bool twelveHour = settingsController.GetClockType() == Controllers::Settings::ClockType::H12;
  auto hour = dateTimeController.Hours();
  if (twelveHour) {
    SetText(periodLabel, hour < 12 ? "AM" : "PM");
    hour = hour % 12;
    if (hour == 0) {
      hour = 12;
    }
    std::snprintf(text, sizeof(text), "%u:%02u", static_cast<unsigned>(hour), static_cast<unsigned>(dateTimeController.Minutes()));
  } else {
    SetText(periodLabel, "");
    std::snprintf(text, sizeof(text), "%02u:%02u", static_cast<unsigned>(hour), static_cast<unsigned>(dateTimeController.Minutes()));
  }
  SetText(timeLabel, text);

  const char* condition = "NO WEATHER";
  std::snprintf(text, sizeof(text), "--");
  if (weatherService != nullptr) {
    if (const auto weather = weatherService->Current()) {
      const bool imperial = settingsController.GetWeatherFormat() == Controllers::Settings::WeatherFormat::Imperial;
      const auto temperature = imperial ? weather->temperature.Fahrenheit() : weather->temperature.Celsius();
      std::snprintf(text, sizeof(text), "%d°%c", static_cast<int>(temperature), imperial ? 'F' : 'C');
      condition = Condition(weather->iconId);
    }
  }
  SetText(temperatureLabel, text);
  SetText(conditionLabel, known ? condition : "SYNC TIME");
}
