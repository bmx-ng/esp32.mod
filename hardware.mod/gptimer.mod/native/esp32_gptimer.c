#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "esp_err.h"
#include "soc/soc_caps.h"

#if SOC_GPTIMER_SUPPORTED
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#endif

static int32_t bmx_esp32_gptimer_create_error = ESP_OK;

#if SOC_GPTIMER_SUPPORTED
typedef struct {
    uint64_t count_value;
    uint64_t alarm_value;
} bmx_esp32_gptimer_alarm_t;

typedef struct {
    gptimer_handle_t timer;
    QueueHandle_t alarm_queue;
    uint32_t resolution_hz;
    volatile uint32_t dropped_alarms;
    uint8_t enabled;
    uint8_t running;
} bmx_esp32_gptimer_t;

static bool bmx_esp32_gptimer_on_alarm(gptimer_handle_t timer,
        const gptimer_alarm_event_data_t *event, void *context) {
    (void)timer;
    bmx_esp32_gptimer_t *state = context;
    bmx_esp32_gptimer_alarm_t alarm = {
        .count_value = event->count_value,
        .alarm_value = event->alarm_value,
    };
    BaseType_t higher_priority_task_woken = pdFALSE;
    if (xQueueSendFromISR(state->alarm_queue, &alarm,
            &higher_priority_task_woken) != pdTRUE) {
        __atomic_fetch_add(&state->dropped_alarms, 1u, __ATOMIC_RELAXED);
    }
    return higher_priority_task_woken == pdTRUE;
}
#endif

int32_t bmx_esp32_gptimer_supported(void) {
#if SOC_GPTIMER_SUPPORTED
    return 1;
#else
    return 0;
#endif
}

int32_t bmx_esp32_gptimer_last_create_error(void) {
    return bmx_esp32_gptimer_create_error;
}

void *bmx_esp32_gptimer_new(uint32_t resolution_hz, int32_t count_down,
        int32_t allow_power_down, uint32_t alarm_queue_capacity) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_create_error = ESP_OK;
    if (!resolution_hz || !alarm_queue_capacity || alarm_queue_capacity > 1024u) {
        bmx_esp32_gptimer_create_error = ESP_ERR_INVALID_ARG;
        return NULL;
    }
    bmx_esp32_gptimer_t *state = calloc(1, sizeof(*state));
    if (!state) {
        bmx_esp32_gptimer_create_error = ESP_ERR_NO_MEM;
        return NULL;
    }
    state->alarm_queue = xQueueCreate(alarm_queue_capacity,
        sizeof(bmx_esp32_gptimer_alarm_t));
    if (!state->alarm_queue) {
        bmx_esp32_gptimer_create_error = ESP_ERR_NO_MEM;
        free(state);
        return NULL;
    }
    gptimer_config_t config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = count_down ? GPTIMER_COUNT_DOWN : GPTIMER_COUNT_UP,
        .resolution_hz = resolution_hz,
        .intr_priority = 0,
        .flags.allow_pd = allow_power_down != 0,
    };
    esp_err_t result = gptimer_new_timer(&config, &state->timer);
    if (result == ESP_OK) {
        gptimer_event_callbacks_t callbacks = {
            .on_alarm = bmx_esp32_gptimer_on_alarm,
        };
        result = gptimer_register_event_callbacks(state->timer, &callbacks, state);
    }
    if (result != ESP_OK) {
        bmx_esp32_gptimer_create_error = result;
        if (state->timer) (void)gptimer_del_timer(state->timer);
        vQueueDelete(state->alarm_queue);
        free(state);
        return NULL;
    }
    if (gptimer_get_resolution(state->timer, &state->resolution_hz) != ESP_OK)
        state->resolution_hz = resolution_hz;
    return state;
#else
    (void)resolution_hz; (void)count_down; (void)allow_power_down;
    (void)alarm_queue_capacity;
    bmx_esp32_gptimer_create_error = ESP_ERR_NOT_SUPPORTED;
    return NULL;
#endif
}

void bmx_esp32_gptimer_delete(void *handle) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_t *state = handle;
    if (!state) return;
    if (state->running) (void)gptimer_stop(state->timer);
    if (state->enabled) (void)gptimer_disable(state->timer);
    gptimer_event_callbacks_t callbacks = { 0 };
    (void)gptimer_register_event_callbacks(state->timer, &callbacks, NULL);
    (void)gptimer_del_timer(state->timer);
    vQueueDelete(state->alarm_queue);
    free(state);
