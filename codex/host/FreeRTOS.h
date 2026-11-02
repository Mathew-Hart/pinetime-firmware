#pragma once
// Host-only adapters: the renderer and lv_conf.h are the firmware versions.
#include <stdint.h>
#include <stdlib.h>
#define pvPortMalloc malloc
#define vPortFree free
#ifdef __cplusplus
extern "C" {
#endif
uint32_t xTaskGetTickCount(void);
#ifdef __cplusplus
}
#endif
