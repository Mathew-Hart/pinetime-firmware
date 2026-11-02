#pragma once
#include <FreeRTOS.h>
#include "components/ble/GpsData.h"
namespace Pinetime::Controllers {
  // Only the NimBLE GATT boundary is replaced. The decoder, atomic store,
  // source age and connection reset behavior are actual production code.
  class GpsService {
  public:
    GpsData::Snapshot GetSnapshot() const {
      if (incoherent) { GpsData::Snapshot value; value.connected = true; value.coherent = false; return value; }
      return data.GetSnapshot(xTaskGetTickCount(), 1000);
    }
    void SetConnected(bool value) { data.SetConnected(value); }
    GpsData::UpdateResult Update(const GpsData::Packet& packet) { return data.Update(packet.data(), packet.size(), xTaskGetTickCount()); }
    GpsData::Store data;
    bool incoherent = false;
  };
}
