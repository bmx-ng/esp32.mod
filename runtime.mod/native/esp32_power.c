#include <stdint.h>

#include "esp_err.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum {
    BMX_POWER_CAPABILITY_IDLE = 0x01u,
    BMX_POWER_CAPABILITY_SLEEP_INTERRUPT = 0x02u,
    BMX_POWER_CAPABILITY_SLEEP_TIMER = 0x04u,
    BMX_POWER_CAPABILITY_SLEEP_GPIO_RETENTION = 0x40u,
    BMX_POWER_UNAVAILABLE = -17,
    BMX_POWER_INVALID_ARGUMENT = -5,
    BMX_POWER_PRECONDITION_NOT_MET = -14,
    BMX_POWER_ERROR = -1
};

int64_t bmx_esp32_calendar_wake_delay_us(void);
int32_t bmx_embedded_gpio_get_direction(uint32_t gpio);
int32_t bmx_esp32_gpio_restore_held(uint32_t gpio);

static uint8_t bmx_esp32_power_retained_gpio[GPIO_NUM_MAX];
static uint8_t bmx_esp32_power_held_gpio[GPIO_NUM_MAX];

static int32_t bmx_esp32_power_enable_gpio_holds(void) {
    for (uint32_t gpio = 0; gpio < GPIO_NUM_MAX; ++gpio) {
        if (!bmx_esp32_power_retained_gpio[gpio]) continue;
        if (!GPIO_IS_VALID_OUTPUT_GPIO((int32_t)gpio) ||
                !bmx_embedded_gpio_get_direction(gpio)) {
            for (uint32_t held = 0; held < gpio; ++held) {
                if (!bmx_esp32_power_held_gpio[held]) continue;
                (void)gpio_hold_dis((gpio_num_t)held);
                bmx_esp32_power_held_gpio[held] = 0;
            }
            return BMX_POWER_PRECONDITION_NOT_MET;
        }
        if (gpio_hold_en((gpio_num_t)gpio) != ESP_OK) {
            for (uint32_t held = 0; held < gpio; ++held) {
                if (!bmx_esp32_power_held_gpio[held]) continue;
                (void)gpio_hold_dis((gpio_num_t)held);
                bmx_esp32_power_held_gpio[held] = 0;
            }
            return BMX_POWER_ERROR;
        }
        bmx_esp32_power_held_gpio[gpio] = 1;
    }
    return 0;
}

static int32_t bmx_esp32_power_release_gpio_holds(void) {
    int32_t result = 0;
    for (uint32_t gpio = 0; gpio < GPIO_NUM_MAX; ++gpio) {
        if (!bmx_esp32_power_held_gpio[gpio]) continue;
        if (!bmx_esp32_gpio_restore_held(gpio)) result = BMX_POWER_ERROR;
        if (gpio_hold_dis((gpio_num_t)gpio) != ESP_OK) result = BMX_POWER_ERROR;
        bmx_esp32_power_held_gpio[gpio] = 0;
    }
    return result;
}

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
    int32_t hold_result = bmx_esp32_power_enable_gpio_holds();
    if (hold_result != 0) {
        if (wake_delay_us > 0)
            (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
        return hold_result;
    }
    esp_err_t result = esp_light_sleep_start();
    int32_t release_result = bmx_esp32_power_release_gpio_holds();
    if (wake_delay_us > 0)
        (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    if (result != ESP_OK || release_result != 0) return BMX_POWER_ERROR;
    return 0;
}

uint32_t bmx_embedded_power_capabilities(void) {
    return BMX_POWER_CAPABILITY_IDLE | BMX_POWER_CAPABILITY_SLEEP_INTERRUPT |
        BMX_POWER_CAPABILITY_SLEEP_TIMER |
        BMX_POWER_CAPABILITY_SLEEP_GPIO_RETENTION;
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

int32_t bmx_embedded_power_set_sleep_gpio_retention(uint32_t pin,
        int32_t enabled) {
    if (pin > INT32_MAX || !GPIO_IS_VALID_OUTPUT_GPIO((int32_t)pin))
        return BMX_POWER_INVALID_ARGUMENT;
    if (enabled && !bmx_embedded_gpio_get_direction(pin))
        return BMX_POWER_PRECONDITION_NOT_MET;
    bmx_esp32_power_retained_gpio[pin] = enabled != 0;
    return 0;
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
