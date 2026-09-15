#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

typedef struct BMXESP32CalendarDateTime {
    int32_t year;
    int32_t month;
    int32_t day;
    int32_t hour;
    int32_t minute;
    int32_t second;
    int32_t millisecond;
    int32_t utc;
    int32_t offset;
    int32_t dst;
} BMXESP32CalendarDateTime;

static esp_timer_handle_t bmx_esp32_calendar_alarm;
static uint32_t bmx_esp32_calendar_alarm_events;
static int32_t bmx_esp32_calendar_stopped;
static int32_t bmx_esp32_calendar_alarm_wakes;
static int64_t bmx_esp32_calendar_alarm_wake_deadline;
static portMUX_TYPE bmx_esp32_calendar_lock = portMUX_INITIALIZER_UNLOCKED;

static int32_t bmx_esp32_calendar_days_in_month(int32_t year, int32_t month) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    int32_t result = days[month - 1];
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) ++result;
    return result;
}

static int32_t bmx_esp32_calendar_valid(const BMXESP32CalendarDateTime *value) {
    return value && value->year >= 1970 && value->year <= 9999 &&
        value->month >= 1 && value->month <= 12 && value->day >= 1 &&
        value->day <= bmx_esp32_calendar_days_in_month(value->year, value->month) &&
        value->hour >= 0 && value->hour <= 23 && value->minute >= 0 &&
        value->minute <= 59 && value->second >= 0 && value->second <= 59 &&
        value->millisecond >= 0 && value->millisecond <= 999;
}

static int32_t bmx_esp32_calendar_to_timeval(
    const BMXESP32CalendarDateTime *value, struct timeval *result) {
    if (!result || !bmx_esp32_calendar_valid(value)) return 0;
    struct tm calendar;
    memset(&calendar, 0, sizeof(calendar));
    calendar.tm_year = value->year - 1900;
    calendar.tm_mon = value->month - 1;
    calendar.tm_mday = value->day;
    calendar.tm_hour = value->hour;
    calendar.tm_min = value->minute;
    calendar.tm_sec = value->second;
    time_t seconds = timegm(&calendar);
    if (seconds == (time_t)-1) return 0;
    if (!value->utc) {
        seconds -= (time_t)value->offset * 60;
        if (value->dst == 1) seconds -= 3600;
    }
    result->tv_sec = seconds;
    result->tv_usec = value->millisecond * 1000;
    return 1;
}

static void bmx_esp32_calendar_alarm_callback(void *argument) {
    (void)argument;
    portENTER_CRITICAL(&bmx_esp32_calendar_lock);
    if (bmx_esp32_calendar_alarm_events != UINT32_MAX)
        ++bmx_esp32_calendar_alarm_events;
    bmx_esp32_calendar_alarm_wakes = 0;
    bmx_esp32_calendar_alarm_wake_deadline = 0;
    portEXIT_CRITICAL(&bmx_esp32_calendar_lock);
}

static int32_t bmx_esp32_calendar_alarm_create(void) {
    if (bmx_esp32_calendar_alarm) return 1;
    const esp_timer_create_args_t arguments = {
        .callback = bmx_esp32_calendar_alarm_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "bmx_calendar",
        .skip_unhandled_events = false
    };
    return esp_timer_create(&arguments, &bmx_esp32_calendar_alarm) == ESP_OK;
}

int32_t bmx_esp32_calendar_start(const BMXESP32CalendarDateTime *value) {
    struct timeval time;
    if (!bmx_esp32_calendar_to_timeval(value, &time) || settimeofday(&time, NULL) != 0)
        return 0;
    bmx_esp32_calendar_stopped = 0;
    return 1;
}

void bmx_esp32_calendar_disable_alarm(void);

void bmx_esp32_calendar_stop(void) {
    bmx_esp32_calendar_disable_alarm();
    bmx_esp32_calendar_stopped = 1;
}

int32_t bmx_esp32_calendar_set(const BMXESP32CalendarDateTime *value) {
    return bmx_esp32_calendar_start(value);
}

int32_t bmx_esp32_calendar_is_running(void) {
    if (bmx_esp32_calendar_stopped) return 0;
    struct timeval now;
    return gettimeofday(&now, NULL) == 0 && now.tv_sec >= 0;
}

