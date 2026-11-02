#pragma once

#include "components/ble/GpsData.h"

#define min // workaround: NimBLE's min/max macros conflict with libstdc++
#define max
#include <host/ble_gap.h>
#include <host/ble_uuid.h>
#undef max
#undef min

namespace Pinetime::Controllers {
  class GpsService {
  public:
    GpsService();
    void Init();
    // Call after ble_gatts_start/RestoreBond, before advertising. Old services
    // keep their handles; only the newly appended GPS range needs discovery.
    bool AnnounceDatabaseChange();
    int OnCommand(struct ble_gatt_access_ctxt* ctxt);
    void SetConnected(bool connected);
    GpsData::Snapshot GetSnapshot() const;

  private:
    ble_gatt_chr_def characteristicDefinition[2] {};
    ble_gatt_svc_def serviceDefinition[2] {};
    GpsData::Store data;
    bool databaseChangeAnnounced = false;
  };
}
