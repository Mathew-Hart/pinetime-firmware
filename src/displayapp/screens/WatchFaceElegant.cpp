#include "displayapp/screens/WatchFaceElegant.h"
#include <limits>
#include "components/datetime/DateTimeController.h"

using namespace Pinetime::Applications::Screens;

namespace {
  constexpr int16_t HourLength = 70;
  constexpr int16_t MinuteLength = 90;
  constexpr int16_t TrigScale = std::numeric_limits<int16_t>::max();

  // Half-degree input preserves the hour hand's movement for each minute.
  int32_t Sine(int16_t halfDegrees) {
    const auto degrees = halfDegrees / 2;
    const auto value = _lv_trigo_sin(degrees);
    return halfDegrees % 2 == 0 ? value : (static_cast<int32_t>(value) + _lv_trigo_sin(degrees + 1)) / 2;
  }

  lv_point_t HandTip(int16_t length, int16_t halfDegrees) {
    return lv_point_t {.x = static_cast<lv_coord_t>(LV_HOR_RES / 2 + length * Sine(halfDegrees) / TrigScale),
                       .y = static_cast<lv_coord_t>(LV_VER_RES / 2 - length * Sine(halfDegrees + 180) / TrigScale)};
  }

  void CreateTicks(uint16_t count, uint16_t length, uint16_t width, lv_color_t color) {
    auto* ticks = lv_linemeter_create(lv_scr_act(), nullptr);
    // The first and last lines coincide, producing a complete circular dial.
    lv_linemeter_set_scale(ticks, 360, count + 1);
    lv_linemeter_set_angle_offset(ticks, 180);
    lv_obj_set_size(ticks, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(ticks, nullptr, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_click(ticks, false);
    lv_obj_set_style_local_bg_opa(ticks, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_scale_width(ticks, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, length);
    lv_obj_set_style_local_scale_end_line_width(ticks, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, width);
    lv_obj_set_style_local_scale_end_color(ticks, LV_LINEMETER_PART_MAIN, LV_STATE_DEFAULT, color);
  }

  lv_obj_t* CreateHand() {
    auto* hand = lv_line_create(lv_scr_act(), nullptr);
    lv_obj_set_click(hand, false);
    lv_obj_set_style_local_line_width(hand, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, 7);
    lv_obj_set_style_local_line_color(hand, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_obj_set_style_local_line_rounded(hand, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, true);
    return hand;
  }
}

WatchFaceElegant::WatchFaceElegant(Controllers::DateTime& dateTimeController) : dateTimeController {dateTimeController} {
  // Retain Analog's gray minute ticks, white hour ticks, and aqua cardinals.
  CreateTicks(60, 4, 1, LV_COLOR_GRAY);
  CreateTicks(12, 6, 4, LV_COLOR_WHITE);
  CreateTicks(4, 20, 4, LV_COLOR_AQUA);

  hourHand = CreateHand();
  minuteHand = CreateHand();
  taskRefresh = lv_task_create(RefreshTaskCallback, 1000, LV_TASK_PRIO_LOW, this);
  Refresh();
}

WatchFaceElegant::~WatchFaceElegant() {
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void WatchFaceElegant::Refresh() {
  const auto hour = dateTimeController.Hours();
  const auto minute = dateTimeController.Minutes();
  if (hour == displayedHour && minute == displayedMinute) {
    return;
  }
  displayedHour = hour;
  displayedMinute = minute;

  hourPoints[0] = minutePoints[0] =
    lv_point_t {static_cast<lv_coord_t>(LV_HOR_RES / 2), static_cast<lv_coord_t>(LV_VER_RES / 2)};
  hourPoints[1] = HandTip(HourLength, (hour % 12) * 60 + minute);
  minutePoints[1] = HandTip(MinuteLength, minute * 12);
  lv_line_set_points(hourHand, hourPoints, 2);
  lv_line_set_points(minuteHand, minutePoints, 2);
}
