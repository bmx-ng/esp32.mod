#ifndef BLITZMAX_ESP32_TIME_H
#define BLITZMAX_ESP32_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t bmx_esp32_system_clock_hz(void);
int32_t bmx_esp32_alarm_after_ms(uint32_t milliseconds);
int32_t bmx_esp32_alarm_after_us(uint64_t microseconds);
int32_t bmx_esp32_repeating_alarm_ms(uint32_t milliseconds);
int32_t bmx_esp32_repeating_alarm_us(uint64_t microseconds);
int32_t bmx_esp32_alarm_cancel(int32_t handle);
int32_t bmx_esp32_alarm_active(int32_t handle);
uint32_t bmx_esp32_alarm_pending_events(int32_t handle);
uint32_t bmx_esp32_alarm_take_events(int32_t handle);
int64_t bmx_esp32_alarm_remaining_us(int32_t handle);
int32_t bmx_esp32_alarm_remaining_ms(int32_t handle);

#ifdef __cplusplus
}
#endif

#endif
