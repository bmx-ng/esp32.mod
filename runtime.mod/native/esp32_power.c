#include <stdint.h>

#include "esp_err.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum {
    BMX_POWER_CAPABILITY_IDLE = 0x01u,
    BMX_POWER_CAPABILITY_SLEEP_INTERRUPT = 0x02u,
    BMX_POWER_CAPABILITY_SLEEP_TIMER = 0x04u,
    BMX_POWER_UNAVAILABLE = -17,
    BMX_POWER_INVALID_ARGUMENT = -5,
    BMX_POWER_ERROR = -1
};

int64_t bmx_esp32_calendar_wake_delay_us(void);

static int32_t bmx_esp32_power_light_sleep(int64_t requested_delay_us) {
    int64_t calendar_delay_us = bmx_esp32_calendar_wake_delay_us();
    int64_t wake_delay_us = requested_delay_us;
    if (calendar_delay_us > 0 &&
        (wake_delay_us < 0 || calendar_delay_us < wake_delay_us)) {
        wake_delay_us = calendar_delay_us;
    }
    if (wake_delay_us > 0 &&
        esp_sleep_enable_timer_wakeup((uint64_t)wake_delay_us) != ESP_OK) {
        return BMX_POWER_ERROR;
    }
    esp_err_t result = esp_light_sleep_start();
    if (wake_delay_us > 0)
        (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    return result == ESP_OK ? 0 : BMX_POWER_ERROR;
}

uint32_t bmx_embedded_power_capabilities(void) {
    return BMX_POWER_CAPABILITY_IDLE | BMX_POWER_CAPABILITY_SLEEP_INTERRUPT |
        BMX_POWER_CAPABILITY_SLEEP_TIMER;
}

void bmx_embedded_power_idle(void) {
#if defined(__XTENSA__)
    __asm__ volatile("waiti 0");
#elif defined(__riscv)
    __asm__ volatile("wfi");
#else
    taskYIELD();
#endif
}

int32_t bmx_embedded_power_sleep_until_interrupt(void) {
    return bmx_esp32_power_light_sleep(-1);
}

int32_t bmx_embedded_power_sleep_for_ms(uint32_t milliseconds, int32_t exclusive) {
    (void)exclusive;
    if (!milliseconds) return BMX_POWER_INVALID_ARGUMENT;
    return bmx_esp32_power_light_sleep((int64_t)milliseconds * 1000);
}

int32_t bmx_embedded_power_dormant_for_ms(uint32_t milliseconds) {
    (void)milliseconds;
    return BMX_POWER_UNAVAILABLE;
}

int32_t bmx_embedded_power_dormant_until_gpio(uint32_t pin, int32_t edge, int32_t high) {
    (void)pin;
    (void)edge;
    (void)high;
    return BMX_POWER_UNAVAILABLE;
}

int32_t bmx_embedded_power_set_unused_pins_low_leakage(uint64_t exclude_mask) {
    (void)exclude_mask;
    return BMX_POWER_UNAVAILABLE;
}

int32_t bmx_esp32_power_deep_sleep_for_ms(uint64_t milliseconds) {
    if (!milliseconds || milliseconds > UINT64_MAX / 1000u)
        return BMX_POWER_INVALID_ARGUMENT;
    if (esp_sleep_enable_timer_wakeup(milliseconds * 1000u) != ESP_OK)
        return BMX_POWER_ERROR;
    esp_deep_sleep_start();
    return BMX_POWER_ERROR;
}

uint32_t bmx_esp32_power_wake_causes(void) {
    return esp_sleep_get_wakeup_causes();
}
