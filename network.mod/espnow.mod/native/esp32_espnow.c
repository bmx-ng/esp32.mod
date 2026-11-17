#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "blitzmax/embedded_runtime.h"
#include "blitzmax/embedded_system.h"

#define BMX_ESPNOW_ADDRESS_LENGTH 6
#define BMX_ESPNOW_KEY_LENGTH 16
#define BMX_ESPNOW_MAXIMUM_PAYLOAD_V1 250
#define BMX_ESPNOW_MAXIMUM_PAYLOAD_V2 1470
#define BMX_ESPNOW_MAXIMUM_QUEUE_CAPACITY 64

typedef struct BMXESPNowReceivedPacket {
    uint8_t source_address[BMX_ESPNOW_ADDRESS_LENGTH];
    uint8_t destination_address[BMX_ESPNOW_ADDRESS_LENGTH];
    int32_t rssi;
    int32_t channel;
    int32_t length;
} BMXESPNowReceivedPacket;

typedef struct BMXESPNowDeliveryReport {
    uint8_t destination_address[BMX_ESPNOW_ADDRESS_LENGTH];
    int32_t delivered;
} BMXESPNowDeliveryReport;

static BMXESPNowReceivedPacket *bmx_espnow_received;
static uint8_t *bmx_espnow_received_payloads;
static BMXESPNowDeliveryReport *bmx_espnow_deliveries;
static uint32_t bmx_espnow_received_capacity;
static uint32_t bmx_espnow_delivery_capacity;
static uint32_t bmx_espnow_maximum_received_payload;
static uint32_t bmx_espnow_received_put;
static uint32_t bmx_espnow_received_get;
static uint32_t bmx_espnow_delivery_put;
static uint32_t bmx_espnow_delivery_get;
static uint32_t bmx_espnow_dropped_received;
static uint32_t bmx_espnow_dropped_deliveries;
static bool bmx_espnow_active;
static portMUX_TYPE bmx_espnow_lock = portMUX_INITIALIZER_UNLOCKED;

extern int32_t bmx_embedded_wifi_initialized(void);

static void bmx_espnow_receive_callback(const esp_now_recv_info_t *info,
        const uint8_t *data, int length) {
    if (!info || !info->src_addr || !info->des_addr || !data || length < 0) return;
    BMXESPNowReceivedPacket packet = {0};
    memcpy(packet.source_address, info->src_addr, BMX_ESPNOW_ADDRESS_LENGTH);
    memcpy(packet.destination_address, info->des_addr, BMX_ESPNOW_ADDRESS_LENGTH);
    packet.length = length;
    if (info->rx_ctrl) {
        packet.rssi = info->rx_ctrl->rssi;
        packet.channel = info->rx_ctrl->channel;
    }
    bool queued = false;
    portENTER_CRITICAL(&bmx_espnow_lock);
    if (!bmx_espnow_active) {
        portEXIT_CRITICAL(&bmx_espnow_lock);
        return;
    }
    if ((uint32_t)length > bmx_espnow_maximum_received_payload) {
        if (bmx_espnow_dropped_received != UINT32_MAX) ++bmx_espnow_dropped_received;
        portEXIT_CRITICAL(&bmx_espnow_lock);
        return;
    }
    if (bmx_espnow_received_put - bmx_espnow_received_get < bmx_espnow_received_capacity) {
        uint32_t index = bmx_espnow_received_put % bmx_espnow_received_capacity;
        bmx_espnow_received[index] = packet;
        if (length) memcpy(bmx_espnow_received_payloads +
            (size_t)index * bmx_espnow_maximum_received_payload, data, (size_t)length);
        ++bmx_espnow_received_put;
        queued = true;
    } else if (bmx_espnow_dropped_received != UINT32_MAX) {
        ++bmx_espnow_dropped_received;
    }
    portEXIT_CRITICAL(&bmx_espnow_lock);
    if (queued) bmx_embedded_system_wake();
}

static void bmx_espnow_send_callback(const esp_now_send_info_t *info,
        esp_now_send_status_t status) {
    BMXESPNowDeliveryReport report = {0};
    if (info && info->des_addr) {
        memcpy(report.destination_address, info->des_addr, BMX_ESPNOW_ADDRESS_LENGTH);
    }
    report.delivered = status == ESP_NOW_SEND_SUCCESS;
    bool queued = false;
    portENTER_CRITICAL(&bmx_espnow_lock);
    if (!bmx_espnow_active) {
        portEXIT_CRITICAL(&bmx_espnow_lock);
        return;
    }
    if (bmx_espnow_delivery_put - bmx_espnow_delivery_get < bmx_espnow_delivery_capacity) {
        bmx_espnow_deliveries[bmx_espnow_delivery_put % bmx_espnow_delivery_capacity] = report;
        ++bmx_espnow_delivery_put;
        queued = true;
    } else if (bmx_espnow_dropped_deliveries != UINT32_MAX) {
        ++bmx_espnow_dropped_deliveries;
    }
    portEXIT_CRITICAL(&bmx_espnow_lock);
    if (queued) bmx_embedded_system_wake();
}

