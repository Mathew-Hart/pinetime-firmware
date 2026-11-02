#pragma once

#include <cstdint>
#include <optional>
#include <lvgl/lvgl.h>
#include "displayapp/TouchEvents.h"

namespace Pinetime::Applications::Screens {
  // A transparent, generously sized tap target above the clock hands. Shared
  // with the native renderer so its fonts, layout and input are tested directly.
  class AnalogWeather {
  public:
    using OpenWeather = void (*)(void*);

    AnalogWeather(lv_obj_t* parent, OpenWeather openWeather, void* context);
    ~AnalogWeather();
    void SetTemperature(std::optional<int16_t> temperature, bool imperial);
    void OnTouchEvent(TouchEvents event);

  private:
    static void OnEvent(lv_obj_t* object, lv_event_t event);

    lv_obj_t* root;
    lv_obj_t* label;
    OpenWeather openWeather;
    void* context;
    bool suppressTap = false;
  };
}
