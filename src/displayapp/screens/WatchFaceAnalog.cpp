#include "displayapp/screens/WatchFaceAnalog.h"
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstring>
#include <lvgl/lvgl.h>
#include "displayapp/screens/BatteryIcon.h"
#include "displayapp/screens/BleIcon.h"
#include "displayapp/screens/Symbols.h"
#include "displayapp/screens/NotificationIcon.h"
#include "components/settings/Settings.h"
#include "components/ble/SimpleWeatherService.h"
#include "components/heartrate/HeartRateController.h"
#include "displayapp/DisplayApp.h"
#include "displayapp/InfiniTimeTheme.h"

using namespace Pinetime::Applications::Screens;

namespace {
  constexpr int16_t HourLength = 70;
  constexpr int16_t MinuteLength = 90;
  constexpr int16_t SecondLength = 110;

  // LVGL sin isn't constexpr (though it could be if it were C++) so fix size here
  // All the types are hardcoded anyway and would need changing if the size changed
  // sin(90) = 1 so the value of _lv_trigo_sin(90) is the scaling factor
  constexpr int16_t LV_TRIG_SCALE = std::numeric_limits<int16_t>::max(); // = _lv_trigo_sin(90)

  int16_t Cosine(int16_t angle) {
    return _lv_trigo_sin(angle + 90);
  }

  int16_t Sine(int16_t angle) {
    return _lv_trigo_sin(angle);
  }

  int16_t CoordinateXRelocate(int16_t x) {
    return (x + LV_HOR_RES / 2);
  }

  int16_t CoordinateYRelocate(int16_t y) {
    return std::abs(y - LV_HOR_RES / 2);
  }

  lv_point_t CoordinateRelocate(int16_t radius, int16_t angle) {
    return lv_point_t {.x = CoordinateXRelocate(radius * static_cast<int32_t>(Sine(angle)) / LV_TRIG_SCALE),
                       .y = CoordinateYRelocate(radius * static_cast<int32_t>(Cosine(angle)) / LV_TRIG_SCALE)};
  }

}

