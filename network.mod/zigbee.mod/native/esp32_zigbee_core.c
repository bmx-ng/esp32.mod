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
#include "ezbee/zcl/cluster/custom.h"
#include "blitzmax/embedded_system.h"

#define ZB_QUEUE_CAPACITY 64u
#define ZB_MAX_EXTRA_ENDPOINTS 8u
#define ZB_MAX_CUSTOM_CLUSTERS 8u
#define ZB_MAX_CUSTOM_ATTRIBUTES 32u
#define ZB_MAX_HUMIDITY_CLUSTERS 18u
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
#define ZB_EVENT_REPORT_CONFIGURED 14
#define ZB_EVENT_REPORT 15
#define ZB_EVENT_GROUP_ADDED 16
#define ZB_EVENT_GROUP_REMOVED 17
#define ZB_EVENT_WRITE 18

typedef struct {
    int32_t kind, status, signal, endpoint, cluster, attribute, value;
    int32_t short_addr, pan_id, channel, factory_new;
    int32_t data_type, value_size;
} zb_event_t;

typedef struct {
    int32_t role, device, endpoint, auto_commission;
    uint32_t primary_mask, secondary_mask;
} zb_config_t;

typedef struct {
    uint8_t device, endpoint;
} zb_endpoint_config_t;

typedef struct {
    uint8_t endpoint, role;
    uint16_t cluster;
} zb_custom_cluster_t;

typedef struct {
    uint8_t endpoint, role;
} zb_humidity_cluster_t;

typedef struct {
    uint8_t endpoint, type, access;
    uint16_t cluster, attribute;
    int32_t value;
} zb_custom_attribute_t;

static zb_event_t event_queue[ZB_QUEUE_CAPACITY];
static uint32_t queue_write, queue_read, queue_dropped;
static portMUX_TYPE queue_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static int32_t state; /* 0 stopped, 1 starting, 2 running, 3 stopping */
static zb_config_t requested;
static zb_endpoint_config_t extra_endpoints[ZB_MAX_EXTRA_ENDPOINTS];
static uint8_t extra_endpoint_count;
static zb_custom_cluster_t custom_clusters[ZB_MAX_CUSTOM_CLUSTERS];
static uint8_t custom_cluster_count;
static zb_custom_attribute_t custom_attributes[ZB_MAX_CUSTOM_ATTRIBUTES];
static uint8_t custom_attribute_count;
static zb_humidity_cluster_t humidity_clusters[ZB_MAX_HUMIDITY_CLUSTERS];
static uint8_t humidity_cluster_count;

static int endpoint_kind(int endpoint)
{
    if (endpoint == requested.endpoint) return requested.device;
    for (unsigned i = 0; i < extra_endpoint_count; ++i)
        if (endpoint == extra_endpoints[i].endpoint) return extra_endpoints[i].device;
    return 0;
}

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

static void copy_scalar(zb_event_t *event, int type, const void *data)
{
    event->data_type = type;
    if (!data) return;
    switch (type) {
        case EZB_ZCL_ATTR_TYPE_BOOL:
        case EZB_ZCL_ATTR_TYPE_DATA8:
        case EZB_ZCL_ATTR_TYPE_MAP8:
        case EZB_ZCL_ATTR_TYPE_UINT8:
        case EZB_ZCL_ATTR_TYPE_ENUM8:
            event->value = *(const uint8_t *)data;
            event->value_size = 1;
            break;
        case EZB_ZCL_ATTR_TYPE_INT8:
            event->value = *(const int8_t *)data;
            event->value_size = 1;
            break;
        case EZB_ZCL_ATTR_TYPE_DATA16:
        case EZB_ZCL_ATTR_TYPE_MAP16:
        case EZB_ZCL_ATTR_TYPE_UINT16:
        case EZB_ZCL_ATTR_TYPE_ENUM16: {
            uint16_t value;
            memcpy(&value, data, sizeof(value));
            event->value = value;
            event->value_size = 2;
            break;
        }
        case EZB_ZCL_ATTR_TYPE_INT16: {
            int16_t value;
            memcpy(&value, data, sizeof(value));
            event->value = value;
            event->value_size = 2;
            break;
        }
        case EZB_ZCL_ATTR_TYPE_DATA32:
        case EZB_ZCL_ATTR_TYPE_MAP32:
        case EZB_ZCL_ATTR_TYPE_UINT32: {
            uint32_t value;
            memcpy(&value, data, sizeof(value));
            event->value = (int32_t)value;
            event->value_size = 4;
            break;
        }
        case EZB_ZCL_ATTR_TYPE_INT32:
            memcpy(&event->value, data, sizeof(event->value));
            event->value_size = 4;
            break;
        default:
            break;
    }
}

