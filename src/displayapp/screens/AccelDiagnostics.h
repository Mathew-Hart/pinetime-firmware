#pragma once

#include <array>
#include <cstddef>
#include "displayapp/apps/Apps.h"
#include "displayapp/Controllers.h"
#include "displayapp/screens/Screen.h"
#include "displayapp/screens/Symbols.h"
#include "drivers/AccelDiagnostics.h"

namespace Pinetime::Applications {
  namespace Screens {
    class AccelDiagnostics : public Screen {
    public:
      explicit AccelDiagnostics(Drivers::AccelDiagnostics* diagnostics);
      ~AccelDiagnostics() override;
      void OnButtonEvent(lv_obj_t* object, lv_event_t event);

    private:
      static constexpr size_t pageCount = 5;
      Drivers::AccelDiagnostics* diagnostics;
      Drivers::AccelDiagnostics::Snapshot snapshot {};
      size_t page = 0;
      lv_obj_t* title;
      std::array<lv_obj_t*, 7> rows;
      lv_obj_t* previousButton;
      lv_obj_t* refreshButton;
      lv_obj_t* nextButton;
      void TakeSample();
      void Render();
      void RenderBus();
      void RenderRaw(const Drivers::AccelDiagnostics::Device& device);
      void RenderRegisters(const Drivers::AccelDiagnostics::Device& device);
      void RegisterRow(size_t row, const char* name, const Drivers::AccelDiagnostics::Register& value);
    };
  }

  template <>
  struct AppTraits<Apps::AccelDiagnostics> {
    static constexpr Apps app = Apps::AccelDiagnostics;
    static constexpr const char* icon = Screens::Symbols::tachometer;

    static Screens::Screen* Create(AppControllers& controllers) {
      return new Screens::AccelDiagnostics(controllers.accelDiagnostics);
    }

    static bool IsAvailable(Controllers::FS& /*filesystem*/) {
      return true;
    }
  };
}
