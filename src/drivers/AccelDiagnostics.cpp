#include "drivers/AccelDiagnostics.h"
#include "drivers/Bma421.h"
#include "drivers/Bma421_C/bma423.h"

using namespace Pinetime::Drivers;

AccelDiagnostics::AccelDiagnostics(TwiMaster& twiMaster, Bma421& accelerometer) : twiMaster {twiMaster}, accelerometer {accelerometer} {
}

AccelDiagnostics::Register AccelDiagnostics::ReadRegister(uint8_t address, uint8_t reg) const {
  Register value;
  value.result = twiMaster.ReadDiagnostic(address, reg, &value.value, 1);
  return value;
}

AccelDiagnostics::Snapshot AccelDiagnostics::Refresh() const {
  Snapshot snapshot;
  snapshot.driverInitialized = accelerometer.IsOk();
  for (size_t i = 0; i < snapshot.devices.size(); ++i) {
    auto& device = snapshot.devices[i];
    device.address = 0x18 + i;
    device.chipId = ReadRegister(device.address, BMA4_CHIP_ID_ADDR);
    if (!device.chipId.result.IsOk() || (device.chipId.value != BMA423_CHIP_ID && device.chipId.value != BMA425_CHIP_ID)) {
      // Do not interpret registers on an absent or unrecognized device. No bus
      // scan, reset, self-test, configuration, or power-mode write is performed.
      continue;
    }
    device.error = ReadRegister(device.address, BMA4_ERROR_ADDR);
    device.status = ReadRegister(device.address, BMA4_STATUS_ADDR);
    device.internalStatus = ReadRegister(device.address, BMA4_INTERNAL_STAT);
    device.powerConf = ReadRegister(device.address, BMA4_POWER_CONF_ADDR);
    device.powerCtrl = ReadRegister(device.address, BMA4_POWER_CTRL_ADDR);
    device.accelConfig = ReadRegister(device.address, BMA4_ACCEL_CONFIG_ADDR);
    device.accelRange = ReadRegister(device.address, BMA4_ACCEL_RANGE_ADDR);
    device.axes.result = twiMaster.ReadDiagnostic(device.address, BMA4_DATA_8_ADDR, device.axes.bytes.data(), device.axes.bytes.size());
    if (device.axes.result.IsOk()) {
      const auto word = [&](size_t offset) {
        // Avoid implementation-defined unsigned-to-signed conversion in tests.
        const uint16_t value =
          static_cast<uint16_t>(device.axes.bytes[offset]) | (static_cast<uint16_t>(device.axes.bytes[offset + 1]) << 8);
        return static_cast<int16_t>(value < 0x8000 ? static_cast<int32_t>(value) : static_cast<int32_t>(value) - 0x10000);
      };
      device.axes.x = word(0);
      device.axes.y = word(2);
      device.axes.z = word(4);
    }
  }
  return snapshot;
}

const char* AccelDiagnostics::ChipName(uint8_t chipId) {
  switch (chipId) {
    case BMA423_CHIP_ID:
      return "BMA421/423";
    case BMA425_CHIP_ID:
      return "BMA425";
    default:
      return "Unknown ID";
  }
}

const char* AccelDiagnostics::ErrorName(TwiMaster::DiagnosticError error) {
  using Error = TwiMaster::DiagnosticError;
  switch (error) {
    case Error::None:
      return "OK";
    case Error::NotRead:
      return "Not read";
    case Error::AddressNack:
      return "Address NACK";
    case Error::DataNack:
      return "Data NACK";
    case Error::Overrun:
      return "Overrun";
    case Error::StartTimeout:
      return "Start timeout";
    case Error::TransferTimeout:
      return "Data timeout";
    case Error::StopTimeout:
      return "Stop timeout";
    case Error::SuspendTimeout:
      return "Pause timeout";
    case Error::LockTimeout:
      return "Bus busy";
    case Error::InvalidArgument:
      return "Invalid read";
    case Error::UnknownBusError:
      return "Bus error";
  }
  return "Unknown error";
}