static int scalar_size(int type)
{
    zb_event_t event = {0};
    uint32_t zero = 0;
    copy_scalar(&event, type, &zero);
    return event.value_size;
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
        copy_scalar(&event, attribute->in.attribute.data.type,
                attribute->in.attribute.data.value);
        emit(event);
    } else if (action == EZB_ZCL_CORE_READ_ATTR_RSP_CB_ID && message) {
        const ezb_zcl_cmd_read_attr_rsp_message_t *response = message;
        for (const ezb_zcl_read_attr_rsp_variable_t *attribute = response->in.variables;
                attribute; attribute = attribute->next) {
            zb_event_t event = {.kind = ZB_EVENT_READ, .status = attribute->status,
                                .endpoint = response->info.dst_ep,
                                .cluster = response->info.cluster_id,
                                .attribute = attribute->attr_id};
            if (attribute->status == EZB_ZCL_STATUS_SUCCESS)
                copy_scalar(&event, attribute->attr_type, attribute->attr_value);
            emit(event);
        }
    } else if (action == EZB_ZCL_CORE_WRITE_ATTR_RSP_CB_ID && message) {
        const ezb_zcl_cmd_write_attr_rsp_message_t *response = message;
        for (const ezb_zcl_write_attr_rsp_variable_t *attribute = response->in.variables;
                attribute; attribute = attribute->next) {
            zb_event_t event = {.kind = ZB_EVENT_WRITE, .status = attribute->status,
                                .endpoint = response->info.dst_ep,
                                .cluster = response->info.cluster_id,
                                .attribute = attribute->attr_id};
            emit(event);
        }
    } else if (action == EZB_ZCL_CORE_CONFIG_REPORT_RSP_CB_ID && message) {
        const ezb_zcl_cmd_config_report_rsp_message_t *response = message;
        for (const ezb_zcl_config_report_rsp_variable_t *record = response->in.variables;
                record; record = record->next) {
            zb_event_t event = {.kind = ZB_EVENT_REPORT_CONFIGURED,
                                .status = record->status,
                                .endpoint = response->info.dst_ep,
                                .cluster = response->info.cluster_id,
                                .attribute = record->attr_id,
                                .value = record->direction};
            emit(event);
        }
    } else if (action == EZB_ZCL_CORE_REPORT_ATTR_CB_ID && message) {
        const ezb_zcl_cmd_report_attr_message_t *report = message;
        for (const ezb_zcl_report_attr_variable_t *attribute = report->in.variables;
                attribute; attribute = attribute->next) {
            zb_event_t event = {.kind = ZB_EVENT_REPORT,
                                .status = report->info.status,
                                .endpoint = report->info.dst_ep,
                                .cluster = report->info.cluster_id,
                                .attribute = attribute->attr_id};
            copy_scalar(&event, attribute->attr_type, attribute->attr_value);
            if (report->in.header && report->in.header->src_addr.addr_mode == EZB_ADDR_MODE_SHORT)
                event.short_addr = report->in.header->src_addr.u.short_addr;
            emit(event);
        }
    } else if (action == EZB_ZCL_CORE_GROUPS_ADD_GROUP_RSP_CB_ID && message) {
        const ezb_zcl_groups_add_group_rsp_message_t *response = message;
        zb_event_t event = {.kind = ZB_EVENT_GROUP_ADDED,
                            .status = response->in.status,
                            .endpoint = response->info.dst_ep,
                            .cluster = EZB_ZCL_CLUSTER_ID_GROUPS,
                            .value = response->in.group_id};
        emit(event);
    } else if (action == EZB_ZCL_CORE_GROUPS_REMOVE_GROUP_RSP_CB_ID && message) {
        const ezb_zcl_groups_remove_group_rsp_message_t *response = message;
        zb_event_t event = {.kind = ZB_EVENT_GROUP_REMOVED,
                            .status = response->in.status,
                            .endpoint = response->info.dst_ep,
                            .cluster = EZB_ZCL_CLUSTER_ID_GROUPS,
                            .value = response->in.group_id};
        emit(event);
    }
}

