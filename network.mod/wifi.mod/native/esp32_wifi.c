#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "nvs_flash.h"
#include "blitzmax/embedded_runtime.h"
#include "blitzmax/embedded_system.h"

#define BMX_WIFI_EVENT_CAPACITY 64u
#define BMX_WIFI_EVENT_MASK (BMX_WIFI_EVENT_CAPACITY - 1u)
#define BMX_WIFI_SCAN_RESULT 1
#define BMX_WIFI_SCAN_COMPLETE 2
#define BMX_WIFI_LINK_STATE 3

#define BMX_WIFI_LINK_DOWN 0
#define BMX_WIFI_LINK_JOINED 1
#define BMX_WIFI_LINK_NO_IP 2
#define BMX_WIFI_LINK_UP 3
#define BMX_WIFI_LINK_FAILED -1
#define BMX_WIFI_LINK_NO_NETWORK -2
#define BMX_WIFI_LINK_BAD_AUTHENTICATION -3

typedef struct BMXWiFiEvent {
    int32_t kind;
    uint8_t ssid_length;
    uint8_t ssid[32];
    uint8_t bssid[6];
    uint16_t channel;
    int16_t rssi;
    uint8_t security;
    int32_t link_status;
    uint32_t address;
    uint32_t netmask;
    uint32_t gateway;
} BMXWiFiEvent;

static BMXWiFiEvent bmx_wifi_events[BMX_WIFI_EVENT_CAPACITY];
static uint32_t bmx_wifi_event_put;
static uint32_t bmx_wifi_event_get;
static uint32_t bmx_wifi_dropped;
static portMUX_TYPE bmx_wifi_lock = portMUX_INITIALIZER_UNLOCKED;
static esp_netif_t *bmx_wifi_netif;
static esp_netif_t *bmx_wifi_ap_netif;
static esp_event_handler_instance_t bmx_wifi_event_handler;
static esp_event_handler_instance_t bmx_wifi_ip_handler;
static bool bmx_wifi_initialized;
static bool bmx_wifi_scan_active;
static bool bmx_wifi_explicit_disconnect;
static bool bmx_wifi_ap_active;
static int32_t bmx_wifi_link_status;

extern uint32_t bmx_embedded_net_active_socket_count(void) __attribute__((weak));

static uint32_t bmx_wifi_pack_ip(esp_ip4_addr_t address) {
    return address.addr;
}

static bool bmx_wifi_queue(const BMXWiFiEvent *event) {
    bool queued = false;
    portENTER_CRITICAL(&bmx_wifi_lock);
    if (bmx_wifi_event_put - bmx_wifi_event_get < BMX_WIFI_EVENT_CAPACITY) {
        bmx_wifi_events[bmx_wifi_event_put & BMX_WIFI_EVENT_MASK] = *event;
        ++bmx_wifi_event_put;
        queued = true;
    } else if (bmx_wifi_dropped != UINT32_MAX) {
        ++bmx_wifi_dropped;
    }
    portEXIT_CRITICAL(&bmx_wifi_lock);
    if (queued) bmx_embedded_system_wake();
    return queued;
}

static uint8_t bmx_wifi_security(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN: return 0;
        case WIFI_AUTH_WEP: return 1;
        case WIFI_AUTH_WPA_PSK: return 2;
        case WIFI_AUTH_WPA2_PSK: return 4;
        case WIFI_AUTH_WPA_WPA2_PSK: return 2 | 4;
        case WIFI_AUTH_WPA3_PSK: return 8;
        case WIFI_AUTH_WPA2_WPA3_PSK: return 4 | 8;
        default: return 0xff;
    }
}

static void bmx_wifi_queue_link(int32_t status, const esp_netif_ip_info_t *info) {
    BMXWiFiEvent event = {0};
    event.kind = BMX_WIFI_LINK_STATE;
    event.link_status = status;
    if (info) {
        event.address = bmx_wifi_pack_ip(info->ip);
        event.netmask = bmx_wifi_pack_ip(info->netmask);
        event.gateway = bmx_wifi_pack_ip(info->gw);
    }
    bmx_wifi_link_status = status;
    bmx_wifi_queue(&event);
}

