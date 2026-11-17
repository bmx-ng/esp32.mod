/* Copyright (c) 2026 Bruce A Henderson and contributors. SPDX-License-Identifier: Zlib */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "esp_zigbee.h"
#include "ezbee/zha.h"
#include "blitzmax/embedded_system.h"

#define ZB_QUEUE_CAPACITY 64u
#define ZB_CHANNEL_MASK 0x07fff800u
#define ZB_EVENT_STARTED 1
#define ZB_EVENT_STARTUP 2
#define ZB_EVENT_FORMED 3
#define ZB_EVENT_JOINED 4
#define ZB_EVENT_PERMIT_JOIN 5
#define ZB_EVENT_DEVICE 6
#define ZB_EVENT_ATTRIBUTE 7
#define ZB_EVENT_READ 8
#define ZB_EVENT_LEFT 9
#define ZB_EVENT_STOPPED 10
#define ZB_EVENT_ERROR 11
#define ZB_EVENT_MATCH 12
#define ZB_EVENT_BOUND 13

typedef struct {
    int32_t kind, status, signal, endpoint, cluster, attribute, value;
    int32_t short_addr, pan_id, channel, factory_new;
} zb_event_t;

typedef struct {
    int32_t role, device, endpoint, auto_commission;
    uint32_t primary_mask, secondary_mask;
} zb_config_t;

static zb_event_t event_queue[ZB_QUEUE_CAPACITY];
static uint32_t queue_write, queue_read, queue_dropped;
static portMUX_TYPE queue_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static int32_t state; /* 0 stopped, 1 starting, 2 running, 3 stopping */
static zb_config_t requested;

static void emit(zb_event_t event)
{
    bool queued = false;
    portENTER_CRITICAL(&queue_lock);
    if (queue_write - queue_read < ZB_QUEUE_CAPACITY) {
        event_queue[queue_write % ZB_QUEUE_CAPACITY] = event;
        ++queue_write;
        queued = true;
    } else if (queue_dropped != UINT32_MAX) {
        ++queue_dropped;
    }
    portEXIT_CRITICAL(&queue_lock);
    if (queued) bmx_embedded_system_wake();
}

static void emit_status(int kind, int status, int signal)
{
    zb_event_t event = {.kind = kind, .status = status, .signal = signal};
    emit(event);
}

static bool signal_handler(const ezb_app_signal_t *signal)
{
    ezb_app_signal_type_t type = ezb_app_signal_get_type(signal);
    const void *params = ezb_app_signal_get_params(signal);
    if (type == EZB_ZDO_SIGNAL_SKIP_STARTUP) {
        ezb_err_t result = ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_INITIALIZATION);
        if (result != EZB_ERR_NONE) emit_status(ZB_EVENT_ERROR, result, type);
    } else if (type == EZB_BDB_SIGNAL_DEVICE_FIRST_START || type == EZB_BDB_SIGNAL_DEVICE_REBOOT) {
        int status = params ? *(const ezb_bdb_comm_status_t *)params : -1;
        zb_event_t event = {.kind = ZB_EVENT_STARTUP, .status = status, .signal = type,
                            .factory_new = ezb_bdb_is_factory_new()};
        emit(event);
        if (status == EZB_BDB_STATUS_SUCCESS && requested.auto_commission && event.factory_new) {
            ezb_bdb_comm_mode_mask_t mode = requested.role == 1 ?
                    EZB_BDB_MODE_NETWORK_FORMATION : EZB_BDB_MODE_NETWORK_STEERING;
            ezb_err_t result = ezb_bdb_start_top_level_commissioning(mode);
            if (result != EZB_ERR_NONE) emit_status(ZB_EVENT_ERROR, result, type);
        }
    } else if (type == EZB_BDB_SIGNAL_FORMATION || type == EZB_BDB_SIGNAL_STEERING) {
        int status = params ? *(const ezb_bdb_comm_status_t *)params : -1;
        zb_event_t event = {.kind = type == EZB_BDB_SIGNAL_FORMATION ? ZB_EVENT_FORMED : ZB_EVENT_JOINED,
                            .status = status, .signal = type, .pan_id = ezb_nwk_get_panid(),
                            .short_addr = ezb_nwk_get_short_address(),
                            .channel = ezb_nwk_get_current_channel()};
        emit(event);
        if (type == EZB_BDB_SIGNAL_FORMATION && status == EZB_BDB_STATUS_SUCCESS && requested.auto_commission)
            (void)ezb_bdb_start_top_level_commissioning(EZB_BDB_MODE_NETWORK_STEERING);
    } else if (type == EZB_NWK_SIGNAL_PERMIT_JOIN_STATUS) {
        zb_event_t event = {.kind = ZB_EVENT_PERMIT_JOIN, .signal = type,
                            .value = params ? *(const uint8_t *)params : 0,
                            .pan_id = ezb_nwk_get_panid()};
        emit(event);
    } else if (type == EZB_ZDO_SIGNAL_DEVICE_ANNCE) {
        const ezb_zdo_signal_device_annce_params_t *device = params;
        zb_event_t event = {.kind = ZB_EVENT_DEVICE, .signal = type,
                            .short_addr = device ? device->short_addr : -1};
        emit(event);
    } else if (type == EZB_ZDO_SIGNAL_LEAVE) {
        emit_status(ZB_EVENT_LEFT, 0, type);
    }
    return true;
}

