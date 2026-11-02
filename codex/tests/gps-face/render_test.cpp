#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#include <lvgl/lvgl.h>
#include "components/ble/GpsService.h"
#include "components/ble/BleController.h"
#include "components/datetime/DateTimeController.h"
#include "components/settings/Settings.h"
#include "displayapp/InfiniTimeTheme.h"
#include "displayapp/screens/WatchFaceGps.h"

using Pinetime::Applications::TouchEvents;
using Pinetime::Applications::Screens::WatchFaceGps;
using Pinetime::Controllers::DateTime;
using Pinetime::Controllers::Settings;
using Pinetime::Controllers::GpsService;
using Pinetime::Controllers::Ble;
namespace Gps = Pinetime::Controllers::GpsData;
static_assert(static_cast<unsigned>(Pinetime::Applications::WatchFace::Gps) == 12);
static std::array<lv_color32_t, 240 * 240> framebuffer;
static uint32_t ticks = 2000;
static unsigned assertions = 0;
static size_t liveBytes = 0, peakBytes = 0, liveAllocations = 0;
static std::vector<lv_area_t> flushAreas;
struct alignas(std::max_align_t) Allocation { size_t bytes; };
extern "C" void* pvPortMalloc(size_t bytes) {
  auto* allocation = static_cast<Allocation*>(std::malloc(sizeof(Allocation) + bytes));
  assert(allocation); allocation->bytes = bytes; liveBytes += bytes; ++liveAllocations;
  peakBytes = std::max(peakBytes, liveBytes); return allocation + 1;
}
extern "C" void vPortFree(void* pointer) {
  if (!pointer) return;
  auto* allocation = static_cast<Allocation*>(pointer) - 1;
  assert(liveBytes >= allocation->bytes && liveAllocations > 0);
  liveBytes -= allocation->bytes; --liveAllocations; std::free(allocation);
}
extern "C" uint32_t xTaskGetTickCount(void) { return ticks; }
static void check(bool condition) { ++assertions; assert(condition); }
static void flush(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* pixels) {
  flushAreas.push_back(*area);
  for (int y = area->y1; y <= area->y2; ++y) for (int x = area->x1; x <= area->x2; ++x) {
    const auto pixel = *pixels++;
    if (x >= 0 && x < 240 && y >= 0 && y < 240) framebuffer[y * 240 + x].full = lv_color_to32(pixel);
  }
  lv_disp_flush_ready(driver);
}
static void settle() {
  for (unsigned i = 0; i < 3; ++i) { ticks += 25; lv_task_handler(); }
  lv_refr_now(nullptr);
}
static unsigned tasks() {
  unsigned count = 0;
  for (auto* task = lv_task_get_next(nullptr); task; task = lv_task_get_next(task)) ++count;
  return count;
}
static std::vector<lv_obj_t*> objects(const char* kind, lv_obj_t* parent = lv_scr_act()) {
  std::vector<lv_obj_t*> result;
  for (auto* child = lv_obj_get_child(parent, nullptr); child; child = lv_obj_get_child(parent, child)) {
    lv_obj_type_t type {}; lv_obj_get_type(child, &type);
    if (type.type[0] && std::strcmp(type.type[0], kind) == 0) result.push_back(child);
    auto nested = objects(kind, child); result.insert(result.end(), nested.begin(), nested.end());
  }
  return result;
}
static bool hasLabel(const char* text) {
  for (auto* label : objects("lv_label")) if (std::strcmp(lv_label_get_text(label), text) == 0) return true;
  return false;
}
static void text(const char* value) { check(hasLabel(value)); }
static lv_obj_t* at(int x, int y) {
  for (auto* label : objects("lv_label")) if (lv_obj_get_y(label) == y && (x < 0 || lv_obj_get_x(label) == x)) return label;
  assert(false); return nullptr;
}
static void row(size_t index, const char* value) {
  const int y = 76 + static_cast<int>(index) * 28;
  for (auto* label : objects("lv_label")) if (lv_obj_get_y(label) == y && lv_obj_get_x(label) != 8) {
    check(std::strcmp(lv_label_get_text(label), value) == 0); return;
  }
  assert(false);
}
static void noMeasurements() { for (size_t i = 0; i < 5; ++i) row(i, "--"); }
static void layout() {
  check(lv_obj_count_children(lv_scr_act()) == 16); check(objects("lv_label").size() == 15);
  std::vector<lv_area_t> bounds;
  for (auto* label : objects("lv_label")) {
    check(!lv_obj_get_click(label));
    if (std::strlen(lv_label_get_text(label)) == 0) { check(lv_obj_get_hidden(label)); continue; }
    lv_area_t area {}; lv_obj_get_coords(label, &area);
    check(area.x1 >= 0 && area.y1 >= 0 && area.x2 < 240 && area.y2 < 240);
    for (const auto& previous : bounds) check(area.y2 < previous.y1 || previous.y2 < area.y1 || area.x2 < previous.x1 || previous.x2 < area.x1);
    bounds.push_back(area);
  }
}
static void render(const std::filesystem::path& folder, const char* name) {
  settle(); layout(); auto* file = std::fopen((folder / (std::string(name) + ".ppm")).c_str(), "wb"); assert(file);
  std::fprintf(file, "P6\n240 240\n255\n");
  for (const auto pixel : framebuffer) { std::fputc(pixel.ch.red, file); std::fputc(pixel.ch.green, file); std::fputc(pixel.ch.blue, file); }
  std::fclose(file);
}
static Gps::Packet packet(Gps::Fix fix = Gps::Fix::ThreeD, uint8_t flags = 31,
                          int32_t latitude = 373880000, int32_t longitude = -1220838000,
                          int16_t altitude = 24, uint8_t used = 7, uint8_t visible = 12,
                          uint16_t hdop = 123, uint16_t age = 2) {
  Gps::Packet value {}; value[0] = 1; value[1] = static_cast<uint8_t>(fix); value[2] = flags;
  value[3] = used; value[4] = visible;
  const auto put16 = [&](size_t index, uint16_t data) { value[index] = data; value[index + 1] = data >> 8; };
  const auto put32 = [&](size_t index, uint32_t data) { for (unsigned i = 0; i < 4; ++i) value[index + i] = data >> (i * 8); };
  put16(6, age); put32(8, static_cast<uint32_t>(latitude)); put32(12, static_cast<uint32_t>(longitude));
  put16(16, static_cast<uint16_t>(altitude)); put16(18, hdop); return value;
}
static void send(GpsService& gps, const Gps::Packet& value) { check(gps.Update(value) == Gps::UpdateResult::Accepted); }
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::filesystem::path output = argv[1]; std::filesystem::create_directories(output);
  lv_init(); lv_theme_set_act(lv_pinetime_theme_init());
  static lv_color_t pixels[240 * 40]; static lv_disp_buf_t buffer; lv_disp_buf_init(&buffer, pixels, nullptr, 240 * 40);
  lv_disp_drv_t driver; lv_disp_drv_init(&driver); driver.hor_res = driver.ver_res = 240;
  driver.buffer = &buffer; driver.flush_cb = flush; lv_disp_drv_register(&driver);
  settle(); const auto initialTasks = tasks(); const auto initialBytes = liveBytes;
  DateTime dateTime; Settings settings; Ble ble; GpsService gps; gps.SetConnected(true);
  dateTime.SetLocal(2026, 11, 1, 13, 15, 0);
  {
    WatchFaceGps face(dateTime, settings, ble, &gps); settle();
    check(dateTime.updates > 0); check(tasks() == initialTasks + 1);
    text("13:15"); text("PHONE GPS"); text("NO DATA"); noMeasurements(); render(output, "gps-no-data");
    send(gps, packet()); face.Refresh(); settle(); text("3D"); text("2s");
    row(0, "37.388000 N"); row(1, "122.083800 W"); row(2, "24 m"); row(3, "7/12"); row(4, "1.23");
    render(output, "gps-3d-sample");
    flushAreas.clear(); face.Refresh(); settle(); check(flushAreas.empty());
    settings.weatherFormat = Settings::WeatherFormat::Imperial; face.Refresh(); settle(); row(2, "79 ft");
    settings.clockType = Settings::ClockType::H12; face.Refresh(); settle(); text("1:15"); text("PM");
    render(output, "gps-3d-feet-12h");
    settings.weatherFormat = Settings::WeatherFormat::Metric; settings.clockType = Settings::ClockType::H24;
    send(gps, packet(Gps::Fix::TwoD, 29, -338688004, 1512093006, 0)); face.Refresh(); settle(); text("2D");
    row(0, "33.868800 S"); row(1, "151.209301 E"); row(2, "--"); render(output, "gps-2d-sample");
    send(gps, packet(Gps::Fix::Unknown, 29, 373880000, -1220838000, 0)); face.Refresh(); settle(); text(".");
    row(0, "37.388000 N"); row(1, "122.083800 W"); row(2, "--"); render(output, "gps-unknown-dimension");
    send(gps, packet(Gps::Fix::None, 28, 0, 0, 0, 0, 12, 500)); face.Refresh(); settle(); text("NONE");
    row(0, "--"); row(1, "--"); row(2, "--"); row(3, "0/12"); row(4, "5.00"); render(output, "gps-no-fix");
    send(gps, packet(Gps::Fix::ThreeD, 31, 0, 0, 0, 0, 0, 0)); face.Refresh(); settle();
    row(0, "0.000000 N"); row(1, "0.000000 E"); row(2, "0 m"); row(3, "0/0"); row(4, "0.00");
    send(gps, packet(Gps::Fix::ThreeD, 31, -900000000, -1800000000, -32768, 254, 254, 65534));
    face.Refresh(); settle(); row(0, "90.000000 S"); row(1, "180.000000 W"); row(2, "-32768 m");
    row(3, "254/254"); row(4, "655.34"); render(output, "gps-boundary-values");
    settings.weatherFormat = Settings::WeatherFormat::Imperial; face.Refresh(); settle(); row(2, "-107507 ft"); layout();
    settings.weatherFormat = Settings::WeatherFormat::Metric;
    send(gps, packet(Gps::Fix::ThreeD, 1, 373880000, -1220838000, 0, 255, 255, 65535));
    face.Refresh(); settle(); row(2, "--"); row(3, "--/--"); row(4, "--");
    send(gps, packet(Gps::Fix::ThreeD, 31, 373880000, -1220838000, 24, 7, 12, 123, 29)); face.Refresh(); settle();
    text("29s"); ticks += 1000; face.Refresh(); settle(); text("STALE 30s"); noMeasurements(); render(output, "gps-stale");
    send(gps, packet(Gps::Fix::ThreeD, 31, 373880000, -1220838000, 24, 7, 12, 123, Gps::UnknownSourceAge));
    face.Refresh(); settle(); text("NO DATA"); noMeasurements();
    send(gps, packet()); face.Refresh(); settle(); row(0, "37.388000 N");
    ble.connected = false; face.Refresh(); settle(); text("NO PHONE"); noMeasurements(); render(output, "gps-disconnected");
    gps.SetConnected(false); ble.connected = true; gps.SetConnected(true); face.Refresh(); settle(); text("NO DATA"); noMeasurements();
    gps.incoherent = true; face.Refresh(); settle(); text("WAIT"); noMeasurements(); gps.incoherent = false;
    for (auto event : {TouchEvents::Tap, TouchEvents::DoubleTap, TouchEvents::LongTap, TouchEvents::SwipeLeft,
      TouchEvents::SwipeRight, TouchEvents::SwipeUp, TouchEvents::SwipeDown}) check(!face.OnTouchEvent(event));
    check(!face.OnButtonPushed());
  }
  settle(); check(lv_obj_get_child(lv_scr_act(), nullptr) == nullptr); check(tasks() == initialTasks);
  const auto warmBytes = liveBytes, warmAllocations = liveAllocations;
  for (unsigned i = 0; i < 12; ++i) {
    { WatchFaceGps face(dateTime, settings, ble, i % 2 ? &gps : nullptr); settle(); noMeasurements(); face.Refresh(); settle(); }
    settle(); check(lv_obj_get_child(lv_scr_act(), nullptr) == nullptr); check(tasks() == initialTasks);
    check(liveBytes == warmBytes && liveAllocations == warmAllocations);
  }
  std::printf("GPS screen assertions passed: %u; LVGL initial=%zu warm=%zu peak=%zu bytes; no retained screen allocations.\n",
    assertions, initialBytes, warmBytes, peakBytes);
  auto* receipt = std::fopen((output / "test-result.json").c_str(), "w"); assert(receipt);
  std::fprintf(receipt,
    "{\"result\":\"PASS\",\"assertions\":%u,\"actual_watch_face_and_lvgl\":true,\"actual_gps_decoder_store\":true,"
    "\"refresh_unchanged_no_draw\":true,\"repeated_lifetimes\":12,\"lvgl_initial_bytes\":%zu,\"lvgl_warm_bytes\":%zu,"
    "\"lvgl_peak_bytes\":%zu,\"allocator_boundary\":\"counted host malloc; not a simulated FreeRTOS heap\",\"phone_actions\":[]}\n",
    assertions, initialBytes, warmBytes, peakBytes);
  std::fclose(receipt);
}