static int32_t bmx_wifi_disconnect_status(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_NO_AP_FOUND:
        case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
            return BMX_WIFI_LINK_NO_NETWORK;
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_ASSOC_NOT_AUTHED:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
        case WIFI_REASON_IE_IN_4WAY_DIFFERS:
        case WIFI_REASON_802_1X_AUTH_FAILED:
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return BMX_WIFI_LINK_BAD_AUTHENTICATION;
        default:
            return BMX_WIFI_LINK_FAILED;
    }
}

static void bmx_wifi_scan_done(void) {
    uint16_t available = 0;
    if (esp_wifi_scan_get_ap_num(&available) == ESP_OK) {
        for (uint16_t index = 0; index < available; ++index) {
            wifi_ap_record_t record;
            if (esp_wifi_scan_get_ap_record(&record) != ESP_OK) break;
            BMXWiFiEvent event = {0};
            event.kind = BMX_WIFI_SCAN_RESULT;
            event.ssid_length = (uint8_t)strnlen((const char *)record.ssid, sizeof(event.ssid));
            memcpy(event.ssid, record.ssid, event.ssid_length);
            memcpy(event.bssid, record.bssid, sizeof(event.bssid));
            event.channel = record.primary;
            event.rssi = record.rssi;
            event.security = bmx_wifi_security(record.authmode);
            bmx_wifi_queue(&event);
        }
        esp_wifi_clear_ap_list();
    }
    BMXWiFiEvent complete = {0};
    complete.kind = BMX_WIFI_SCAN_COMPLETE;
    bmx_wifi_scan_active = false;
    bmx_wifi_queue(&complete);
}

static void bmx_wifi_handle(void *argument, esp_event_base_t base,
        int32_t id, void *data) {
    (void)argument;
    if (base == WIFI_EVENT) {
        switch (id) {
            case WIFI_EVENT_SCAN_DONE:
                bmx_wifi_scan_done();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                bmx_wifi_queue_link(BMX_WIFI_LINK_JOINED, NULL);
                break;
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *event = data;
                int32_t status = bmx_wifi_explicit_disconnect ? BMX_WIFI_LINK_DOWN :
                    bmx_wifi_disconnect_status(event ? event->reason : 0);
                bmx_wifi_explicit_disconnect = false;
                bmx_wifi_queue_link(status, NULL);
                break;
            }
            case WIFI_EVENT_STA_STOP:
                bmx_wifi_queue_link(BMX_WIFI_LINK_DOWN, NULL);
                break;
        }
    } else if (base == IP_EVENT) {
        if (id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = data;
            bmx_wifi_queue_link(BMX_WIFI_LINK_UP, event ? &event->ip_info : NULL);
        } else if (id == IP_EVENT_STA_LOST_IP) {
            bmx_wifi_queue_link(BMX_WIFI_LINK_NO_IP, NULL);
        }
    }
}