static void action_handler(ezb_zcl_core_action_callback_id_t action, void *message)
{
    if (action == EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID && message) {
        const ezb_zcl_set_attr_value_message_t *attribute = message;
        zb_event_t event = {.kind = ZB_EVENT_ATTRIBUTE,
                            .status = attribute->info.status,
                            .endpoint = attribute->info.dst_ep,
                            .cluster = attribute->info.cluster_id,
                            .attribute = attribute->in.attribute.id};
        if (attribute->in.attribute.data.value && event.cluster == EZB_ZCL_CLUSTER_ID_ON_OFF &&
                event.attribute == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID)
            event.value = *(const uint8_t *)attribute->in.attribute.data.value;
        emit(event);
    } else if (action == EZB_ZCL_CORE_READ_ATTR_RSP_CB_ID && message) {
        const ezb_zcl_cmd_read_attr_rsp_message_t *response = message;
        for (const ezb_zcl_read_attr_rsp_variable_t *attribute = response->in.variables;
                attribute; attribute = attribute->next) {
            zb_event_t event = {.kind = ZB_EVENT_READ, .status = attribute->status,
                                .endpoint = response->info.dst_ep,
                                .cluster = response->info.cluster_id,
                                .attribute = attribute->attr_id};
            if (attribute->status == EZB_ZCL_STATUS_SUCCESS && attribute->attr_value &&
                    event.cluster == EZB_ZCL_CLUSTER_ID_ON_OFF &&
                    event.attribute == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID)
                event.value = *(const uint8_t *)attribute->attr_value;
            emit(event);
        }
    }
}

static void match_result(const ezb_zdo_match_desc_req_result_t *result, void *context)
{
    (void)context;
    if (!result || !result->rsp) return; /* End of a broadcast search. */
    if (result->error != EZB_ERR_NONE || result->rsp->status != EZB_ZDP_STATUS_SUCCESS) {
        emit_status(ZB_EVENT_ERROR, result->error ? result->error : result->rsp->status, 0);
        return;
    }
    for (size_t i = 0; i < result->rsp->match_length; ++i) {
        zb_event_t event = {.kind = ZB_EVENT_MATCH,
                            .short_addr = result->rsp->nwk_addr_of_interest,
                            .endpoint = result->rsp->match_list[i],
                            .cluster = EZB_ZCL_CLUSTER_ID_ON_OFF};
        emit(event);
    }
}

