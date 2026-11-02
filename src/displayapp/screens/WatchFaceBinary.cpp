#include "displayapp/screens/WatchFaceBinary.h"
#include "components/datetime/DateTimeController.h"

using namespace Pinetime::Applications::Screens;

namespace {
  constexpr std::array<int16_t, 3> ColumnCenters {78, 140, 202};
  constexpr std::array<const char*, 3> ColumnNames {"H", "M", "S"};
  constexpr std::array<const char*, 6> RowNames {"32", "16", "8", "4", "2", "1"};
  constexpr std::array<lv_color_t, 3> ColumnColors {LV_COLOR_AQUA, LV_COLOR_WHITE, LV_COLOR_MAKE(0x8f, 0xff, 0x9d)};
  constexpr lv_color_t OffFill = LV_COLOR_MAKE(0x15, 0x1a, 0x1f);
  constexpr lv_color_t OffOutline = LV_COLOR_MAKE(0x48, 0x50, 0x58);
  constexpr int16_t FirstRowCenter = 55;
  constexpr int16_t RowSpacing = 31;
  constexpr int16_t DotSize = 22;

  lv_obj_t* CreateLabel(const char* text, int16_t x, int16_t y, int16_t width, lv_label_align_t alignment, lv_color_t color) {
    auto* label = lv_label_create(lv_scr_act(), nullptr);
    lv_label_set_text_static(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CROP);
    lv_obj_set_width(label, width);
    lv_label_set_align(label, alignment);
    lv_obj_set_style_local_text_letter_space(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, color);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_click(label, false);
    return label;
  }
}

WatchFaceBinary::WatchFaceBinary(Controllers::DateTime& dateTimeController) : dateTimeController {dateTimeController} {
  CreateLabel("24H", 2, 8, 36, LV_LABEL_ALIGN_LEFT, LV_COLOR_GRAY);
  for (size_t column = 0; column < dots.size(); ++column) {
    CreateLabel(ColumnNames[column], ColumnCenters[column] - 20, 8, 40, LV_LABEL_ALIGN_CENTER, ColumnColors[column]);
    for (size_t row = 0; row < dots[column].size(); ++row) {
      auto* dot = lv_obj_create(lv_scr_act(), nullptr);
      dots[column][row] = dot;
      lv_obj_reset_style_list(dot, LV_OBJ_PART_MAIN);
      lv_obj_set_size(dot, DotSize, DotSize);
      lv_obj_set_pos(dot, ColumnCenters[column] - DotSize / 2, FirstRowCenter + row * RowSpacing - DotSize / 2);
      lv_obj_set_click(dot, false);
      lv_obj_set_style_local_radius(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
      lv_obj_set_style_local_bg_opa(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
      lv_obj_set_style_local_bg_color(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, OffFill);
      lv_obj_set_style_local_border_width(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 1);
      lv_obj_set_style_local_border_color(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, OffOutline);
    }
  }
  for (size_t row = 0; row < RowNames.size(); ++row) {
    CreateLabel(RowNames[row], 2, FirstRowCenter + row * RowSpacing - 12, 32, LV_LABEL_ALIGN_RIGHT, LV_COLOR_GRAY);
  }

  taskRefresh = lv_task_create(RefreshTaskCallback, 1000, LV_TASK_PRIO_LOW, this);
  Refresh();
}

WatchFaceBinary::~WatchFaceBinary() {
  lv_task_del(taskRefresh);
  lv_obj_clean(lv_scr_act());
}

void WatchFaceBinary::UpdateColumn(size_t column, uint8_t value) {
  const auto previous = displayedValues[column];
  if (previous == value) {
    return;
  }
  const auto changedBits = previous == 0xff ? 0x3f : previous ^ value;
  displayedValues[column] = value;
  for (size_t row = 0; row < dots[column].size(); ++row) {
    const auto bit = 1u << (5 - row);
    if ((changedBits & bit) == 0) {
      continue;
    }
    const bool lit = (value & bit) != 0;
    auto* dot = dots[column][row];
    lv_obj_set_style_local_bg_color(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lit ? ColumnColors[column] : OffFill);
    lv_obj_set_style_local_border_color(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lit ? ColumnColors[column] : OffOutline);
  }
}

void WatchFaceBinary::Refresh() {
  // Hours/Minutes/Seconds read the cached local time; advance it from the RTC.
  dateTimeController.CurrentDateTime();
  UpdateColumn(0, dateTimeController.Hours());
  UpdateColumn(1, dateTimeController.Minutes());
  UpdateColumn(2, dateTimeController.Seconds());
}
