#include "displayapp/screens/WatchFaceGps.h"

#include <cstdio>
#include <cstring>
#include "components/ble/BleController.h"
#include "components/ble/GpsService.h"
#include "components/datetime/DateTimeController.h"
#include "components/settings/Settings.h"

using namespace Pinetime::Applications::Screens;
namespace Gps = Pinetime::Controllers::GpsData;

namespace {
  constexpr int16_t RightEdge = 232;
  constexpr std::array<const char*, 5> RowNames {"LAT", "LON", "ALT", "SAT", "HDOP"};
  constexpr std::array<int16_t, 5> RowY {76, 104, 132, 160, 188};

  lv_obj_t* CreateLabel(int16_t x, int16_t y, const lv_font_t* font, lv_color_t color) {
    auto* label = lv_label_create(lv_scr_act(), nullptr);
    lv_label_set_text_static(label, "");
    lv_label_set_long_mode(label, LV_LABEL_LONG_EXPAND);
    lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font);
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
    lv_obj_set_style_local_text_letter_space(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_pos(label, x, y);
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

  void SetColor(lv_obj_t* label, lv_color_t color) {
    if (lv_obj_get_style_text_color(label, LV_LABEL_PART_MAIN).full != color.full) {
      lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
    }
  }

  void Coordinate(char* text, size_t size, int32_t valueE7, char positive, char negative) {
    // Round the magnitude to six decimal degrees without floating printf or
    // overflowing on a negative longitude. Preserve the supplied hemisphere.
    const auto magnitude = static_cast<uint64_t>(valueE7 < 0 ? -static_cast<int64_t>(valueE7) : valueE7);
    const auto microdegrees = (magnitude + 5) / 10;
    std::snprintf(text, size, "%lu.%06lu %c", static_cast<unsigned long>(microdegrees / 1000000),
                  static_cast<unsigned long>(microdegrees % 1000000), valueE7 < 0 ? negative : positive);
  }

  void Age(char* text, size_t size, uint32_t seconds) {
    if (seconds < 60) {
      std::snprintf(text, size, "STALE %lus", static_cast<unsigned long>(seconds));
    } else if (seconds < 3600) {
      std::snprintf(text, size, "STALE %lum", static_cast<unsigned long>(seconds / 60));
    } else if (seconds < 86400) {
      std::snprintf(text, size, "STALE %luh", static_cast<unsigned long>(seconds / 3600));
    } else if (seconds < 10 * 86400) {
      std::snprintf(text, size, "STALE %lud", static_cast<unsigned long>(seconds / 86400));
    } else {
      std::snprintf(text, size, "STALE >9d");
    }
  }
}

WatchFaceGps::WatchFaceGps(Controllers::DateTime& dateTimeController,
                         Controllers::Settings& settingsController,
                         const Controllers::Ble& bleController,
                         Controllers::GpsService* gpsService)
  : dateTimeController {dateTimeController}, settingsController {settingsController},
    bleController {bleController}, gpsService {gpsService} {
  timeLabel = CreateLabel(8, 4, &jetbrains_mono_42, LV_COLOR_WHITE);
  periodLabel = CreateLabel(146, 9, &jetbrains_mono_bold_20, LV_COLOR_SILVER);
  fixLabel = CreateLabel(184, 9, &jetbrains_mono_bold_20, LV_COLOR_GRAY);
  sourceLabel = CreateLabel(8, 44, &jetbrains_mono_bold_20, LV_COLOR_GRAY);
  ageLabel = CreateLabel(148, 44, &jetbrains_mono_bold_20, LV_COLOR_GRAY);
  SetText(sourceLabel, "PHONE GPS");
  separator = lv_obj_create(lv_scr_act(), nullptr);
  lv_obj_reset_style_list(separator, LV_OBJ_PART_MAIN);
  lv_obj_set_pos(separator, 8, 70);
  lv_obj_set_size(separator, 224, 1);
  lv_obj_set_style_local_bg_opa(separator, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
  lv_obj_set_style_local_bg_color(separator, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_color_make(55, 68, 76));
  lv_obj_set_click(separator, false);
  for (size_t i = 0; i < RowNames.size(); ++i) {
    rowLabels[i] = CreateLabel(8, RowY[i], &jetbrains_mono_bold_20, LV_COLOR_GRAY);
    SetText(rowLabels[i], RowNames[i]);
    values[i] = CreateLabel(148, RowY[i], &jetbrains_mono_bold_20, i < 2 ? LV_COLOR_AQUA : LV_COLOR_WHITE);
  }
  Refresh();
  refreshTask = lv_task_create(RefreshTaskCallback, 1000, LV_TASK_PRIO_LOW, this);
}

WatchFaceGps::~WatchFaceGps() {
  lv_task_del(refreshTask);
  lv_obj_del(timeLabel);
  lv_obj_del(periodLabel);
  lv_obj_del(fixLabel);
  lv_obj_del(sourceLabel);
  lv_obj_del(ageLabel);
  lv_obj_del(separator);
  for (auto* label : rowLabels) {
    lv_obj_del(label);
  }
  for (auto* label : values) {
    lv_obj_del(label);
  }
}

void WatchFaceGps::Refresh() {
  dateTimeController.CurrentDateTime();
  char text[32];
  const bool twelveHour = settingsController.GetClockType() == Controllers::Settings::ClockType::H12;
  const auto hour = dateTimeController.Hours();
  if (twelveHour) {
    const auto displayHour = hour % 12 == 0 ? 12 : hour % 12;
    std::snprintf(text, sizeof(text), "%u:%02u", static_cast<unsigned>(displayHour), static_cast<unsigned>(dateTimeController.Minutes()));
    SetText(periodLabel, hour < 12 ? "AM" : "PM");
  } else {
    std::snprintf(text, sizeof(text), "%02u:%02u", static_cast<unsigned>(hour), static_cast<unsigned>(dateTimeController.Minutes()));
    SetText(periodLabel, "");
  }
  SetText(timeLabel, text);

  const auto snapshot = gpsService == nullptr ? Gps::Snapshot {} : gpsService->GetSnapshot();
  const bool connected = bleController.IsConnected() && (gpsService == nullptr || snapshot.connected);
  const bool usable = connected && snapshot.received && snapshot.coherent && snapshot.fresh;
  const char* fix = "--";
  lv_color_t fixColor = LV_COLOR_GRAY;
  if (usable) {
    switch (snapshot.fix) {
      case Gps::Fix::TwoD: fix = "2D"; fixColor = LV_COLOR_AQUA; break;
      case Gps::Fix::ThreeD: fix = "3D"; fixColor = LV_COLOR_LIME; break;
      case Gps::Fix::Unknown: fix = "."; break;
      case Gps::Fix::None: fix = "NONE"; break;
    }
  }
  SetText(fixLabel, fix, true);
  SetColor(fixLabel, fixColor);
  if (!connected) {
    SetText(ageLabel, "NO PHONE", true);
  } else if (!snapshot.coherent) {
    SetText(ageLabel, "WAIT", true);
  } else if (!snapshot.received || snapshot.ageSeconds == Gps::UnknownAge) {
    SetText(ageLabel, "NO DATA", true);
  } else if (!snapshot.fresh) {
    Age(text, sizeof(text), snapshot.ageSeconds);
    SetText(ageLabel, text, true);
  } else {
    std::snprintf(text, sizeof(text), "%lus", static_cast<unsigned long>(snapshot.ageSeconds));
    SetText(ageLabel, text, true);
  }
  SetColor(ageLabel, usable ? LV_COLOR_SILVER : LV_COLOR_ORANGE);

  for (size_t i = 0; i < values.size(); ++i) {
    std::snprintf(text, sizeof(text), "--");
    if (usable) {
      if (i == 0 && snapshot.Has(Gps::Flag::Position)) {
        Coordinate(text, sizeof(text), snapshot.latitudeE7, 'N', 'S');
      } else if (i == 1 && snapshot.Has(Gps::Flag::Position)) {
        Coordinate(text, sizeof(text), snapshot.longitudeE7, 'E', 'W');
      } else if (i == 2 && snapshot.fix == Gps::Fix::ThreeD && snapshot.Has(Gps::Flag::Altitude)) {
        const auto meters = static_cast<int32_t>(snapshot.altitudeMeters);
        if (settingsController.GetWeatherFormat() == Controllers::Settings::WeatherFormat::Imperial) {
          const auto magnitude = meters < 0 ? -meters : meters;
          const auto feet = (magnitude * 10000 + 1524) / 3048;
          std::snprintf(text, sizeof(text), "%ld ft", static_cast<long>(meters < 0 ? -feet : feet));
        } else {
          std::snprintf(text, sizeof(text), "%ld m", static_cast<long>(meters));
        }
      } else if (i == 3) {
        char used[4] = "--", visible[4] = "--";
        if (snapshot.Has(Gps::Flag::SatellitesUsed)) {
          std::snprintf(used, sizeof(used), "%u", static_cast<unsigned>(snapshot.satellitesUsed));
        }
        if (snapshot.Has(Gps::Flag::SatellitesInView)) {
          std::snprintf(visible, sizeof(visible), "%u", static_cast<unsigned>(snapshot.satellitesInView));
        }
        std::snprintf(text, sizeof(text), "%s/%s", used, visible);
      } else if (i == 4 && snapshot.Has(Gps::Flag::Hdop)) {
        std::snprintf(text, sizeof(text), "%u.%02u", static_cast<unsigned>(snapshot.hdopCenti / 100),
                      static_cast<unsigned>(snapshot.hdopCenti % 100));
      }
    }
    SetText(values[i], text, true);
  }
}
