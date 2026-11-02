#pragma once

#include <array>
#include <cstdint>
#include "drivers/TwiMaster.h"

namespace Pinetime::Drivers {
  class Bma421;

  // Read-only Bosch register diagnostics. Each value carries its own transaction
  // result; a failed or skipped read is never presented as a valid zero.
  class AccelDiagnostics {
  public:
    struct Register {
      uint8_t value = 0;
      TwiMaster::DiagnosticResult result;
    };

    struct RawAxes {
      std::array<uint8_t, 6> bytes {};
      // Signed raw register words, before resolution/range scaling or axis swap.
      int16_t x = 0;
      int16_t y = 0;
      int16_t z = 0;
      TwiMaster::DiagnosticResult result;
    };

    struct Device {
      uint8_t address = 0;
      Register chipId;
      Register error;
      Register status;
      Register internalStatus;
      Register powerConf;
      Register powerCtrl;
      Register accelConfig;
      Register accelRange;
      RawAxes axes;
    };

    struct Snapshot {
      bool driverInitialized = false;
      std::array<Device, 2> devices {};
    };

    AccelDiagnostics(TwiMaster& twiMaster, Bma421& accelerometer);
    Snapshot Refresh() const;
    static const char* ErrorName(TwiMaster::DiagnosticError error);
    static const char* ChipName(uint8_t chipId);

  private:
    Register ReadRegister(uint8_t address, uint8_t reg) const;
    TwiMaster& twiMaster;
    Bma421& accelerometer;
  };
}
