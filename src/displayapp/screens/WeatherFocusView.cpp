#include "displayapp/screens/WeatherFocusView.h"
#include <cstring>

LV_FONT_DECLARE(weather_segments_90);
LV_FONT_DECLARE(weather_segments_36);
LV_FONT_DECLARE(weather_segments_24);
LV_FONT_DECLARE(weather_dots_17);
LV_FONT_DECLARE(weather_small_12);

using namespace Pinetime::Applications::Screens;

WeatherFocusView::WeatherFocusView(lv_obj_t* parent) {
  root = lv_obj_create(parent, nullptr);
  lv_obj_reset_style_list(root, LV_OBJ_PART_MAIN);
  lv_obj_set_size(root, 240, 240);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_style_local_bg_color(root, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
  lv_obj_set_style_local_bg_opa(root, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
  lv_obj_set_click(root, false);

  Add(Field::Date, &weather_dots_17, 8, 9);
  Add(Field::Status, &weather_small_12, 233, 9, Anchor::Right);
  Add(Field::Alarm, &weather_small_12, 8, 34);
  Add(Field::Zone, &weather_small_12, 231, 35, Anchor::Right, true);
  Add(Field::Time, &weather_segments_90, 120, 62, Anchor::Center);
  Add(Field::Period, &weather_small_12, 232, 130, Anchor::Right, true);
  Add(Field::WeatherIcon, &fontawesome_weathericons, 9, 169);
  Add(Field::Temperature, &weather_segments_36, 44, 163);
  Add(Field::Unit, &weather_small_12, 96, 170);
  Add(Field::Condition, &weather_small_12, 231, 165, Anchor::Right);
  Add(Field::Range, &weather_small_12, 231, 187, Anchor::Right, true);
  Add(Field::Paris, &weather_small_12, 9, 220);
  Add(Field::ParisTime, &weather_segments_24, 43, 215);
  Add(Field::ParisDay, &weather_small_12, 96, 220, Anchor::Left, true);
  Add(Field::Tokyo, &weather_small_12, 135, 220);
  Add(Field::TokyoTime, &weather_segments_24, 163, 215);
  Add(Field::TokyoDay, &weather_small_12, 216, 220, Anchor::Left, true);

  lv_style_init(&lineStyle);
  lv_style_set_line_width(&lineStyle, LV_STATE_DEFAULT, 1);
  lv_style_set_line_color(&lineStyle, LV_STATE_DEFAULT, lv_color_hex(0x98B69A));
  lv_style_init(&mutedLineStyle);
  lv_style_set_line_width(&mutedLineStyle, LV_STATE_DEFAULT, 1);
  lv_style_set_line_color(&mutedLineStyle, LV_STATE_DEFAULT, lv_color_hex(0x78917A));
  for (auto y : {46, 143, 196}) {
    auto* line = lv_line_create(root, nullptr);
    lv_line_set_points(line, rulePoints, 3);
    lv_obj_add_style(line, LV_LINE_PART_MAIN, &lineStyle);
    lv_obj_set_pos(line, 6, y);
  }
  auto* divider = lv_line_create(root, nullptr);
  lv_line_set_points(divider, dividerPoints, 2);
  lv_obj_add_style(divider, LV_LINE_PART_MAIN, &mutedLineStyle);
  lv_obj_set_pos(divider, 125, 215);
  Set(Field::Paris, "PAR");
  Set(Field::Tokyo, "TYO");
}

WeatherFocusView::~WeatherFocusView() {
  lv_obj_del(root);
  lv_style_reset(&lineStyle);
  lv_style_reset(&mutedLineStyle);
}

void WeatherFocusView::Add(Field field, const lv_font_t* font, int16_t x, int16_t y, Anchor anchor, bool muted) {
  auto* label = lv_label_create(root, nullptr);
  lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, font);
  lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(muted ? 0x78917A : 0x98B69A));
  lv_obj_set_style_local_text_letter_space(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_click(label, false);
  lv_label_set_text_static(label, "");
  lv_obj_set_pos(label, x, y);
  labels[static_cast<size_t>(field)] = {label, x, anchor};
}

void WeatherFocusView::Set(Field field, const char* text) {
  auto& label = labels[static_cast<size_t>(field)];
  if (std::strcmp(lv_label_get_text(label.object), text) == 0) {
    return;
  }
  lv_label_set_text(label.object, text);
  auto x = label.x;
  if (label.anchor == Anchor::Right) {
    x -= lv_obj_get_width(label.object);
  } else if (label.anchor == Anchor::Center) {
    x -= lv_obj_get_width(label.object) / 2;
  }
  if (field == Field::Unit) {
    x = 44 + lv_obj_get_width(labels[static_cast<size_t>(Field::Temperature)].object) + 3;
  }
  lv_obj_set_x(label.object, x);
  if (field == Field::Temperature) {
    lv_obj_set_x(labels[static_cast<size_t>(Field::Unit)].object, 44 + lv_obj_get_width(label.object) + 3);
  }
}

void WeatherFocusView::SetAlarmEnabled(bool enabled) {
  if (alarmEnabled == enabled) return;
  alarmEnabled = enabled;
  lv_obj_set_style_local_text_color(labels[static_cast<size_t>(Field::Alarm)].object,
                                    LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, lv_color_hex(enabled ? 0x98B69A : 0x78917A));
}
