/* Copyright (c) 2026 Bruce A Henderson and contributors. SPDX-License-Identifier: Zlib */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "esp_pm.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
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
#define ZB_MAX_CUSTOM_ENDPOINTS 8u
#define ZB_MAX_CUSTOM_CLUSTERS 8u
#define ZB_MAX_CUSTOM_ATTRIBUTES 32u
#define ZB_MAX_STRING_BYTES 64u
#define ZB_MAX_HUMIDITY_CLUSTERS 18u
#define ZB_MAX_ILLUMINANCE_CLUSTERS 18u
#define ZB_MAX_ADDITIONAL_MEASUREMENT_CLUSTERS 32u
#define ZB_OTA_PREFIX_BYTES 128u
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
#define ZB_EVENT_GROUP_VIEWED 19
#define ZB_EVENT_GROUP_MEMBER 20
#define ZB_EVENT_GROUP_MEMBERSHIP_COMPLETE 21
#define ZB_EVENT_OTA 22

typedef struct {
    int32_t kind, status, signal, endpoint, cluster, attribute, value;
    int32_t short_addr, pan_id, channel, factory_new;
    int32_t data_type, value_size;
    uint8_t text[ZB_MAX_STRING_BYTES];
} zb_event_t;

typedef struct {
    int32_t role, device, endpoint, auto_commission;
    uint32_t primary_mask, secondary_mask;
} zb_config_t;

typedef struct {
    uint8_t device, endpoint;
} zb_endpoint_config_t;

typedef struct {
    uint8_t endpoint, version;
    uint16_t profile, device;
} zb_custom_endpoint_t;

typedef struct {
    uint8_t endpoint, role;
    uint16_t cluster;
} zb_custom_cluster_t;

typedef struct {
    uint8_t endpoint, role;
} zb_humidity_cluster_t;

typedef struct {
    uint8_t endpoint, role;
} zb_illuminance_cluster_t;

typedef struct {
    uint8_t endpoint, role, sensor_type;
    uint16_t cluster;
} zb_measurement_cluster_t;

typedef struct {
    uint8_t endpoint;
    uint16_t manufacturer, image_type;
    uint32_t version;
    bool configured;
} zb_ota_cluster_t;

typedef struct {
    esp_ota_handle_t handle;
    const esp_partition_t *partition;
    uint32_t total, received, image_offset, image_size, written;
    uint8_t prefix[ZB_OTA_PREFIX_BYTES];
    uint32_t prefix_used;
    bool active, validated, applied, header_valid;
} zb_ota_download_t;

typedef struct {
    uint8_t endpoint, type, access;
    uint16_t cluster, attribute;
    int32_t value;
    uint8_t text[ZB_MAX_STRING_BYTES + 1u];
} zb_custom_attribute_t;

static zb_event_t event_queue[ZB_QUEUE_CAPACITY];
static uint32_t queue_write, queue_read, queue_dropped;
static portMUX_TYPE queue_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static int32_t state; /* 0 stopped, 1 starting, 2 running, 3 stopping */
static zb_config_t requested;
static bool install_codes_enabled;
static esp_pm_config_t previous_pm_config;
static bool restore_pm_config;
static zb_endpoint_config_t extra_endpoints[ZB_MAX_EXTRA_ENDPOINTS];
static uint8_t extra_endpoint_count;
static zb_custom_endpoint_t custom_endpoints[ZB_MAX_CUSTOM_ENDPOINTS];
static uint8_t custom_endpoint_count;
static zb_custom_cluster_t custom_clusters[ZB_MAX_CUSTOM_CLUSTERS];
static uint8_t custom_cluster_count;
static zb_custom_attribute_t custom_attributes[ZB_MAX_CUSTOM_ATTRIBUTES];
static uint8_t custom_attribute_count;
static zb_humidity_cluster_t humidity_clusters[ZB_MAX_HUMIDITY_CLUSTERS];
static uint8_t humidity_cluster_count;
static zb_illuminance_cluster_t illuminance_clusters[ZB_MAX_ILLUMINANCE_CLUSTERS];
static uint8_t illuminance_cluster_count;
static zb_measurement_cluster_t measurement_clusters[ZB_MAX_ADDITIONAL_MEASUREMENT_CLUSTERS];
static uint8_t measurement_cluster_count;
static zb_ota_cluster_t ota_client, ota_server;
static ezb_zcl_ota_file_handle_t ota_server_file;
static uint16_t ota_served_manufacturer, ota_served_image_type;
static uint32_t ota_served_version;
static zb_ota_download_t ota_download;

int bmx_esp32_zigbee_core_state(void);

