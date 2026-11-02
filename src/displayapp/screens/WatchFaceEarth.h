#pragma once

#include <cstdint>
#include <lvgl/lvgl.h>
#include "displayapp/Controllers.h"
#include "displayapp/apps/Apps.h"
#include "displayapp/screens/EarthGlobe.h"
#include "displayapp/screens/Screen.h"

namespace Pinetime::Applications {
  namespace Screens {
    class WatchFaceEarth : public Screen {
    public:
      WatchFaceEarth(Controllers::DateTime& dateTimeController,
                     Controllers::Settings& settingsController,
                     Controllers::SimpleWeatherService* weatherService);
      ~WatchFaceEarth() override;

      void Refresh() override;

    private:
      Controllers::DateTime& dateTimeController;
      Controllers::Settings& settingsController;
      Controllers::SimpleWeatherService* weatherService;
      EarthGlobe globe;
      lv_obj_t* dateLabel;
      lv_obj_t* timeLabel;
      lv_obj_t* periodLabel;
      lv_obj_t* temperatureLabel;
      lv_obj_t* conditionLabel;
      lv_task_t* refreshTask;
      int64_t sunBucket = 0;
      bool sunKnown = false;
      bool sunInitialized = false;
    };
  }

  template <>
  struct WatchFaceTraits<WatchFace::Earth> {
    static constexpr WatchFace watchFace = WatchFace::Earth;
    static constexpr const char* name = "Earth";

    static Screens::Screen* Create(AppControllers& controllers) {
      return new Screens::WatchFaceEarth(controllers.dateTimeController, controllers.settingsController, controllers.weatherController);
    }

    static bool IsAvailable(Controllers::FS& /*filesystem*/) {
      return true;
    }
  };
}
