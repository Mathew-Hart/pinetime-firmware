#pragma once

#include <cstdint>
#include <lvgl/lvgl.h>
#include "displayapp/screens/Screen.h"
#include "displayapp/Controllers.h"
#include "displayapp/apps/Apps.h"

namespace Pinetime::Applications {
  namespace Screens {
    class WatchFaceElegant : public Screen {
    public:
      explicit WatchFaceElegant(Controllers::DateTime& dateTimeController);
      ~WatchFaceElegant() override;

      void Refresh() override;

    private:
      Controllers::DateTime& dateTimeController;
      uint8_t displayedHour = 0xff;
      uint8_t displayedMinute = 0xff;
      lv_obj_t* hourHand;
      lv_obj_t* minuteHand;
      lv_point_t hourPoints[2];
      lv_point_t minutePoints[2];
      lv_task_t* taskRefresh;
    };
  }

  template <>
  struct WatchFaceTraits<WatchFace::Elegant> {
    static constexpr WatchFace watchFace = WatchFace::Elegant;
    static constexpr const char* name = "Elegant";

    static Screens::Screen* Create(AppControllers& controllers) {
      return new Screens::WatchFaceElegant(controllers.dateTimeController);
    }

    static bool IsAvailable(Controllers::FS& /*filesystem*/) {
      return true;
    }
  };
}
