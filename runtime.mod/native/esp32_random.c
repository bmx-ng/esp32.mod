#include <stddef.h>
#include <stdint.h>

#include "blitzmax/embedded_random.h"
#include "bootloader_random.h"
#include "esp_random.h"

static int32_t bmx_esp32_internal_entropy_enabled;

extern int32_t bmx_esp32_adc_any_initialized(void);

uint32_t bmx_embedded_random_uint32(void) {
    return esp_random();
}

uint64_t bmx_embedded_random_uint64(void) {
    uint64_t high = esp_random();
    return (high << 32u) | esp_random();
}

int32_t bmx_embedded_random_fill(void *buffer, int32_t length) {
    if (length < 0 || (length && !buffer)) return 0;
    if (length) esp_fill_random(buffer, (size_t)length);
    return 1;
}

void bmx_esp32_random_enable_internal_entropy(void) {
    if (bmx_esp32_internal_entropy_enabled || bmx_esp32_adc_any_initialized()) return;
    bootloader_random_enable();
    bmx_esp32_internal_entropy_enabled = 1;
}

void bmx_esp32_random_disable_internal_entropy(void) {
    if (!bmx_esp32_internal_entropy_enabled) return;
    bootloader_random_disable();
    bmx_esp32_internal_entropy_enabled = 0;
}

int32_t bmx_esp32_random_internal_entropy_enabled(void) {
    return bmx_esp32_internal_entropy_enabled;
}