static int endpoint_kind(int endpoint)
{
    if (endpoint == requested.endpoint) return requested.device;
    for (unsigned i = 0; i < extra_endpoint_count; ++i)
        if (endpoint == extra_endpoints[i].endpoint) return extra_endpoints[i].device;
    for (unsigned i = 0; i < custom_endpoint_count; ++i)
        if (endpoint == custom_endpoints[i].endpoint) return 4;
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
    if (type == EZB_ZCL_ATTR_TYPE_STRING || type == EZB_ZCL_ATTR_TYPE_OCTSTR) {
        const uint8_t *string = data;
        unsigned length = string[0];
        if (length > ZB_MAX_STRING_BYTES) {
            event->value_size = -1;
        } else {
            memcpy(event->text, string + 1, length);
            event->value_size = (int32_t)length;
        }
        return;
    }
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

static uint16_t ota_u16(const uint8_t *data)
{
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

static uint32_t ota_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
            ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void ota_emit(int endpoint, int progress, int status, uint32_t value, uint32_t total,
        int role)
{
    zb_event_t event = {.kind = ZB_EVENT_OTA, .status = status, .signal = progress,
                        .endpoint = endpoint, .cluster = EZB_ZCL_CLUSTER_ID_OTA_UPGRADE,
                        .value = (int32_t)value, .attribute = (int32_t)total,
                        .data_type = role};
    emit(event);
}

static void ota_abort_download(void)
{
    if (ota_download.active) (void)esp_ota_abort(ota_download.handle);
    memset(&ota_download, 0, sizeof(ota_download));
}

/* This first firmware path accepts one complete, uncompressed upgrade-image element. */
static esp_err_t ota_parse_prefix(void)
{
    const uint8_t *data = ota_download.prefix;
    if (ota_download.prefix_used < 56) return ESP_OK;
    uint16_t header_size = ota_u16(data + 6);
    if (ota_u32(data) != EZB_ZCL_OTA_FILE_IDENTIFIER ||
            ota_u16(data + 4) != EZB_ZCL_OTA_FILE_HEADER_VERSION ||
            header_size != 56 || ota_u16(data + 8) != 0 ||
            ota_u16(data + 10) != ota_client.manufacturer ||
            ota_u16(data + 12) != ota_client.image_type ||
            ota_u32(data + 14) <= ota_client.version ||
            ota_u32(data + 52) != ota_download.total)
        return ESP_ERR_INVALID_RESPONSE;
    if ((uint32_t)header_size + 6u > ota_download.total) return ESP_ERR_INVALID_SIZE;
    if (ota_download.prefix_used < (uint32_t)header_size + 6u) return ESP_OK;
    uint32_t image_size = ota_u32(data + header_size + 2);
    if (ota_u16(data + header_size) != EZB_ZCL_OTA_FILE_TAG_ID_UPGRADE_IMAGE ||
            !image_size || image_size > ota_download.partition->size ||
            image_size != ota_download.total - (uint32_t)header_size - 6u)
        return ESP_ERR_INVALID_SIZE;
    ota_download.image_offset = (uint32_t)header_size + 6u;
    ota_download.image_size = image_size;
    ota_download.header_valid = true;
    return ESP_OK;
}

static esp_err_t ota_receive(uint32_t offset, const uint8_t *block, uint8_t size)
{
    if (!ota_download.active || !block || !size ||
            offset > ota_download.total || size > ota_download.total - offset)
        return ESP_ERR_INVALID_ARG;
    if (offset < ota_download.received &&
            size <= ota_download.received - offset) return ESP_OK; /* Retransmitted block. */
    if (offset != ota_download.received) return ESP_ERR_INVALID_STATE;
    uint32_t prefix_end = offset + size;
    if (prefix_end > ZB_OTA_PREFIX_BYTES) prefix_end = ZB_OTA_PREFIX_BYTES;
    if (prefix_end > ota_download.prefix_used && offset <= ota_download.prefix_used) {
        uint32_t length = prefix_end - ota_download.prefix_used;
        memcpy(ota_download.prefix + ota_download.prefix_used,
                block + ota_download.prefix_used - offset, length);
        ota_download.prefix_used = prefix_end;
    }
    if (!ota_download.header_valid) {
        esp_err_t result = ota_parse_prefix();
        if (result != ESP_OK) return result;
    }
    if (ota_download.header_valid && offset + size > ota_download.image_offset) {
        uint32_t skip = offset < ota_download.image_offset ?
                ota_download.image_offset - offset : 0;
        uint32_t length = size - skip;
        esp_err_t result = esp_ota_write(ota_download.handle, block + skip, length);
        if (result != ESP_OK) return result;
        ota_download.written += length;
    }
    ota_download.received += size;
    if ((ota_download.written & 0x3fff) < size ||
            ota_download.received == ota_download.total)
        ota_emit(ota_client.endpoint, EZB_ZCL_OTA_UPGRADE_PROGRESS_RECEIVING, ESP_OK,
                ota_download.written, ota_download.image_size, EZB_ZCL_CLUSTER_CLIENT);
    return ESP_OK;
}

static void ota_reboot_task(void *context)
{
    (void)context;
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

static void ota_client_progress(ezb_zcl_ota_upgrade_client_progress_message_t *message)
{
    esp_err_t result = ESP_OK;
    int progress = message->in.progress;
    if (progress == EZB_ZCL_OTA_UPGRADE_PROGRESS_START) {
        ota_abort_download();
        uint32_t size = message->in.start.image_size;
        const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
        if (!partition) result = ESP_ERR_NOT_FOUND;
        else if (message->in.start.manuf_code != ota_client.manufacturer ||
                message->in.start.image_type != ota_client.image_type ||
                message->in.start.file_version <= ota_client.version ||
                size < 62 || size > partition->size + ZB_OTA_PREFIX_BYTES)
            result = ESP_ERR_INVALID_ARG;
        else {
            result = esp_ota_begin(partition, OTA_SIZE_UNKNOWN, &ota_download.handle);
            if (result == ESP_OK) {
                ota_download.partition = partition;
                ota_download.total = size;
                ota_download.active = true;
            }
        }
        ota_emit(ota_client.endpoint, progress, result, 0, size, EZB_ZCL_CLUSTER_CLIENT);
    } else if (progress == EZB_ZCL_OTA_UPGRADE_PROGRESS_RECEIVING) {
        result = ota_receive(message->in.receiving.file_offset,
                message->in.receiving.block, message->in.receiving.block_size);
    } else if (progress == EZB_ZCL_OTA_UPGRADE_PROGRESS_CHECK) {
        if (!ota_download.active || !ota_download.header_valid ||
                ota_download.received != ota_download.total ||
                ota_download.written != ota_download.image_size)
            result = ESP_ERR_INVALID_STATE;
        else {
            result = esp_ota_end(ota_download.handle);
            ota_download.active = false;
            ota_download.validated = result == ESP_OK;
        }
        ota_emit(ota_client.endpoint, progress, result, ota_download.written,
                ota_download.image_size, EZB_ZCL_CLUSTER_CLIENT);
    } else if (progress == EZB_ZCL_OTA_UPGRADE_PROGRESS_APPLY) {
        result = ota_download.validated ?
                esp_ota_set_boot_partition(ota_download.partition) : ESP_ERR_INVALID_STATE;
        ota_download.applied = result == ESP_OK;
        ota_emit(ota_client.endpoint, progress, result, ota_download.written,
                ota_download.image_size, EZB_ZCL_CLUSTER_CLIENT);
    } else if (progress == EZB_ZCL_OTA_UPGRADE_PROGRESS_FINISH) {
        result = ota_download.applied ? ESP_OK : ESP_ERR_INVALID_STATE;
        if (result == ESP_OK && xTaskCreate(ota_reboot_task, "zigbee_ota_reboot",
                2048, NULL, 5, NULL) != pdPASS) result = ESP_ERR_NO_MEM;
        ota_emit(ota_client.endpoint, progress, result, ota_download.written,
                ota_download.image_size, EZB_ZCL_CLUSTER_CLIENT);
        message->out.result = result == ESP_OK ? EZB_ZCL_STATUS_SUCCESS : EZB_ZCL_STATUS_ABORT;
        return;
    } else if (progress == EZB_ZCL_OTA_UPGRADE_PROGRESS_ABORT) {
        ota_abort_download();
        ota_emit(ota_client.endpoint, progress, ESP_FAIL, 0, 0, EZB_ZCL_CLUSTER_CLIENT);
    }
    if (result != ESP_OK) {
        ota_abort_download();
        ota_emit(ota_client.endpoint, EZB_ZCL_OTA_UPGRADE_PROGRESS_ABORT, result, 0, 0,
                EZB_ZCL_CLUSTER_CLIENT);
    }
    message->out.result = result == ESP_OK ? EZB_ZCL_STATUS_SUCCESS : EZB_ZCL_STATUS_ABORT;
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
    if (action == EZB_ZCL_CORE_OTA_UPGRADE_CLIENT_PROGRESS_CB_ID && message) {
        ota_client_progress((ezb_zcl_ota_upgrade_client_progress_message_t *)message);
    } else if (action == EZB_ZCL_CORE_OTA_UPGRADE_QUERY_NEXT_IMAGE_RSP_CB_ID && message) {
        ezb_zcl_ota_upgrade_query_next_image_rsp_message_t *response = message;
        ota_emit(ota_client.endpoint, -1, response->in.image.status,
                response->in.image.file_version, response->in.image.size,
                EZB_ZCL_CLUSTER_CLIENT);
        response->out.result = EZB_ZCL_STATUS_SUCCESS;
    } else if (action == EZB_ZCL_CORE_OTA_UPGRADE_SERVER_PROGRESS_CB_ID && message) {
        ezb_zcl_ota_upgrade_server_progress_message_t *progress = message;
        uint32_t offset = progress->in.progress == EZB_ZCL_OTA_UPGRADE_PROGRESS_SENDING ?
                progress->in.sending.file_offset : 0;
        if (progress->in.progress != EZB_ZCL_OTA_UPGRADE_PROGRESS_SENDING ||
                (offset & 0x3fff) < 223)
            ota_emit(ota_server.endpoint, progress->in.progress, ESP_OK, offset, 0,
                    EZB_ZCL_CLUSTER_SERVER);
        progress->out.result = EZB_ZCL_STATUS_SUCCESS;
        if (progress->in.progress == EZB_ZCL_OTA_UPGRADE_PROGRESS_FINISH) {
            progress->out.finish.current_time = 0;
            progress->out.finish.upgrade_time = 0;
        }
    } else if (action == EZB_ZCL_CORE_SET_ATTR_VALUE_CB_ID && message) {
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
    } else if (action == EZB_ZCL_CORE_GROUPS_VIEW_GROUP_RSP_CB_ID && message) {
        const ezb_zcl_groups_view_group_rsp_message_t *response = message;
        zb_event_t event = {.kind = ZB_EVENT_GROUP_VIEWED,
                            .status = response->in.status,
                            .endpoint = response->info.dst_ep,
                            .cluster = EZB_ZCL_CLUSTER_ID_GROUPS,
                            .value = response->in.group_id};
        if (response->in.header &&
                response->in.header->src_addr.addr_mode == EZB_ADDR_MODE_SHORT)
            event.short_addr = response->in.header->src_addr.u.short_addr;
        emit(event);
    } else if (action == EZB_ZCL_CORE_GROUPS_GET_GROUP_MEMBERSHIP_RSP_CB_ID && message) {
        const ezb_zcl_groups_get_group_membership_rsp_message_t *response = message;
        zb_event_t event = {.status = response->info.status,
                            .endpoint = response->info.dst_ep,
                            .cluster = EZB_ZCL_CLUSTER_ID_GROUPS};
        if (response->in.group_count && !response->in.group_list)
            event.status = ESP_FAIL;
        if (response->in.header &&
                response->in.header->src_addr.addr_mode == EZB_ADDR_MODE_SHORT)
            event.short_addr = response->in.header->src_addr.u.short_addr;
        for (unsigned i = 0; i < response->in.group_count && response->in.group_list; ++i) {
            event.kind = ZB_EVENT_GROUP_MEMBER;
            event.value = response->in.group_list[i];
            emit(event);
        }
        event.kind = ZB_EVENT_GROUP_MEMBERSHIP_COMPLETE;
        event.value = response->in.group_count;
        event.attribute = response->in.capacity;
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
                        attribute->type, attribute->access,
                        (attribute->type == EZB_ZCL_ATTR_TYPE_STRING ||
                         attribute->type == EZB_ZCL_ATTR_TYPE_OCTSTR) ?
                        (const void *)attribute->text : (const void *)&attribute->value);
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

static esp_err_t add_illuminance_clusters(ezb_af_device_desc_t device)
{
    for (unsigned i = 0; i < illuminance_cluster_count; ++i) {
        const zb_illuminance_cluster_t *entry = &illuminance_clusters[i];
        ezb_af_ep_desc_t endpoint = ezb_af_device_get_endpoint_desc(device, entry->endpoint);
        if (!endpoint || ezb_af_endpoint_get_cluster_desc(endpoint,
                EZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, entry->role))
            return ESP_ERR_INVALID_ARG;
        ezb_zcl_illuminance_measurement_cluster_server_config_t server = {
            .measured_value = 0xffff, .min_measured_value = 1, .max_measured_value = 0xfffe};
        ezb_zcl_cluster_desc_t cluster = ezb_zcl_illuminance_measurement_create_cluster_desc(
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

static esp_err_t add_measurement_clusters(ezb_af_device_desc_t device)
{
    for (unsigned i = 0; i < measurement_cluster_count; ++i) {
        const zb_measurement_cluster_t *entry = &measurement_clusters[i];
        ezb_af_ep_desc_t endpoint = ezb_af_device_get_endpoint_desc(device, entry->endpoint);
        if (!endpoint || ezb_af_endpoint_get_cluster_desc(endpoint, entry->cluster, entry->role))
            return ESP_ERR_INVALID_ARG;
        ezb_zcl_cluster_desc_t cluster = NULL;
        if (entry->cluster == EZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT) {
            ezb_zcl_pressure_measurement_cluster_server_config_t server = {
                .measured_value = INT16_MIN, .min_measured_value = -32767,
                .max_measured_value = INT16_MAX};
            cluster = ezb_zcl_pressure_measurement_create_cluster_desc(
                    entry->role == EZB_ZCL_CLUSTER_SERVER ? &server : NULL, entry->role);
        } else if (entry->cluster == EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING) {
            uint8_t bitmap = entry->sensor_type == 2 ? 3 :
                    entry->sensor_type == 3 ? 4 : (uint8_t)(1u << entry->sensor_type);
            ezb_zcl_occupancy_sensing_cluster_server_config_t server = {
                .occupancy = 0, .occupancy_sensor_type = entry->sensor_type,
                .occupancy_sensor_type_bitmap = bitmap};
            cluster = ezb_zcl_occupancy_sensing_create_cluster_desc(
                    entry->role == EZB_ZCL_CLUSTER_SERVER ? &server : NULL, entry->role);
        } else if (entry->cluster == EZB_ZCL_CLUSTER_ID_POWER_CONFIG) {
            cluster = ezb_zcl_power_config_create_cluster_desc(NULL, entry->role);
            if (cluster && entry->role == EZB_ZCL_CLUSTER_SERVER) {
                uint8_t unknown = 0xff;
                esp_err_t result = ezb_zcl_power_config_cluster_desc_add_attr(cluster,
                        EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, &unknown);
                if (result == ESP_OK) result = ezb_zcl_power_config_cluster_desc_add_attr(
                        cluster, EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID,
                        &unknown);
                if (result != ESP_OK) {
                    ezb_zcl_free_cluster_desc(cluster);
                    return result;
                }
            }
        } else if (entry->cluster == EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT) {
            ezb_zcl_electrical_measurement_cluster_server_config_t server = {
                .measurement_type = EZB_ZCL_ELECTRICAL_MEASUREMENT_MEASUREMENT_TYPE_ACTIVE_MEASUREMENT_AC};
            cluster = ezb_zcl_electrical_measurement_create_cluster_desc(
                    entry->role == EZB_ZCL_CLUSTER_SERVER ? &server : NULL, entry->role);
            if (cluster && entry->role == EZB_ZCL_CLUSTER_SERVER) {
                uint16_t unknown = 0xffff, current_divisor = 1000, one = 1;
                int16_t unknown_power = INT16_MIN;
                const uint16_t scale_attributes[] = {
                    EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_MULTIPLIER_ID,
                    EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_VOLTAGE_DIVISOR_ID,
                    EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_MULTIPLIER_ID,
                    EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_CURRENT_DIVISOR_ID,
                    EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_MULTIPLIER_ID,
                    EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_AC_POWER_DIVISOR_ID};
                esp_err_t result = ezb_zcl_electrical_measurement_cluster_desc_add_attr(
                        cluster, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_ID, &unknown);
                if (result == ESP_OK) result = ezb_zcl_electrical_measurement_cluster_desc_add_attr(
                        cluster, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_ID, &unknown);
                if (result == ESP_OK) result = ezb_zcl_electrical_measurement_cluster_desc_add_attr(
                        cluster, EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID, &unknown_power);
                for (unsigned j = 0; result == ESP_OK && j < 6; ++j) {
                    const uint16_t *value = j == 3 ? &current_divisor : &one;
                    result = ezb_zcl_electrical_measurement_cluster_desc_add_attr(
                            cluster, scale_attributes[j], value);
                }
                if (result != ESP_OK) {
                    ezb_zcl_free_cluster_desc(cluster);
                    return result;
                }
            }
        } else if (entry->cluster == EZB_ZCL_CLUSTER_ID_LEVEL) {
            ezb_zcl_level_cluster_server_config_t server = {.current_level = 0};
            cluster = ezb_zcl_level_create_cluster_desc(
                    entry->role == EZB_ZCL_CLUSTER_SERVER ? &server : NULL, entry->role);
        } else if (entry->cluster == EZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
            ezb_zcl_color_control_cluster_server_config_t server = {
                .current_x = 0x4000, .current_y = 0x4000,
                .color_capabilities = EZB_ZCL_COLOR_CONTROL_COLOR_CAPABILITIES_XY_SUPPORTED};
            cluster = ezb_zcl_color_control_create_cluster_desc(
                    entry->role == EZB_ZCL_CLUSTER_SERVER ? &server : NULL, entry->role);
        }
        if (!cluster) return ESP_ERR_NO_MEM;
        esp_err_t result = ezb_af_endpoint_add_cluster_desc(endpoint, cluster);
        if (result != ESP_OK) {
            ezb_zcl_free_cluster_desc(cluster);
            return result;
        }
    }
    return ESP_OK;
}

static esp_err_t add_ota_clusters(ezb_af_device_desc_t device)
{
    if (ota_client.configured) {
        ezb_af_ep_desc_t endpoint = ezb_af_device_get_endpoint_desc(device, ota_client.endpoint);
        if (!endpoint || ezb_af_endpoint_get_cluster_desc(endpoint,
                EZB_ZCL_CLUSTER_ID_OTA_UPGRADE, EZB_ZCL_CLUSTER_CLIENT))
            return ESP_ERR_INVALID_ARG;
        ezb_zcl_ota_upgrade_cluster_client_config_t config = {
            .upgrade_server_id = EZB_ZCL_OTA_UPGRADE_UPGRADE_SERVER_ID_DEFAULT_VALUE,
            .file_offset = 0,
            .image_upgrade_status = EZB_ZCL_OTA_UPGRADE_IMAGE_UPGRADE_STATUS_NORMAL,
            .manufacturer_id = ota_client.manufacturer,
            .image_type_id = ota_client.image_type};
        ezb_zcl_cluster_desc_t cluster = ezb_zcl_ota_upgrade_create_cluster_desc(
                &config, EZB_ZCL_CLUSTER_CLIENT);
        if (!cluster) return ESP_ERR_NO_MEM;
        esp_err_t result = ezb_zcl_ota_upgrade_cluster_desc_add_attr(cluster,
                EZB_ZCL_ATTR_OTA_UPGRADE_CURRENT_FILE_VERSION_ID, &ota_client.version);
        if (result == ESP_OK) result = ezb_af_endpoint_add_cluster_desc(endpoint, cluster);
        if (result != ESP_OK) {
            ezb_zcl_free_cluster_desc(cluster);
            return result;
        }
    }
    if (ota_server.configured) {
        ezb_af_ep_desc_t endpoint = ezb_af_device_get_endpoint_desc(device, ota_server.endpoint);
        if (!endpoint || ezb_af_endpoint_get_cluster_desc(endpoint,
                EZB_ZCL_CLUSTER_ID_OTA_UPGRADE, EZB_ZCL_CLUSTER_SERVER))
            return ESP_ERR_INVALID_ARG;
        ezb_zcl_ota_upgrade_cluster_server_config_t config = {
            .ota_upgrade_file_table_size = 1};
        ezb_zcl_cluster_desc_t cluster = ezb_zcl_ota_upgrade_create_cluster_desc(
                &config, EZB_ZCL_CLUSTER_SERVER);
        if (!cluster) return ESP_ERR_NO_MEM;
        esp_err_t result = ezb_af_endpoint_add_cluster_desc(endpoint, cluster);
        if (result != ESP_OK) {
            ezb_zcl_free_cluster_desc(cluster);
            return result;
        }
    }
    return ESP_OK;
}

static esp_err_t add_custom_endpoints(ezb_af_device_desc_t device)
{
    for (unsigned i = 0; i < custom_endpoint_count; ++i) {
        const zb_custom_endpoint_t *entry = &custom_endpoints[i];
        ezb_af_ep_config_t config = {
            .ep_id = entry->endpoint, .app_profile_id = entry->profile,
            .app_device_id = entry->device, .app_device_version = entry->version};
        ezb_af_ep_desc_t endpoint = ezb_af_create_endpoint_desc(&config);
        if (!endpoint) return ESP_ERR_NO_MEM;
        ezb_zcl_basic_cluster_server_config_t basic = {
            .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
            .power_source = EZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE};
        ezb_zcl_cluster_desc_t cluster = ezb_zcl_basic_create_cluster_desc(&basic,
                EZB_ZCL_CLUSTER_SERVER);
        if (!cluster) {
            ezb_af_free_endpoint_desc(endpoint);
            return ESP_ERR_NO_MEM;
        }
        esp_err_t result = ezb_af_endpoint_add_cluster_desc(endpoint, cluster);
        if (result != ESP_OK) {
            ezb_zcl_free_cluster_desc(cluster);
            ezb_af_free_endpoint_desc(endpoint);
            return result;
        }
        result = ezb_af_device_add_endpoint_desc(device, endpoint);
        if (result != ESP_OK) {
            ezb_af_free_endpoint_desc(endpoint);
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
    if (result == ESP_OK) result = add_custom_endpoints(device);
    if (result == ESP_OK) result = add_humidity_clusters(device);
    if (result == ESP_OK) result = add_illuminance_clusters(device);
    if (result == ESP_OK) result = add_measurement_clusters(device);
    if (result == ESP_OK) result = add_ota_clusters(device);
    if (result == ESP_OK) result = add_custom_clusters(device);
    if (result != ESP_OK) {
        ezb_af_free_device_desc(device);
        return result;
    }
    result = ezb_af_device_desc_register(device);
    if (result != ESP_OK) {
        ezb_af_free_device_desc(device);
        return result;
    }
    return ota_client.configured ?
            ezb_zcl_ota_upgrade_set_download_block_size(ota_client.endpoint, 223) : ESP_OK;
}

static void stack_task(void *context)
{
    (void)context;
    esp_zigbee_config_t config = {
        .device_config = {
            .device_type = requested.role == 1 ? EZB_NWK_DEVICE_TYPE_COORDINATOR :
                    requested.role == 2 ? EZB_NWK_DEVICE_TYPE_ROUTER : EZB_NWK_DEVICE_TYPE_END_DEVICE,
            .install_code_policy = install_codes_enabled,
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
    if (requested.role >= 3) ezb_nwk_set_rx_on_when_idle(requested.role != 4);
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
    if (ota_server_file) {
        ezb_zcl_ota_upgrade_remove_ota_file(ota_server.endpoint, ota_server_file);
        ota_server_file = NULL;
    }
    ota_abort_download();
    if (initialized) (void)esp_zigbee_deinit();
    (void)nvs_flash_deinit_partition("zb_storage");
    if (restore_pm_config) {
        (void)esp_pm_configure(&previous_pm_config);
        restore_pm_config = false;
    }
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
    for (unsigned i = 0; i < custom_endpoint_count; ++i) {
        if (custom_endpoints[i].endpoint == endpoint) {
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

int bmx_esp32_zigbee_core_add_custom_endpoint(int endpoint, int profile_id,
        int device_id, int version)
{
    if (endpoint < 1 || endpoint > 240 || profile_id < 1 || profile_id > 65534 ||
            device_id < 0 || device_id > 65535 || version < 0 || version > 15)
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
    for (unsigned i = 0; i < custom_endpoint_count; ++i) {
        if (custom_endpoints[i].endpoint == endpoint) {
            portEXIT_CRITICAL(&state_lock);
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (custom_endpoint_count == ZB_MAX_CUSTOM_ENDPOINTS) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_NO_MEM;
    }
    custom_endpoints[custom_endpoint_count++] = (zb_custom_endpoint_t){
        .endpoint = (uint8_t)endpoint, .profile = (uint16_t)profile_id,
        .device = (uint16_t)device_id, .version = (uint8_t)version};
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_clear_custom_endpoints(void)
{
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    custom_endpoint_count = 0;
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

int bmx_esp32_zigbee_core_add_illuminance_cluster(int endpoint, int role)
{
    if (endpoint < 1 || endpoint > 240 ||
            (role != EZB_ZCL_CLUSTER_SERVER && role != EZB_ZCL_CLUSTER_CLIENT))
        return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    for (unsigned i = 0; i < illuminance_cluster_count; ++i) {
        if (illuminance_clusters[i].endpoint == endpoint &&
                illuminance_clusters[i].role == role) {
            portEXIT_CRITICAL(&state_lock);
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (illuminance_cluster_count == ZB_MAX_ILLUMINANCE_CLUSTERS) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_NO_MEM;
    }
    illuminance_clusters[illuminance_cluster_count++] = (zb_illuminance_cluster_t){
        .endpoint = (uint8_t)endpoint, .role = (uint8_t)role};
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_clear_illuminance_clusters(void)
{
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    illuminance_cluster_count = 0;
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

static int add_measurement_cluster(int endpoint, int role, int cluster, int sensor_type)
{
    if (endpoint < 1 || endpoint > 240 ||
            (role != EZB_ZCL_CLUSTER_SERVER && role != EZB_ZCL_CLUSTER_CLIENT) ||
            (cluster != EZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT &&
             cluster != EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING &&
             cluster != EZB_ZCL_CLUSTER_ID_POWER_CONFIG &&
             cluster != EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT &&
             cluster != EZB_ZCL_CLUSTER_ID_LEVEL &&
             cluster != EZB_ZCL_CLUSTER_ID_COLOR_CONTROL) ||
            (cluster == EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING &&
             (sensor_type < 0 || sensor_type > 3))) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    for (unsigned i = 0; i < measurement_cluster_count; ++i) {
        if (measurement_clusters[i].endpoint == endpoint &&
                measurement_clusters[i].role == role &&
                measurement_clusters[i].cluster == cluster) {
            portEXIT_CRITICAL(&state_lock);
            return ESP_ERR_INVALID_ARG;
        }
    }
    if (measurement_cluster_count == ZB_MAX_ADDITIONAL_MEASUREMENT_CLUSTERS) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_NO_MEM;
    }
    measurement_clusters[measurement_cluster_count++] = (zb_measurement_cluster_t){
        .endpoint = (uint8_t)endpoint, .role = (uint8_t)role,
        .sensor_type = (uint8_t)sensor_type, .cluster = (uint16_t)cluster};
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

static int clear_measurement_clusters(int cluster)
{
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    unsigned keep = 0;
    for (unsigned i = 0; i < measurement_cluster_count; ++i)
        if (measurement_clusters[i].cluster != cluster)
            measurement_clusters[keep++] = measurement_clusters[i];
    measurement_cluster_count = (uint8_t)keep;
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_add_pressure_cluster(int endpoint, int role)
{
    return add_measurement_cluster(endpoint, role, EZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, 0);
}

int bmx_esp32_zigbee_core_clear_pressure_clusters(void)
{
    return clear_measurement_clusters(EZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT);
}

int bmx_esp32_zigbee_core_add_occupancy_cluster(int endpoint, int role, int sensor_type)
{
    return add_measurement_cluster(endpoint, role, EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING,
            sensor_type);
}

int bmx_esp32_zigbee_core_clear_occupancy_clusters(void)
{
    return clear_measurement_clusters(EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING);
}

int bmx_esp32_zigbee_core_add_power_config_cluster(int endpoint, int role)
{
    return add_measurement_cluster(endpoint, role, EZB_ZCL_CLUSTER_ID_POWER_CONFIG, 0);
}

int bmx_esp32_zigbee_core_clear_power_config_clusters(void)
{
    return clear_measurement_clusters(EZB_ZCL_CLUSTER_ID_POWER_CONFIG);
}

int bmx_esp32_zigbee_core_add_electrical_measurement_cluster(int endpoint, int role)
{
    return add_measurement_cluster(endpoint, role, EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, 0);
}

int bmx_esp32_zigbee_core_clear_electrical_measurement_clusters(void)
{
    return clear_measurement_clusters(EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT);
}

int bmx_esp32_zigbee_core_add_level_cluster(int endpoint, int role)
{
    return add_measurement_cluster(endpoint, role, EZB_ZCL_CLUSTER_ID_LEVEL, 0);
}

int bmx_esp32_zigbee_core_clear_level_clusters(void)
{
    return clear_measurement_clusters(EZB_ZCL_CLUSTER_ID_LEVEL);
}

int bmx_esp32_zigbee_core_add_color_control_cluster(int endpoint, int role)
{
    return add_measurement_cluster(endpoint, role, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, 0);
}

int bmx_esp32_zigbee_core_clear_color_control_clusters(void)
{
    return clear_measurement_clusters(EZB_ZCL_CLUSTER_ID_COLOR_CONTROL);
}

int bmx_esp32_zigbee_core_add_ota_client(int endpoint, int manufacturer,
        int image_type, uint32_t version)
{
    if (endpoint < 1 || endpoint > 240 || manufacturer < 0 || manufacturer > 65534 ||
            image_type < 0 || image_type > 65534 || version == UINT32_MAX)
        return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    ota_client = (zb_ota_cluster_t){.endpoint = (uint8_t)endpoint,
        .manufacturer = (uint16_t)manufacturer, .image_type = (uint16_t)image_type,
        .version = version, .configured = true};
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_add_ota_server(int endpoint)
{
    if (endpoint < 1 || endpoint > 240) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    ota_server = (zb_ota_cluster_t){.endpoint = (uint8_t)endpoint, .configured = true};
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_clear_ota_clusters(void)
{
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    memset(&ota_client, 0, sizeof(ota_client));
    memset(&ota_server, 0, sizeof(ota_server));
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

static bool ota_file_valid(const uint8_t *image, uint32_t size)
{
    if (!image || size <= 62 || ota_u32(image) != EZB_ZCL_OTA_FILE_IDENTIFIER ||
            ota_u16(image + 4) != EZB_ZCL_OTA_FILE_HEADER_VERSION ||
            ota_u32(image + 52) != size) return false;
    uint16_t header = ota_u16(image + 6);
    return header == 56 && ota_u16(image + 8) == 0 &&
            ota_u16(image + header) == EZB_ZCL_OTA_FILE_TAG_ID_UPGRADE_IMAGE &&
            ota_u32(image + header + 2) == size - header - 6;
}

int bmx_esp32_zigbee_core_ota_serve_image(int endpoint, const uint8_t *image,
        uint32_t size)
{
    if (!ota_server.configured || endpoint != ota_server.endpoint ||
            !ota_file_valid(image, size)) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    if (ota_server_file) {
        esp_zigbee_lock_release();
        return ESP_ERR_INVALID_STATE;
    }
    ota_server_file = ezb_zcl_ota_upgrade_add_ota_file((uint8_t)endpoint, image, size);
    if (ota_server_file) {
        ota_served_manufacturer = ota_u16(image + 10);
        ota_served_image_type = ota_u16(image + 12);
        ota_served_version = ota_u32(image + 14);
    }
    esp_zigbee_lock_release();
    return ota_server_file ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_ota_remove_image(int endpoint)
{
    if (!ota_server.configured || endpoint != ota_server.endpoint) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    if (ota_server_file) {
        ezb_zcl_ota_upgrade_remove_ota_file((uint8_t)endpoint, ota_server_file);
        ota_server_file = NULL;
        ota_served_version = 0;
    }
    esp_zigbee_lock_release();
    return ESP_OK;
}

int bmx_esp32_zigbee_core_ota_notify(int source_ep, int destination_addr,
        int destination_ep, int manufacturer, int image_type, uint32_t version)
{
    if (!ota_server.configured || source_ep != ota_server.endpoint || !ota_server_file ||
            destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240 || manufacturer < 0 ||
            manufacturer > 65534 || image_type < 0 || image_type > 65534 ||
            manufacturer != ota_served_manufacturer ||
            image_type != ota_served_image_type || version != ota_served_version)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_ota_upgrade_image_notify_cmd_t request = {.cmd_ctrl = {
        .dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
        .dst_ep = (uint8_t)destination_ep, .src_ep = (uint8_t)source_ep},
        .payload = {.type = EZB_ZCL_OTA_UPGRADE_IMAGE_NOTIFY_PAYLOAD_TYPE_JITTER_CODE_IMAGE_VERSION,
                    .query_jitter = 100, .manuf_code = (uint16_t)manufacturer,
                    .image_type = (uint16_t)image_type, .file_version = version}};
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_ota_upgrade_image_notify_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_ota_query(int source_ep, int destination_addr,
        int destination_ep)
{
    if (!ota_client.configured || source_ep != ota_client.endpoint ||
            destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_ota_upgrade_query_next_image_req_cmd_t request = {.cmd_ctrl = {
        .dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
        .dst_ep = (uint8_t)destination_ep, .src_ep = (uint8_t)source_ep},
        .payload = {.fc = 0, .manuf_code = ota_client.manufacturer,
                    .image_type = ota_client.image_type,
                    .file_version = ota_client.version}};
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_ota_upgrade_query_next_image_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
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

static int add_custom_bytes(int endpoint, int cluster_id, int attribute_id, int access,
        const uint8_t *text, int length, uint8_t type)
{
    if (endpoint < 1 || endpoint > 240 || cluster_id < EZB_ZCL_CLUSTER_ID_MIN_CUSTOM ||
            cluster_id > 65535 || attribute_id < 0 || attribute_id > 65535 ||
            access < 1 || access > 7 || !(access & EZB_ZCL_ATTR_ACCESS_READ) ||
            length < 0 || length > ZB_MAX_STRING_BYTES || (length && !text))
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
    zb_custom_attribute_t *attribute = &custom_attributes[custom_attribute_count++];
    memset(attribute, 0, sizeof(*attribute));
    attribute->endpoint = (uint8_t)endpoint;
    attribute->cluster = (uint16_t)cluster_id;
    attribute->attribute = (uint16_t)attribute_id;
    attribute->type = type;
    attribute->access = (uint8_t)access;
    attribute->text[0] = (uint8_t)length;
    if (length) memcpy(attribute->text + 1, text, length);
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

int bmx_esp32_zigbee_core_add_custom_string(int endpoint, int cluster_id,
        int attribute_id, int access, const uint8_t *text, int length)
{
    return add_custom_bytes(endpoint, cluster_id, attribute_id, access, text, length,
            EZB_ZCL_ATTR_TYPE_STRING);
}

int bmx_esp32_zigbee_core_add_custom_octets(int endpoint, int cluster_id,
        int attribute_id, int access, const uint8_t *bytes, int length)
{
    return add_custom_bytes(endpoint, cluster_id, attribute_id, access, bytes, length,
            EZB_ZCL_ATTR_TYPE_OCTSTR);
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
    for (unsigned i = 0; i < custom_endpoint_count; ++i) {
        if (custom_endpoints[i].endpoint == endpoint) {
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
    esp_err_t result = ESP_OK;
    if (role == 4) {
#if CONFIG_PM_ENABLE && CONFIG_FREERTOS_USE_TICKLESS_IDLE
        result = esp_pm_get_configuration(&previous_pm_config);
        if (result != ESP_OK) goto failed;
        esp_pm_config_t sleepy_pm_config = previous_pm_config;
        sleepy_pm_config.light_sleep_enable = true;
        result = esp_pm_configure(&sleepy_pm_config);
        if (result != ESP_OK) goto failed;
        restore_pm_config = true;
#else
        result = ESP_ERR_NOT_SUPPORTED;
        goto failed;
#endif
    }
    result = nvs_flash_init_partition("zb_storage");
    if (result != ESP_OK) goto failed;
    if (xTaskCreate(stack_task, "zigbee_stack", 6144, NULL, 5, NULL) != pdPASS) {
        result = ESP_ERR_NO_MEM;
        (void)nvs_flash_deinit_partition("zb_storage");
        goto failed;
    }
    return ESP_OK;
failed:
    if (restore_pm_config) {
        (void)esp_pm_configure(&previous_pm_config);
        restore_pm_config = false;
    }
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

int bmx_esp32_zigbee_core_enable_install_codes(int enabled)
{
    portENTER_CRITICAL(&state_lock);
    if (state != 0) {
        portEXIT_CRITICAL(&state_lock);
        return ESP_ERR_INVALID_STATE;
    }
    install_codes_enabled = enabled != 0;
    portEXIT_CRITICAL(&state_lock);
    return ESP_OK;
}

static bool install_code_type(int length, ezb_secur_ic_type_t *type)
{
    switch (length) {
        case 8: *type = EZB_SECUR_IC_TYPE_48; return true;
        case 10: *type = EZB_SECUR_IC_TYPE_64; return true;
        case 14: *type = EZB_SECUR_IC_TYPE_96; return true;
        case 18: *type = EZB_SECUR_IC_TYPE_128; return true;
        default: return false;
    }
}

int bmx_esp32_zigbee_core_ieee_address(uint64_t *address)
{
    if (!address) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_extaddr_t ieee;
    ezb_nwk_get_extended_address(&ieee);
    *address = ieee.u64;
    esp_zigbee_lock_release();
    return ESP_OK;
}

int bmx_esp32_zigbee_core_require_install_codes(int required)
{
    if (bmx_esp32_zigbee_core_state() != 2 || requested.role != 1)
        return ESP_ERR_INVALID_STATE;
    if (required && !install_codes_enabled) return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_secur_set_ic_required(required != 0);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : esp_zigbee_err_to_esp(result);
}

int bmx_esp32_zigbee_core_require_link_key_exchange(int required)
{
    if (required != 0 && required != 1) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_secur_set_tclk_exchange_required(required != 0);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : esp_zigbee_err_to_esp(result);
}

int bmx_esp32_zigbee_core_allow_well_known_key_rejoins(int allow)
{
    if (allow != 0 && allow != 1) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2 || requested.role != 1)
        return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_secur_tcpol_set_allow_rejoins_with_well_known_key(allow != 0);
    bool applied = ezb_secur_tcpol_get_allow_rejoins_with_well_known_key() == (allow != 0);
    esp_zigbee_lock_release();
    return applied ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_add_joiner_install_code(uint64_t address,
        const uint8_t *code, int length)
{
    ezb_secur_ic_type_t type;
    if (!code || !install_code_type(length, &type) || address == 0 || address == UINT64_MAX)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2 || requested.role != 1 || !install_codes_enabled)
        return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_extaddr_t ieee = {.u64 = address};
    ezb_err_t result = ezb_secur_ic_add(&ieee, type, code);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : esp_zigbee_err_to_esp(result);
}

int bmx_esp32_zigbee_core_remove_joiner_install_code(uint64_t address)
{
    if (address == 0 || address == UINT64_MAX) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2 || requested.role != 1 || !install_codes_enabled)
        return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_extaddr_t ieee = {.u64 = address};
    ezb_err_t result = ezb_secur_ic_remove(&ieee);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : esp_zigbee_err_to_esp(result);
}

int bmx_esp32_zigbee_core_set_local_install_code(const uint8_t *code, int length)
{
    ezb_secur_ic_type_t type;
    if (!code || !install_code_type(length, &type)) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2 || requested.role == 1 || !install_codes_enabled)
        return ESP_ERR_INVALID_STATE;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_secur_ic_set(type, code);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : esp_zigbee_err_to_esp(result);
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

static int write_bytes(int source_ep, int destination_addr, int destination_ep,
        int cluster_id, int attribute_id, const uint8_t *text, int length, uint8_t type)
{
    if (!endpoint_kind(source_ep) || destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240 || cluster_id < 0 || cluster_id > 65535 ||
            attribute_id < 0 || attribute_id > 65535 || length < 0 ||
            length > ZB_MAX_STRING_BYTES || (length && !text)) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint8_t value[ZB_MAX_STRING_BYTES + 1u] = {(uint8_t)length};
    if (length) memcpy(value + 1, text, length);
    ezb_zcl_attribute_t attribute = {.id = (uint16_t)attribute_id,
                                     .data = {.type = type,
                                              .size = (uint16_t)(length + 1), .value = value}};
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

int bmx_esp32_zigbee_core_write_string(int source_ep, int destination_addr, int destination_ep,
        int cluster_id, int attribute_id, const uint8_t *text, int length)
{
    return write_bytes(source_ep, destination_addr, destination_ep, cluster_id, attribute_id,
            text, length, EZB_ZCL_ATTR_TYPE_STRING);
}

int bmx_esp32_zigbee_core_write_octets(int source_ep, int destination_addr, int destination_ep,
        int cluster_id, int attribute_id, const uint8_t *bytes, int length)
{
    return write_bytes(source_ep, destination_addr, destination_ep, cluster_id, attribute_id,
            bytes, length, EZB_ZCL_ATTR_TYPE_OCTSTR);
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

int bmx_esp32_zigbee_core_set_local_illuminance(int endpoint, int encoded_value)
{
    if (endpoint < 1 || endpoint > 240 || encoded_value < 0 || encoded_value > 0xfffe)
        return ESP_ERR_INVALID_ARG;
    bool server = false;
    for (unsigned i = 0; i < illuminance_cluster_count; ++i)
        if (illuminance_clusters[i].endpoint == endpoint &&
                illuminance_clusters[i].role == EZB_ZCL_CLUSTER_SERVER) server = true;
    if (!server) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint16_t value = (uint16_t)encoded_value;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            EZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID,
            EZB_ZCL_STD_MANUF_CODE, &value, false);
    esp_zigbee_lock_release();
    return result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

static bool has_measurement_server(int endpoint, int cluster)
{
    for (unsigned i = 0; i < measurement_cluster_count; ++i)
        if (measurement_clusters[i].endpoint == endpoint &&
                measurement_clusters[i].cluster == cluster &&
                measurement_clusters[i].role == EZB_ZCL_CLUSTER_SERVER) return true;
    return false;
}

int bmx_esp32_zigbee_core_set_local_pressure(int endpoint, int kilopascals)
{
    if (endpoint < 1 || endpoint > 240 || kilopascals < INT16_MIN ||
            kilopascals > INT16_MAX ||
            !has_measurement_server(endpoint, EZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT))
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    int16_t value = (int16_t)kilopascals;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            EZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MEASURED_VALUE_ID,
            EZB_ZCL_STD_MANUF_CODE, &value, false);
    esp_zigbee_lock_release();
    return result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_set_local_occupancy(int endpoint, int occupied)
{
    if (endpoint < 1 || endpoint > 240 || (occupied != 0 && occupied != 1) ||
            !has_measurement_server(endpoint, EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING))
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint8_t value = (uint8_t)occupied;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            EZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID,
            EZB_ZCL_STD_MANUF_CODE, &value, false);
    esp_zigbee_lock_release();
    return result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

static int set_local_battery_attribute(int endpoint, int attribute, int value)
{
    if (endpoint < 1 || endpoint > 240 || value < 0 || value > 0xff ||
            !has_measurement_server(endpoint, EZB_ZCL_CLUSTER_ID_POWER_CONFIG))
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint8_t encoded = (uint8_t)value;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            EZB_ZCL_CLUSTER_ID_POWER_CONFIG, EZB_ZCL_CLUSTER_SERVER,
            (uint16_t)attribute, EZB_ZCL_STD_MANUF_CODE, &encoded, false);
    esp_zigbee_lock_release();
    return result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_set_local_battery_voltage(int endpoint, int decivolts)
{
    return set_local_battery_attribute(endpoint,
            EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, decivolts);
}

int bmx_esp32_zigbee_core_set_local_battery_percentage(int endpoint, int half_percent)
{
    if (half_percent > 200 && half_percent != 0xff) return ESP_ERR_INVALID_ARG;
    return set_local_battery_attribute(endpoint,
            EZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, half_percent);
}

static int set_local_electrical_attribute(int endpoint, int attribute, int value)
{
    if (endpoint < 1 || endpoint > 240 || !has_measurement_server(endpoint,
            EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT)) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint16_t encoded = (uint16_t)value;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            EZB_ZCL_CLUSTER_ID_ELECTRICAL_MEASUREMENT, EZB_ZCL_CLUSTER_SERVER,
            (uint16_t)attribute, EZB_ZCL_STD_MANUF_CODE, &encoded, false);
    esp_zigbee_lock_release();
    return result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_set_local_rms_voltage(int endpoint, int volts)
{
    if (volts < 0 || volts > 0xffff) return ESP_ERR_INVALID_ARG;
    return set_local_electrical_attribute(endpoint,
            EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_VOLTAGE_ID, volts);
}

int bmx_esp32_zigbee_core_set_local_rms_current(int endpoint, int milliamps)
{
    if (milliamps < 0 || milliamps > 0xffff) return ESP_ERR_INVALID_ARG;
    return set_local_electrical_attribute(endpoint,
            EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_RMS_CURRENT_ID, milliamps);
}

int bmx_esp32_zigbee_core_set_local_active_power(int endpoint, int watts)
{
    if (watts < INT16_MIN || watts > INT16_MAX) return ESP_ERR_INVALID_ARG;
    return set_local_electrical_attribute(endpoint,
            EZB_ZCL_ATTR_ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ID, watts);
}

int bmx_esp32_zigbee_core_set_local_level(int endpoint, int level)
{
    if (endpoint < 1 || endpoint > 240 || level < 0 || level > 254 ||
            !has_measurement_server(endpoint, EZB_ZCL_CLUSTER_ID_LEVEL))
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint8_t encoded = (uint8_t)level;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, EZB_ZCL_STD_MANUF_CODE, &encoded, false);
    esp_zigbee_lock_release();
    return result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_move_to_level(int source_ep, int destination_addr,
        int destination_ep, int level, int transition_tenths, int with_onoff)
{
    if (source_ep < 1 || source_ep > 240 || destination_addr < 0 ||
            destination_addr > 65535 || destination_ep < 1 || destination_ep > 240 ||
            level < 0 || level > 254 || transition_tenths < 0 ||
            transition_tenths > 65535 || (with_onoff != 0 && with_onoff != 1))
        return ESP_ERR_INVALID_ARG;
    bool client = false;
    for (unsigned i = 0; i < measurement_cluster_count; ++i)
        if (measurement_clusters[i].endpoint == source_ep &&
                measurement_clusters[i].cluster == EZB_ZCL_CLUSTER_ID_LEVEL &&
                measurement_clusters[i].role == EZB_ZCL_CLUSTER_CLIENT) client = true;
    if (!client) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_level_move_to_level_cmd_t request = {.cmd_ctrl = {
        .dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
        .dst_ep = (uint8_t)destination_ep, .src_ep = (uint8_t)source_ep},
        .payload = {.level = (uint8_t)level,
                    .transition_time = (uint16_t)transition_tenths}};
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = with_onoff ?
            ezb_zcl_level_move_to_level_with_on_off_cmd_req(&request) :
            ezb_zcl_level_move_to_level_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_set_local_color(int endpoint, int x, int y)
{
    if (endpoint < 1 || endpoint > 240 || x < 0 || x > 65535 || y < 0 || y > 65535 ||
            !has_measurement_server(endpoint, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL))
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    uint16_t encoded_x = (uint16_t)x, encoded_y = (uint16_t)y;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t x_result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, EZB_ZCL_CLUSTER_SERVER,
            EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, EZB_ZCL_STD_MANUF_CODE,
            &encoded_x, false);
    ezb_zcl_status_t y_result = x_result == EZB_ZCL_STATUS_SUCCESS ?
            ezb_zcl_set_attr_value((uint8_t)endpoint, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                    EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID,
                    EZB_ZCL_STD_MANUF_CODE, &encoded_y, false) : x_result;
    esp_zigbee_lock_release();
    return y_result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_move_to_color(int source_ep, int destination_addr,
        int destination_ep, int x, int y, int transition_tenths)
{
    if (source_ep < 1 || source_ep > 240 || destination_addr < 0 ||
            destination_addr > 65535 || destination_ep < 1 || destination_ep > 240 ||
            x < 0 || x > 65535 || y < 0 || y > 65535 ||
            transition_tenths < 0 || transition_tenths > 65535)
        return ESP_ERR_INVALID_ARG;
    bool client = false;
    for (unsigned i = 0; i < measurement_cluster_count; ++i)
        if (measurement_clusters[i].endpoint == source_ep &&
                measurement_clusters[i].cluster == EZB_ZCL_CLUSTER_ID_COLOR_CONTROL &&
                measurement_clusters[i].role == EZB_ZCL_CLUSTER_CLIENT) client = true;
    if (!client) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_color_control_move_to_color_cmd_t request = {.cmd_ctrl = {
        .dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
        .dst_ep = (uint8_t)destination_ep, .src_ep = (uint8_t)source_ep},
        .payload = {.color_x = (uint16_t)x, .color_y = (uint16_t)y,
                    .transition_time = (uint16_t)transition_tenths}};
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_color_control_move_to_color_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
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
    if (!attribute || attribute->type == EZB_ZCL_ATTR_TYPE_STRING ||
            attribute->type == EZB_ZCL_ATTR_TYPE_OCTSTR) return ESP_ERR_INVALID_ARG;
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            (uint16_t)cluster_id, EZB_ZCL_CLUSTER_SERVER, (uint16_t)attribute_id,
            EZB_ZCL_STD_MANUF_CODE, &value, false);
    if (result == EZB_ZCL_STATUS_SUCCESS) attribute->value = value;
    esp_zigbee_lock_release();
    return result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

static int set_custom_bytes(int endpoint, int cluster_id, int attribute_id,
        const uint8_t *text, int length, uint8_t type)
{
    if (!endpoint_kind(endpoint) || cluster_id < EZB_ZCL_CLUSTER_ID_MIN_CUSTOM ||
            cluster_id > 65535 || attribute_id < 0 || attribute_id > 65535 ||
            length < 0 || length > ZB_MAX_STRING_BYTES || (length && !text))
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
    if (!attribute || attribute->type != type) return ESP_ERR_INVALID_ARG;
    uint8_t value[ZB_MAX_STRING_BYTES + 1u] = {(uint8_t)length};
    if (length) memcpy(value + 1, text, length);
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_zcl_status_t result = ezb_zcl_set_attr_value((uint8_t)endpoint,
            (uint16_t)cluster_id, EZB_ZCL_CLUSTER_SERVER, (uint16_t)attribute_id,
            EZB_ZCL_STD_MANUF_CODE, value, false);
    if (result == EZB_ZCL_STATUS_SUCCESS)
        memcpy(attribute->text, value, (size_t)length + 1u);
    esp_zigbee_lock_release();
    return result == EZB_ZCL_STATUS_SUCCESS ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_set_custom_string(int endpoint, int cluster_id,
        int attribute_id, const uint8_t *text, int length)
{
    return set_custom_bytes(endpoint, cluster_id, attribute_id, text, length,
            EZB_ZCL_ATTR_TYPE_STRING);
}

int bmx_esp32_zigbee_core_set_custom_octets(int endpoint, int cluster_id,
        int attribute_id, const uint8_t *bytes, int length)
{
    return set_custom_bytes(endpoint, cluster_id, attribute_id, bytes, length,
            EZB_ZCL_ATTR_TYPE_OCTSTR);
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

int bmx_esp32_zigbee_core_remove_all_groups(int source_ep, int destination_addr,
        int destination_ep)
{
    if (!endpoint_kind(source_ep) || destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240) return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_groups_remove_all_groups_cmd_t request = {
        .cmd_ctrl = {.dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
                     .dst_ep = (uint8_t)destination_ep, .src_ep = (uint8_t)source_ep},
    };
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_groups_remove_all_groups_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_view_group(int source_ep, int destination_addr, int destination_ep,
        int group_id)
{
    if (!endpoint_kind(source_ep) || destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240 || group_id < 1 || group_id > 0xfff7)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_groups_view_group_cmd_t request = {
        .cmd_ctrl = {.dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
                     .dst_ep = (uint8_t)destination_ep, .src_ep = (uint8_t)source_ep},
        .payload = {.group_id = (uint16_t)group_id},
    };
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_groups_view_group_cmd_req(&request);
    esp_zigbee_lock_release();
    return result == EZB_ERR_NONE ? ESP_OK : ESP_FAIL;
}

int bmx_esp32_zigbee_core_get_group_membership(int source_ep, int destination_addr,
        int destination_ep)
{
    if (!endpoint_kind(source_ep) || destination_addr < 0 || destination_addr > 65535 ||
            destination_ep < 1 || destination_ep > 240)
        return ESP_ERR_INVALID_ARG;
    if (bmx_esp32_zigbee_core_state() != 2) return ESP_ERR_INVALID_STATE;
    ezb_zcl_groups_get_group_membership_cmd_t request = {
        .cmd_ctrl = {.dst_addr = EZB_ADDRESS_SHORT((uint16_t)destination_addr),
                     .dst_ep = (uint8_t)destination_ep, .src_ep = (uint8_t)source_ep},
        .payload = {.group_count = 0, .group_list = NULL},
    };
    if (!esp_zigbee_lock_acquire(pdMS_TO_TICKS(1000))) return ESP_ERR_TIMEOUT;
    ezb_err_t result = ezb_zcl_groups_get_group_membership_cmd_req(&request);
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
        int *channel, int *factory_new, int *data_type, int *value_size,
        uint8_t *text, int text_capacity)
{
    if (!kind || !status || !signal || !endpoint || !cluster || !attribute || !value ||
            !short_addr || !pan_id || !channel || !factory_new || !data_type || !value_size ||
            !text || text_capacity < ZB_MAX_STRING_BYTES) return 0;
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
    if ((event.data_type == EZB_ZCL_ATTR_TYPE_STRING ||
            event.data_type == EZB_ZCL_ATTR_TYPE_OCTSTR) && event.value_size > 0)
        memcpy(text, event.text, (size_t)event.value_size);
    return 1;
}

uint32_t bmx_esp32_zigbee_core_dropped(void)
{
    portENTER_CRITICAL(&queue_lock);
    uint32_t dropped = queue_dropped;
    portEXIT_CRITICAL(&queue_lock);
    return dropped;
}
