#pragma once
#include <cstdint>
constexpr uint8_t BLE_UUID_TYPE_128 = 128;
struct ble_uuid_t { uint8_t type; };
struct ble_uuid128_t { ble_uuid_t u; uint8_t value[16]; };
