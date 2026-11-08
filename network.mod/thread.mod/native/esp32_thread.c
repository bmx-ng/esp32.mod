/* Copyright (c) 2026 Bruce A Henderson and contributors
 * SPDX-License-Identifier: Zlib */
#include "sdkconfig.h"
#if !CONFIG_IDF_TARGET_ESP32H2 && !CONFIG_IDF_TARGET_ESP32C6 && !CONFIG_IDF_TARGET_ESP32S31
#error "ESP32.Network.Thread requires an ESP32-H2, ESP32-C6, or ESP32-S31 board profile"
#endif

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_openthread.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "esp_vfs_eventfd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static SemaphoreHandle_t bmx_thread_ready;
static volatile int bmx_thread_status = -1;

int bmx_thread_platform_is_initialized(void) {
    return bmx_thread_status == 0;
}

static void bmx_thread_task(void *argument) {
    (void)argument;
    esp_openthread_platform_config_t platform = {
        .radio_config = {.radio_mode = RADIO_MODE_NATIVE},
        .host_config = {.host_connection_mode = HOST_CONNECTION_MODE_NONE},
        .port_config = {
            .storage_partition_name = "nvs",
            .netif_queue_size = 10,
            .task_queue_size = 10,
        },
    };
    esp_netif_inherent_config_t inherent = ESP_NETIF_INHERENT_DEFAULT_OPENTHREAD();
    esp_netif_config_t config = {
        .base = &inherent,
        .stack = &g_esp_netif_netstack_default_openthread,
    };
    esp_netif_t *netif = esp_netif_new(&config);
    if (netif) {
        esp_err_t error = esp_openthread_init(&platform);
        if (error == ESP_OK) {
            error = esp_netif_attach(netif, esp_openthread_netif_glue_init(&platform));
        }
        if (error == ESP_OK) bmx_thread_status = 0;
    }
    xSemaphoreGive(bmx_thread_ready);
    if (bmx_thread_status == 0) esp_openthread_launch_mainloop();
    vTaskDelete(NULL);
}

int bmx_thread_platform_initialize(void) {
    if (bmx_thread_ready) return bmx_thread_status;
    esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) return -(int)error;
    error = esp_netif_init();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) return -(int)error;
    error = esp_event_loop_create_default();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) return -(int)error;
    esp_vfs_eventfd_config_t eventfd = {.max_fds = 4};
    error = esp_vfs_eventfd_register(&eventfd);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) return -(int)error;
    bmx_thread_ready = xSemaphoreCreateBinary();
    if (!bmx_thread_ready) return -12;
    if (xTaskCreate(bmx_thread_task, "bmx_thread", 10240, NULL, 5, NULL) != pdPASS) {
        vSemaphoreDelete(bmx_thread_ready);
        bmx_thread_ready = NULL;
        return -12;
    }
    if (xSemaphoreTake(bmx_thread_ready, pdMS_TO_TICKS(10000)) != pdTRUE) return -110;
    return bmx_thread_status;
}

otInstance *bmx_thread_platform_instance(void) {
    return esp_openthread_get_instance();
}

void bmx_thread_platform_lock(void) {
    esp_openthread_lock_acquire(portMAX_DELAY);
}

void bmx_thread_platform_unlock(void) {
    esp_openthread_lock_release();
}