int32_t bmx_embedded_wifi_initialize(uint32_t country) {
    if (bmx_wifi_initialized) return ESP_OK;
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        result = nvs_flash_erase();
        if (result == ESP_OK) result = nvs_flash_init();
    }
    if (result != ESP_OK) return result;
    result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;
    bmx_wifi_netif = esp_netif_create_default_wifi_sta();
    if (!bmx_wifi_netif) return ESP_ERR_NO_MEM;
    bmx_wifi_ap_netif = esp_netif_create_default_wifi_ap();
    if (!bmx_wifi_ap_netif) {
        esp_netif_destroy_default_wifi(bmx_wifi_netif);
        bmx_wifi_netif = NULL;
        return ESP_ERR_NO_MEM;
    }
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&config);
    if (result != ESP_OK) return result;
    result = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        bmx_wifi_handle, NULL, &bmx_wifi_event_handler);
    if (result != ESP_OK) return result;
    result = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
        bmx_wifi_handle, NULL, &bmx_wifi_ip_handler);
    if (result != ESP_OK) return result;
    char code[3] = {(char)(country & 0xffu), (char)((country >> 8) & 0xffu), 0};
    if (code[0] == 'X' && code[1] == 'X') { code[0] = '0'; code[1] = '1'; }
    result = esp_wifi_set_country_code(code, true);
    if (result != ESP_OK) return result;
    result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (result != ESP_OK) return result;
    result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (result != ESP_OK) return result;
    result = esp_wifi_start();
    if (result != ESP_OK) return result;
    bmx_wifi_event_put = bmx_wifi_event_get = bmx_wifi_dropped = 0;
    bmx_wifi_scan_active = false;
    bmx_wifi_explicit_disconnect = false;
    bmx_wifi_ap_active = false;
    bmx_wifi_link_status = BMX_WIFI_LINK_DOWN;
    bmx_wifi_initialized = true;
    return ESP_OK;
}

int32_t bmx_embedded_wifi_deinitialize(void) {
    if (!bmx_wifi_initialized) return ESP_OK;
    if (bmx_wifi_scan_active) return ESP_ERR_INVALID_STATE;
    if (bmx_embedded_net_active_socket_count &&
            bmx_embedded_net_active_socket_count()) return ESP_ERR_INVALID_STATE;
    esp_err_t result = esp_wifi_stop();
    if (result != ESP_OK) return result;
    esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, bmx_wifi_ip_handler);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, bmx_wifi_event_handler);
    result = esp_wifi_deinit();
    if (result != ESP_OK) return result;
    if (bmx_wifi_netif) {
        esp_netif_destroy_default_wifi(bmx_wifi_netif);
        bmx_wifi_netif = NULL;
    }
    if (bmx_wifi_ap_netif) {
        esp_netif_destroy_default_wifi(bmx_wifi_ap_netif);
        bmx_wifi_ap_netif = NULL;
    }
    bmx_wifi_initialized = false;
    bmx_wifi_ap_active = false;
    bmx_wifi_event_put = bmx_wifi_event_get = 0;
    bmx_wifi_link_status = BMX_WIFI_LINK_DOWN;
    return ESP_OK;
}

int32_t bmx_embedded_wifi_initialized(void) { return bmx_wifi_initialized; }

int32_t bmx_embedded_wifi_start_scan(void) {
    if (!bmx_wifi_initialized) return ESP_ERR_INVALID_STATE;
    if (bmx_wifi_scan_active) return ESP_ERR_INVALID_STATE;
    esp_err_t result = esp_wifi_scan_start(NULL, false);
    if (result == ESP_OK) bmx_wifi_scan_active = true;
    return result;
}

static wifi_auth_mode_t bmx_wifi_authentication(uint32_t authentication) {
    switch (authentication) {
        case 0: return WIFI_AUTH_OPEN;
        case 1: return WIFI_AUTH_WPA_PSK;
        case 2: return WIFI_AUTH_WPA2_PSK;
        case 3: return WIFI_AUTH_WPA_WPA2_PSK;
        case 4: return WIFI_AUTH_WPA3_PSK;
        case 5: return WIFI_AUTH_WPA2_WPA3_PSK;
        default: return WIFI_AUTH_MAX;
    }
}

