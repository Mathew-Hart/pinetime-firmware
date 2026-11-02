#pragma once

#include <array>
#include <cstdint>
#include <lvgl/lvgl.h>
#include "displayapp/screens/Screen.h"
#include "displayapp/Controllers.h"
#include "displayapp/apps/Apps.h"

namespace Pinetime::Applications {
  namespace Screens {
    class WatchFaceBinary : public Screen {
    public:
      explicit WatchFaceBinary(Controllers::DateTime& dateTimeController);
      ~WatchFaceBinary() override;

      void Refresh() override;

    private:
      void UpdateColumn(size_t column, uint8_t value);

      Controllers::DateTime& dateTimeController;
      std::array<std::array<lv_obj_t*, 6>, 3> dots;
      std::array<uint8_t, 3> displayedValues {0xff, 0xff, 0xff};
      lv_task_t* taskRefresh;
    };
  }

  template <>
  struct WatchFaceTraits<WatchFace::Binary> {
    static constexpr WatchFace watchFace = WatchFace::Binary;
    static constexpr const char* name = "Binary";

    static Screens::Screen* Create(AppControllers& controllers) {
      return new Screens::WatchFaceBinary(controllers.dateTimeController);
    }

    static bool IsAvailable(Controllers::FS& /*filesystem*/) {
      return true;
    }
  };
}