static void match_result(const ezb_zdo_match_desc_req_result_t *result, void *context)
{
    int cluster_id = (int)(uintptr_t)context;
    if (!result || !result->rsp) return; /* End of a broadcast search. */
    if (result->error != EZB_ERR_NONE || result->rsp->status != EZB_ZDP_STATUS_SUCCESS) {
        emit_status(ZB_EVENT_ERROR, result->error ? result->error : result->rsp->status, 0);
        return;
    }
    for (size_t i = 0; i < result->rsp->match_length; ++i) {
        zb_event_t event = {.kind = ZB_EVENT_MATCH,
                            .short_addr = result->rsp->nwk_addr_of_interest,
                            .endpoint = result->rsp->match_list[i],
                            .cluster = cluster_id};
        emit(event);
    }
}

static void bind_result(const ezb_zdp_bind_req_result_t *result, void *context)
{
    uintptr_t identity = (uintptr_t)context;
    zb_event_t event = {.kind = ZB_EVENT_BOUND, .endpoint = identity & 0xff,
                        .cluster = (identity >> 8) & 0xffff};
    event.status = !result ? ESP_FAIL : result->error != EZB_ERR_NONE ? result->error :
            !result->rsp ? ESP_FAIL : result->rsp->status;
    emit(event);
}

static esp_err_t add_standard_endpoint(ezb_af_device_desc_t device, int kind, int id)
{
    ezb_af_ep_desc_t endpoint;
    if (kind == 1) {
        ezb_zha_on_off_light_config_t light = EZB_ZHA_ON_OFF_LIGHT_CONFIG();
        endpoint = ezb_zha_create_on_off_light(id, &light);
    } else if (kind == 2) {
        ezb_zha_on_off_switch_config_t sw = EZB_ZHA_ON_OFF_SWITCH_CONFIG();
        endpoint = ezb_zha_create_on_off_switch(id, &sw);
    } else {
        ezb_zha_temperature_sensor_config_t sensor = EZB_ZHA_TEMPERATURE_SENSOR_CONFIG();
        sensor.temp_meas_cfg.min_measured_value = -27315;
        sensor.temp_meas_cfg.max_measured_value = 32767;
        endpoint = ezb_zha_create_temperature_sensor(id, &sensor);
    }
    if (!endpoint) return ESP_ERR_NO_MEM;
    if (kind == 2 && !ezb_af_endpoint_get_cluster_desc(endpoint,
            EZB_ZCL_CLUSTER_ID_GROUPS, EZB_ZCL_CLUSTER_CLIENT)) {
        ezb_zcl_cluster_desc_t groups = ezb_zcl_groups_create_cluster_desc(NULL,
                EZB_ZCL_CLUSTER_CLIENT);
        if (!groups) {
            ezb_af_free_endpoint_desc(endpoint);
            return ESP_ERR_NO_MEM;
        }
        esp_err_t added = ezb_af_endpoint_add_cluster_desc(endpoint, groups);
        if (added != ESP_OK) {
            ezb_zcl_free_cluster_desc(groups);
            ezb_af_free_endpoint_desc(endpoint);
            return added;
        }
    }
    if (kind == 2 && !ezb_af_endpoint_get_cluster_desc(endpoint,
            EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_CLIENT)) {
        ezb_zcl_cluster_desc_t temperature =
                ezb_zcl_temperature_measurement_create_cluster_desc(NULL,
                        EZB_ZCL_CLUSTER_CLIENT);
        if (!temperature) {
            ezb_af_free_endpoint_desc(endpoint);
            return ESP_ERR_NO_MEM;
        }
        esp_err_t added = ezb_af_endpoint_add_cluster_desc(endpoint, temperature);
        if (added != ESP_OK) {
            ezb_zcl_free_cluster_desc(temperature);
            ezb_af_free_endpoint_desc(endpoint);
            return added;
        }
    }
    esp_err_t result = ezb_af_device_add_endpoint_desc(device, endpoint);
    if (result != ESP_OK) {
        ezb_af_free_endpoint_desc(endpoint);
        return result;
    }
    return ESP_OK;
}

