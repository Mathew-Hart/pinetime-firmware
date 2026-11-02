#pragma once

#include <array>
#include <lvgl/lvgl.h>

namespace Pinetime::Applications::Screens {
  // Shared by the watch and the native LVGL preview, so the preview uses the
  // exact fonts, positions and clipping rules that run on the 240x240 display.
  class WeatherFocusView {
  public:
    enum class Field { Date, Status, Alarm, Zone, Time, Period, WeatherIcon, Temperature, Unit, Condition, Range,
                       Paris, ParisTime, ParisDay, Tokyo, TokyoTime, TokyoDay, Count };

    explicit WeatherFocusView(lv_obj_t* parent);
    ~WeatherFocusView();
    void Set(Field field, const char* text);
    void SetAlarmEnabled(bool enabled);

  private:
    enum class Anchor { Left, Center, Right };
    struct Label {
      lv_obj_t* object;
      int16_t x;
      Anchor anchor;
    };
    void Add(Field field, const lv_font_t* font, int16_t x, int16_t y, Anchor anchor = Anchor::Left, bool muted = false);
    lv_obj_t* root;
    std::array<Label, static_cast<size_t>(Field::Count)> labels {};
    lv_style_t lineStyle;
    lv_style_t mutedLineStyle;
    bool alarmEnabled = true;
    const lv_point_t rulePoints[3] {{0, 5}, {222, 5}, {228, 0}};
    const lv_point_t dividerPoints[2] {{0, 0}, {0, 21}};
  };
}
