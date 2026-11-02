#include "displayapp/screens/ClockTools.h"
#include "displayapp/DisplayApp.h"
#include "displayapp/InfiniTimeTheme.h"

using namespace Pinetime::Applications;
using namespace Pinetime::Applications::Screens;

namespace {
  struct Choice {
    const char* name;
    const char* icon;
    Apps app;
  };

  constexpr std::array<Choice, 3> choices {{{"Alarm", Symbols::bell, Apps::Alarm},
                                           {"Timer", Symbols::hourGlass, Apps::Timer},
                                           {"Stopwatch", Symbols::stopWatch, Apps::StopWatch}}};

  void ButtonEventHandler(lv_obj_t* object, lv_event_t event) {
    static_cast<ClockTools*>(object->user_data)->OnButtonEvent(object, event);
  }
}

ClockTools::ClockTools(DisplayApp* app) : app {app} {
  auto* title = lv_label_create(lv_scr_act(), nullptr);
  lv_label_set_text_static(title, "Clock tools");
  lv_obj_align(title, nullptr, LV_ALIGN_IN_TOP_MID, 0, 6);

  for (size_t i = 0; i < choices.size(); ++i) {
    auto* button = lv_btn_create(lv_scr_act(), nullptr);
    buttons[i] = button;
    button->user_data = this;
    lv_btn_set_layout(button, LV_LAYOUT_OFF);
    lv_obj_set_size(button, 224, 60);
    lv_obj_set_pos(button, 8, 42 + i * 66);
    lv_obj_set_style_local_radius(button, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 12);
    lv_obj_set_style_local_bg_color(button, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, Colors::bgAlt);
    lv_obj_set_event_cb(button, ButtonEventHandler);

    auto* icon = lv_label_create(button, nullptr);
    lv_label_set_text_static(icon, choices[i].icon);
    lv_obj_set_style_local_text_color(icon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_YELLOW);
    lv_obj_align(icon, button, LV_ALIGN_IN_LEFT_MID, 14, 0);

    auto* text = lv_label_create(button, nullptr);
    lv_label_set_text_static(text, choices[i].name);
    lv_obj_align(text, button, LV_ALIGN_IN_LEFT_MID, 54, 0);
  }
}

ClockTools::~ClockTools() {
  lv_obj_clean(lv_scr_act());
}

void ClockTools::OnButtonEvent(lv_obj_t* object, lv_event_t event) {
  if (event != LV_EVENT_CLICKED) {
    return;
  }
  for (size_t i = 0; i < buttons.size(); ++i) {
    if (buttons[i] == object) {
      // StartApp queues the transition until after LVGL finishes this callback.
      app->StartApp(choices[i].app, DisplayApp::FullRefreshDirections::Up);
      return;
    }
  }
}
