#pragma once

#include <array>
#include <lvgl/lvgl.h>
#include "displayapp/Controllers.h"
#include "displayapp/apps/Apps.h"
#include "displayapp/screens/Screen.h"

namespace Pinetime::Applications {
  namespace Screens {
    class WatchFaceGps : public Screen {
    public:
      WatchFaceGps(Controllers::DateTime& dateTimeController,
                   Controllers::Settings& settingsController,
                   const Controllers::Ble& bleController,
                   Controllers::GpsService* gpsService);
      ~WatchFaceGps() override;
      void Refresh() override;

    private:
      Controllers::DateTime& dateTimeController;
      Controllers::Settings& settingsController;
      const Controllers::Ble& bleController;
      Controllers::GpsService* gpsService;
      lv_obj_t* timeLabel;
      lv_obj_t* periodLabel;
      lv_obj_t* fixLabel;
      lv_obj_t* sourceLabel;
      lv_obj_t* ageLabel;
      lv_obj_t* separator;
      std::array<lv_obj_t*, 5> rowLabels {};
      std::array<lv_obj_t*, 5> values {};
      lv_task_t* refreshTask;
    };
  }

  template <>
  struct WatchFaceTraits<WatchFace::Gps> {
    static constexpr WatchFace watchFace = WatchFace::Gps;
    static constexpr const char* name = "GPS";
    static Screens::Screen* Create(AppControllers& controllers) {
      return new Screens::WatchFaceGps(controllers.dateTimeController, controllers.settingsController,
                                      controllers.bleController, controllers.gpsService);
    }
    static bool IsAvailable(Controllers::FS& /*filesystem*/) {
      return true;
    }
  };
}