int32_t bmx_embedded_wifi_connect(const uint8_t *ssid, uint32_t ssid_length,
        const uint8_t *password, uint32_t password_length, uint32_t authentication) {
    if (!bmx_wifi_initialized) return ESP_ERR_INVALID_STATE;
    if (!ssid || !ssid_length || ssid_length > 32 || password_length > 64 ||
            (password_length && !password)) return ESP_ERR_INVALID_ARG;
    wifi_auth_mode_t auth = password_length ? bmx_wifi_authentication(authentication) : WIFI_AUTH_OPEN;
    if (auth == WIFI_AUTH_MAX) return ESP_ERR_INVALID_ARG;
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, ssid, ssid_length);
    if (password_length) memcpy(config.sta.password, password, password_length);
    config.sta.threshold.authmode = auth;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
    config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    esp_err_t result = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (result != ESP_OK) return result;
    bmx_wifi_explicit_disconnect = false;
    bmx_wifi_link_status = BMX_WIFI_LINK_DOWN;
    return esp_wifi_connect();
}

int32_t bmx_embedded_wifi_disconnect(void) {
    if (!bmx_wifi_initialized) return ESP_ERR_INVALID_STATE;
    bmx_wifi_explicit_disconnect = true;
    esp_err_t result = esp_wifi_disconnect();
    if (result != ESP_OK) bmx_wifi_explicit_disconnect = false;
    return result;
}

int32_t bmx_embedded_wifi_scan_active(void) { return bmx_wifi_scan_active; }
int32_t bmx_embedded_wifi_link_status(void) { return bmx_wifi_link_status; }
void bmx_embedded_wifi_service(void) {}

int32_t bmx_embedded_wifi_take_event(int32_t *kind, uint8_t *ssid,
        int32_t *ssid_length, uint8_t *bssid, int32_t *channel,
        int32_t *rssi, int32_t *security, int32_t *link_status,
        uint32_t *address, uint32_t *netmask, uint32_t *gateway) {
    if (!kind || !ssid || !ssid_length || !bssid || !channel || !rssi ||
            !security || !link_status || !address || !netmask || !gateway) return 0;
    portENTER_CRITICAL(&bmx_wifi_lock);
    if (bmx_wifi_event_get == bmx_wifi_event_put) {
        portEXIT_CRITICAL(&bmx_wifi_lock);
        return 0;
    }
    BMXWiFiEvent event = bmx_wifi_events[bmx_wifi_event_get & BMX_WIFI_EVENT_MASK];
    ++bmx_wifi_event_get;
    portEXIT_CRITICAL(&bmx_wifi_lock);
    *kind = event.kind;
    *ssid_length = event.ssid_length;
    memcpy(ssid, event.ssid, sizeof(event.ssid));
    memcpy(bssid, event.bssid, sizeof(event.bssid));
    *channel = event.channel;
    *rssi = event.rssi;
    *security = event.security;
    *link_status = event.link_status;
    *address = event.address;
    *netmask = event.netmask;
    *gateway = event.gateway;
    return 1;
}

static uint32_t bmx_wifi_address(int which) {
    if (!bmx_wifi_initialized || !bmx_wifi_netif) return 0;
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(bmx_wifi_netif, &info) != ESP_OK) return 0;
    return bmx_wifi_pack_ip(which == 0 ? info.ip : which == 1 ? info.netmask : info.gw);
}

uint32_t bmx_embedded_wifi_ipv4_address(void) { return bmx_wifi_address(0); }
uint32_t bmx_embedded_wifi_ipv4_netmask(void) { return bmx_wifi_address(1); }
uint32_t bmx_embedded_wifi_ipv4_gateway(void) { return bmx_wifi_address(2); }
uint32_t bmx_embedded_wifi_dropped_events(void) { return bmx_wifi_dropped; }

const BMXEmbeddedString *bmx_esp32_wifi_result_name(int32_t result) {
    const char *name = esp_err_to_name((esp_err_t)result);
    return bmx_embedded_string_from_ascii(name, (int32_t)strlen(name));
}

int32_t bmx_esp32_wifi_set_power_save(int32_t mode) {
    if (!bmx_wifi_initialized) return ESP_ERR_WIFI_NOT_INIT;
    if (mode < WIFI_PS_NONE || mode > WIFI_PS_MAX_MODEM) return ESP_ERR_INVALID_ARG;
    return (int32_t)esp_wifi_set_ps((wifi_ps_type_t)mode);
}

