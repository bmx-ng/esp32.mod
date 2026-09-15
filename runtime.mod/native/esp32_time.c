#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "blitzmax/esp32_time.h"
#include "esp_private/esp_clk.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"

#ifndef BMX_ESP32_ALARM_CAPACITY
#define BMX_ESP32_ALARM_CAPACITY 8u
#endif

_Static_assert(BMX_ESP32_ALARM_CAPACITY > 0u && BMX_ESP32_ALARM_CAPACITY <= 255u,
    "ESP32 alarm handles reserve one byte for the slot number");

enum {
    BMX_ESP32_ALARM_FREE = 0,
    BMX_ESP32_ALARM_RESERVED = 1,
    BMX_ESP32_ALARM_ARMED = 2,
    BMX_ESP32_ALARM_FIRED = 3,
    BMX_ESP32_ALARM_RELEASING = 4
};

typedef struct BMXESP32AlarmSlot {
    uint32_t state;
    uint32_t pending;
    uint32_t generation;
    esp_timer_handle_t timer;
    uint64_t interval_us;
    uint64_t next_deadline_us;
} BMXESP32AlarmSlot;

static BMXESP32AlarmSlot bmx_esp32_alarm_slots[BMX_ESP32_ALARM_CAPACITY];
static portMUX_TYPE bmx_esp32_alarm_lock = portMUX_INITIALIZER_UNLOCKED;

extern int32_t bmx_esp32_managed_context_valid(void);

static int32_t bmx_esp32_alarm_handle(uint32_t index, uint32_t generation) {
    return (int32_t)((generation << 8u) | (index + 1u));
}

static BMXESP32AlarmSlot *bmx_esp32_alarm_slot_locked(int32_t handle) {
    uint32_t encoded = (uint32_t)handle;
    uint32_t slot_number = encoded & 0xffu;
    uint32_t generation = encoded >> 8u;
    if (!slot_number || slot_number > BMX_ESP32_ALARM_CAPACITY || !generation) return NULL;
    BMXESP32AlarmSlot *slot = &bmx_esp32_alarm_slots[slot_number - 1u];
    if (slot->generation != generation || slot->state == BMX_ESP32_ALARM_FREE) return NULL;
    return slot;
}

static void bmx_esp32_alarm_callback(void *user_data) {
    BMXESP32AlarmSlot *slot = (BMXESP32AlarmSlot *)user_data;
    if (!slot) return;
    portENTER_CRITICAL(&bmx_esp32_alarm_lock);
    if (slot->state == BMX_ESP32_ALARM_ARMED) {
        if (slot->pending != UINT32_MAX) ++slot->pending;
        if (slot->interval_us) {
            slot->next_deadline_us += slot->interval_us;
        } else {
            slot->state = BMX_ESP32_ALARM_FIRED;
        }
    }
    portEXIT_CRITICAL(&bmx_esp32_alarm_lock);
}

static void bmx_esp32_alarm_wait_inactive(esp_timer_handle_t timer) {
    while (timer && esp_timer_is_active(timer)) taskYIELD();
}

static int32_t bmx_esp32_alarm_schedule(uint64_t delay_us, uint64_t interval_us) {
    if (!delay_us || delay_us > INT64_MAX || interval_us > INT64_MAX ||
            !bmx_esp32_managed_context_valid()) return 0;

    uint64_t now = (uint64_t)esp_timer_get_time();
    if (delay_us > (uint64_t)INT64_MAX - now) return 0;

    portENTER_CRITICAL(&bmx_esp32_alarm_lock);
    uint32_t index = BMX_ESP32_ALARM_CAPACITY;
    for (uint32_t candidate = 0; candidate < BMX_ESP32_ALARM_CAPACITY; ++candidate) {
        if (bmx_esp32_alarm_slots[candidate].state == BMX_ESP32_ALARM_FREE) {
            index = candidate;
            break;
        }
    }
    if (index == BMX_ESP32_ALARM_CAPACITY) {
        portEXIT_CRITICAL(&bmx_esp32_alarm_lock);
        return 0;
    }

    BMXESP32AlarmSlot *slot = &bmx_esp32_alarm_slots[index];
    uint32_t generation = slot->generation + 1u;
    if (!generation || generation > 0x7fffffu) generation = 1u;
    slot->generation = generation;
    slot->pending = 0;
    slot->interval_us = interval_us;
    slot->next_deadline_us = now + delay_us;
    slot->state = BMX_ESP32_ALARM_RESERVED;
    esp_timer_handle_t timer = slot->timer;
    portEXIT_CRITICAL(&bmx_esp32_alarm_lock);

    if (!timer) {
        const esp_timer_create_args_t arguments = {
            .callback = bmx_esp32_alarm_callback,
            .arg = slot,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "bmx_alarm",
            .skip_unhandled_events = false
        };
        if (esp_timer_create(&arguments, &timer) != ESP_OK) {
            portENTER_CRITICAL(&bmx_esp32_alarm_lock);
            slot->state = BMX_ESP32_ALARM_FREE;
            portEXIT_CRITICAL(&bmx_esp32_alarm_lock);
            return 0;
        }
        slot->timer = timer;
    }

    portENTER_CRITICAL(&bmx_esp32_alarm_lock);
    slot->state = BMX_ESP32_ALARM_ARMED;
    portEXIT_CRITICAL(&bmx_esp32_alarm_lock);
    esp_err_t started = interval_us ? esp_timer_start_periodic(timer, interval_us)
                                    : esp_timer_start_once(timer, delay_us);
    if (started != ESP_OK) {
        portENTER_CRITICAL(&bmx_esp32_alarm_lock);
        slot->state = BMX_ESP32_ALARM_FREE;
        portEXIT_CRITICAL(&bmx_esp32_alarm_lock);
        return 0;
    }
    return bmx_esp32_alarm_handle(index, generation);
}

