#include "displayapp/screens/AccelDiagnostics.h"
#include "displayapp/InfiniTimeTheme.h"

using namespace Pinetime::Applications;
using namespace Pinetime::Applications::Screens;
using DiagnosticError = Pinetime::Drivers::TwiMaster::DiagnosticError;

namespace {
  void ButtonEventHandler(lv_obj_t* object, lv_event_t event) {
    static_cast<AccelDiagnostics*>(object->user_data)->OnButtonEvent(object, event);
  }

  const char* ReadResult(DiagnosticError error) {
    switch (error) {
      case DiagnosticError::None: return "OK";
      case DiagnosticError::NotRead: return "not read";
      case DiagnosticError::AddressNack: return "addr NACK";
      case DiagnosticError::DataNack: return "data NACK";
      case DiagnosticError::Overrun: return "overrun";
      case DiagnosticError::StartTimeout: return "start TO";
      case DiagnosticError::TransferTimeout: return "transfer TO";
      case DiagnosticError::StopTimeout: return "stop TO";
      case DiagnosticError::SuspendTimeout: return "suspend TO";
      case DiagnosticError::LockTimeout: return "lock TO";
      case DiagnosticError::InvalidArgument: return "bad argument";
      case DiagnosticError::UnknownBusError: return "bus error";
    }
    return "unknown";
  }

  lv_obj_t* Button(AccelDiagnostics* screen, int x, int width, const char* text) {
    auto* button = lv_btn_create(lv_scr_act(), nullptr);
    button->user_data = screen;
    lv_btn_set_layout(button, LV_LAYOUT_OFF);
    lv_obj_set_pos(button, x, 200);
    lv_obj_set_size(button, width, 36);
    lv_obj_set_style_local_radius(button, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 8);
    lv_obj_set_style_local_bg_color(button, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, Colors::bgAlt);
    lv_obj_set_event_cb(button, ButtonEventHandler);
    auto* label = lv_label_create(button, nullptr);
    lv_label_set_text_static(label, text);
    lv_obj_align(label, button, LV_ALIGN_CENTER, 0, 0);
    return button;
  }
}

AccelDiagnostics::AccelDiagnostics(Drivers::AccelDiagnostics* diagnostics) : diagnostics {diagnostics} {
  title = lv_label_create(lv_scr_act(), nullptr);
  lv_obj_set_pos(title, 6, 5);
  lv_obj_set_style_local_text_color(title, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_YELLOW);
  for (size_t index = 0; index < rows.size(); ++index) {
    rows[index] = lv_label_create(lv_scr_act(), nullptr);
    lv_obj_set_pos(rows[index], 6, 37 + index * 22);
    lv_label_set_text_static(rows[index], "");
  }
  previousButton = Button(this, 6, 44, "<");
  refreshButton = Button(this, 56, 128, "Refresh");
  nextButton = Button(this, 190, 44, ">");
  TakeSample();
}

AccelDiagnostics::~AccelDiagnostics() {
  lv_obj_clean(lv_scr_act());
}

void AccelDiagnostics::TakeSample() {
  // No timer: an open diagnostic app leaves the bus idle until Refresh is tapped.
  if (diagnostics != nullptr) {
    snapshot = diagnostics->Refresh();
  }
  Render();
}

void AccelDiagnostics::OnButtonEvent(lv_obj_t* object, lv_event_t event) {
  if (event != LV_EVENT_CLICKED || diagnostics == nullptr) {
    return;
  }
  if (object == refreshButton) {
    TakeSample();
  } else if (object == previousButton && page > 0) {
    --page;
    Render();
  } else if (object == nextButton && page + 1 < pageCount) {
    ++page;
    Render();
  }
}

