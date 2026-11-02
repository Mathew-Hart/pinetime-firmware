#include "displayapp/screens/AnalogWeather.h"

#include <cstdio>
#include <cstring>

using namespace Pinetime::Applications::Screens;

AnalogWeather::AnalogWeather(lv_obj_t* parent, OpenWeather openWeather, void* context)
  : openWeather {openWeather}, context {context} {
  root = lv_obj_create(parent, nullptr);
  lv_obj_reset_style_list(root, LV_OBJ_PART_MAIN);
  lv_obj_set_size(root, 96, 64);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_click(root, true);
  lv_obj_set_event_cb(root, OnEvent);
  root->user_data = this;

  label = lv_label_create(root, nullptr);
  lv_obj_set_style_local_text_font(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &jetbrains_mono_bold_20);
  lv_obj_set_style_local_text_color(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_AQUA);
  lv_obj_set_style_local_text_letter_space(label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 0);
  lv_obj_set_click(label, false);
  lv_obj_set_pos(label, 6, 6);
  SetTemperature({}, false);
}

AnalogWeather::~AnalogWeather() {
  lv_obj_del(root);
}

void AnalogWeather::SetTemperature(std::optional<int16_t> temperature, bool imperial) {
  char text[16];
  if (temperature) {
    std::snprintf(text, sizeof(text), "%d°%c", *temperature, imperial ? 'F' : 'C');
  } else {
    std::snprintf(text, sizeof(text), "--°%c", imperial ? 'F' : 'C');
  }
  if (std::strcmp(text, lv_label_get_text(label)) != 0) {
    lv_label_set_text(label, text);
  }
}

void AnalogWeather::OnTouchEvent(TouchEvents event) {
  if (event != TouchEvents::None) {
    suppressTap = event != TouchEvents::Tap;
  }
}

void AnalogWeather::OnEvent(lv_obj_t* object, lv_event_t event) {
  auto& view = *static_cast<AnalogWeather*>(object->user_data);
  if (event == LV_EVENT_SHORT_CLICKED && !view.suppressTap && view.openWeather != nullptr) {
    view.openWeather(view.context);
  } else if (event == LV_EVENT_RELEASED || event == LV_EVENT_PRESS_LOST) {
    // LVGL emits SHORT_CLICKED before RELEASED. A gesture suppresses the entire
    // contact, including the no-op swipe when only one clock face is enabled.
    view.suppressTap = false;
  }
}
