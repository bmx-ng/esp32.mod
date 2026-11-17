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
    BMX_POWER_CAPABILITY_SLEEP_GPIO_WAKE = 0x80u,
    BMX_POWER_UNAVAILABLE = -17,
    BMX_POWER_INVALID_ARGUMENT = -5,
    BMX_POWER_INVALID_STATE = -12,
    BMX_POWER_PRECONDITION_NOT_MET = -14,
    BMX_POWER_ERROR = -1
};

int64_t bmx_esp32_calendar_wake_delay_us(void);
int32_t bmx_embedded_gpio_get_direction(uint32_t gpio);
int32_t bmx_esp32_gpio_restore_held(uint32_t gpio);

static uint8_t bmx_esp32_power_retained_gpio[GPIO_NUM_MAX];
static uint8_t bmx_esp32_power_held_gpio[GPIO_NUM_MAX];
/* 0 = disabled, 1 = wake low, 2 = wake high. */
static uint8_t bmx_esp32_power_wake_gpio[GPIO_NUM_MAX];
static uint64_t bmx_esp32_power_last_wake_gpios;

static int32_t bmx_esp32_power_result(esp_err_t result) {
    if (result == ESP_OK) return 0;
    if (result == ESP_ERR_INVALID_ARG) return BMX_POWER_INVALID_ARGUMENT;
    if (result == ESP_ERR_INVALID_STATE) return BMX_POWER_INVALID_STATE;
    return BMX_POWER_ERROR;
}

static int32_t bmx_esp32_power_validate_wake_levels(void) {
    for (uint32_t gpio = 0; gpio < GPIO_NUM_MAX; ++gpio) {
        uint8_t policy = bmx_esp32_power_wake_gpio[gpio];
        if (!policy) continue;
        int active_level = policy == 2;
        if (gpio_get_level((gpio_num_t)gpio) == active_level)
            return BMX_POWER_PRECONDITION_NOT_MET;
    }
    return 0;
}

static void bmx_esp32_power_capture_wake_gpios(void) {
    bmx_esp32_power_last_wake_gpios = 0;
    if (!(esp_sleep_get_wakeup_causes() & (1u << ESP_SLEEP_WAKEUP_GPIO))) return;
    for (uint32_t gpio = 0; gpio < GPIO_NUM_MAX && gpio < 64u; ++gpio) {
        uint8_t policy = bmx_esp32_power_wake_gpio[gpio];
        if (!policy) continue;
        if (gpio_get_level((gpio_num_t)gpio) == (policy == 2))
            bmx_esp32_power_last_wake_gpios |= 1ULL << gpio;
    }
}

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
    int32_t wake_level_result = bmx_esp32_power_validate_wake_levels();
    if (wake_level_result != 0) {
        if (wake_delay_us > 0)
            (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
        return wake_level_result;
    }
    int32_t hold_result = bmx_esp32_power_enable_gpio_holds();
    if (hold_result != 0) {
        if (wake_delay_us > 0)
            (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
        return hold_result;
    }
    esp_err_t result = esp_light_sleep_start();
    bmx_esp32_power_capture_wake_gpios();
    int32_t release_result = bmx_esp32_power_release_gpio_holds();
    if (wake_delay_us > 0)
        (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    if (result != ESP_OK || release_result != 0) return BMX_POWER_ERROR;
    return 0;
}

uint32_t bmx_embedded_power_capabilities(void) {
    return BMX_POWER_CAPABILITY_IDLE | BMX_POWER_CAPABILITY_SLEEP_INTERRUPT |
        BMX_POWER_CAPABILITY_SLEEP_TIMER |
        BMX_POWER_CAPABILITY_SLEEP_GPIO_RETENTION |
        BMX_POWER_CAPABILITY_SLEEP_GPIO_WAKE;
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

int32_t bmx_embedded_power_set_sleep_gpio_wake(uint32_t pin, int32_t high,
        int32_t enabled) {
    if (pin > INT32_MAX || !GPIO_IS_VALID_GPIO((int32_t)pin))
        return BMX_POWER_INVALID_ARGUMENT;

    if (enabled) {
        gpio_int_type_t trigger = high ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL;
        esp_err_t result = gpio_wakeup_enable((gpio_num_t)pin, trigger);
        if (result != ESP_OK) return bmx_esp32_power_result(result);
        result = esp_sleep_enable_gpio_wakeup();
        if (result != ESP_OK) {
            (void)gpio_wakeup_disable((gpio_num_t)pin);
            return bmx_esp32_power_result(result);
        }
        bmx_esp32_power_wake_gpio[pin] = high ? 2 : 1;
        return 0;
    }

    esp_err_t result = gpio_wakeup_disable((gpio_num_t)pin);
    if (result != ESP_OK) return bmx_esp32_power_result(result);
    bmx_esp32_power_wake_gpio[pin] = 0;

    for (uint32_t gpio = 0; gpio < GPIO_NUM_MAX; ++gpio) {
        if (bmx_esp32_power_wake_gpio[gpio]) return 0;
    }
    result = esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    return bmx_esp32_power_result(result);
}

int32_t bmx_esp32_power_deep_sleep_for_ms(uint64_t milliseconds) {
    if (!milliseconds || milliseconds > UINT64_MAX / 1000u)
        return BMX_POWER_INVALID_ARGUMENT;
    if (esp_sleep_enable_timer_wakeup(milliseconds * 1000u) != ESP_OK)
        return BMX_POWER_ERROR;
    esp_deep_sleep_start();
    return BMX_POWER_ERROR;
}

int32_t bmx_esp32_power_deep_sleep_until_gpio(uint32_t pin, int32_t high) {
    if (pin >= 64u || pin > INT32_MAX ||
            !esp_sleep_is_valid_wakeup_gpio((gpio_num_t)pin))
        return BMX_POWER_INVALID_ARGUMENT;
    if (gpio_get_level((gpio_num_t)pin) == (high != 0))
        return BMX_POWER_PRECONDITION_NOT_MET;

#if SOC_PM_SUPPORT_EXT1_WAKEUP_MODE_PER_PIN
    esp_sleep_ext1_wakeup_mode_t mode = high ? ESP_EXT1_WAKEUP_ANY_HIGH :
        ESP_EXT1_WAKEUP_ANY_LOW;
#else
    esp_sleep_ext1_wakeup_mode_t mode = high ? ESP_EXT1_WAKEUP_ANY_HIGH :
        ESP_EXT1_WAKEUP_ALL_LOW;
#endif
    esp_err_t result = esp_sleep_enable_ext1_wakeup_io(1ULL << pin, mode);
    if (result != ESP_OK) return bmx_esp32_power_result(result);
    esp_deep_sleep_start();
    return BMX_POWER_ERROR;
}

uint32_t bmx_esp32_power_wake_causes(void) {
    return esp_sleep_get_wakeup_causes();
}

uint64_t bmx_esp32_power_wake_gpios(void) {
    if (bmx_esp32_power_last_wake_gpios)
        return bmx_esp32_power_last_wake_gpios;
    return esp_sleep_get_ext1_wakeup_status();
}