#else
    (void)handle;
#endif
}

uint32_t bmx_esp32_gptimer_resolution(void *handle) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_t *state = handle;
    return state ? state->resolution_hz : 0;
#else
    (void)handle;
    return 0;
#endif
}

int32_t bmx_esp32_gptimer_set_count(void *handle, uint64_t value) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_t *state = handle;
    return state ? gptimer_set_raw_count(state->timer, value) : ESP_ERR_INVALID_ARG;
#else
    (void)handle; (void)value;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

int32_t bmx_esp32_gptimer_get_count(void *handle, uint64_t *value) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_t *state = handle;
    return state && value ? gptimer_get_raw_count(state->timer, value) :
        ESP_ERR_INVALID_ARG;
#else
    (void)handle; (void)value;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

int32_t bmx_esp32_gptimer_set_alarm(void *handle, uint64_t alarm_count,
        uint64_t reload_count, int32_t auto_reload) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_t *state = handle;
    if (!state) return ESP_ERR_INVALID_ARG;
    gptimer_alarm_config_t config = {
        .alarm_count = alarm_count,
        .reload_count = reload_count,
        .flags.auto_reload_on_alarm = auto_reload != 0,
    };
    return gptimer_set_alarm_action(state->timer, &config);
#else
    (void)handle; (void)alarm_count; (void)reload_count; (void)auto_reload;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

int32_t bmx_esp32_gptimer_disable_alarm(void *handle) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_t *state = handle;
    return state ? gptimer_set_alarm_action(state->timer, NULL) :
        ESP_ERR_INVALID_ARG;
#else
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

int32_t bmx_esp32_gptimer_start(void *handle) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_t *state = handle;
    if (!state) return ESP_ERR_INVALID_ARG;
    if (state->running) return ESP_ERR_INVALID_STATE;
    if (!state->enabled) {
        esp_err_t result = gptimer_enable(state->timer);
        if (result != ESP_OK) return result;
        state->enabled = 1;
    }
    esp_err_t result = gptimer_start(state->timer);
    if (result == ESP_OK) state->running = 1;
    return result;
#else
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

int32_t bmx_esp32_gptimer_stop(void *handle) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_t *state = handle;
    if (!state || !state->running) return ESP_ERR_INVALID_STATE;
    esp_err_t result = gptimer_stop(state->timer);
    if (result == ESP_OK) state->running = 0;
    return result;
#else
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

int32_t bmx_esp32_gptimer_disable(void *handle) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_t *state = handle;
    if (!state || state->running || !state->enabled)
        return ESP_ERR_INVALID_STATE;
    esp_err_t result = gptimer_disable(state->timer);
    if (result == ESP_OK) state->enabled = 0;
    return result;
#else
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

int32_t bmx_esp32_gptimer_wait_alarm(void *handle, int32_t timeout_ms,
        uint64_t *count_value, uint64_t *alarm_value) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_t *state = handle;
    if (!state || !count_value || !alarm_value || timeout_ms < -1)
        return ESP_ERR_INVALID_ARG;
    TickType_t ticks;
    if (timeout_ms < 0) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS((uint32_t)timeout_ms);
        if (timeout_ms > 0 && ticks == 0) ticks = 1;
    }
    bmx_esp32_gptimer_alarm_t alarm;
    if (xQueueReceive(state->alarm_queue, &alarm, ticks) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    *count_value = alarm.count_value;
    *alarm_value = alarm.alarm_value;
    return ESP_OK;
#else
    (void)handle; (void)timeout_ms; (void)count_value; (void)alarm_value;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

uint32_t bmx_esp32_gptimer_dropped_alarms(void *handle, int32_t clear) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_t *state = handle;
    if (!state) return 0;
    if (clear)
        return __atomic_exchange_n(&state->dropped_alarms, 0u, __ATOMIC_RELAXED);
    return __atomic_load_n(&state->dropped_alarms, __ATOMIC_RELAXED);
#else
    (void)handle; (void)clear;
    return 0;
#endif
}

void bmx_esp32_gptimer_clear_alarm_queue(void *handle) {
#if SOC_GPTIMER_SUPPORTED
    bmx_esp32_gptimer_t *state = handle;
    if (state) (void)xQueueReset(state->alarm_queue);
#else
    (void)handle;
#endif
}