static void bind_result(const ezb_zdp_bind_req_result_t *result, void *context)
{
    zb_event_t event = {.kind = ZB_EVENT_BOUND, .endpoint = (int)(uintptr_t)context,
                        .cluster = EZB_ZCL_CLUSTER_ID_ON_OFF};
    event.status = !result ? ESP_FAIL : result->error != EZB_ERR_NONE ? result->error :
            !result->rsp ? ESP_FAIL : result->rsp->status;
    emit(event);
}

static esp_err_t register_device(const zb_config_t *options)
{
    ezb_af_device_desc_t device = ezb_af_create_device_desc();
    ezb_af_ep_desc_t endpoint;
    if (options->device == 1) {
        ezb_zha_on_off_light_config_t light = EZB_ZHA_ON_OFF_LIGHT_CONFIG();
        endpoint = ezb_zha_create_on_off_light(options->endpoint, &light);
    } else {
        ezb_zha_on_off_switch_config_t sw = EZB_ZHA_ON_OFF_SWITCH_CONFIG();
        endpoint = ezb_zha_create_on_off_switch(options->endpoint, &sw);
    }
    if (!device || !endpoint) {
        if (endpoint) ezb_af_free_endpoint_desc(endpoint);
        if (device) ezb_af_free_device_desc(device);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t result = ezb_af_device_add_endpoint_desc(device, endpoint);
    if (result != ESP_OK) {
        ezb_af_free_endpoint_desc(endpoint);
        ezb_af_free_device_desc(device);
        return result;
    }
    result = ezb_af_device_desc_register(device);
    if (result != ESP_OK) ezb_af_free_device_desc(device);
    return result;
}

static void stack_task(void *context)
{
    (void)context;
    esp_zigbee_config_t config = {
        .device_config = {
            .device_type = requested.role == 1 ? EZB_NWK_DEVICE_TYPE_COORDINATOR :
                    requested.role == 2 ? EZB_NWK_DEVICE_TYPE_ROUTER : EZB_NWK_DEVICE_TYPE_END_DEVICE,
        },
        .platform_config = {
            .storage_partition_name = "zb_storage",
            .radio_config = {.radio_mode = ESP_ZIGBEE_RADIO_MODE_NATIVE},
        },
    };
    if (requested.role <= 2) config.device_config.zczr_config.max_children = 10;
    else {
        config.device_config.zed_config.ed_timeout = EZB_NWK_ED_TIMEOUT_64MIN;
        config.device_config.zed_config.keep_alive = requested.role == 4 ? 8000 : 4000;
    }
    bool initialized = false;
    esp_err_t result = esp_zigbee_init(&config);
    if (result != ESP_OK) goto failed;
    initialized = true;
    result = ezb_bdb_set_primary_channel_set(requested.primary_mask);
    if (result != ESP_OK) goto failed;
    result = ezb_bdb_set_secondary_channel_set(requested.secondary_mask);
    if (result != ESP_OK) goto failed;
    result = ezb_app_signal_add_handler(signal_handler);
    if (result != ESP_OK) goto failed;
    result = register_device(&requested);
    if (result != ESP_OK) goto failed;
    ezb_zcl_core_action_handler_register(action_handler);
    result = esp_zigbee_start(false);
    if (result != ESP_OK) goto failed;
    portENTER_CRITICAL(&state_lock);
    state = 2;
    portEXIT_CRITICAL(&state_lock);
    emit_status(ZB_EVENT_STARTED, ESP_OK, 0);
    result = esp_zigbee_launch_mainloop();
failed:
    if (result != ESP_OK) emit_status(ZB_EVENT_ERROR, result, 0);
    if (initialized) (void)esp_zigbee_deinit();
    (void)nvs_flash_deinit_partition("zb_storage");
    portENTER_CRITICAL(&state_lock);
    state = 0;
    portEXIT_CRITICAL(&state_lock);
    emit_status(ZB_EVENT_STOPPED, result, 0);
    vTaskDelete(NULL);
}

int bmx_esp32_zigbee_core_start(int role, int device, int endpoint, uint32_t primary_mask,
        uint32_t secondary_mask, int auto_commission)
{
    if (role < 1 || role > 4 || device < 1 || device > 2 || endpoint < 1 || endpoint > 240 ||
            !(primary_mask & ZB_CHANNEL_MASK) || (primary_mask & ~ZB_CHANNEL_MASK) ||
            (secondary_mask & ~ZB_CHANNEL_MASK)) return ESP_ERR_INVALID_ARG;
#if CONFIG_ZB_ZCZR
    if (role > 2) return ESP_ERR_NOT_SUPPORTED;
#else
    if (role < 3) return ESP_ERR_NOT_SUPPORTED;
#endif
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    state = 1;
    requested = (zb_config_t){.role = role, .device = device, .endpoint = endpoint,
            .primary_mask = primary_mask, .secondary_mask = secondary_mask,
            .auto_commission = auto_commission != 0};
    portEXIT_CRITICAL(&state_lock);
    portENTER_CRITICAL(&queue_lock);
    queue_read = queue_write = queue_dropped = 0;
    portEXIT_CRITICAL(&queue_lock);
    esp_err_t result = nvs_flash_init_partition("zb_storage");
    if (result != ESP_OK) goto failed;
    if (xTaskCreate(stack_task, "zigbee_stack", 6144, NULL, 5, NULL) != pdPASS) {
        result = ESP_ERR_NO_MEM;
        (void)nvs_flash_deinit_partition("zb_storage");
        goto failed;
    }
    return ESP_OK;
failed:
    portENTER_CRITICAL(&state_lock);
    state = 0;
    portEXIT_CRITICAL(&state_lock);
    return result;
}

int bmx_esp32_zigbee_core_state(void)
{
    portENTER_CRITICAL(&state_lock);
    int current = state;
    portEXIT_CRITICAL(&state_lock);
    return current;
}

int bmx_esp32_zigbee_core_stop(void)
{
    portENTER_CRITICAL(&state_lock);
    if (state != 2) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    state = 3;
    portEXIT_CRITICAL(&state_lock);
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) {
        portENTER_CRITICAL(&state_lock);
        if (state == 3) state = 2;
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t result = esp_zigbee_stop();
    esp_zigbee_lock_release();
    if (result != ESP_OK) {
        portENTER_CRITICAL(&state_lock);
        if (state == 3) state = 2;
        portEXIT_CRITICAL(&state_lock);
    }
    return result;
}

static int commission(ezb_bdb_comm_mode_mask_t mode)
{
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_bdb_start_top_level_commissioning(mode);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_form(void) { return commission(EZB_BDB_MODE_NETWORK_FORMATION); }
int bmx_esp32_zigbee_core_join(void) { return commission(EZB_BDB_MODE_NETWORK_STEERING); }

int bmx_esp32_zigbee_core_permit_join(int seconds)
{
    if (seconds < 0 || seconds > 254) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = seconds ? ezb_bdb_open_network((uint8_t)seconds) : ezb_bdb_close_network();
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_reset(void)
{
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_bdb_reset_via_local_action();
    esp_zigbee_lock_release();
    return ESP_OK;
}

int bmx_esp32_zigbee_core_network(int *joined, int *factory_new, int *pan_id,
        int *short_addr, int *channel, uint64_t *extended_pan)
{
    if (!joined || !factory_new || !pan_id || !short_addr || !channel || !extended_pan)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_extpanid_t extended;
    ezb_nwk_get_extended_panid(&extended);
    *joined = ezb_bdb_dev_joined();
    *factory_new = ezb_bdb_is_factory_new();
    *pan_id = ezb_nwk_get_panid();
    *short_addr = ezb_nwk_get_short_address();
    *channel = ezb_nwk_get_current_channel();
    *extended_pan = extended.u64;
    esp_zigbee_lock_release();
    return ESP_OK;
}

int bmx_esp32_zigbee_core_onoff(int source_ep, int destination_addr, int destination_ep, int command)
{
    if (source_ep < 1 || source_ep > 240 || destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240 || command < 0 || command > 2)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_on_off_cmd_t request = {.cmd_ctrl = {
        .dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
        .dst_ep = destination_ep, .src_ep = source_ep,
    }};
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = command == 0 ? ezb_zcl_on_off_off_cmd_req(&request) :
            command == 1 ? ezb_zcl_on_off_on_cmd_req(&request) :
            ezb_zcl_on_off_toggle_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_read_onoff(int source_ep, int destination_addr, int destination_ep)
{
    if (source_ep < 1 || source_ep > 240 || destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint16_t attribute_id = EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
    ezb_zcl_read_attr_cmd_t request = {.cmd_ctrl = {
        .dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
        .dst_ep = destination_ep, .src_ep = source_ep,
        .cluster_id = EZB_ZCL_CLUSTER_ID_ON_OFF,
    }, .payload = {.attr_number = 1, .attr_field = &attribute_id}};
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_read_attr_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_find_onoff(void)
{
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint16_t cluster = EZB_ZCL_CLUSTER_ID_ON_OFF;
    ezb_zdo_match_desc_req_t request = {
        .dst_nwk_addr = 0xfffd,
        .field = {.nwk_addr_of_interest = 0xfffd,
                  .profile_id = EZB_AF_HA_PROFILE_ID,
                  .num_in_clusters = 1, .cluster_list = &cluster},
        .cb = match_result,
    };
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zdo_match_desc_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_bind_onoff(int source_ep, int destination_addr, int destination_ep)
{
    if (source_ep != requested.endpoint || requested.device != 2 ||
            destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zdo_bind_req_t request = {
        .field = {.src_ep = source_ep, .cluster_id = EZB_ZCL_CLUSTER_ID_ON_OFF,
                  .dst_addr_mode = EZB_ADDR_MODE_EXT, .dst_ep = destination_ep},
        .cb = bind_result, .user_ctx = (void *)(uintptr_t)source_ep,
    };
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    request.dst_nwk_addr = ezb_nwk_get_short_address();
    ezb_nwk_get_extended_address(&request.field.src_addr);
    ezb_err_t result = ezb_address_extended_by_short((uint16_t)destination_addr,
            &request.field.dst_addr.extended_addr);
    if (result == EZB_ERR_NONE) result = ezb_zdo_bind_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_bound_onoff(int source_ep, int command)
{
    if (source_ep != requested.endpoint || requested.device != 2 || command < 0 || command > 2)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_on_off_cmd_t request = {.cmd_ctrl = {
        .dst_addr.addr_mode = EZB_ADDR_MODE_NONE, .src_ep = source_ep,
    }};
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = command == 0 ? ezb_zcl_on_off_off_cmd_req(&request) :
            command == 1 ? ezb_zcl_on_off_on_cmd_req(&request) :
            ezb_zcl_on_off_toggle_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_poll(int *kind, int *status, int *signal, int *endpoint,
        int *cluster, int *attribute, int *value, int *short_addr, int *pan_id,
        int *channel, int *factory_new)
{
    if (!kind || !status || !signal || !endpoint || !cluster || !attribute || !value ||
            !short_addr || !pan_id || !channel || !factory_new) return 0;
    portENTER_CRITICAL(&queue_lock);
    if (queue_read == queue_write) {
        portEXIT_CRITICAL(&queue_lock);
        return 0;
    }
    zb_event_t event = event_queue[queue_read % ZB_QUEUE_CAPACITY];
    ++queue_read;
    portEXIT_CRITICAL(&queue_lock);
    *kind = event.kind; *status = event.status; *signal = event.signal;
    *endpoint = event.endpoint; *cluster = event.cluster;
    *attribute = event.attribute; *value = event.value;
    *short_addr = event.short_addr; *pan_id = event.pan_id;
    *channel = event.channel; *factory_new = event.factory_new;
    return 1;
}

uint32_t bmx_esp32_zigbee_core_dropped(void)
{
    portENTER_CRITICAL(&queue_lock);
    uint32_t dropped = queue_dropped;
    portEXIT_CRITICAL(&queue_lock);
    return dropped;
}
