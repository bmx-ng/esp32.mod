#include <stdbool.h>
#include <stdint.h>

#include "blitzmax/embedded_watchdog.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "sdkconfig.h"

#define BMX_ESP32_WATCHDOG_MAXIMUM_DELAY_MS (UINT32_MAX / 4u)
#ifndef CONFIG_ESP_TASK_WDT_TIMEOUT_S
#define CONFIG_ESP_TASK_WDT_TIMEOUT_S 5
#endif
#ifndef CONFIG_ESP_TASK_WDT_PANIC
#define CONFIG_ESP_TASK_WDT_PANIC 0
#endif

static int32_t bmx_esp32_watchdog_active;
static int32_t bmx_esp32_watchdog_added_task;
static int32_t bmx_esp32_watchdog_initialized;

static uint32_t bmx_esp32_watchdog_default_idle_mask(void) {
    uint32_t mask = 0;
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
    mask |= 1u;
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
    mask |= 2u;
#endif
    return mask;
}

static esp_task_wdt_config_t bmx_esp32_watchdog_config(uint32_t delay_ms,
        bool trigger_panic) {
    esp_task_wdt_config_t config = {
        .timeout_ms = delay_ms,
        .idle_core_mask = bmx_esp32_watchdog_default_idle_mask(),
        .trigger_panic = trigger_panic,
    };
    return config;
}

uint32_t bmx_embedded_watchdog_maximum_delay_ms(void) {
    return BMX_ESP32_WATCHDOG_MAXIMUM_DELAY_MS;
}

int32_t bmx_embedded_watchdog_enable(uint32_t delay_ms) {
    if (!delay_ms || delay_ms > BMX_ESP32_WATCHDOG_MAXIMUM_DELAY_MS) return 0;
    if (bmx_esp32_watchdog_active) {
        esp_task_wdt_config_t config = bmx_esp32_watchdog_config(delay_ms, true);
        return esp_task_wdt_reconfigure(&config) == ESP_OK;
    }

    esp_err_t status = esp_task_wdt_status(NULL);
    esp_task_wdt_config_t config = bmx_esp32_watchdog_config(delay_ms, true);
    if (status == ESP_ERR_INVALID_STATE) {
        if (esp_task_wdt_init(&config) != ESP_OK) return 0;
        bmx_esp32_watchdog_initialized = 1;
    } else if (esp_task_wdt_reconfigure(&config) != ESP_OK) {
        return 0;
    }

    if (status != ESP_OK) {
        if (esp_task_wdt_add(NULL) != ESP_OK) {
            if (bmx_esp32_watchdog_initialized) {
                (void)esp_task_wdt_deinit();
                bmx_esp32_watchdog_initialized = 0;
            } else {
                esp_task_wdt_config_t defaults = bmx_esp32_watchdog_config(
                    CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000u,
                    CONFIG_ESP_TASK_WDT_PANIC != 0);
                (void)esp_task_wdt_reconfigure(&defaults);
            }
            return 0;
        }
        bmx_esp32_watchdog_added_task = 1;
    }
    bmx_esp32_watchdog_active = 1;
    return 1;
}

int32_t bmx_embedded_watchdog_disable(void) {
    if (!bmx_esp32_watchdog_active) return 1;
    if (bmx_esp32_watchdog_added_task && esp_task_wdt_delete(NULL) != ESP_OK)
        return 0;
    bmx_esp32_watchdog_added_task = 0;

    esp_err_t result;
    if (bmx_esp32_watchdog_initialized) {
        result = esp_task_wdt_deinit();
        if (result == ESP_OK) bmx_esp32_watchdog_initialized = 0;
    } else {
        esp_task_wdt_config_t defaults = bmx_esp32_watchdog_config(
            CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000u,
            CONFIG_ESP_TASK_WDT_PANIC != 0);
        result = esp_task_wdt_reconfigure(&defaults);
    }
    if (result != ESP_OK) return 0;
    bmx_esp32_watchdog_active = 0;
    return 1;
}

int32_t bmx_embedded_watchdog_feed(void) {
    if (!bmx_esp32_watchdog_active || esp_task_wdt_status(NULL) != ESP_OK) return 0;
    return esp_task_wdt_reset() == ESP_OK;
}

int32_t bmx_embedded_watchdog_is_enabled(void) {
    return bmx_esp32_watchdog_active && esp_task_wdt_status(NULL) == ESP_OK;
}

int32_t bmx_embedded_watchdog_caused_reboot(void) {
    esp_reset_reason_t reason = esp_reset_reason();
    return reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT ||
        reason == ESP_RST_WDT;
}
