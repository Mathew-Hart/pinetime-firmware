#include "displayapp/screens/settings/Settings.h"
#include <lvgl/lvgl.h>
#include <functional>
#include <algorithm>
#include "displayapp/apps/Apps.h"
#include "displayapp/DisplayApp.h"

using namespace Pinetime::Applications::Screens;

constexpr std::array<List::Applications, Settings::entries.size()> Settings::entries;

auto Settings::CreateScreenList() const {
  std::array<std::function<std::unique_ptr<Screen>()>, nScreens> screens;
  for (size_t i = 0; i < screens.size(); i++) {
    screens[i] = [this, i]() -> std::unique_ptr<Screen> {
      return CreateScreen(i);
    };
  }
  return screens;
}

Settings::Settings(Pinetime::Applications::DisplayApp* app, Pinetime::Controllers::Settings& settingsController)
  : app {app},
    settingsController {settingsController},
    screens {app,
             std::min<uint8_t>(settingsController.GetSettingsMenu(), nScreens - 1),
             CreateScreenList(),
             Screens::ScreenListModes::UpDown} {
}

Settings::~Settings() {
  lv_obj_clean(lv_scr_act());
}

bool Settings::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  return screens.OnTouchEvent(event);
}

std::unique_ptr<Screen> Settings::CreateScreen(unsigned int screenNum) const {
  std::array<List::Applications, entriesPerScreen> screens;
  for (size_t i = 0; i < screens.size(); i++) {
    const auto entry = screenNum * entriesPerScreen + i;
    screens[i] = entry < entries.size() ? entries[entry] : List::Applications {nullptr, nullptr, Apps::None};
  }

  return std::make_unique<Screens::List>(screenNum, nScreens, app, settingsController, screens);
}
