#pragma once
// Host-only allocation accounting. No production memory settings change.
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void* pvPortMalloc(size_t size);
void vPortFree(void* pointer);
uint32_t xTaskGetTickCount(void);
#ifdef __cplusplus
}
#endif
