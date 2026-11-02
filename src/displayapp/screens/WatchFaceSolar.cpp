#include "displayapp/screens/WatchFaceSolar.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include "components/ble/SimpleWeatherService.h"
#include "components/datetime/DateTimeController.h"
#include "components/settings/Settings.h"
#include "displayapp/screens/SolarCurve.h"

extern "C" {
  LV_FONT_DECLARE(solar_time_42)
  LV_FONT_DECLARE(solar_text_18)
}

using namespace Pinetime::Applications::Screens;

namespace {
  constexpr int16_t RightEdge = 232;
  constexpr int16_t SunDiameter = 10;

  lv_obj_t* CreateLabel(int16_t y, const lv_font_t* font, lv_color_t color) {
    auto* label = lv_label_create(lv_scr_act(), nullptr);
    lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font);
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
    lv_obj_set_style_local_text_letter_space(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_label_set_text_static(label, "");
    lv_label_set_long_mode(label, LV_LABEL_LONG_EXPAND);
    lv_obj_set_pos(label, 10, y);
    lv_obj_set_click(label, false);
    lv_obj_set_hidden(label, true);
    return label;
  }

  void SetText(lv_obj_t* label, const char* text, bool rightAligned = false) {
    if (std::strcmp(lv_label_get_text(label), text) != 0) {
      lv_label_set_text(label, text);
      if (rightAligned) {
        lv_obj_set_x(label, RightEdge - lv_obj_get_width(label));
      }
      lv_obj_set_hidden(label, text[0] == '\0');
    }
  }

  lv_obj_t* CreateLine(lv_color_t color) {
    auto* line = lv_line_create(lv_scr_act(), nullptr);
    lv_obj_set_style_local_line_color(line, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, color);
    lv_obj_set_style_local_line_width(line, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, 1);
    lv_obj_set_click(line, false);
    lv_obj_set_pos(line, 0, 0);
    return line;
  }
}

WatchFaceSolar::WatchFaceSolar(Controllers::DateTime& dateTimeController,
                             Controllers::Settings& settingsController,
                             Controllers::SimpleWeatherService* weatherService)
  : dateTimeController {dateTimeController}, settingsController {settingsController}, weatherService {weatherService}, glow {lv_scr_act()} {
  horizon = CreateLine(lv_color_make(55, 68, 76));
  lv_line_set_points(horizon, horizonPoints.data(), horizonPoints.size());
  curve = CreateLine(lv_color_make(150, 168, 179));
  sun = lv_obj_create(lv_scr_act(), nullptr);
  lv_obj_set_size(sun, SunDiameter, SunDiameter);
  lv_obj_set_style_local_radius(sun, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
  lv_obj_set_style_local_bg_color(sun, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_obj_set_style_local_bg_opa(sun, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
  lv_obj_set_style_local_border_width(sun, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_click(sun, false);
  dateLabel = CreateLabel(8, &solar_text_18, lv_color_make(205, 213, 218));
  timeLabel = CreateLabel(38, &solar_time_42, LV_COLOR_WHITE);
  periodLabel = CreateLabel(77, &solar_text_18, lv_color_make(173, 193, 203));
  Refresh();
  refreshTask = lv_task_create(RefreshTaskCallback, 1000, LV_TASK_PRIO_LOW, this);
}

WatchFaceSolar::~WatchFaceSolar() {
  lv_task_del(refreshTask);
  lv_obj_del(horizon);
  lv_obj_del(curve);
  lv_obj_del(sun);
  lv_obj_del(dateLabel);
  lv_obj_del(timeLabel);
  lv_obj_del(periodLabel);
  // SolarGlow owns its image and decoder, including cache invalidation.
}

void WatchFaceSolar::SetCurve(int16_t sunrise, int16_t sunset) {
  // Half-minute units preserve peaks even in very short daylight/night spans.
  std::array<uint16_t, 53> samples {};
  for (size_t i = 0; i < 49; ++i) {
    samples[i] = i * 60;
  }
  samples[49] = sunrise * 2;
  samples[50] = sunrise + sunset;
  samples[51] = sunset * 2;
  samples[52] = (sunset + 1440 + sunrise) % 2880;
  std::sort(samples.begin(), samples.end());
  const auto end = std::unique(samples.begin(), samples.end());
  const auto count = static_cast<size_t>(end - samples.begin());
  for (size_t i = 0; i < count; ++i) {
    const auto point = SolarCurve::PointAt(static_cast<double>(samples[i]) / 2.0, sunrise, sunset);
    curvePoints[i] = {point.x, point.y};
  }
  lv_line_set_points(curve, curvePoints.data(), count);
}

void WatchFaceSolar::Refresh() {
  const auto localEpoch = std::chrono::duration_cast<std::chrono::seconds>(dateTimeController.CurrentDateTime().time_since_epoch()).count();
  // Nominal 06:00/18:00 makes a useful daily illustration without phone data.
  // Version-1 weather may supply actual local sunrise/set for the current day.
  int16_t sunrise = 360;
  int16_t sunset = 1080;
  if (weatherService != nullptr && localEpoch >= 0) {
    if (const auto weather = weatherService->Current()) {
      const auto now = static_cast<uint64_t>(localEpoch);
      if (weather->timestamp <= now && weather->timestamp / 86400 == now / 86400 &&
          SolarCurve::IsValidTimes(weather->sunrise, weather->sunset)) {
        sunrise = weather->sunrise;
        sunset = weather->sunset;
      }
    }
  }

  const auto minute = static_cast<int16_t>(dateTimeController.Hours() * 60 + dateTimeController.Minutes());
  const bool newCurve = sunrise != lastSunrise || sunset != lastSunset;
  if (newCurve) {
    SetCurve(sunrise, sunset);
    lastSunrise = sunrise;
    lastSunset = sunset;
  }
  if (newCurve || minute != lastMinute) {
    const auto point = SolarCurve::PointAt(minute, sunrise, sunset);
    lv_obj_set_pos(glow.GetObject(), point.x - SolarGlow::Diameter / 2, point.y - SolarGlow::Diameter / 2);
    lv_obj_set_pos(sun, point.x - SunDiameter / 2, point.y - SunDiameter / 2);
    lastMinute = minute;
  }

  char text[24];
  std::snprintf(text, sizeof(text), "%s %u", dateTimeController.DayOfWeekShortToString(), static_cast<unsigned>(dateTimeController.Day()));
  SetText(dateLabel, text);
  const bool twelveHour = settingsController.GetClockType() == Controllers::Settings::ClockType::H12;
  auto hour = dateTimeController.Hours();
  if (twelveHour) {
    SetText(periodLabel, hour < 12 ? "AM" : "PM", true);
    hour %= 12;
    if (hour == 0) {
      hour = 12;
    }
    std::snprintf(text, sizeof(text), "%u:%02u", static_cast<unsigned>(hour), static_cast<unsigned>(dateTimeController.Minutes()));
  } else {
    SetText(periodLabel, "", true);
    std::snprintf(text, sizeof(text), "%02u:%02u", static_cast<unsigned>(hour), static_cast<unsigned>(dateTimeController.Minutes()));
  }
  SetText(timeLabel, text, true);
}
