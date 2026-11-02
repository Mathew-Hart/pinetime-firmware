#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <type_traits>
#include <vector>
#include <lvgl/lvgl.h>
#include "components/settings/Settings.h"
#include "displayapp/DisplayApp.h"
#include "displayapp/InfiniTimeTheme.h"
#include "displayapp/WatchFaceNavigation.h"
#include "displayapp/screens/settings/SettingWatchFace.h"

using Pinetime::Applications::DisplayApp;
using Pinetime::Applications::NextAvailableWatchFace;
using Pinetime::Applications::TouchEvents;
using Pinetime::Applications::WatchFace;
using Pinetime::Applications::Screens::SettingWatchFace;
using Pinetime::Controllers::FS;
using Pinetime::Controllers::Settings;
using Pinetime::Controllers::WatchFaceSelection;

static_assert(static_cast<uint8_t>(WatchFace::Digital) == 0);
static_assert(static_cast<uint8_t>(WatchFace::Analog) == 1);
static_assert(static_cast<uint8_t>(WatchFace::PineTimeStyle) == 2);
static_assert(static_cast<uint8_t>(WatchFace::Terminal) == 3);
static_assert(static_cast<uint8_t>(WatchFace::Infineat) == 4);
static_assert(static_cast<uint8_t>(WatchFace::CasioStyleG7710) == 5);
static_assert(static_cast<uint8_t>(WatchFace::PrideFlag) == 6);
static_assert(static_cast<uint8_t>(WatchFace::WeatherFocus) == 7);
static_assert(static_cast<uint8_t>(WatchFace::Elegant) == 8);
static_assert(static_cast<uint8_t>(WatchFace::Binary) == 9);
static_assert(static_cast<uint8_t>(WatchFace::Earth) == 10);
static_assert(static_cast<uint8_t>(WatchFace::Solar) == 11);
static_assert(static_cast<uint8_t>(WatchFace::Gps) == 12);
static_assert(Pinetime::Applications::UserWatchFaceTypes::Count == 9);
static_assert(std::is_same_v<Pinetime::Applications::UserWatchFaceTypes,
  Pinetime::Applications::WatchFaceTypeList<WatchFace::Digital, WatchFace::Analog, WatchFace::Terminal,
    WatchFace::WeatherFocus, WatchFace::Elegant, WatchFace::Binary, WatchFace::Earth, WatchFace::Solar, WatchFace::Gps>>);
template <WatchFace... Faces>
constexpr auto configuredFaces(Pinetime::Applications::WatchFaceTypeList<Faces...>) { return std::array {Faces...}; }
constexpr auto faceIds = configuredFaces(Pinetime::Applications::UserWatchFaceTypes {});
constexpr std::array removedIds {WatchFace::PineTimeStyle, WatchFace::Infineat, WatchFace::CasioStyleG7710, WatchFace::PrideFlag};
constexpr uint32_t removedMask = (1u << 2) | (1u << 5);


