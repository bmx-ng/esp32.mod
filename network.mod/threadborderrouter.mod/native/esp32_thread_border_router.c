/* Copyright (c) 2026 Bruce A Henderson and contributors
 * SPDX-License-Identifier: Zlib */
#include "sdkconfig.h"
#if !CONFIG_OPENTHREAD_BORDER_ROUTER
#error "ESP32.Network.ThreadBorderRouter requires CONFIG_OPENTHREAD_BORDER_ROUTER"
#endif

#include "esp_err.h"
#include "esp_netif.h"
#include "esp_openthread.h"
#include "esp_openthread_border_router.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "openthread/border_routing.h"
#include <stdio.h>
#include <string.h>

extern void bmx_thread_platform_lock(void);
extern void bmx_thread_platform_unlock(void);
extern int bmx_thread_platform_is_initialized(void);
extern otInstance *bmx_thread_platform_instance(void);

static esp_netif_t *bmx_backbone;
static volatile int bmx_running;
static volatile int bmx_address_task_running;
static int bmx_address_owned;
static esp_ip6_addr_t bmx_owned_address;

static int bmx_has_address(const esp_ip6_addr_t *address) {
    esp_ip6_addr_t addresses[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
    int count = esp_netif_get_all_ip6(bmx_backbone, addresses);
    for (int i = 0; i < count; ++i) {
        if (memcmp(&addresses[i], address, sizeof(*address)) == 0) return 1;
    }
    return 0;
}

static int bmx_has_address_in_prefix(const otIp6Prefix *prefix) {
    esp_ip6_addr_t addresses[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
    int count = esp_netif_get_all_ip6(bmx_backbone, addresses);
    for (int i = 0; i < count; ++i) {
        if (memcmp(addresses[i].addr, prefix->mPrefix.mFields.m8, 8) == 0) return 1;
    }
    return 0;
}

static void bmx_remove_owned_address(void) {
    if (!bmx_address_owned) return;
    esp_netif_remove_ip6_address(bmx_backbone, &bmx_owned_address);
    bmx_address_owned = 0;
}

static void bmx_sync_backbone_address(void) {
    otIp6Prefix prefix = {0};
    if (esp_netif_is_netif_up(bmx_backbone)) {
        bmx_thread_platform_lock();
        otError error = otBorderRoutingGetFavoredOnLinkPrefix(bmx_thread_platform_instance(), &prefix);
        bmx_thread_platform_unlock();
        if (error != OT_ERROR_NONE) return;
    }
    if (prefix.mLength != 64) {
        bmx_remove_owned_address();
        return;
    }

    uint8_t mac[6];
    if (esp_netif_get_mac(bmx_backbone, mac) != ESP_OK) return;
    esp_ip6_addr_t address = {0};
    uint8_t *bytes = (uint8_t *)address.addr;
    memcpy(bytes, prefix.mPrefix.mFields.m8, 8);
    bytes[8] = mac[0] ^ 0x02;
    bytes[9] = mac[1];
    bytes[10] = mac[2];
    bytes[11] = 0xff;
    bytes[12] = 0xfe;
    bytes[13] = mac[3];
    bytes[14] = mac[4];
    bytes[15] = mac[5];

    if (bmx_address_owned && memcmp(&address, &bmx_owned_address, sizeof(address)) == 0 &&
            bmx_has_address(&address)) return;
    bmx_remove_owned_address();
    if (bmx_has_address_in_prefix(&prefix)) return;
    if (esp_netif_add_ip6_address(bmx_backbone, address, true) == ESP_OK) {
        bmx_owned_address = address;
        bmx_address_owned = 1;
    }
}

static void bmx_address_task(void *context) {
    (void)context;
    while (bmx_running) {
        bmx_sync_backbone_address();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    bmx_address_task_running = 0;
    vTaskDelete(NULL);
}

static int bmx_use_backbone(const char *if_key) {
    if (bmx_thread_platform_is_initialized()) return ESP_ERR_INVALID_STATE;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey(if_key);
    if (!netif) return ESP_ERR_INVALID_STATE;
    esp_err_t error = esp_netif_create_ip6_linklocal(netif);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) return error;
    error = mdns_init();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) return error;
    error = mdns_hostname_set("bmx-thread-br");
    if (error != ESP_OK) return error;
    esp_openthread_set_backbone_netif(netif);
    bmx_backbone = netif;
    return ESP_OK;
}

int bmx_esp32_thread_border_router_use_wifi(void) {
    return bmx_use_backbone("WIFI_STA_DEF");
}

int bmx_esp32_thread_border_router_use_ethernet(void) {
    return bmx_use_backbone("ETH_DEF");
}

int bmx_esp32_thread_border_router_start(void) {
    if (!bmx_backbone || !bmx_thread_platform_is_initialized()) return ESP_ERR_INVALID_STATE;
    if (bmx_running) return ESP_ERR_INVALID_STATE;
    bmx_thread_platform_lock();
    esp_err_t error = esp_openthread_border_router_init();
    bmx_thread_platform_unlock();
    if (error == ESP_OK) {
        bmx_running = 1;
        bmx_address_task_running = 1;
        if (xTaskCreate(bmx_address_task, "bmx_br_ip6", 4096, NULL, 4, NULL) != pdPASS) {
            bmx_address_task_running = bmx_running = 0;
            bmx_thread_platform_lock();
            esp_openthread_border_router_deinit();
            bmx_thread_platform_unlock();
            error = ESP_ERR_NO_MEM;
        }
    }
    return error;
}

int bmx_esp32_thread_border_router_stop(void) {
    if (!bmx_running) return ESP_ERR_INVALID_STATE;
    bmx_thread_platform_lock();
    esp_err_t error = esp_openthread_border_router_deinit();
    bmx_thread_platform_unlock();
    if (error == ESP_OK) {
        bmx_running = 0;
        while (bmx_address_task_running) vTaskDelay(pdMS_TO_TICKS(10));
        bmx_remove_owned_address();
    }
    return error;
}

int bmx_esp32_thread_border_router_running(void) {
    return bmx_running;
}

int bmx_esp32_thread_border_router_backbone_ipv6_address(int index, char *value, int capacity) {
    if (!bmx_backbone || index < 0 || !value || capacity < 1) return -1;
    esp_ip6_addr_t addresses[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
    int count = esp_netif_get_all_ip6(bmx_backbone, addresses);
    if (index >= count) return 0;
    int length = snprintf(value, (size_t)capacity, IPV6STR, IPV62STR(addresses[index]));
    return length >= 0 && length < capacity ? length : -1;
}
