#include "components/ble/GpsService.h"

#include <task.h>
#include <services/gatt/ble_svc_gatt.h>

namespace {
  // The 0005 family is the existing weather service. GPS uses 0006.
  // 00060000 / 00060001-78fc-48fe-8e23-433b3a1942d0.
  constexpr ble_uuid128_t gpsUuid {
    .u = {.type = BLE_UUID_TYPE_128},
    .value = {0xd0, 0x42, 0x19, 0x3a, 0x3b, 0x43, 0x23, 0x8e, 0xfe, 0x48, 0xfc, 0x78, 0x00, 0x00, 0x06, 0x00}};
  constexpr ble_uuid128_t gpsDataUuid {
    .u = {.type = BLE_UUID_TYPE_128},
    .value = {0xd0, 0x42, 0x19, 0x3a, 0x3b, 0x43, 0x23, 0x8e, 0xfe, 0x48, 0xfc, 0x78, 0x01, 0x00, 0x06, 0x00}};

  int GpsCallback(uint16_t /*connection*/, uint16_t /*attribute*/, ble_gatt_access_ctxt* context, void* argument) {
    return static_cast<Pinetime::Controllers::GpsService*>(argument)->OnCommand(context);
  }
}

using namespace Pinetime::Controllers;

GpsService::GpsService() {
  characteristicDefinition[0] = {.uuid = &gpsDataUuid.u,
                                  .access_cb = GpsCallback,
                                  .arg = this,
                                  .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC};
  serviceDefinition[0] = {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &gpsUuid.u, .characteristics = characteristicDefinition};
}

void GpsService::Init() {
  int result = ble_gatts_count_cfg(serviceDefinition);
  ASSERT(result == 0);
  result = ble_gatts_add_svcs(serviceDefinition);
  ASSERT(result == 0);
}

bool GpsService::AnnounceDatabaseChange() {
  if (databaseChangeAnnounced) {
    return true;
  }
  uint16_t startHandle = 0;
  if (ble_gatts_find_svc(&gpsUuid.u, &startHandle) != 0 || startHandle == 0) {
    return false;
  }
  ble_svc_gatt_changed(startHandle, 0xffff);
  databaseChangeAnnounced = true;
  return true;
}

int GpsService::OnCommand(ble_gatt_access_ctxt* context) {
  if (context == nullptr || context->om == nullptr) {
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }
  if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
    return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
  }
  if (OS_MBUF_PKTLEN(context->om) != GpsData::PacketSize) {
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }
  GpsData::Packet bytes {};
  if (os_mbuf_copydata(context->om, 0, bytes.size(), bytes.data()) != 0) {
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }
  switch (data.Update(bytes.data(), bytes.size(), xTaskGetTickCount())) {
    case GpsData::UpdateResult::Accepted:
      return 0;
    case GpsData::UpdateResult::Disconnected:
      return BLE_ATT_ERR_INSUFFICIENT_AUTHOR;
    case GpsData::UpdateResult::Invalid:
      return BLE_ATT_ERR_UNLIKELY;
  }
  return BLE_ATT_ERR_UNLIKELY;
}

void GpsService::SetConnected(bool connected) {
  data.SetConnected(connected);
}

GpsData::Snapshot GpsService::GetSnapshot() const {
  return data.GetSnapshot(xTaskGetTickCount(), configTICK_RATE_HZ);
}
