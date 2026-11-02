#pragma once

#include "displayapp/screens/Screen.h"
#include "displayapp/screens/WeatherFocusView.h"
#include "displayapp/Controllers.h"
#include "displayapp/apps/Apps.h"

namespace Pinetime::Applications::Screens {
  class WatchFaceWeatherFocus : public Screen {
  public:
    explicit WatchFaceWeatherFocus(AppControllers& controllers);
    ~WatchFaceWeatherFocus() override;
    void Refresh() override;

  private:
    AppControllers& controllers;
    WeatherFocusView view;
    lv_task_t* refreshTask;
  };
}

namespace Pinetime::Applications {
  template <>
  struct WatchFaceTraits<WatchFace::WeatherFocus> {
    static constexpr WatchFace watchFace = WatchFace::WeatherFocus;
    static constexpr const char* name = "Weather focus";
    static Screens::Screen* Create(AppControllers& controllers) {
      return new Screens::WatchFaceWeatherFocus(controllers);
    }
    static bool IsAvailable(Controllers::FS&) {
      return true;
    }
  };
}