int32_t bmx_esp32_espnow_initialize(int32_t receive_capacity, int32_t delivery_capacity,
        int32_t maximum_received_payload) {
    portENTER_CRITICAL(&bmx_espnow_lock);
    bool active = bmx_espnow_active;
    portEXIT_CRITICAL(&bmx_espnow_lock);
    if (active) return ESP_OK;
    if (!bmx_embedded_wifi_initialized()) return ESP_ERR_INVALID_STATE;
    if (receive_capacity < 1 || receive_capacity > BMX_ESPNOW_MAXIMUM_QUEUE_CAPACITY ||
            delivery_capacity < 1 || delivery_capacity > BMX_ESPNOW_MAXIMUM_QUEUE_CAPACITY ||
            maximum_received_payload < 1 || maximum_received_payload > BMX_ESPNOW_MAXIMUM_PAYLOAD_V2) {
        return ESP_ERR_INVALID_ARG;
    }
    BMXESPNowReceivedPacket *received = calloc((size_t)receive_capacity, sizeof(*received));
    uint8_t *payloads = calloc((size_t)receive_capacity, (size_t)maximum_received_payload);
    BMXESPNowDeliveryReport *deliveries = calloc((size_t)delivery_capacity, sizeof(*deliveries));
    if (!received || !payloads || !deliveries) {
        free(received);
        free(payloads);
        free(deliveries);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t result = esp_now_init();
    if (result != ESP_OK) {
        free(received);
        free(payloads);
        free(deliveries);
        return result;
    }
    result = esp_now_register_recv_cb(bmx_espnow_receive_callback);
    if (result != ESP_OK) {
        esp_now_deinit();
        free(received);
        free(payloads);
        free(deliveries);
        return result;
    }
    result = esp_now_register_send_cb(bmx_espnow_send_callback);
    if (result != ESP_OK) {
        esp_now_unregister_recv_cb();
        esp_now_deinit();
        free(received);
        free(payloads);
        free(deliveries);
        return result;
    }
    portENTER_CRITICAL(&bmx_espnow_lock);
    bmx_espnow_received = received;
    bmx_espnow_received_payloads = payloads;
    bmx_espnow_deliveries = deliveries;
    bmx_espnow_received_capacity = (uint32_t)receive_capacity;
    bmx_espnow_delivery_capacity = (uint32_t)delivery_capacity;
    bmx_espnow_maximum_received_payload = (uint32_t)maximum_received_payload;
    bmx_espnow_received_put = bmx_espnow_received_get = 0;
    bmx_espnow_delivery_put = bmx_espnow_delivery_get = 0;
    bmx_espnow_dropped_received = bmx_espnow_dropped_deliveries = 0;
    bmx_espnow_active = true;
    portEXIT_CRITICAL(&bmx_espnow_lock);
    return ESP_OK;
}

int32_t bmx_esp32_espnow_deinitialize(void) {
    portENTER_CRITICAL(&bmx_espnow_lock);
    if (!bmx_espnow_active) {
        portEXIT_CRITICAL(&bmx_espnow_lock);
        return ESP_OK;
    }
    bmx_espnow_active = false;
    portEXIT_CRITICAL(&bmx_espnow_lock);
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_err_t result = esp_now_deinit();
    free(bmx_espnow_received);
    free(bmx_espnow_received_payloads);
    free(bmx_espnow_deliveries);
    bmx_espnow_received = NULL;
    bmx_espnow_received_payloads = NULL;
    bmx_espnow_deliveries = NULL;
    bmx_espnow_received_capacity = bmx_espnow_delivery_capacity = 0;
    bmx_espnow_maximum_received_payload = 0;
    return result;
}

int32_t bmx_esp32_espnow_initialized(void) {
    portENTER_CRITICAL(&bmx_espnow_lock);
    bool active = bmx_espnow_active;
    portEXIT_CRITICAL(&bmx_espnow_lock);
    return active;
}

const BMXEmbeddedString *bmx_esp32_espnow_result_name(int32_t result) {
    const char *name = esp_err_to_name((esp_err_t)result);
    return bmx_embedded_string_from_ascii(name, (int32_t)strlen(name));
}

int32_t bmx_esp32_espnow_version(uint32_t *version) {
    if (!version) return ESP_ERR_INVALID_ARG;
    return esp_now_get_version(version);
}

int32_t bmx_esp32_espnow_local_address(uint8_t *address) {
    if (!address) return ESP_ERR_INVALID_ARG;
    return esp_wifi_get_mac(WIFI_IF_STA, address);
}

int32_t bmx_esp32_espnow_set_channel(int32_t channel) {
    if (!bmx_embedded_wifi_initialized()) return ESP_ERR_INVALID_STATE;
    if (channel < 1 || channel > 14) return ESP_ERR_INVALID_ARG;
    return esp_wifi_set_channel((uint8_t)channel, WIFI_SECOND_CHAN_NONE);
}

int32_t bmx_esp32_espnow_get_channel(int32_t *channel) {
    if (!channel) return ESP_ERR_INVALID_ARG;
    uint8_t primary = 0;
    wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
    esp_err_t result = esp_wifi_get_channel(&primary, &secondary);
    if (result == ESP_OK) *channel = primary;
    return result;
}

int32_t bmx_esp32_espnow_set_primary_master_key(const uint8_t *key, int32_t length) {
    if (!key || length != BMX_ESPNOW_KEY_LENGTH) return ESP_ERR_INVALID_ARG;
    return esp_now_set_pmk(key);
}

int32_t bmx_esp32_espnow_add_peer(const uint8_t *address, int32_t channel,
        const uint8_t *key, int32_t key_length) {
    if (!address || channel < 0 || channel > 14 ||
            (key && key_length != BMX_ESPNOW_KEY_LENGTH) || (!key && key_length)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, address, BMX_ESPNOW_ADDRESS_LENGTH);
    peer.channel = (uint8_t)channel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = key != NULL;
    if (key) memcpy(peer.lmk, key, BMX_ESPNOW_KEY_LENGTH);
    if (esp_now_is_peer_exist(address)) return esp_now_mod_peer(&peer);
    return esp_now_add_peer(&peer);
}

int32_t bmx_esp32_espnow_delete_peer(const uint8_t *address) {
    if (!address) return ESP_ERR_INVALID_ARG;
    return esp_now_del_peer(address);
}

int32_t bmx_esp32_espnow_peer_exists(const uint8_t *address) {
    return address && esp_now_is_peer_exist(address);
}

int32_t bmx_esp32_espnow_peer_counts(int32_t *total, int32_t *encrypted) {
    if (!total || !encrypted) return ESP_ERR_INVALID_ARG;
    esp_now_peer_num_t counts = {0};
    esp_err_t result = esp_now_get_peer_num(&counts);
    if (result == ESP_OK) {
        *total = counts.total_num;
        *encrypted = counts.encrypt_num;
    }
    return result;
}

int32_t bmx_esp32_espnow_send(const uint8_t *address, const uint8_t *data, int32_t length) {
    if (!address || !data || length < 0 || length > BMX_ESPNOW_MAXIMUM_PAYLOAD_V2) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_now_send(address, data, (size_t)length);
}

int32_t bmx_esp32_espnow_take_received(uint8_t *source_address,
        uint8_t *destination_address, int32_t *rssi, int32_t *channel,
        uint8_t *data, int32_t capacity, int32_t *length) {
    if (!source_address || !destination_address || !rssi || !channel || !data ||
            !length || capacity < 1) return 0;
    BMXESPNowReceivedPacket packet;
    uint32_t index = 0;
    bool available = false;
    portENTER_CRITICAL(&bmx_espnow_lock);
    if (bmx_espnow_received_get != bmx_espnow_received_put) {
        index = bmx_espnow_received_get % bmx_espnow_received_capacity;
        packet = bmx_espnow_received[index];
        if (packet.length > capacity) {
            ++bmx_espnow_received_get;
            if (bmx_espnow_dropped_received != UINT32_MAX) ++bmx_espnow_dropped_received;
            portEXIT_CRITICAL(&bmx_espnow_lock);
            return 0;
        }
        if (packet.length) memcpy(data, bmx_espnow_received_payloads +
            (size_t)index * bmx_espnow_maximum_received_payload, (size_t)packet.length);
        ++bmx_espnow_received_get;
        available = true;
    }
    portEXIT_CRITICAL(&bmx_espnow_lock);
    if (!available) return 0;
    memcpy(source_address, packet.source_address, BMX_ESPNOW_ADDRESS_LENGTH);
    memcpy(destination_address, packet.destination_address, BMX_ESPNOW_ADDRESS_LENGTH);
    *rssi = packet.rssi;
    *channel = packet.channel;
    *length = packet.length;
    return 1;
}

int32_t bmx_esp32_espnow_take_delivery(uint8_t *destination_address, int32_t *delivered) {
    if (!destination_address || !delivered) return 0;
    BMXESPNowDeliveryReport report;
    bool available = false;
    portENTER_CRITICAL(&bmx_espnow_lock);
    if (bmx_espnow_delivery_get != bmx_espnow_delivery_put) {
        report = bmx_espnow_deliveries[bmx_espnow_delivery_get % bmx_espnow_delivery_capacity];
        ++bmx_espnow_delivery_get;
        available = true;
    }
    portEXIT_CRITICAL(&bmx_espnow_lock);
    if (!available) return 0;
    memcpy(destination_address, report.destination_address, BMX_ESPNOW_ADDRESS_LENGTH);
    *delivered = report.delivered;
    return 1;
}

uint32_t bmx_esp32_espnow_dropped_received(void) {
    portENTER_CRITICAL(&bmx_espnow_lock);
    uint32_t dropped = bmx_espnow_dropped_received;
    portEXIT_CRITICAL(&bmx_espnow_lock);
    return dropped;
}

uint32_t bmx_esp32_espnow_dropped_deliveries(void) {
    portENTER_CRITICAL(&bmx_espnow_lock);
    uint32_t dropped = bmx_espnow_dropped_deliveries;
    portEXIT_CRITICAL(&bmx_espnow_lock);
    return dropped;
}
