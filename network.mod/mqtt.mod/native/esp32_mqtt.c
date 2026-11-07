/* Copyright (c) 2026 Bruce A Henderson and contributors. SPDX-License-Identifier: Zlib */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#define EVENT_COUNT 8u
#define TOPIC_MAX 127u
#define PAYLOAD_MAX 1024u
typedef struct {
    int32_t kind, code, qos, retained;
    uint32_t topic_len, payload_len;
    char topic[TOPIC_MAX + 1];
    uint8_t payload[PAYLOAD_MAX];
} mqtt_event_t;
static mqtt_event_t events[EVENT_COUNT], incoming;
static uint32_t put, get, dropped;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static esp_mqtt_client_handle_t client;
static int32_t state;
static bool receiving, overflow;
static char uri[320], client_id[128], user[128], password[128];

static void queue(const mqtt_event_t *event) {
    portENTER_CRITICAL(&lock);
    if (put - get == EVENT_COUNT) {
        if (dropped != UINT32_MAX) ++dropped;
        ++get;
    }
    events[put % EVENT_COUNT] = *event;
    ++put;
    portEXIT_CRITICAL(&lock);
}
static void handle(void *arg, esp_event_base_t base, int32_t event_id, void *data) {
    (void)arg; (void)base;
    esp_mqtt_event_handle_t event = data;
    mqtt_event_t record = {0};
    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        state = 2; record.kind = 1; queue(&record); break;
    case MQTT_EVENT_DISCONNECTED:
        state = 0; record.kind = 2; queue(&record); break;
    case MQTT_EVENT_ERROR:
        record.kind = 4;
        record.code = event && event->error_handle ? event->error_handle->error_type : -1;
        queue(&record); break;
    case MQTT_EVENT_DATA:
        if (!event) break;
        if (event->current_data_offset == 0) {
            memset(&incoming, 0, sizeof(incoming));
            incoming.kind = 3;
            incoming.qos = event->qos;
            incoming.retained = event->retain;
            incoming.topic_len = event->topic_len;
            overflow = event->topic_len < 0 || event->topic_len > TOPIC_MAX ||
                event->total_data_len < 0 || event->total_data_len > PAYLOAD_MAX;
            if (!overflow) memcpy(incoming.topic, event->topic, event->topic_len);
            receiving = true;
        }
        if (!receiving) break;
        if (event->data_len < 0 || incoming.payload_len + event->data_len > PAYLOAD_MAX)
            overflow = true;
        else if (!overflow && event->data_len) {
            memcpy(incoming.payload + incoming.payload_len, event->data, event->data_len);
            incoming.payload_len += event->data_len;
        }
        if (event->current_data_offset + event->data_len >= event->total_data_len) {
            if (!overflow) queue(&incoming);
            else { record.kind = 4; record.code = -4; queue(&record); }
            receiving = false;
        }
        break;
    }
}
static int copy(char *dest, size_t size, const uint8_t *source, bool required) {
    if (!source) { dest[0] = 0; return required ? -1 : 0; }
    size_t length = strnlen((const char *)source, size);
    if (length == size || (required && !length)) return -1;
    memcpy(dest, source, length + 1);
    return 0;
}
void bmx_embedded_mqtt_stop(void) {
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
    }
    portENTER_CRITICAL(&lock);
    state = 0;
    put = get;
    portEXIT_CRITICAL(&lock);
}
int32_t bmx_embedded_mqtt_start(const uint8_t *host, int32_t port,
        const uint8_t *id, const uint8_t *new_user, const uint8_t *new_password,
        int32_t keep_alive) {
    if (port < 1 || port > 65535 || keep_alive < 0 || keep_alive > 65535 ||
        copy(client_id, sizeof(client_id), id, true) ||
        copy(user, sizeof(user), new_user, false) ||
        copy(password, sizeof(password), new_password, false)) return -1;
    char name[256];
    if (copy(name, sizeof(name), host, true)) return -1;
    int length = snprintf(uri, sizeof(uri), "mqtt://%s:%d", name, (int)port);
    if (length < 0 || (size_t)length >= sizeof(uri)) return -1;
    bmx_embedded_mqtt_stop();
    esp_mqtt_client_config_t config = {
        .broker.address.uri = uri,
        .credentials.client_id = client_id,
        .credentials.username = new_user ? user : NULL,
        .credentials.authentication.password = new_password ? password : NULL,
        .session.keepalive = keep_alive,
        .network.disable_auto_reconnect = true,
    };
    client = esp_mqtt_client_init(&config);
    if (!client) return -2;
    esp_err_t result = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, handle, NULL);
    state = 1;
    if (result == ESP_OK) result = esp_mqtt_client_start(client);
    if (result != ESP_OK) { bmx_embedded_mqtt_stop(); return result; }
    return 0;
}
int32_t bmx_embedded_mqtt_state(void) { return state; }
int32_t bmx_embedded_mqtt_publish(const uint8_t *topic, const uint8_t *payload,
        int32_t length, int32_t qos, int32_t retained) {
    if (!client || state != 2 || !topic || length < 0 || (length && !payload) || qos < 0 || qos > 2) return -1;
    int result = esp_mqtt_client_publish(client, (const char *)topic,
        (const char *)payload, length, qos, !!retained);
    return result < 0 ? result : 0;
}
int32_t bmx_embedded_mqtt_subscribe(const uint8_t *topic, int32_t qos) {
    if (!client || state != 2 || !topic || qos < 0 || qos > 2) return -1;
    int result = esp_mqtt_client_subscribe(client, (const char *)topic, qos);
    return result < 0 ? result : 0;
}
int32_t bmx_embedded_mqtt_take_event(int32_t *kind, int32_t *code, uint8_t *topic,
        int32_t *topic_length, uint8_t *payload, int32_t *payload_length,
        int32_t *qos, int32_t *retained) {
    portENTER_CRITICAL(&lock);
    if (get == put) { portEXIT_CRITICAL(&lock); return 0; }
    mqtt_event_t *event = &events[get % EVENT_COUNT];
    *kind = event->kind; *code = event->code;
    *topic_length = (int32_t)event->topic_len;
    *payload_length = (int32_t)event->payload_len;
    *qos = event->qos; *retained = event->retained;
    memcpy(topic, event->topic, event->topic_len);
    memcpy(payload, event->payload, event->payload_len);
    ++get;
    portEXIT_CRITICAL(&lock);
    return 1;
}
uint32_t bmx_embedded_mqtt_dropped_events(void) {
    portENTER_CRITICAL(&lock);
    uint32_t result = dropped;
    portEXIT_CRITICAL(&lock);
    return result;
}