void AccelDiagnostics::Render() {
  for (auto* row : rows) {
    lv_label_set_text_static(row, "");
  }
  lv_btn_set_state(previousButton, diagnostics == nullptr || page == 0 ? LV_BTN_STATE_DISABLED : LV_BTN_STATE_RELEASED);
  lv_btn_set_state(nextButton, diagnostics == nullptr || page + 1 == pageCount ? LV_BTN_STATE_DISABLED : LV_BTN_STATE_RELEASED);
  lv_btn_set_state(refreshButton, diagnostics == nullptr ? LV_BTN_STATE_DISABLED : LV_BTN_STATE_RELEASED);
  if (diagnostics == nullptr) {
    lv_label_set_text_static(title, "Accel I2C");
    lv_label_set_text_static(rows[0], "Provider missing");
    lv_label_set_text_static(rows[2], "No bus reads made");
    return;
  }
  if (page == 0) {
    RenderBus();
    return;
  }
  const auto& device = snapshot.devices[(page - 1) / 2];
  const bool rawPage = page % 2 == 1;
  lv_label_set_text_fmt(title, "0x%02x %s %u/%u", device.address, rawPage ? "raw" : "regs",
                        static_cast<unsigned>(page + 1), static_cast<unsigned>(pageCount));
  if (rawPage) {
    RenderRaw(device);
  } else {
    RenderRegisters(device);
  }
}

void AccelDiagnostics::RenderBus() {
  lv_label_set_text_static(title, "Accel I2C 1/5");
  lv_label_set_text_fmt(rows[0], "Driver init: %s", snapshot.driverInitialized ? "OK" : "FAIL");
  for (size_t index = 0; index < snapshot.devices.size(); ++index) {
    const auto& device = snapshot.devices[index];
    const size_t row = 1 + index * 2;
    if (device.chipId.result.IsOk()) {
      lv_label_set_text_fmt(rows[row], "0x%02x chip: 0x%02x", device.address, device.chipId.value);
      lv_label_set_text_fmt(rows[row + 1], "%s / bus OK", Drivers::AccelDiagnostics::ChipName(device.chipId.value));
    } else {
      lv_label_set_text_fmt(rows[row], "0x%02x: %s", device.address, ReadResult(device.chipId.result.error));
      lv_label_set_text_fmt(rows[row + 1], "Bus src: 0x%08lx", static_cast<unsigned long>(device.chipId.result.errorSource));
    }
  }
  lv_label_set_text_static(rows[5], "Read-only sample");
  lv_label_set_text_static(rows[6], "No reset/config");
}

void AccelDiagnostics::RenderRaw(const Drivers::AccelDiagnostics::Device& device) {
  if (device.chipId.result.IsOk()) {
    lv_label_set_text_fmt(rows[0], "Chip ID: 0x%02x", device.chipId.value);
  } else {
    lv_label_set_text_fmt(rows[0], "Chip: %s", ReadResult(device.chipId.result.error));
  }
  lv_label_set_text_fmt(rows[1], "Axes: %s", ReadResult(device.axes.result.error));
  if (device.axes.result.IsOk()) {
    lv_label_set_text_fmt(rows[2], "X: %d", device.axes.x);
    lv_label_set_text_fmt(rows[3], "Y: %d", device.axes.y);
    lv_label_set_text_fmt(rows[4], "Z: %d", device.axes.z);
  } else {
    lv_label_set_text_static(rows[2], "X: --");
    lv_label_set_text_static(rows[3], "Y: --");
    lv_label_set_text_static(rows[4], "Z: --");
  }
  RegisterRow(5, "ERR", device.error);
  RegisterRow(6, "STAT", device.status);
}

void AccelDiagnostics::RegisterRow(size_t row, const char* name, const Drivers::AccelDiagnostics::Register& value) {
  if (value.result.IsOk()) {
    lv_label_set_text_fmt(rows[row], "%s: 0x%02x", name, value.value);
  } else {
    lv_label_set_text_fmt(rows[row], "%s: %s", name, ReadResult(value.result.error));
  }
}

void AccelDiagnostics::RenderRegisters(const Drivers::AccelDiagnostics::Device& device) {
  RegisterRow(0, "ERR", device.error);
  RegisterRow(1, "STAT", device.status);
  RegisterRow(2, "INIT", device.internalStatus);
  RegisterRow(3, "PWRC", device.powerConf);
  RegisterRow(4, "PWRE", device.powerCtrl);
  RegisterRow(5, "ACFG", device.accelConfig);
  RegisterRow(6, "RANGE", device.accelRange);
}