static esp_err_t add_custom_clusters(ezb_af_device_desc_t device)
{
    for (unsigned i = 0; i < custom_cluster_count; ++i) {
        const zb_custom_cluster_t *entry = &custom_clusters[i];
        ezb_af_ep_desc_t endpoint = ezb_af_device_get_endpoint_desc(device, entry->endpoint);
        if (!endpoint) return ESP_ERR_INVALID_ARG;
        ezb_zcl_custom_cluster_config_t config = {.cluster_id = entry->cluster};
        ezb_zcl_cluster_desc_t cluster = ezb_zcl_custom_create_cluster_desc(&config,
                entry->role);
        if (!cluster) return ESP_ERR_NO_MEM;
        esp_err_t result = ESP_OK;
        for (unsigned j = 0; j < custom_attribute_count; ++j) {
            zb_custom_attribute_t *attribute = &custom_attributes[j];
            if (entry->role == EZB_ZCL_CLUSTER_SERVER &&
                    attribute->endpoint == entry->endpoint &&
                    attribute->cluster == entry->cluster) {
                result = ezb_zcl_custom_cluster_desc_add_attr(cluster, attribute->attribute,
                        attribute->type, attribute->access, &attribute->value);
                if (result != ESP_OK) break;
            }
        }
        if (result == ESP_OK) result = ezb_af_endpoint_add_cluster_desc(endpoint, cluster);
        if (result != ESP_OK) {
            ezb_zcl_free_cluster_desc(cluster);
            return result;
        }
    }
    return ESP_OK;
}

static esp_err_t add_humidity_clusters(ezb_af_device_desc_t device)
{
    for (unsigned i = 0; i < humidity_cluster_count; ++i) {
        const zb_humidity_cluster_t *entry = &humidity_clusters[i];
        ezb_af_ep_desc_t endpoint = ezb_af_device_get_endpoint_desc(device, entry->endpoint);
        if (!endpoint || ezb_af_endpoint_get_cluster_desc(endpoint,
                EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, entry->role))
            return ESP_ERR_INVALID_ARG;
        ezb_zcl_rel_humidity_measurement_cluster_server_config_t server = {
            .measured_value = 0xffff, .min_measured_value = 0, .max_measured_value = 10000};
        ezb_zcl_cluster_desc_t cluster = ezb_zcl_rel_humidity_measurement_create_cluster_desc(
                entry->role == EZB_ZCL_CLUSTER_SERVER ? &server : NULL, entry->role);
        if (!cluster) return ESP_ERR_NO_MEM;
        esp_err_t result = ezb_af_endpoint_add_cluster_desc(endpoint, cluster);
        if (result != ESP_OK) {
            ezb_zcl_free_cluster_desc(cluster);
            return result;
        }
    }
    return ESP_OK;
}