int32_t bmx_esp32_calendar_get(BMXESP32CalendarDateTime *value) {
    if (!value || !bmx_esp32_calendar_is_running()) return 0;
    struct timeval now;
    struct tm calendar;
    if (gettimeofday(&now, NULL) != 0 || !gmtime_r(&now.tv_sec, &calendar)) return 0;
    BMXESP32CalendarDateTime result = {
        .year = calendar.tm_year + 1900,
        .month = calendar.tm_mon + 1,
        .day = calendar.tm_mday,
        .hour = calendar.tm_hour,
        .minute = calendar.tm_min,
        .second = calendar.tm_sec,
        .millisecond = (int32_t)(now.tv_usec / 1000),
        .utc = 1,
        .offset = 0,
        .dst = 0
    };
    *value = result;
    return 1;
}

uint64_t bmx_esp32_calendar_resolution_nanoseconds(void) {
    return 1000u;
}

int32_t bmx_esp32_calendar_set_alarm(
    const BMXESP32CalendarDateTime *value, int32_t wake_from_low_power) {
    struct timeval target;
    struct timeval now;
    if (!bmx_esp32_calendar_to_timeval(value, &target) ||
        gettimeofday(&now, NULL) != 0 || !bmx_esp32_calendar_alarm_create()) return 0;
    int64_t delay = ((int64_t)target.tv_sec - (int64_t)now.tv_sec) * 1000000 +
        ((int64_t)target.tv_usec - (int64_t)now.tv_usec);
    if (delay <= 0) return 0;
    if (esp_timer_is_active(bmx_esp32_calendar_alarm))
        (void)esp_timer_stop(bmx_esp32_calendar_alarm);
    portENTER_CRITICAL(&bmx_esp32_calendar_lock);
    bmx_esp32_calendar_alarm_events = 0;
    portEXIT_CRITICAL(&bmx_esp32_calendar_lock);
    portENTER_CRITICAL(&bmx_esp32_calendar_lock);
    bmx_esp32_calendar_alarm_wakes = wake_from_low_power != 0;
    bmx_esp32_calendar_alarm_wake_deadline = wake_from_low_power
        ? esp_timer_get_time() + delay : 0;
    portEXIT_CRITICAL(&bmx_esp32_calendar_lock);
    if (esp_timer_start_once(bmx_esp32_calendar_alarm, (uint64_t)delay) != ESP_OK) {
        portENTER_CRITICAL(&bmx_esp32_calendar_lock);
        bmx_esp32_calendar_alarm_wakes = 0;
        bmx_esp32_calendar_alarm_wake_deadline = 0;
        portEXIT_CRITICAL(&bmx_esp32_calendar_lock);
        return 0;
    }
    return 1;
}

void bmx_esp32_calendar_disable_alarm(void) {
    if (bmx_esp32_calendar_alarm && esp_timer_is_active(bmx_esp32_calendar_alarm))
        (void)esp_timer_stop(bmx_esp32_calendar_alarm);
    portENTER_CRITICAL(&bmx_esp32_calendar_lock);
    bmx_esp32_calendar_alarm_events = 0;
    bmx_esp32_calendar_alarm_wakes = 0;
    bmx_esp32_calendar_alarm_wake_deadline = 0;
    portEXIT_CRITICAL(&bmx_esp32_calendar_lock);
}

int64_t bmx_esp32_calendar_wake_delay_us(void) {
    int64_t deadline;
    int32_t wakes;
    portENTER_CRITICAL(&bmx_esp32_calendar_lock);
    deadline = bmx_esp32_calendar_alarm_wake_deadline;
    wakes = bmx_esp32_calendar_alarm_wakes;
    portEXIT_CRITICAL(&bmx_esp32_calendar_lock);
    if (!wakes || !deadline) return -1;
    int64_t remaining = deadline - esp_timer_get_time();
    return remaining > 0 ? remaining : 1;
}

uint32_t bmx_esp32_calendar_pending_alarm_events(void) {
    portENTER_CRITICAL(&bmx_esp32_calendar_lock);
    uint32_t result = bmx_esp32_calendar_alarm_events;
    portEXIT_CRITICAL(&bmx_esp32_calendar_lock);
    return result;
}

uint32_t bmx_esp32_calendar_take_alarm_events(void) {
    portENTER_CRITICAL(&bmx_esp32_calendar_lock);
    uint32_t result = bmx_esp32_calendar_alarm_events;
    bmx_esp32_calendar_alarm_events = 0;
    portEXIT_CRITICAL(&bmx_esp32_calendar_lock);
    return result;
}
