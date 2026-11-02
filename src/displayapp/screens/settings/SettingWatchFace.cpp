#include "displayapp/screens/settings/SettingWatchFace.h"
#include "displayapp/widgets/PageIndicator.h"

using namespace Pinetime::Applications::Screens;

constexpr const char* SettingWatchFace::title;
constexpr const char* SettingWatchFace::symbol;

namespace {
  class WatchFaceCheckboxPage : public Screen {
  public:
    using Items = std::array<SettingWatchFace::Item, 4>;

    WatchFaceCheckboxPage(uint8_t screenID,
                          uint8_t numScreens,
                          const char* titleText,
                          const char* symbolText,
                          Items items,
                          Pinetime::Controllers::Settings& settings)
      : items {items}, settings {settings}, pageIndicator {screenID, numScreens} {
      lv_obj_set_style_local_bg_color(lv_scr_act(), LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
      if (numScreens > 1) {
        pageIndicator.Create();
      }
      auto* container = lv_cont_create(lv_scr_act(), nullptr);
      lv_obj_set_style_local_bg_opa(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
      lv_obj_set_style_local_pad_all(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 10);
      lv_obj_set_style_local_pad_inner(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 5);
      lv_obj_set_style_local_border_width(container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
      lv_obj_set_pos(container, 10, 60);
      lv_obj_set_size(container, LV_HOR_RES - 20, LV_VER_RES - 50);
      lv_cont_set_layout(container, LV_LAYOUT_COLUMN_LEFT);

      auto* title = lv_label_create(lv_scr_act(), nullptr);
      lv_label_set_text_static(title, titleText);
      lv_label_set_align(title, LV_LABEL_ALIGN_CENTER);
      lv_obj_align(title, lv_scr_act(), LV_ALIGN_IN_TOP_MID, 10, 15);
      auto* icon = lv_label_create(lv_scr_act(), nullptr);
      lv_obj_set_style_local_text_color(icon, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_ORANGE);
      lv_label_set_text_static(icon, symbolText);
      lv_obj_align(icon, title, LV_ALIGN_OUT_LEFT_MID, -10, 0);

      for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].name[0] == '\0') {
          continue;
        }
        auto* checkbox = lv_checkbox_create(container, nullptr);
        checkboxes[i] = checkbox;
        lv_checkbox_set_text_static(checkbox, items[i].name);
        lv_obj_set_style_local_radius(checkbox, LV_CHECKBOX_PART_BULLET, LV_STATE_DEFAULT, 2);
        lv_checkbox_set_checked(checkbox, settings.IsWatchFaceEnabled(items[i].watchface));
        if (!items[i].available) {
          lv_checkbox_set_disabled(checkbox);
        }
        checkbox->user_data = this;
        lv_obj_set_event_cb(checkbox, [](lv_obj_t* object, lv_event_t event) {
          if (event == LV_EVENT_VALUE_CHANGED) {
            static_cast<WatchFaceCheckboxPage*>(object->user_data)->Changed(object);
          }
        });
      }
    }

    ~WatchFaceCheckboxPage() override {
      settings.SaveSettings();
      lv_obj_clean(lv_scr_act());
    }

  private:
    void Changed(lv_obj_t* object) {
      for (size_t i = 0; i < items.size(); ++i) {
        if (checkboxes[i] == object && items[i].available) {
          settings.SetWatchFaceEnabled(items[i].watchface, lv_checkbox_is_checked(object));
          settings.SaveSettings();
          break;
        }
      }
    }

    Items items;
    Pinetime::Controllers::Settings& settings;
    std::array<lv_obj_t*, 4> checkboxes {};
    Pinetime::Applications::Widgets::PageIndicator pageIndicator;
  };
}

auto SettingWatchFace::CreateScreenList() const {
  std::array<std::function<std::unique_ptr<Screen>()>, nScreens> pages;
  for (size_t i = 0; i < pages.size(); ++i) {
    pages[i] = [this, i]() -> std::unique_ptr<Screen> { return CreateScreen(i); };
  }
  return pages;
}

SettingWatchFace::SettingWatchFace(Pinetime::Applications::DisplayApp* app,
                                   std::array<Item, UserWatchFaceTypes::Count>&& watchfaceItems,
                                   Pinetime::Controllers::Settings& settingsController)
  : watchfaceItems {std::move(watchfaceItems)},
    settingsController {settingsController},
    screens {app, 0, CreateScreenList(), Screens::ScreenListModes::UpDown} {
}

SettingWatchFace::~SettingWatchFace() {
  lv_obj_clean(lv_scr_act());
}

bool SettingWatchFace::OnTouchEvent(Pinetime::Applications::TouchEvents event) {
  return screens.OnTouchEvent(event);
}

std::unique_ptr<Screen> SettingWatchFace::CreateScreen(unsigned int screenNum) const {
  WatchFaceCheckboxPage::Items page {};
  for (size_t i = 0; i < page.size(); ++i) {
    const auto index = i + screenNum * settingsPerScreen;
    page[i] = index < watchfaceItems.size() ? watchfaceItems[index] : Item {"", WatchFace::Digital, false};
  }
  return std::make_unique<WatchFaceCheckboxPage>(screenNum, nScreens, title, symbol, page, settingsController);
}
