#pragma once
#include <cstdint>
#include <cassert>
#include "ble_uuid.h"

#define ASSERT(expression) assert(expression)
constexpr uint16_t BLE_GATT_CHR_F_READ = 2;
constexpr uint16_t BLE_GATT_CHR_F_WRITE = 8;
constexpr uint16_t BLE_GATT_CHR_F_WRITE_ENC = 0x1000;
constexpr uint8_t BLE_GATT_SVC_TYPE_PRIMARY = 1;
constexpr uint8_t BLE_GATT_ACCESS_OP_READ_CHR = 0;
constexpr uint8_t BLE_GATT_ACCESS_OP_WRITE_CHR = 1;
constexpr int BLE_ATT_ERR_WRITE_NOT_PERMITTED = 3;
constexpr int BLE_ATT_ERR_INSUFFICIENT_AUTHOR = 8;
constexpr int BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN = 13;
constexpr int BLE_ATT_ERR_UNLIKELY = 14;

struct os_mbuf {
  const uint8_t* bytes;
  uint16_t length;
  os_mbuf* next;
  uint16_t packetLength;
  int forcedCopyResult;
};
#define OS_MBUF_PKTLEN(buffer) ((buffer)->packetLength)
struct ble_gatt_chr_def;
struct ble_gatt_access_ctxt { uint8_t op; os_mbuf* om; const ble_gatt_chr_def* chr; };
using ble_gatt_access_fn = int (*)(uint16_t, uint16_t, ble_gatt_access_ctxt*, void*);
struct ble_gatt_chr_def {
  const ble_uuid_t* uuid;
  ble_gatt_access_fn access_cb;
  void* arg;
  uint16_t flags;
};
struct ble_gatt_svc_def { uint8_t type; const ble_uuid_t* uuid; const ble_gatt_chr_def* characteristics; };
int ble_gatts_count_cfg(const ble_gatt_svc_def* services);
int ble_gatts_add_svcs(const ble_gatt_svc_def* services);
int ble_gatts_find_svc(const ble_uuid_t* uuid, uint16_t* handle);
int os_mbuf_copydata(const os_mbuf* buffer, int offset, int length, void* output);
