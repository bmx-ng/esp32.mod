/* Copyright (c) 2026 Bruce A Henderson and contributors. SPDX-License-Identifier: Zlib */
/* Initial radio bring-up follows Espressif's v2 on_off_light/on_off_switch examples. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_zigbee.h"
#include "esp_zigbee_version.h"
#include "ezbee/zha.h"

#define LIGHT_EP 10
#define SWITCH_EP 1
#define PRIMARY_CHANNEL_MASK (1U << 15)
static const char *TAG = "BMX_ZIGBEE_BRINGUP";
static bool started;
static volatile bool joined;
static volatile bool bound;
static bool light_on;
static uint16_t light_short_addr;
static uint8_t light_endpoint;

int bmx_esp32_zigbee_sdk_version(void) { return ESP_ZIGBEE_VER_MAJOR; }

static void bind_result(const ezb_zdp_bind_req_result_t *result, void *context)
{
    (void)context;
    bound = result && result->error == EZB_ERR_NONE && result->rsp &&
            result->rsp->status == EZB_ZDP_STATUS_SUCCESS;
    ESP_LOGI(TAG, "Switch binding %s", bound ? "complete" : "failed");
}

static void bind_light(uint16_t short_addr, uint8_t endpoint)
{
    ezb_zdo_bind_req_t request = {
        .dst_nwk_addr = ezb_nwk_get_short_address(),
        .field = {
            .src_ep = SWITCH_EP,
            .cluster_id = EZB_ZCL_CLUSTER_ID_ON_OFF,
            .dst_addr_mode = EZB_ADDR_MODE_EXT,
            .dst_ep = endpoint,
        },
        .cb = bind_result,
    };
    ezb_nwk_get_extended_address(&request.field.src_addr);
    if (ezb_address_extended_by_short(short_addr, &request.field.dst_addr.extended_addr) == EZB_ERR_NONE) {
        light_short_addr = short_addr;
        light_endpoint = endpoint;
        ESP_LOGI(TAG, "Binding light at 0x%04x endpoint %u", short_addr, endpoint);
        (void)ezb_zdo_bind_req(&request);
    }
}

static void match_result(const ezb_zdo_match_desc_req_result_t *result, void *context)
{
    (void)context;
    if (!result || result->error != EZB_ERR_NONE || !result->rsp ||
            result->rsp->status != EZB_ZDP_STATUS_SUCCESS) return;
    for (size_t index = 0; index < result->rsp->match_length; ++index) {
        bind_light(result->rsp->nwk_addr_of_interest, result->rsp->match_list[index]);
    }
}

static void find_light(void)
{
    uint16_t clusters[] = {EZB_ZCL_CLUSTER_ID_ON_OFF};
    ezb_zdo_match_desc_req_t request = {
        .dst_nwk_addr = 0xfffd,
        .field = {
            .nwk_addr_of_interest = 0xfffd,
            .profile_id = EZB_AF_HA_PROFILE_ID,
            .num_in_clusters = 1,
            .cluster_list = clusters,
        },
        .cb = match_result,
    };
    ESP_LOGI(TAG, "Discovering on/off light");
    (void)ezb_zdo_match_desc_req(&request);
}

static bool signal_handler(const ezb_app_signal_t *signal)
{
    ezb_app_signal_type_t type = ezb_app_signal_get_type(signal);
    const void *params = ezb_app_signal_get_params(signal);
    if (type == EZB_ZDO_SIGNAL_SKIP_STARTUP) {
        ESP_LOGI(TAG, "Initializing Zigbee stack");
        (void)ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
    } else if (type == EZB_BDB_SIGNAL_DEVICE_FIRST_START || type == EZB_BDB_SIGNAL_DEVICE_REBOOT) {
        int status = params ? *(const ezb_bdb_comm_status_t *)params : -1;
        ESP_LOGI(TAG, "Startup status=%d factory_new=%d", status, ezb_bdb_is_factory_new());
        if (status == EZB_BDB_STATUS_SUCCESS) {
#ifdef CONFIG_IDF_TARGET_ESP32H2
            if (ezb_bdb_is_factory_new())
                (void)ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_FORMATION);
            else
                (void)ezb_bdb_open_network(180);
#else
            if (ezb_bdb_is_factory_new())
                (void)ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
            else {
                joined = true;
                find_light();
            }
#endif
        }
    } else if (type == EZB_BDB_SIGNAL_FORMATION || type == EZB_BDB_SIGNAL_STEERING) {
        int status = params ? *(const ezb_bdb_comm_status_t *)params : -1;
        ESP_LOGI(TAG, "Commissioning type=%d status=%d pan=0x%04x channel=%u short=0x%04x",
                 type, status, ezb_nwk_get_panid(), ezb_nwk_get_current_channel(),
                 ezb_nwk_get_short_address());
        if (status == EZB_BDB_STATUS_SUCCESS) {
#ifdef CONFIG_IDF_TARGET_ESP32H2
            if (type == EZB_BDB_SIGNAL_FORMATION)
                (void)ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
#else
            if (type == EZB_BDB_SIGNAL_STEERING) { joined = true; find_light(); }
#endif
        }
    } else if (type == EZB_ZDO_SIGNAL_DEVICE_ANNCE) {
        ESP_LOGI(TAG, "Device announced");
    } else if (type == EZB_NWK_SIGNAL_PERMIT_JOIN_STATUS) {
        ESP_LOGI(TAG, "Permit joining: %u seconds", params ? *(const uint8_t *)params : 0);
    }
    return true;
}

static void action_handler(ezb_zcl_core_action_callback_id_t action, void *message)
{
#ifdef CONFIG_IDF_TARGET_ESP32H2
    if (action == EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID && message) {
        ezb_zcl_set_attr_value_message_t *value = message;
        if (value->info.cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF && value->in.attribute.data.value) {
            light_on = *(const uint8_t *)value->in.attribute.data.value != 0;
            ESP_LOGI(TAG, "Light On/Off=%d", light_on);
        }
    }
#else
    if (action == EZB_ZCL_CORE_READ_ATTR_RSP_CB_ID && message) {
        ezb_zcl_cmd_read_attr_rsp_message_t *response = message;
        ezb_zcl_read_attr_rsp_variable_t *value = response->in.variables;
        if (value && value->attr_id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
                value->status == EZB_ZCL_STATUS_SUCCESS && value->attr_value)
            ESP_LOGI(TAG, "Read light On/Off=%u", *(const uint8_t *)value->attr_value);
        else
            ESP_LOGW(TAG, "Read light On/Off failed");
    }
#endif
}

static void stack_task(void *context)
{
    (void)context;
    esp_zigbee_config_t config = {
        .device_config = {
#ifdef CONFIG_IDF_TARGET_ESP32H2
            .device_type = EZB_NWK_DEVICE_TYPE_COORDINATOR,
            .zczr_config = {.max_children = 10},
#else
            .device_type = EZB_NWK_DEVICE_TYPE_END_DEVICE,
            .zed_config = {.ed_timeout = EZB_NWK_ED_TIMEOUT_64MIN, .keep_alive = 4000},
#endif
        },
        .platform_config = {
            .storage_partition_name = "zb_storage",
            .radio_config = {.radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE},
        },
    };
    ESP_ERROR_CHECK(esp_zigbee_init(&config));
    ESP_ERROR_CHECK(ezb_bdb_set_primary_channel_set(PRIMARY_CHANNEL_MASK));
    ESP_ERROR_CHECK(ezb_bdb_set_secondary_channel_set(0));
    ESP_ERROR_CHECK(ezb_app_signal_add_handler(signal_handler));
    ezb_af_device_desc_t device = ezb_af_create_device_desc();
#ifdef CONFIG_IDF_TARGET_ESP32H2
    ezb_zha_on_off_light_config_t device_config = EZB_ZHA_ON_OFF_LIGHT_CONFIG();
    ezb_af_ep_desc_t endpoint = ezb_zha_create_on_off_light(LIGHT_EP, &device_config);
#else
    ezb_zha_on_off_switch_config_t device_config = EZB_ZHA_ON_OFF_SWITCH_CONFIG();
    ezb_af_ep_desc_t endpoint = ezb_zha_create_on_off_switch(SWITCH_EP, &device_config);
#endif
    ESP_ERROR_CHECK(ezb_af_device_add_endpoint_desc(device, endpoint));
    ESP_ERROR_CHECK(ezb_af_device_desc_register(device));
    ezb_zcl_core_action_handler_register(action_handler);
    ESP_ERROR_CHECK(esp_zigbee_start(false));
    ESP_LOGI(TAG, "Zigbee v2 stack launched");
    esp_zigbee_launch_mainloop();
    esp_zigbee_deinit();
    vTaskDelete(NULL);
}

int bmx_esp32_zigbee_bringup_start(void)
{
    if (started) return ESP_ERR_INVALID_STATE;
    esp_err_t result = nvs_flash_init_partition("zb_storage");
    if (result != ESP_OK) return result;
    if (xTaskCreate(stack_task, "zigbee_stack", 6144, NULL, 5, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    started = true;
    return ESP_OK;
}

int bmx_esp32_zigbee_bringup_toggle(void)
{
#ifdef CONFIG_IDF_TARGET_ESP32H2
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!joined || !bound) return ESP_ERR_INVALID_STATE;
    ezb_zcl_on_off_cmd_t command = {
        .cmd_ctrl = {
            .dst_addr.addr_mode = EZB_ADDR_MODE_NONE,
            .src_ep = SWITCH_EP,
        },
    };
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_on_off_toggle_cmd_req(&command);
    esp_zigbee_lock_release();
    ESP_LOGI(TAG, "Toggle request result=%u", result);
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
#endif
}

int bmx_esp32_zigbee_bringup_read(void)
{
#ifdef CONFIG_IDF_TARGET_ESP32H2
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!joined || !bound) return ESP_ERR_INVALID_STATE;
    uint16_t attribute = EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
    ezb_zcl_read_attr_cmd_t command = {
        .cmd_ctrl = {
            .dst_addr = EZB_ADDRESS_SHORT(light_short_addr),
            .dst_ep = light_endpoint,
            .src_ep = SWITCH_EP,
            .cluster_id = EZB_ZCL_CLUSTER_ID_ON_OFF,
        },
        .payload = {.attr_number = 1, .attr_field = &attribute},
    };
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_read_attr_cmd_req(&command);
    esp_zigbee_lock_release();
    ESP_LOGI(TAG, "Read request result=%u", result);
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
#endif
}