WatchFaceAnalog::WatchFaceAnalog(Controllers::DateTime& dateTimeController,
                                 const Controllers::Battery& batteryController,
                                 const Controllers::Ble& bleController,
                                 Controllers::NotificationManager& notificationManager,
                                 Controllers::Settings& settingsController,
                                 Controllers::SimpleWeatherService& weatherService,
                                 Controllers::HeartRateController& heartRateController,
                                 DisplayApp& displayApp)
  : currentDateTime {{}},
    batteryIcon(true),
    dateTimeController {dateTimeController},
    batteryController {batteryController},
    bleController {bleController},
    notificationManager {notificationManager},
    settingsController {settingsController},
    weatherService {weatherService},
    heartRateController {heartRateController} {

  sHour = 99;
  sMinute = 99;
  sSecond = 99;

  minor_scales = lv_linemeter_create(lv_scr_act(), nullptr);
  lv_linemeter_set_scale(minor_scales, 300, 51);
  lv_linemeter_set_angle_offset(minor_scales, 180);
  lv_obj_set_size(minor_scales, 240, 240);
  lv_obj_align(minor_scales, nullptr, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_local_bg_opa(minor_scales, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
  lv_obj_set_style_local_scale_width(minor_scales, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, 4);
  lv_obj_set_style_local_scale_end_line_width(minor_scales, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, 1);
  lv_obj_set_style_local_scale_end_color(minor_scales, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);

  major_scales = lv_linemeter_create(lv_scr_act(), nullptr);
  lv_linemeter_set_scale(major_scales, 300, 11);
  lv_linemeter_set_angle_offset(major_scales, 180);
  lv_obj_set_size(major_scales, 240, 240);
  lv_obj_align(major_scales, nullptr, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_local_bg_opa(major_scales, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
  lv_obj_set_style_local_scale_width(major_scales, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, 6);
  lv_obj_set_style_local_scale_end_line_width(major_scales, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, 4);
  lv_obj_set_style_local_scale_end_color(major_scales, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);

  large_scales = lv_linemeter_create(lv_scr_act(), nullptr);
  lv_linemeter_set_scale(large_scales, 180, 3);
  lv_linemeter_set_angle_offset(large_scales, 180);
  lv_obj_set_size(large_scales, 240, 240);
  lv_obj_align(large_scales, nullptr, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_local_bg_opa(large_scales, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
  lv_obj_set_style_local_scale_width(large_scales, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, 20);
  lv_obj_set_style_local_scale_end_line_width(large_scales, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, 4);
  lv_obj_set_style_local_scale_end_color(large_scales, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_AQUA);

  twelve = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_align(twelve, LV_LABEL_ALIGN_CENTER);
  lv_label_set_text_static(twelve, "12");
  lv_obj_set_pos(twelve, 110, 10);
  lv_obj_set_style_local_text_color(twelve, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_AQUA);

  batteryIcon.Create(lv_scr_act());
  lv_obj_align(batteryIcon.GetObject(), nullptr, LV_ALIGN_IN_TOP_RIGHT, 0, 0);

  plugIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(plugIcon, Symbols::plug);
  lv_obj_align(plugIcon, nullptr, LV_ALIGN_IN_TOP_RIGHT, 0, 0);

  bleIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(bleIcon, "");
  lv_obj_align(bleIcon, nullptr, LV_ALIGN_IN_TOP_RIGHT, -30, 0);

  notificationIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(notificationIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_LIME);
  lv_label_set_text_static(notificationIcon, NotificationIcon::GetIcon(false));
  lv_obj_align(notificationIcon, nullptr, LV_ALIGN_IN_BOTTOM_RIGHT, -6, -6);

  // Date - Day / Week day

  label_date_day = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_color(label_date_day, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, Colors::orange);
  lv_label_set_text_fmt(label_date_day, "%s\n%02i", dateTimeController.DayOfWeekShortToString(), dateTimeController.Day());
  lv_label_set_align(label_date_day, LV_LABEL_ALIGN_CENTER);
  lv_obj_align(label_date_day, nullptr, LV_ALIGN_CENTER, 50, 0);

  minute_body = lv_line_create(lv_scr_act(), nullptr);
  minute_body_trace = lv_line_create(lv_scr_act(), nullptr);
  hour_body = lv_line_create(lv_scr_act(), nullptr);
  hour_body_trace = lv_line_create(lv_scr_act(), nullptr);
  second_body = lv_line_create(lv_scr_act(), nullptr);

  lv_style_init(&second_line_style);
  lv_style_set_line_width(&second_line_style, LV_STATE_DEFAULT, 3);
  lv_style_set_line_color(&second_line_style, LV_STATE_DEFAULT, LV_COLOR_RED);
  lv_style_set_line_rounded(&second_line_style, LV_STATE_DEFAULT, true);
  lv_obj_add_style(second_body, LV_LINE_PART_MAIN, &second_line_style);

  lv_style_init(&minute_line_style);
  lv_style_set_line_width(&minute_line_style, LV_STATE_DEFAULT, 7);
  lv_style_set_line_color(&minute_line_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_style_set_line_rounded(&minute_line_style, LV_STATE_DEFAULT, true);
  lv_obj_add_style(minute_body, LV_LINE_PART_MAIN, &minute_line_style);

  lv_style_init(&minute_line_style_trace);
  lv_style_set_line_width(&minute_line_style_trace, LV_STATE_DEFAULT, 3);
  lv_style_set_line_color(&minute_line_style_trace, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_style_set_line_rounded(&minute_line_style_trace, LV_STATE_DEFAULT, false);
  lv_obj_add_style(minute_body_trace, LV_LINE_PART_MAIN, &minute_line_style_trace);

  lv_style_init(&hour_line_style);
  lv_style_set_line_width(&hour_line_style, LV_STATE_DEFAULT, 7);
  lv_style_set_line_color(&hour_line_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_style_set_line_rounded(&hour_line_style, LV_STATE_DEFAULT, true);
  lv_obj_add_style(hour_body, LV_LINE_PART_MAIN, &hour_line_style);

  lv_style_init(&hour_line_style_trace);
  lv_style_set_line_width(&hour_line_style_trace, LV_STATE_DEFAULT, 3);
  lv_style_set_line_color(&hour_line_style_trace, LV_STATE_DEFAULT, LV_COLOR_WHITE);
  lv_style_set_line_rounded(&hour_line_style_trace, LV_STATE_DEFAULT, false);
  lv_obj_add_style(hour_body_trace, LV_LINE_PART_MAIN, &hour_line_style_trace);

  weather = std::make_unique<AnalogWeather>(lv_scr_act(), [](void* context) {
    static_cast<DisplayApp*>(context)->StartApp(Apps::Weather, DisplayApp::FullRefreshDirections::Up);
  }, &displayApp);

  // Keep these above the clock hands just like the weather complication. They
  // are read-only: the clock's bottom-left gestures retain their normal action.
  heartRateIcon = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(heartRateIcon, Symbols::heartBeat);
  lv_obj_set_style_local_text_color(heartRateIcon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_RED);
  lv_obj_set_click(heartRateIcon, false);
  lv_obj_set_pos(heartRateIcon, 6, 208);

  labelHeartRate = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_style_local_text_font(labelHeartRate, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_bold_20);
  lv_obj_set_style_local_text_letter_space(labelHeartRate, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_label_set_text_static(labelHeartRate, "--");
  lv_obj_set_click(labelHeartRate, false);
  lv_obj_set_pos(labelHeartRate, 31, 208);

  taskRefresh = lv_task_create(RefreshTaskCallback, LV_DISP_DEF_REFR_PERIOD, LV_TASK_PRIO_MID, this);

  Refresh();
}

WatchFaceAnalog::~WatchFaceAnalog() {
  // Also release visibility when another screen replaces this face. This is
  // idempotent with the DisplayApp visibility callback on an ordinary sleep.
  heartRateController.SetAnalogFaceVisible(false);
  lv_task_del(taskRefresh);
  weather.reset();

  lv_style_reset(&hour_line_style);
  lv_style_reset(&hour_line_style_trace);
  lv_style_reset(&minute_line_style);
  lv_style_reset(&minute_line_style_trace);
  lv_style_reset(&second_line_style);

  lv_obj_clean(lv_scr_act());
}

void WatchFaceAnalog::OnDisplayStateChanged(bool visible) {
  heartRateController.SetAnalogFaceVisible(visible);
}

bool WatchFaceAnalog::OnTouchEvent(TouchEvents event) {
  weather->OnTouchEvent(event);
  // DisplayApp retains the clock's swipe, long-press and double-tap actions.
  return false;
}

void WatchFaceAnalog::UpdateClock() {
  uint8_t hour = dateTimeController.Hours();
  uint8_t minute = dateTimeController.Minutes();
  uint8_t second = dateTimeController.Seconds();

  if (sMinute != minute) {
    auto const angle = minute * 6;
    minute_point[0] = CoordinateRelocate(30, angle);
    minute_point[1] = CoordinateRelocate(MinuteLength, angle);

    minute_point_trace[0] = CoordinateRelocate(5, angle);
    minute_point_trace[1] = CoordinateRelocate(31, angle);

    lv_line_set_points(minute_body, minute_point, 2);
    lv_line_set_points(minute_body_trace, minute_point_trace, 2);
  }

  if (sHour != hour || sMinute != minute) {
    sHour = hour;
    sMinute = minute;
    auto const angle = (hour * 30 + minute / 2);

    hour_point[0] = CoordinateRelocate(30, angle);
    hour_point[1] = CoordinateRelocate(HourLength, angle);

    hour_point_trace[0] = CoordinateRelocate(5, angle);
    hour_point_trace[1] = CoordinateRelocate(31, angle);

    lv_line_set_points(hour_body, hour_point, 2);
    lv_line_set_points(hour_body_trace, hour_point_trace, 2);
  }

  if (sSecond != second) {
    sSecond = second;
    auto const angle = second * 6;

    second_point[0] = CoordinateRelocate(-20, angle);
    second_point[1] = CoordinateRelocate(SecondLength, angle);
    lv_line_set_points(second_body, second_point, 2);
  }
}

void WatchFaceAnalog::SetBatteryIcon() {
  auto batteryPercent = batteryPercentRemaining.Get();
  batteryIcon.SetBatteryPercentage(batteryPercent);
}

void WatchFaceAnalog::Refresh() {
  const auto heartRateSample = heartRateController.GetAnalogFaceSample();
  char heartRateText[4];
  if (heartRateSample.valid) {
    std::snprintf(heartRateText, sizeof(heartRateText), "%u", static_cast<unsigned>(heartRateSample.heartRate));
  } else {
    std::snprintf(heartRateText, sizeof(heartRateText), "--");
  }
  if (std::strcmp(heartRateText, lv_label_get_text(labelHeartRate)) != 0) {
    lv_label_set_text(labelHeartRate, heartRateText);
  }
  // The placeholder is aqua while a brief sample is in progress. It turns gray
  // when no valid reading is available; an accepted result remains white.
  const uint8_t appearance = heartRateSample.valid ? 2 : (heartRateSample.measuring ? 1 : 0);
  if (appearance != heartRateAppearance) {
    heartRateAppearance = appearance;
    lv_obj_set_style_local_text_color(labelHeartRate,
                                    LV_LABEL_PART_MAIN,
                                    LV_STATE_DEFAULT,
                                    heartRateSample.valid ? LV_COLOR_WHITE : (heartRateSample.measuring ? LV_COLOR_AQUA : LV_COLOR_GRAY));
  }

  const bool imperial = settingsController.GetWeatherFormat() == Controllers::Settings::WeatherFormat::Imperial;
  const auto currentWeather = weatherService.Current();
  std::optional<int16_t> temperature;
  if (currentWeather) {
    temperature = imperial ? currentWeather->temperature.Fahrenheit() : currentWeather->temperature.Celsius();
  }
  weather->SetTemperature(temperature, imperial);

  isCharging = batteryController.IsCharging();
  if (isCharging.IsUpdated()) {
    if (isCharging.Get()) {
      lv_obj_set_hidden(batteryIcon.GetObject(), true);
      lv_obj_set_hidden(plugIcon, false);
    } else {
      lv_obj_set_hidden(batteryIcon.GetObject(), false);
      lv_obj_set_hidden(plugIcon, true);
      SetBatteryIcon();
    }
  }
  if (!isCharging.Get()) {
    batteryPercentRemaining = batteryController.PercentRemaining();
    if (batteryPercentRemaining.IsUpdated()) {
      SetBatteryIcon();
    }
  }

  bleState = bleController.IsConnected();
  if (bleState.IsUpdated()) {
    if (bleState.Get()) {
      lv_label_set_text_static(bleIcon, Symbols::bluetooth);
    } else {
      lv_label_set_text_static(bleIcon, "");
    }
  }

  notificationState = notificationManager.AreNewNotificationsAvailable();

  if (notificationState.IsUpdated()) {
    lv_label_set_text_static(notificationIcon, NotificationIcon::GetIcon(notificationState.Get()));
    lv_obj_align(notificationIcon, nullptr, LV_ALIGN_IN_BOTTOM_RIGHT, -6, -6);
  }

  currentDateTime = dateTimeController.CurrentDateTime();
  if (currentDateTime.IsUpdated()) {
    UpdateClock();

    currentDate = std::chrono::time_point_cast<std::chrono::days>(currentDateTime.Get());
    if (currentDate.IsUpdated()) {
      lv_label_set_text_fmt(label_date_day, "%s\n%02i", dateTimeController.DayOfWeekShortToString(), dateTimeController.Day());
    }
  }
}
