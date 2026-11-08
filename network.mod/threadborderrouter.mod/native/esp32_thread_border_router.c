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
#include "mdns.h"
#include <stdio.h>

extern void bmx_thread_platform_lock(void);
extern void bmx_thread_platform_unlock(void);
extern int bmx_thread_platform_is_initialized(void);

static esp_netif_t *bmx_backbone;
static int bmx_running;

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
    if (error == ESP_OK) bmx_running = 1;
    return error;
}

int bmx_esp32_thread_border_router_stop(void) {
    if (!bmx_running) return ESP_ERR_INVALID_STATE;
    bmx_thread_platform_lock();
    esp_err_t error = esp_openthread_border_router_deinit();
    bmx_thread_platform_unlock();
    if (error == ESP_OK) bmx_running = 0;
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