static esp_err_t register_device(const zb_config_t *options)
{
    ezb_af_device_desc_t device = ezb_af_create_device_desc();
    if (!device) return ESP_ERR_NO_MEM;
    esp_err_t result = add_standard_endpoint(device, options->device, options->endpoint);
    for (unsigned i = 0; result == ESP_OK && i < extra_endpoint_count; ++i)
        result = add_standard_endpoint(device, extra_endpoints[i].device,
                extra_endpoints[i].endpoint);
    if (result == ESP_OK) result = add_humidity_clusters(device);
    if (result == ESP_OK) result = add_custom_clusters(device);
    if (result != ESP_OK) {
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

int bmx_esp32_zigbee_core_add_endpoint(int device, int endpoint)
{
    if (device < 1 || device > 3 || endpoint < 1 || endpoint > 240)
        return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    for (unsigned i = 0; i < extra_endpoint_count; ++i) {
        if (extra_endpoints[i].endpoint == endpoint) {
            portEXIT_CRITICAL(&state_lock);
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (extra_endpoint_count == ZB_MAX_EXTRA_ENDPOINTS) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_NO_MEM;
    }
    extra_endpoints[extra_endpoint_count++] = (zb_endpoint_config_t){
        .device = (uint8_t)device, .endpoint = (uint8_t)endpoint};
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_clear_endpoints(void)
{
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    extra_endpoint_count = 0;
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_add_humidity_cluster(int endpoint, int role)
{
    if (endpoint < 1 || endpoint > 240 ||
            (role != EZB_ZCL_CLUSTER_SERVER && role != EZB_ZCL_CLUSTER_CLIENT))
        return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    for (unsigned i = 0; i < humidity_cluster_count; ++i) {
        if (humidity_clusters[i].endpoint == endpoint && humidity_clusters[i].role == role) {
            portEXIT_CRITICAL(&state_lock);
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (humidity_cluster_count == ZB_MAX_HUMIDITY_CLUSTERS) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_NO_MEM;
    }
    humidity_clusters[humidity_cluster_count++] = (zb_humidity_cluster_t){
        .endpoint = (uint8_t)endpoint, .role = (uint8_t)role};
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_clear_humidity_clusters(void)
{
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    humidity_cluster_count = 0;
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_add_custom_cluster(int endpoint, int cluster_id, int role)
{
    if (endpoint < 1 || endpoint > 240 || cluster_id < EZB_ZCL_CLUSTER_ID_MIN_CUSTOM ||
            cluster_id > 65535 || (role != EZB_ZCL_CLUSTER_SERVER &&
            role != EZB_ZCL_CLUSTER_CLIENT)) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    for (unsigned i = 0; i < custom_cluster_count; ++i) {
        if (custom_clusters[i].endpoint == endpoint &&
                custom_clusters[i].cluster == cluster_id && custom_clusters[i].role == role) {
            portEXIT_CRITICAL(&state_lock);
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (custom_cluster_count == ZB_MAX_CUSTOM_CLUSTERS) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_NO_MEM;
    }
    custom_clusters[custom_cluster_count++] = (zb_custom_cluster_t){
        .endpoint = (uint8_t)endpoint, .cluster = (uint16_t)cluster_id, .role = (uint8_t)role};
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_add_custom_scalar(int endpoint, int cluster_id,
        int attribute_id, int type, int access, int32_t value)
{
    if (endpoint < 1 || endpoint > 240 || cluster_id < EZB_ZCL_CLUSTER_ID_MIN_CUSTOM ||
            cluster_id > 65535 || attribute_id < 0 || attribute_id > 65535 ||
            !scalar_size(type) || access < 1 || access > 7 || !(access & EZB_ZCL_ATTR_ACCESS_READ))
        return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    bool found = false;
    for (unsigned i = 0; i < custom_cluster_count; ++i)
        if (custom_clusters[i].endpoint == endpoint && custom_clusters[i].cluster == cluster_id &&
                custom_clusters[i].role == EZB_ZCL_CLUSTER_SERVER) found = true;
    if (!found) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_ARG;
    }
    for (unsigned i = 0; i < custom_attribute_count; ++i) {
        if (custom_attributes[i].endpoint == endpoint &&
                custom_attributes[i].cluster == cluster_id &&
                custom_attributes[i].attribute == attribute_id) {
            portEXIT_CRITICAL(&state_lock);
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (custom_attribute_count == ZB_MAX_CUSTOM_ATTRIBUTES) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_NO_MEM;
    }
    custom_attributes[custom_attribute_count++] = (zb_custom_attribute_t){
        .endpoint = (uint8_t)endpoint, .cluster = (uint16_t)cluster_id,
        .attribute = (uint16_t)attribute_id, .type = (uint8_t)type,
        .access = (uint8_t)access, .value = value};
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_clear_custom_clusters(void)
{
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    custom_cluster_count = custom_attribute_count = 0;
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_start(int role, int device, int endpoint, uint32_t primary_mask,
        uint32_t secondary_mask, int auto_commission)
{
    if (role < 1 || role > 4 || device < 1 || device > 3 || endpoint < 1 || endpoint > 240 ||
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
    for (unsigned i = 0; i < extra_endpoint_count; ++i) {
        if (extra_endpoints[i].endpoint == endpoint) {
            portEXIT_CRITICAL(&state_lock);
            return ESP_ERR_INVALID_ARG;
        }
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
    if (!endpoint_kind(source_ep) || destination_addr < 0 || destination_addr > 65535 ||
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
    if (!endpoint_kind(source_ep) || destination_addr < 0 || destination_addr > 65535 ||
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

int bmx_esp32_zigbee_core_read_attribute(int source_ep, int destination_addr, int destination_ep,
        int cluster_id, int attribute_id)
{
    if (!endpoint_kind(source_ep) || destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240 || cluster_id < 0 || cluster_id > 65535 ||
            attribute_id < 0 || attribute_id > 65535) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint16_t attribute = (uint16_t)attribute_id;
    ezb_zcl_read_attr_cmd_t request = {.cmd_ctrl = {
        .dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
        .dst_ep = (uint8_t)destination_ep, .src_ep = (uint8_t)source_ep,
        .cluster_id = (uint16_t)cluster_id,
    }, .payload = {.attr_number = 1, .attr_field = &attribute}};
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_read_attr_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_write_scalar(int source_ep, int destination_addr, int destination_ep,
        int cluster_id, int attribute_id, int data_type, int32_t value)
{
    int size = scalar_size(data_type);
    if (!endpoint_kind(source_ep) || destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240 || cluster_id < 0 || cluster_id > 65535 ||
            attribute_id < 0 || attribute_id > 65535 || !size) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_attribute_t attribute = {.id = (uint16_t)attribute_id,
                                     .data = {.type = (uint8_t)data_type,
                                              .size = (uint16_t)size, .value = &value}};
    ezb_zcl_write_attr_cmd_t request = {.cmd_ctrl = {
        .dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
        .dst_ep = (uint8_t)destination_ep, .src_ep = (uint8_t)source_ep,
        .cluster_id = (uint16_t)cluster_id,
    }, .payload = {.attr_number = 1, .attr_field = &attribute}};
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_write_attr_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_find_servers(int profile_id, int cluster_id)
{
    if (profile_id < 0 || profile_id > 65535 || cluster_id < 0 || cluster_id > 65535)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint16_t cluster = (uint16_t)cluster_id;
    ezb_zdo_match_desc_req_t request = {
        .dst_nwk_addr = 0xfffd,
        .field = {.nwk_addr_of_interest = 0xfffd,
                  .profile_id = (uint16_t)profile_id,
                  .num_in_clusters = 1, .cluster_list = &cluster},
        .cb = match_result, .user_ctx = (void *)(uintptr_t)cluster_id,
    };
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zdo_match_desc_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_find_onoff(void)
{
    return bmx_esp32_zigbee_core_find_servers(EZB_AF_HA_PROFILE_ID,
            EZB_ZCL_CLUSTER_ID_ON_OFF);
}

int bmx_esp32_zigbee_core_bind_cluster(int source_ep, int destination_addr,
        int destination_ep, int cluster_id)
{
    if (!endpoint_kind(source_ep) ||
            destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240 ||
            cluster_id < 0 || cluster_id > 65535) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zdo_bind_req_t request = {
        .field = {.src_ep = source_ep, .cluster_id = (uint16_t)cluster_id,
                  .dst_addr_mode = EZB_ADDR_MODE_EXT, .dst_ep = destination_ep},
        .cb = bind_result,
        .user_ctx = (void *)(uintptr_t)(((uint32_t)cluster_id << 8) | source_ep),
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

int bmx_esp32_zigbee_core_bind_onoff(int source_ep, int destination_addr, int destination_ep)
{
    return bmx_esp32_zigbee_core_bind_cluster(source_ep, destination_addr,
            destination_ep, EZB_ZCL_CLUSTER_ID_ON_OFF);
}

int bmx_esp32_zigbee_core_bound_onoff(int source_ep, int command)
{
    if (endpoint_kind(source_ep) != 2 || command < 0 || command > 2)
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

int bmx_esp32_zigbee_core_set_local_onoff(int endpoint, int on)
{
    if (endpoint_kind(endpoint) != 1 || (on != 0 && on != 1))
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint8_t value = (uint8_t)on;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_STD_MANUF_CODE, &value, false);
    esp_zigbee_lock_release();
    return result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_set_local_temperature(int endpoint, int centi_celsius)
{
    if (endpoint_kind(endpoint) != 3 || centi_celsius < -27315 || centi_celsius > 32767)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    int16_t value = (int16_t)centi_celsius;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            EZB_ZCL_CLUSTER_ID_TEMPERATURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_TEMPERATURE_MEASUREMENT_MEASURED_VALUE_ID,
            EZB_ZCL_STD_MANUF_CODE, &value, false);
    esp_zigbee_lock_release();
    return result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_set_local_humidity(int endpoint, int centi_percent)
{
    if (endpoint < 1 || endpoint > 240 || centi_percent < 0 || centi_percent > 10000)
        return ESP_ERR_INVALID_ARG;
    bool server = false;
    for (unsigned i = 0; i < humidity_cluster_count; ++i)
        if (humidity_clusters[i].endpoint == endpoint &&
                humidity_clusters[i].role == EZB_ZCL_CLUSTER_SERVER) server = true;
    if (!server) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint16_t value = (uint16_t)centi_percent;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            EZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MEASURED_VALUE_ID,
            EZB_ZCL_STD_MANUF_CODE, &value, false);
    esp_zigbee_lock_release();
    return result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_set_custom_scalar(int endpoint, int cluster_id,
        int attribute_id, int32_t value)
{
    if (!endpoint_kind(endpoint) || cluster_id < EZB_ZCL_CLUSTER_ID_MIN_CUSTOM ||
            cluster_id > 65535 || attribute_id < 0 || attribute_id > 65535)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    zb_custom_attribute_t *attribute = NULL;
    for (unsigned i = 0; i < custom_attribute_count; ++i) {
        if (custom_attributes[i].endpoint == endpoint &&
                custom_attributes[i].cluster == cluster_id &&
                custom_attributes[i].attribute == attribute_id) {
            attribute = &custom_attributes[i];
            break;
        }
    }
    if (!attribute) return ESP_ERR_INVALID_ARG;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            (uint16_t)cluster_id, EZB_ZCL_CLUSTER_SERVER, (uint16_t)attribute_id,
            EZB_ZCL_STD_MANUF_CODE, &value, false);
    if (result == EZB_ZCL_STATUS_SUCCESS) attribute->value = value;
    esp_zigbee_lock_release();
    return result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_configure_reporting(int source_ep, int destination_addr,
        int destination_ep, int cluster_id, int attribute_id, int data_type,
        int min_interval, int max_interval, int32_t reportable_change)
{
    int size = scalar_size(data_type);
    if (!endpoint_kind(source_ep) || destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240 ||
            cluster_id < 0 || cluster_id > 65535 || attribute_id < 0 || attribute_id > 65535 ||
            !size || reportable_change < 0 || (size == 1 && reportable_change > 255) ||
            (size == 2 && reportable_change > 65535) ||
            min_interval < 0 || min_interval > 65534 ||
            max_interval < 1 || max_interval > 65535 ||
            (max_interval != 65535 && min_interval > max_interval))
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_config_report_record_t record = {
        .direction = EZB_ZCL_REPORTING_SEND,
        .attr_id = (uint16_t)attribute_id,
        .client = {.attr_type = (uint8_t)data_type,
                   .min_interval = (uint16_t)min_interval,
                   .max_interval = (uint16_t)max_interval,
                   .reportable_change.u32 = (uint32_t)reportable_change},
    };
    ezb_zcl_config_report_cmd_t request = {
        .cmd_ctrl = {.dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
                     .dst_ep = (uint8_t)destination_ep, .src_ep = (uint8_t)source_ep,
                     .cluster_id = (uint16_t)cluster_id},
        .payload = {.record_number = 1, .record_field = &record},
    };
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_config_report_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_configure_onoff_reporting(int source_ep, int destination_addr,
        int destination_ep, int min_interval, int max_interval)
{
    if (endpoint_kind(source_ep) != 2) return ESP_ERR_INVALID_ARG;
    return bmx_esp32_zigbee_core_configure_reporting(source_ep, destination_addr, destination_ep,
            EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, EZB_ZCL_ATTR_TYPE_BOOL,
            min_interval, max_interval, 0);
}

int bmx_esp32_zigbee_core_add_group(int source_ep, int destination_addr, int destination_ep,
        int group_id)
{
    if (!endpoint_kind(source_ep) || destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240 || group_id < 1 || group_id > 0xfff7)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    static char empty_name[] = "";
    ezb_zcl_groups_add_group_cmd_t request = {
        .cmd_ctrl = {.dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
                     .dst_ep = (uint8_t)destination_ep, .src_ep = (uint8_t)source_ep},
        .payload = {.group_id = (uint16_t)group_id, .group_name = empty_name},
    };
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_groups_add_group_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_remove_group(int source_ep, int destination_addr, int destination_ep,
        int group_id)
{
    if (!endpoint_kind(source_ep) || destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240 || group_id < 1 || group_id > 0xfff7)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_groups_remove_group_cmd_t request = {
        .cmd_ctrl = {.dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
                     .dst_ep = (uint8_t)destination_ep, .src_ep = (uint8_t)source_ep},
        .payload = {.group_id = (uint16_t)group_id},
    };
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_groups_remove_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_group_onoff(int source_ep, int group_id, int command)
{
    if (endpoint_kind(source_ep) != 2 ||
            group_id < 1 || group_id > 0xfff7 || command < 0 || command > 2)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_on_off_cmd_t request = {.cmd_ctrl = {
        .dst_addr = EZB_ADDRESS_GROUP((uint16_t)group_id, 0xfffd),
        .src_ep = (uint8_t)source_ep,
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
        int *channel, int *factory_new, int *data_type, int *value_size)
{
    if (!kind || !status || !signal || !endpoint || !cluster || !attribute || !value ||
            !short_addr || !pan_id || !channel || !factory_new || !data_type || !value_size) return 0;
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
    *data_type = event.data_type; *value_size = event.value_size;
    return 1;
}

uint32_t bmx_esp32_zigbee_core_dropped(void)
{
    portENTER_CRITICAL(&queue_lock);
    uint32_t dropped = queue_dropped;
    portEXIT_CRITICAL(&queue_lock);
    return dropped;
}