uint32_t bmx_esp32_system_clock_hz(void) {
    return (uint32_t)esp_clk_cpu_freq();
}

int32_t bmx_esp32_alarm_after_ms(uint32_t milliseconds) {
    return bmx_esp32_alarm_schedule((uint64_t)milliseconds * 1000u, 0);
}

int32_t bmx_esp32_alarm_after_us(uint64_t microseconds) {
    return bmx_esp32_alarm_schedule(microseconds, 0);
}

int32_t bmx_esp32_repeating_alarm_ms(uint32_t milliseconds) {
    uint64_t interval = (uint64_t)milliseconds * 1000u;
    return bmx_esp32_alarm_schedule(interval, interval);
}

int32_t bmx_esp32_repeating_alarm_us(uint64_t microseconds) {
    return bmx_esp32_alarm_schedule(microseconds, microseconds);
}

int32_t bmx_esp32_alarm_cancel(int32_t handle) {
    if (!bmx_esp32_managed_context_valid()) return 0;
    portENTER_CRITICAL(&bmx_esp32_alarm_lock);
    BMXESP32AlarmSlot *slot = bmx_esp32_alarm_slot_locked(handle);
    if (!slot || (slot->state != BMX_ESP32_ALARM_ARMED &&
            slot->state != BMX_ESP32_ALARM_FIRED)) {
        portEXIT_CRITICAL(&bmx_esp32_alarm_lock);
        return 0;
    }
    uint32_t generation = slot->generation;
    int32_t was_armed = slot->state == BMX_ESP32_ALARM_ARMED;
    esp_timer_handle_t timer = slot->timer;
    slot->state = BMX_ESP32_ALARM_RELEASING;
    portEXIT_CRITICAL(&bmx_esp32_alarm_lock);

    if (was_armed) (void)esp_timer_stop(timer);
    bmx_esp32_alarm_wait_inactive(timer);

    portENTER_CRITICAL(&bmx_esp32_alarm_lock);
    if (slot->generation == generation && slot->state == BMX_ESP32_ALARM_RELEASING) {
        slot->pending = 0;
        slot->state = BMX_ESP32_ALARM_FREE;
    }
    portEXIT_CRITICAL(&bmx_esp32_alarm_lock);
    return 1;
}

int32_t bmx_esp32_alarm_active(int32_t handle) {
    if (!bmx_esp32_managed_context_valid()) return 0;
    portENTER_CRITICAL(&bmx_esp32_alarm_lock);
    BMXESP32AlarmSlot *slot = bmx_esp32_alarm_slot_locked(handle);
    int32_t active = slot && slot->state == BMX_ESP32_ALARM_ARMED;
    portEXIT_CRITICAL(&bmx_esp32_alarm_lock);
    return active;
}

uint32_t bmx_esp32_alarm_pending_events(int32_t handle) {
    if (!bmx_esp32_managed_context_valid()) return 0;
    portENTER_CRITICAL(&bmx_esp32_alarm_lock);
    BMXESP32AlarmSlot *slot = bmx_esp32_alarm_slot_locked(handle);
    uint32_t pending = slot ? slot->pending : 0;
    portEXIT_CRITICAL(&bmx_esp32_alarm_lock);
    return pending;
}

uint32_t bmx_esp32_alarm_take_events(int32_t handle) {
    if (!bmx_esp32_managed_context_valid()) return 0;
    portENTER_CRITICAL(&bmx_esp32_alarm_lock);
    BMXESP32AlarmSlot *slot = bmx_esp32_alarm_slot_locked(handle);
    uint32_t pending = slot ? slot->pending : 0;
    uint32_t generation = slot ? slot->generation : 0;
    esp_timer_handle_t timer = NULL;
    if (slot) {
        slot->pending = 0;
        if (slot->state == BMX_ESP32_ALARM_FIRED) {
            slot->state = BMX_ESP32_ALARM_RELEASING;
            timer = slot->timer;
        }
    }
    portEXIT_CRITICAL(&bmx_esp32_alarm_lock);

    if (timer) {
        bmx_esp32_alarm_wait_inactive(timer);
        portENTER_CRITICAL(&bmx_esp32_alarm_lock);
        if (slot->generation == generation && slot->state == BMX_ESP32_ALARM_RELEASING) {
            slot->state = BMX_ESP32_ALARM_FREE;
        }
        portEXIT_CRITICAL(&bmx_esp32_alarm_lock);
    }
    return pending;
}

int64_t bmx_esp32_alarm_remaining_us(int32_t handle) {
    if (!bmx_esp32_managed_context_valid()) return -1;
    portENTER_CRITICAL(&bmx_esp32_alarm_lock);
    BMXESP32AlarmSlot *slot = bmx_esp32_alarm_slot_locked(handle);
    uint64_t deadline = slot && slot->state == BMX_ESP32_ALARM_ARMED
        ? slot->next_deadline_us : 0;
    portEXIT_CRITICAL(&bmx_esp32_alarm_lock);
    if (!deadline) return -1;
    uint64_t now = (uint64_t)esp_timer_get_time();
    return deadline > now ? (int64_t)(deadline - now) : 0;
}

int32_t bmx_esp32_alarm_remaining_ms(int32_t handle) {
    int64_t remaining = bmx_esp32_alarm_remaining_us(handle);
    if (remaining < 0) return -1;
    uint64_t milliseconds = (uint64_t)remaining / 1000u;
    return milliseconds > INT32_MAX ? INT32_MAX : (int32_t)milliseconds;
}