int32_t bmx_esp32_wifi_get_power_save(int32_t *mode) {
    if (!mode) return ESP_ERR_INVALID_ARG;
    wifi_ps_type_t native_mode;
    esp_err_t result = esp_wifi_get_ps(&native_mode);
    if (result == ESP_OK) *mode = (int32_t)native_mode;
    return (int32_t)result;
}

int32_t bmx_esp32_wifi_set_maximum_transmit_power(int32_t power) {
    if (!bmx_wifi_initialized) return ESP_ERR_WIFI_NOT_INIT;
    if (power < INT8_MIN || power > INT8_MAX) return ESP_ERR_INVALID_ARG;
    return (int32_t)esp_wifi_set_max_tx_power((int8_t)power);
}

int32_t bmx_esp32_wifi_get_maximum_transmit_power(int32_t *power) {
    if (!power) return ESP_ERR_INVALID_ARG;
    int8_t native_power;
    esp_err_t result = esp_wifi_get_max_tx_power(&native_power);
    if (result == ESP_OK) *power = native_power;
    return (int32_t)result;
}

int32_t bmx_esp32_wifi_set_station_protocols(uint32_t protocols) {
    if (!bmx_wifi_initialized) return ESP_ERR_WIFI_NOT_INIT;
    if (protocols > UINT8_MAX) return ESP_ERR_INVALID_ARG;
    return (int32_t)esp_wifi_set_protocol(WIFI_IF_STA, (uint8_t)protocols);
}

int32_t bmx_esp32_wifi_get_station_protocols(uint32_t *protocols) {
    if (!protocols) return ESP_ERR_INVALID_ARG;
    uint8_t native_protocols;
    esp_err_t result = esp_wifi_get_protocol(WIFI_IF_STA, &native_protocols);
    if (result == ESP_OK) *protocols = native_protocols;
    return (int32_t)result;
}

int32_t bmx_esp32_wifi_set_station_bandwidth(int32_t bandwidth) {
    if (!bmx_wifi_initialized) return ESP_ERR_WIFI_NOT_INIT;
    if (bandwidth < WIFI_BW20 || bandwidth > WIFI_BW80_BW80) return ESP_ERR_INVALID_ARG;
    return (int32_t)esp_wifi_set_bandwidth(WIFI_IF_STA, (wifi_bandwidth_t)bandwidth);
}

int32_t bmx_esp32_wifi_get_station_bandwidth(int32_t *bandwidth) {
    if (!bandwidth) return ESP_ERR_INVALID_ARG;
    wifi_bandwidth_t native_bandwidth;
    esp_err_t result = esp_wifi_get_bandwidth(WIFI_IF_STA, &native_bandwidth);
    if (result == ESP_OK) *bandwidth = (int32_t)native_bandwidth;
    return (int32_t)result;
}

int32_t bmx_esp32_wifi_get_station_info(uint8_t *ssid, int32_t *ssid_length,
        uint8_t *bssid, int32_t *channel, int32_t *secondary_channel,
        int32_t *rssi, int32_t *security, int32_t *bandwidth,
        uint32_t *protocols) {
    if (!ssid || !ssid_length || !bssid || !channel || !secondary_channel ||
            !rssi || !security || !bandwidth || !protocols) return ESP_ERR_INVALID_ARG;
    wifi_ap_record_t record;
    esp_err_t result = esp_wifi_sta_get_ap_info(&record);
    if (result != ESP_OK) return (int32_t)result;
    uint8_t native_protocols = 0;
    result = esp_wifi_get_protocol(WIFI_IF_STA, &native_protocols);
    if (result != ESP_OK) return (int32_t)result;
    *ssid_length = (int32_t)strnlen((const char *)record.ssid, 32);
    memcpy(ssid, record.ssid, (size_t)*ssid_length);
    memcpy(bssid, record.bssid, 6);
    *channel = record.primary;
    *secondary_channel = (int32_t)record.second;
    *rssi = record.rssi;
    *security = bmx_wifi_security(record.authmode);
    *bandwidth = (int32_t)record.bandwidth;
    *protocols = native_protocols;
    return ESP_OK;
}