static std::array<lv_color32_t, 240 * 240> framebuffer;
static uint32_t ticks = 2000;
static lv_point_t pointer {-1, -1};
static lv_indev_state_t pointerState = LV_INDEV_STATE_REL;
static unsigned assertions = 0;
extern "C" uint32_t xTaskGetTickCount(void) { return ticks; }
static void check(bool condition) { ++assertions; assert(condition); }
static void flush(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* pixels) {
  for (int y = area->y1; y <= area->y2; ++y) for (int x = area->x1; x <= area->x2; ++x) {
    auto color = *pixels++;
    if (x >= 0 && x < 240 && y >= 0 && y < 240) framebuffer[y * 240 + x].full = lv_color_to32(color);
  }
  lv_disp_flush_ready(driver);
}
static bool readPointer(lv_indev_drv_t*, lv_indev_data_t* value) { value->point = pointer; value->state = pointerState; return false; }
static void settle() { for (unsigned i = 0; i < 3; ++i) { ticks += 25; lv_task_handler(); } lv_refr_now(nullptr); }
static std::vector<lv_obj_t*> boxes(lv_obj_t* parent = lv_scr_act()) {
  std::vector<lv_obj_t*> result;
  for (auto* child = lv_obj_get_child(parent, nullptr); child; child = lv_obj_get_child(parent, child)) {
    lv_obj_type_t type {}; lv_obj_get_type(child, &type);
    if (type.type[0] && std::strcmp(type.type[0], "lv_checkbox") == 0) result.push_back(child);
    auto nested = boxes(child); result.insert(result.end(), nested.begin(), nested.end());
  }
  return result;
}
static lv_obj_t* named(const char* name) {
  for (auto* checkbox : boxes()) if (std::strcmp(lv_checkbox_get_text(checkbox), name) == 0) return checkbox;
  assert(false); return nullptr;
}
static void clickAt(int x, int y) {
  pointer = {static_cast<lv_coord_t>(x), static_cast<lv_coord_t>(y)};
  pointerState = LV_INDEV_STATE_PR; settle(); pointerState = LV_INDEV_STATE_REL; settle();
}
static void click(const char* name) {
  auto* checkbox = named(name); lv_area_t area {}; lv_obj_get_coords(checkbox, &area);
  check(area.x1 >= 0 && area.x2 < 240 && area.y1 >= 0 && area.y2 < 240);
  clickAt(area.x1 + 8, (area.y1 + area.y2) / 2);
}
static auto items() {
  return std::array<SettingWatchFace::Item, 9> {{{"Digital", WatchFace::Digital, true}, {"Analog", WatchFace::Analog, true},
    {"Terminal", WatchFace::Terminal, true}, {"Weather focus", WatchFace::WeatherFocus, true}, {"Elegant", WatchFace::Elegant, true}, {"Binary", WatchFace::Binary, true}, {"Earth", WatchFace::Earth, true}, {"Solar", WatchFace::Solar, true}, {"GPS", WatchFace::Gps, true}}};
}
static void enabled(const Settings& settings, WatchFace face, const char* name, bool expected) {
  check(settings.IsWatchFaceEnabled(face) == expected); check(lv_checkbox_is_checked(named(name)) == expected);
}
static void render(const std::filesystem::path& folder, const char* name) {
  settle(); auto file = folder / (std::string(name) + ".ppm"); auto* output = std::fopen(file.c_str(), "wb"); assert(output);
  std::fprintf(output, "P6\n240 240\n255\n");
  for (auto pixel : framebuffer) { std::fputc(pixel.ch.red, output); std::fputc(pixel.ch.green, output); std::fputc(pixel.ch.blue, output); }
  std::fclose(output);
  for (auto* checkbox : boxes()) {
    lv_area_t bounds {}; lv_obj_get_coords(checkbox, &bounds);
    std::printf("%s: %s [%d,%d]-[%d,%d] checked=%d disabled=%d\n", name, lv_checkbox_get_text(checkbox), bounds.x1, bounds.y1,
      bounds.x2, bounds.y2, lv_checkbox_is_checked(checkbox), lv_checkbox_is_inactive(checkbox));
    check(bounds.x1 >= 0 && bounds.x2 < 240 && bounds.y1 >= 0 && bounds.y2 < 240);
  }
}
static FS::Bytes record(uint32_t disabled) {
  const auto bytes = WatchFaceSelection {disabled}.Encode(); return {bytes.begin(), bytes.end()};
}
static void removedMembershipPreserved(const Settings& settings) {
  check(!settings.IsWatchFaceEnabled(WatchFace::PineTimeStyle));
  check(settings.IsWatchFaceEnabled(WatchFace::Infineat));
  check(!settings.IsWatchFaceEnabled(WatchFace::CasioStyleG7710));
  check(settings.IsWatchFaceEnabled(WatchFace::PrideFlag));
}
static void cycleChecks(Settings& settings) {
  struct Face { WatchFace watchFace; bool available; };
  std::array<Face, faceIds.size()> faces;
  for (size_t i = 0; i < faces.size(); ++i) faces[i] = {faceIds[i], true};
  const auto included = [&](const Face& face) { return face.available && settings.IsWatchFaceEnabled(face.watchFace); };
  // Only the actual compiled default list participates, even if removed IDs
  // remain enabled in an old settings sidecar.
  for (auto id : removedIds) settings.SetWatchFaceEnabled(id, true);
  for (auto face : faces) settings.SetWatchFaceEnabled(face.watchFace, true);
  for (size_t i = 0; i < faces.size(); ++i) {
    check(NextAvailableWatchFace(faces, faces[i].watchFace, included) == faces[(i + 1) % faces.size()].watchFace);
  }
  for (auto id : removedIds) check(NextAvailableWatchFace(faces, id, included) == WatchFace::Digital);
  settings.SetWatchFaceEnabled(WatchFace::Analog, false);
  check(NextAvailableWatchFace(faces, WatchFace::Digital, included) == WatchFace::Terminal);
  settings.SetWatchFaceEnabled(WatchFace::Gps, false);
  check(NextAvailableWatchFace(faces, WatchFace::Solar, included) == WatchFace::Digital);
  settings.SetWatchFaceEnabled(WatchFace::Gps, true);
  faces[8].available = false;
  check(NextAvailableWatchFace(faces, WatchFace::Solar, included) == WatchFace::Digital);
  faces[8].available = true;
  for (auto face : faces) settings.SetWatchFaceEnabled(face.watchFace, false);
  settings.SetWatchFaceEnabled(WatchFace::Gps, true);
  check(NextAvailableWatchFace(faces, WatchFace::Elegant, included) == WatchFace::Gps);
  check(!NextAvailableWatchFace(faces, WatchFace::Gps, included));
  for (auto id : removedIds) check(NextAvailableWatchFace(faces, id, included) == WatchFace::Gps);
  settings.SetWatchFaceEnabled(WatchFace::Gps, false);
  check(!NextAvailableWatchFace(faces, WatchFace::WeatherFocus, included));
  // Enabled removed faces cannot make an otherwise empty nine-face cycle nonempty.
  for (auto id : removedIds) { check(settings.IsWatchFaceEnabled(id)); check(!NextAvailableWatchFace(faces, id, included)); }
  const std::array<Face, 0> empty {};
  check(!NextAvailableWatchFace(empty, WatchFace::Gps, included));
}
static void page(SettingWatchFace& picker, size_t expected) {
  check(picker.OnTouchEvent(TouchEvents::SwipeUp)); settle(); check(boxes().size() == expected);
}
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::filesystem::path output = argv[1]; std::filesystem::create_directories(output);
  lv_init(); lv_theme_set_act(lv_pinetime_theme_init());
  static lv_color_t pixels[240 * 40]; static lv_disp_buf_t buffer; lv_disp_buf_init(&buffer, pixels, nullptr, 240 * 40);
  lv_disp_drv_t driver; lv_disp_drv_init(&driver); driver.hor_res = driver.ver_res = 240; driver.buffer = &buffer;
  driver.flush_cb = flush; lv_disp_drv_register(&driver);
  lv_indev_drv_t input; lv_indev_drv_init(&input); input.type = LV_INDEV_TYPE_POINTER; input.read_cb = readPointer; lv_indev_drv_register(&input);
  constexpr uint32_t oldEightMask = removedMask | (1u << 0) | (1u << 3) | (1u << 8) | (1u << 9) | (1u << 10) | (1u << 11);
  FS oldFs; oldFs.files["/watchface-cycle.dat"] = record(oldEightMask);
  Settings upgraded {oldFs}; upgraded.Init();
  for (auto id : faceIds) check(upgraded.IsWatchFaceEnabled(id) == ((oldEightMask & (1u << static_cast<uint8_t>(id))) == 0));
  check(upgraded.IsWatchFaceEnabled(WatchFace::Gps)); removedMembershipPreserved(upgraded);
  upgraded.SetWatchFaceEnabled(WatchFace::Gps, false); upgraded.SaveSettings();
  check(oldFs.files.at("/watchface-cycle.dat") == record(oldEightMask | (1u << 12)));
  Settings upgradedReboot {oldFs}; upgradedReboot.Init(); check(!upgradedReboot.IsWatchFaceEnabled(WatchFace::Gps));
  for (auto id : faceIds) if (id != WatchFace::Gps) check(upgradedReboot.IsWatchFaceEnabled(id) == upgraded.IsWatchFaceEnabled(id));
  removedMembershipPreserved(upgradedReboot);
  FS fs; fs.files["/watchface-cycle.dat"] = record(removedMask);
  Settings settings {fs}; settings.Init(); DisplayApp app;
  settings.SetWatchFace(WatchFace::WeatherFocus); settings.SaveSettings();
  const auto ordinarySettings = fs.files.at("/settings.dat");
  {
    SettingWatchFace picker(&app, items(), settings); settle(); check(boxes().size() == 4);
    click("Analog"); click("Weather focus");
    enabled(settings, WatchFace::Digital, "Digital", true); enabled(settings, WatchFace::Analog, "Analog", false);
    enabled(settings, WatchFace::Terminal, "Terminal", true); enabled(settings, WatchFace::WeatherFocus, "Weather focus", false);
    render(output, "page1-four-retained"); page(picker, 4);
    click("Elegant"); click("Binary"); click("Earth"); click("Solar");
    for (auto id : {WatchFace::Elegant, WatchFace::Binary, WatchFace::Earth, WatchFace::Solar}) check(!settings.IsWatchFaceEnabled(id));
    check(settings.IsWatchFaceEnabled(WatchFace::Gps)); render(output, "page2-four-retained"); page(picker, 1);
    enabled(settings, WatchFace::Gps, "GPS", true); render(output, "page3-gps-default");
    const auto before = fs.files.at("/watchface-cycle.dat"); const auto writes = fs.Count(FS::Op::Write, "/watchface-cycle.tmp");
    clickAt(30, 114); clickAt(30, 154); clickAt(30, 194);
    check(boxes().size() == 1); check(fs.files.at("/watchface-cycle.dat") == before);
    check(fs.Count(FS::Op::Write, "/watchface-cycle.tmp") == writes);
    click("GPS"); enabled(settings, WatchFace::Gps, "GPS", false);
    check(fs.files.at("/watchface-cycle.dat") == record(removedMask | (1u << 1) | (1u << 7) | (1u << 8) | (1u << 9) | (1u << 10) | (1u << 11) | (1u << 12)));
    check(settings.GetWatchFace() == WatchFace::WeatherFocus); removedMembershipPreserved(settings);
    render(output, "page3-gps-disabled"); page(picker, 1);
    check(picker.OnTouchEvent(TouchEvents::SwipeDown)); settle(); check(boxes().size() == 4);
    for (auto* checkbox : boxes()) check(!lv_checkbox_is_checked(checkbox));
    check(picker.OnTouchEvent(TouchEvents::SwipeDown)); settle(); check(boxes().size() == 4);
    enabled(settings, WatchFace::Digital, "Digital", true); enabled(settings, WatchFace::Analog, "Analog", false);
    enabled(settings, WatchFace::Terminal, "Terminal", true); enabled(settings, WatchFace::WeatherFocus, "Weather focus", false);
    check(!picker.OnTouchEvent(TouchEvents::SwipeDown));
  }
  check(fs.files.at("/settings.dat") == ordinarySettings);
  Settings rebooted {fs}; rebooted.Init(); check(rebooted.GetWatchFace() == WatchFace::WeatherFocus);
  removedMembershipPreserved(rebooted);
  for (auto id : faceIds) check(rebooted.IsWatchFaceEnabled(id) == (id == WatchFace::Digital || id == WatchFace::Terminal));
  {
    SettingWatchFace reopened(&app, items(), rebooted); page(reopened, 4); page(reopened, 1);
    enabled(rebooted, WatchFace::Gps, "GPS", false); click("GPS"); enabled(rebooted, WatchFace::Gps, "GPS", true);
  }
  Settings reenabled {fs}; reenabled.Init(); check(reenabled.IsWatchFaceEnabled(WatchFace::Gps));
  for (auto id : {WatchFace::Elegant, WatchFace::Binary, WatchFace::Earth, WatchFace::Solar}) check(!reenabled.IsWatchFaceEnabled(id));
  removedMembershipPreserved(reenabled);
  for (auto id : faceIds) reenabled.SetWatchFaceEnabled(id, false);
  reenabled.SaveSettings(); Settings allOff {fs}; allOff.Init();
  for (auto id : faceIds) check(!allOff.IsWatchFaceEnabled(id)); removedMembershipPreserved(allOff);
  cycleChecks(reenabled); check(reenabled.GetWatchFace() == WatchFace::WeatherFocus); check(fs.files.at("/settings.dat") == ordinarySettings);
  for (auto id : {WatchFace::Elegant, WatchFace::Binary, WatchFace::Earth, WatchFace::Solar, WatchFace::Gps}) {
    FS currentFs; Settings current {currentFs}; current.Init(); current.SetWatchFace(id); current.SaveSettings();
    Settings currentReboot {currentFs}; currentReboot.Init(); check(currentReboot.GetWatchFace() == id);
  }
  std::printf("Face selection assertions passed: %u; actual Settings.cpp and LVGL; GPS stable ID12.\n", assertions);
  auto* receipt = std::fopen((output / "test-result.json").c_str(), "w"); assert(receipt);
  std::fprintf(receipt, "{\"result\":\"PASS\",\"assertions\":%u,\"actual_lvgl_pointer_clicks\":true,\"actual_settings_cpp\":true,\"production_cycle_helper\":true,\"pages\":[4,4,1],\"configured_ids\":[0,1,3,7,8,9,10,11,12],\"removed_ids\":[2,4,5,6],\"phone_actions\":[]}\n", assertions);
  std::fclose(receipt);
}
