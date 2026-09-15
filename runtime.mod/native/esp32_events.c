#include <stdint.h>

#include "blitzmax/embedded_events.h"
#include "blitzmax/embedded_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"

#define BMX_ESP32_EVENT_CAPACITY 32u
#define BMX_ESP32_EVENT_MASK (BMX_ESP32_EVENT_CAPACITY - 1u)

typedef struct BMXESP32DeferredEvent {
    uint32_t token;
    uint32_t event_data;
    uint32_t event_mods;
    uint32_t event_x;
    uint32_t event_y;
} BMXESP32DeferredEvent;

static BMXESP32DeferredEvent bmx_esp32_deferred_events[BMX_ESP32_EVENT_CAPACITY];
static uint32_t bmx_esp32_deferred_event_put;
static uint32_t bmx_esp32_deferred_event_get;
static uint32_t bmx_esp32_deferred_event_dropped;
static portMUX_TYPE bmx_esp32_deferred_event_lock = portMUX_INITIALIZER_UNLOCKED;
static StaticSemaphore_t bmx_esp32_deferred_event_signal_storage;
static SemaphoreHandle_t bmx_esp32_deferred_event_signal;

static int32_t bmx_esp32_event_post_locked(uint32_t token,
        uint32_t event_data, uint32_t event_mods, uint32_t event_x,
        uint32_t event_y) {
    if (bmx_esp32_deferred_event_put - bmx_esp32_deferred_event_get >=
            BMX_ESP32_EVENT_CAPACITY) {
        if (bmx_esp32_deferred_event_dropped != UINT32_MAX) {
            ++bmx_esp32_deferred_event_dropped;
        }
        return 0;
    }
    BMXESP32DeferredEvent *event = &bmx_esp32_deferred_events[
        bmx_esp32_deferred_event_put & BMX_ESP32_EVENT_MASK];
    event->token = token;
    event->event_data = event_data;
    event->event_mods = event_mods;
    event->event_x = event_x;
    event->event_y = event_y;
    ++bmx_esp32_deferred_event_put;
    return 1;
}

int32_t bmx_embedded_event_post(uint32_t token, uint32_t event_data,
        uint32_t detail) {
    return bmx_embedded_event_post_ex(token, event_data, 0, detail, 0);
}

int32_t bmx_embedded_event_post_ex(uint32_t token, uint32_t event_data,
        uint32_t event_mods, uint32_t event_x, uint32_t event_y) {
    if (!token) return 0;
    portENTER_CRITICAL_SAFE(&bmx_esp32_deferred_event_lock);
    int32_t posted = bmx_esp32_event_post_locked(token, event_data,
        event_mods, event_x, event_y);
    SemaphoreHandle_t signal = bmx_esp32_deferred_event_signal;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_deferred_event_lock);
    if (posted && signal) (void)xSemaphoreGive(signal);
    return posted;
}

int32_t bmx_embedded_event_post_from_isr(uint32_t token,
        uint32_t event_data, uint32_t detail) {
    return bmx_embedded_event_post_from_isr_ex(token, event_data, 0, detail, 0);
}

int32_t bmx_embedded_event_post_from_isr_ex(uint32_t token,
        uint32_t event_data, uint32_t event_mods, uint32_t event_x,
        uint32_t event_y) {
    if (!token) return 0;
    portENTER_CRITICAL_ISR(&bmx_esp32_deferred_event_lock);
    int32_t posted = bmx_esp32_event_post_locked(token, event_data,
        event_mods, event_x, event_y);
    SemaphoreHandle_t signal = bmx_esp32_deferred_event_signal;
    portEXIT_CRITICAL_ISR(&bmx_esp32_deferred_event_lock);
    if (posted && signal) {
        BaseType_t higher_priority_task_woken = pdFALSE;
        (void)xSemaphoreGiveFromISR(signal, &higher_priority_task_woken);
        if (higher_priority_task_woken) portYIELD_FROM_ISR();
    }
    return posted;
}

int32_t bmx_embedded_event_take(uint32_t *token, uint32_t *event_data,
        uint32_t *event_mods, uint32_t *event_x, uint32_t *event_y) {
    if (!token || !event_data || !event_mods || !event_x || !event_y) return 0;
    portENTER_CRITICAL_SAFE(&bmx_esp32_deferred_event_lock);
    if (bmx_esp32_deferred_event_get == bmx_esp32_deferred_event_put) {
        portEXIT_CRITICAL_SAFE(&bmx_esp32_deferred_event_lock);
        return 0;
    }
    BMXESP32DeferredEvent event = bmx_esp32_deferred_events[
        bmx_esp32_deferred_event_get & BMX_ESP32_EVENT_MASK];
    ++bmx_esp32_deferred_event_get;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_deferred_event_lock);
    *token = event.token;
    *event_data = event.event_data;
    *event_mods = event.event_mods;
    *event_x = event.event_x;
    *event_y = event.event_y;
    return 1;
}

uint32_t bmx_embedded_event_pending(void) {
    portENTER_CRITICAL_SAFE(&bmx_esp32_deferred_event_lock);
    uint32_t pending = bmx_esp32_deferred_event_put -
        bmx_esp32_deferred_event_get;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_deferred_event_lock);
    return pending;
}

uint32_t bmx_embedded_event_dropped(void) {
    portENTER_CRITICAL_SAFE(&bmx_esp32_deferred_event_lock);
    uint32_t dropped = bmx_esp32_deferred_event_dropped;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_deferred_event_lock);
    return dropped;
}

void bmx_embedded_system_wait(void) {
    if (!bmx_esp32_deferred_event_signal) {
        bmx_esp32_deferred_event_signal = xSemaphoreCreateBinaryStatic(
            &bmx_esp32_deferred_event_signal_storage);
    }
    portENTER_CRITICAL_SAFE(&bmx_esp32_deferred_event_lock);
    if (bmx_esp32_deferred_event_get != bmx_esp32_deferred_event_put) {
        portEXIT_CRITICAL_SAFE(&bmx_esp32_deferred_event_lock);
        return;
    }
    SemaphoreHandle_t signal = bmx_esp32_deferred_event_signal;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_deferred_event_lock);
    if (signal) (void)xSemaphoreTake(signal, portMAX_DELAY);
}

void bmx_embedded_system_wake(void) {
    if (!bmx_esp32_deferred_event_signal) {
        bmx_esp32_deferred_event_signal = xSemaphoreCreateBinaryStatic(
            &bmx_esp32_deferred_event_signal_storage);
    }
    SemaphoreHandle_t signal = bmx_esp32_deferred_event_signal;
    if (signal) (void)xSemaphoreGive(signal);
}
