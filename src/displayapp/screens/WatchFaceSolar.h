#pragma once

#include <array>
#include <cstdint>
#include <lvgl/lvgl.h>
#include "displayapp/Controllers.h"
#include "displayapp/apps/Apps.h"
#include "displayapp/screens/Screen.h"
#include "displayapp/screens/SolarGlow.h"

namespace Pinetime::Applications {
  namespace Screens {
    class WatchFaceSolar : public Screen {
    public:
      WatchFaceSolar(Controllers::DateTime& dateTimeController,
                     Controllers::Settings& settingsController,
                     Controllers::SimpleWeatherService* weatherService);
      ~WatchFaceSolar() override;

      void Refresh() override;

    private:
      void SetCurve(int16_t sunrise, int16_t sunset);

      Controllers::DateTime& dateTimeController;
      Controllers::Settings& settingsController;
      Controllers::SimpleWeatherService* weatherService;
      SolarGlow glow;
      lv_obj_t* horizon;
      lv_obj_t* curve;
      lv_obj_t* sun;
      lv_obj_t* dateLabel;
      lv_obj_t* timeLabel;
      lv_obj_t* periodLabel;
      lv_task_t* refreshTask;
      std::array<lv_point_t, 53> curvePoints {};
      std::array<lv_point_t, 2> horizonPoints {{{8, 151}, {232, 151}}};
      int16_t lastSunrise = -1;
      int16_t lastSunset = -1;
      int16_t lastMinute = -1;
    };
  }

  template <>
  struct WatchFaceTraits<WatchFace::Solar> {
    static constexpr WatchFace watchFace = WatchFace::Solar;
    static constexpr const char* name = "Solar";

    static Screens::Screen* Create(AppControllers& controllers) {
      return new Screens::WatchFaceSolar(controllers.dateTimeController, controllers.settingsController, controllers.weatherController);
    }

    static bool IsAvailable(Controllers::FS& /*filesystem*/) {
      return true;
    }
  };
}
