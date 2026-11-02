#pragma once

#include <array>
#include "displayapp/apps/Apps.h"
#include "displayapp/Controllers.h"
#include "displayapp/screens/Screen.h"
#include "displayapp/screens/Symbols.h"

namespace Pinetime::Applications {
  namespace Screens {
    class ClockTools : public Screen {
    public:
      explicit ClockTools(DisplayApp* app);
      ~ClockTools() override;
      void OnButtonEvent(lv_obj_t* object, lv_event_t event);

    private:
      DisplayApp* app;
      std::array<lv_obj_t*, 3> buttons;
    };
  }

  template <>
  struct AppTraits<Apps::ClockTools> {
    static constexpr Apps app = Apps::ClockTools;
    static constexpr const char* icon = Screens::Symbols::clock;

    static Screens::Screen* Create(AppControllers& controllers) {
      return new Screens::ClockTools(controllers.displayApp);
    }

    static bool IsAvailable(Controllers::FS& /*filesystem*/) {
      return true;
    }
  };
}