int32_t bmx_esp32_wifi_start_access_point(const uint8_t *ssid,
        uint32_t ssid_length, const uint8_t *password, uint32_t password_length,
        uint32_t authentication, uint32_t channel, uint32_t maximum_connections,
        int32_t hidden) {
    if (!bmx_wifi_initialized) return ESP_ERR_WIFI_NOT_INIT;
    if (!ssid || !ssid_length || ssid_length > 32 || password_length > 63 ||
            (password_length && !password) || channel > UINT8_MAX ||
            !maximum_connections || maximum_connections > UINT8_MAX) return ESP_ERR_INVALID_ARG;
    wifi_auth_mode_t auth = bmx_wifi_authentication(authentication);
    if (auth == WIFI_AUTH_MAX || auth == WIFI_AUTH_WEP ||
            (auth == WIFI_AUTH_OPEN && password_length) ||
            (auth != WIFI_AUTH_OPEN && password_length < 8)) return ESP_ERR_INVALID_ARG;
    wifi_config_t config = {0};
    memcpy(config.ap.ssid, ssid, ssid_length);
    config.ap.ssid_len = (uint8_t)ssid_length;
    if (password_length) memcpy(config.ap.password, password, password_length);
    config.ap.channel = (uint8_t)channel;
    config.ap.authmode = auth;
    config.ap.ssid_hidden = hidden ? 1 : 0;
    config.ap.max_connection = (uint8_t)maximum_connections;
    config.ap.beacon_interval = 100;
    config.ap.pmf_cfg.capable = true;
    config.ap.pmf_cfg.required = false;
    config.ap.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    esp_err_t result = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (result == ESP_OK) result = esp_wifi_set_config(WIFI_IF_AP, &config);
    if (result != ESP_OK) {
        esp_wifi_set_mode(WIFI_MODE_STA);
        return (int32_t)result;
    }
    bmx_wifi_ap_active = true;
    return ESP_OK;
}

int32_t bmx_esp32_wifi_stop_access_point(void) {
    if (!bmx_wifi_initialized) return ESP_ERR_WIFI_NOT_INIT;
    if (!bmx_wifi_ap_active) return ESP_OK;
    esp_err_t result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (result == ESP_OK) bmx_wifi_ap_active = false;
    return (int32_t)result;
}

int32_t bmx_esp32_wifi_access_point_active(void) {
    return bmx_wifi_ap_active;
}

int32_t bmx_esp32_wifi_access_point_client_count(uint32_t *count) {
    if (!count) return ESP_ERR_INVALID_ARG;
    if (!bmx_wifi_ap_active) {
        *count = 0;
        return ESP_OK;
    }
    wifi_sta_list_t stations;
    esp_err_t result = esp_wifi_ap_get_sta_list(&stations);
    if (result == ESP_OK) *count = stations.num;
    return (int32_t)result;
}

static uint32_t bmx_wifi_ap_address(int which) {
    if (!bmx_wifi_initialized || !bmx_wifi_ap_active || !bmx_wifi_ap_netif) return 0;
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(bmx_wifi_ap_netif, &info) != ESP_OK) return 0;
    return bmx_wifi_pack_ip(which == 0 ? info.ip : which == 1 ? info.netmask : info.gw);
}

uint32_t bmx_esp32_wifi_access_point_ipv4_address(void) { return bmx_wifi_ap_address(0); }
uint32_t bmx_esp32_wifi_access_point_ipv4_netmask(void) { return bmx_wifi_ap_address(1); }
uint32_t bmx_esp32_wifi_access_point_ipv4_gateway(void) { return bmx_wifi_ap_address(2); }
