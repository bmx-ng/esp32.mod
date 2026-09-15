#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "host/ble_gap.h"
#include "host/ble_att.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nvs_flash.h"
#include "blitzmax/embedded_runtime.h"
#include "blitzmax/embedded_system.h"

#define BMX_BLE_EVENT_CAPACITY 64u
#define BMX_BLE_EVENT_MASK (BMX_BLE_EVENT_CAPACITY - 1u)
#define BMX_BLE_DATA_CAPACITY 512u
#define BMX_BLE_READY 1
#define BMX_BLE_SCAN_RESULT 2
#define BMX_BLE_SCAN_COMPLETE 3
#define BMX_BLE_RESET 4
#define BMX_BLE_CONNECTED 5
#define BMX_BLE_DISCONNECTED 6
#define BMX_BLE_GATT_WRITE 7
#define BMX_BLE_GATT_SUBSCRIBE 8
#define BMX_BLE_ADV_COMPLETE 9
#define BMX_BLE_SERVICE_DISCOVERED 10
#define BMX_BLE_SERVICE_DISCOVERY_COMPLETE 11
#define BMX_BLE_CHARACTERISTIC_DISCOVERED 12
#define BMX_BLE_CHARACTERISTIC_DISCOVERY_COMPLETE 13
#define BMX_BLE_DESCRIPTOR_DISCOVERED 14
#define BMX_BLE_DESCRIPTOR_DISCOVERY_COMPLETE 15
#define BMX_BLE_READ_COMPLETE 16
#define BMX_BLE_WRITE_COMPLETE 17
#define BMX_BLE_NOTIFICATION 18
#define BMX_BLE_MTU_CHANGED 19
#define BMX_BLE_GATT_UPDATE_COMPLETE 20
#define BMX_BLE_SECURITY_CHANGED 21
#define BMX_BLE_PASSKEY_ACTION 22
#define BMX_BLE_CONNECTION_UPDATED 23
#define BMX_BLE_PHY_UPDATED 24
#define BMX_BLE_GATT_MAX_SERVICES 8u
#define BMX_BLE_GATT_MAX_CHARACTERISTICS_PER_SERVICE 8u
#define BMX_BLE_GATT_MAX_CHARACTERISTICS 32u
#define BMX_BLE_MAX_PENDING_READS 4u
/* ESP-IDF permits at most 70 NimBLE connections on supported controllers. */
#define BMX_BLE_MAX_CONNECTIONS 70u

#define BMX_BLE_GATT_READ 0x01u
#define BMX_BLE_GATT_WRITE_FLAG 0x02u
#define BMX_BLE_GATT_NOTIFY 0x04u
#define BMX_BLE_GATT_INDICATE 0x08u
#define BMX_BLE_GATT_WRITE_NO_RESPONSE 0x10u
#define BMX_BLE_GATT_READ_ENCRYPTED 0x20u
#define BMX_BLE_GATT_READ_AUTHENTICATED 0x40u
#define BMX_BLE_GATT_WRITE_ENCRYPTED 0x80u
#define BMX_BLE_GATT_WRITE_AUTHENTICATED 0x100u
#define BMX_BLE_GATT_SUBSCRIBE_ENCRYPTED 0x200u
#define BMX_BLE_GATT_SUBSCRIBE_AUTHENTICATED 0x400u

typedef struct BMXBLEEvent {
    int32_t kind;
    int32_t status;
    uint8_t address_type;
    uint8_t address[6];
    uint8_t event_type;
    int8_t rssi;
    uint16_t data_length;
    uint8_t data[BMX_BLE_DATA_CAPACITY];
    uint16_t connection_handle;
    uint16_t attribute_id;
    uint8_t notifications;
    uint8_t indications;
    uint16_t attribute_end;
    uint16_t parent_attribute;
    uint8_t properties;
} BMXBLEEvent;

typedef struct BMXBLEGATTCharacteristic {
    uint16_t identifier;
    uint16_t value_handle;
    uint16_t capacity;
    uint16_t length;
    uint32_t flags;
    ble_uuid_any_t uuid;
    uint8_t value[BMX_BLE_DATA_CAPACITY];
} BMXBLEGATTCharacteristic;

typedef struct BMXBLEClientRead {
    bool active;
    uint16_t connection_handle;
    uint16_t attribute_handle;
    uint16_t start_offset;
    uint16_t length;
    uint8_t value[BMX_BLE_DATA_CAPACITY];
} BMXBLEClientRead;

static BMXBLEEvent bmx_ble_events[BMX_BLE_EVENT_CAPACITY];
static uint32_t bmx_ble_event_put;
static uint32_t bmx_ble_event_get;
static uint32_t bmx_ble_dropped;
static portMUX_TYPE bmx_ble_lock = portMUX_INITIALIZER_UNLOCKED;
static bool bmx_ble_initialized;
static bool bmx_ble_ready;
static bool bmx_ble_scan_active;
static bool bmx_ble_advertising_active;
static bool bmx_ble_advertising_auto_restart;
static int32_t bmx_ble_advertising_service;
static uint32_t bmx_ble_advertising_duration;
static bool bmx_ble_advertising_connectable;
static bool bmx_ble_connecting;
static struct ble_gatt_svc_def bmx_ble_gatt_services[BMX_BLE_GATT_MAX_SERVICES + 1u];
static struct ble_gatt_chr_def bmx_ble_gatt_characteristic_defs[BMX_BLE_GATT_MAX_SERVICES][BMX_BLE_GATT_MAX_CHARACTERISTICS_PER_SERVICE + 1u];
static ble_uuid_any_t bmx_ble_gatt_service_uuids[BMX_BLE_GATT_MAX_SERVICES];
static BMXBLEGATTCharacteristic bmx_ble_gatt_characteristics[BMX_BLE_GATT_MAX_CHARACTERISTICS];
static uint8_t bmx_ble_gatt_characteristic_counts[BMX_BLE_GATT_MAX_SERVICES];
static uint16_t bmx_ble_gatt_service_count;
static uint16_t bmx_ble_gatt_characteristic_count;
static BMXBLEClientRead bmx_ble_client_reads[BMX_BLE_MAX_PENDING_READS];
static uint16_t bmx_ble_connections[BMX_BLE_MAX_CONNECTIONS];
static uint8_t bmx_ble_connection_count;
static bool bmx_ble_security_configured;
static uint8_t bmx_ble_security_io_capability = BLE_HS_IO_NO_INPUT_OUTPUT;
static bool bmx_ble_security_bonding;
static bool bmx_ble_security_authentication;
static bool bmx_ble_security_secure_connections = true;
static bool bmx_ble_security_secure_connections_only;

static int bmx_ble_gap_event(struct ble_gap_event *event, void *argument);

static void bmx_ble_add_connection(uint16_t connection_handle) {
    portENTER_CRITICAL(&bmx_ble_lock);
    for (uint8_t index = 0; index < bmx_ble_connection_count; ++index) {
        if (bmx_ble_connections[index] == connection_handle) {
            portEXIT_CRITICAL(&bmx_ble_lock);
            return;
        }
    }
    if (bmx_ble_connection_count < BMX_BLE_MAX_CONNECTIONS) {
        bmx_ble_connections[bmx_ble_connection_count++] = connection_handle;
    }
    portEXIT_CRITICAL(&bmx_ble_lock);
}

static void bmx_ble_remove_connection(uint16_t connection_handle) {
    portENTER_CRITICAL(&bmx_ble_lock);
    for (uint8_t index = 0; index < bmx_ble_connection_count; ++index) {
        if (bmx_ble_connections[index] == connection_handle) {
            --bmx_ble_connection_count;
            bmx_ble_connections[index] = bmx_ble_connections[bmx_ble_connection_count];
            break;
        }
    }
    portEXIT_CRITICAL(&bmx_ble_lock);
}

static bool bmx_ble_queue(const BMXBLEEvent *event) {
    bool queued = false;
    portENTER_CRITICAL(&bmx_ble_lock);
    if (bmx_ble_event_put - bmx_ble_event_get < BMX_BLE_EVENT_CAPACITY) {
        bmx_ble_events[bmx_ble_event_put & BMX_BLE_EVENT_MASK] = *event;
        ++bmx_ble_event_put;
        queued = true;
    } else if (bmx_ble_dropped != UINT32_MAX) {
        ++bmx_ble_dropped;
    }
    portEXIT_CRITICAL(&bmx_ble_lock);
    if (queued) bmx_embedded_system_wake();
    return queued;
}

static void bmx_ble_queue_status(int32_t kind, int32_t status) {
    BMXBLEEvent event = {0};
    event.kind = kind;
    event.status = status;
    bmx_ble_queue(&event);
}

static void bmx_ble_queue_uuid(BMXBLEEvent *event, const ble_uuid_t *uuid) {
    char text[BLE_UUID_STR_LEN];
    ble_uuid_to_str(uuid, text);
    event->data_length = strlen(text);
    memcpy(event->data, text, event->data_length);
}

static void bmx_ble_queue_mbuf(BMXBLEEvent *event, const struct os_mbuf *value) {
    if (!value) return;
    uint16_t length = OS_MBUF_PKTLEN(value);
    event->data_length = length > BMX_BLE_DATA_CAPACITY ? BMX_BLE_DATA_CAPACITY : length;
    uint16_t copied = 0;
    int result = ble_hs_mbuf_to_flat(value, event->data, event->data_length, &copied);
    event->data_length = copied;
    if (result != 0 || length > BMX_BLE_DATA_CAPACITY) event->status = BLE_HS_EMSGSIZE;
}

static int bmx_ble_client_service_callback(uint16_t connection_handle,
        const struct ble_gatt_error *error, const struct ble_gatt_svc *service,
        void *argument) {
    (void)argument;
    BMXBLEEvent event = {0};
    event.connection_handle = connection_handle;
    if (error->status == 0 && service) {
        event.kind = BMX_BLE_SERVICE_DISCOVERED;
        event.attribute_id = service->start_handle;
        event.attribute_end = service->end_handle;
        bmx_ble_queue_uuid(&event, &service->uuid.u);
    } else {
        event.kind = BMX_BLE_SERVICE_DISCOVERY_COMPLETE;
        event.status = error->status == BLE_HS_EDONE ? 0 : error->status;
    }
    bmx_ble_queue(&event);
    return 0;
}

static int bmx_ble_client_characteristic_callback(uint16_t connection_handle,
        const struct ble_gatt_error *error, const struct ble_gatt_chr *characteristic,
        void *argument) {
    BMXBLEEvent event = {0};
    event.connection_handle = connection_handle;
    event.parent_attribute = (uint16_t)(uintptr_t)argument;
    if (error->status == 0 && characteristic) {
        event.kind = BMX_BLE_CHARACTERISTIC_DISCOVERED;
        event.attribute_id = characteristic->def_handle;
        event.attribute_end = characteristic->val_handle;
        event.properties = characteristic->properties;
        bmx_ble_queue_uuid(&event, &characteristic->uuid.u);
    } else {
        event.kind = BMX_BLE_CHARACTERISTIC_DISCOVERY_COMPLETE;
        event.status = error->status == BLE_HS_EDONE ? 0 : error->status;
    }
    bmx_ble_queue(&event);
    return 0;
}

static int bmx_ble_client_descriptor_callback(uint16_t connection_handle,
        const struct ble_gatt_error *error, uint16_t characteristic_value_handle,
        const struct ble_gatt_dsc *descriptor, void *argument) {
    (void)argument;
    BMXBLEEvent event = {0};
    event.connection_handle = connection_handle;
    event.parent_attribute = characteristic_value_handle;
    if (error->status == 0 && descriptor) {
        event.kind = BMX_BLE_DESCRIPTOR_DISCOVERED;
        event.attribute_id = descriptor->handle;
        bmx_ble_queue_uuid(&event, &descriptor->uuid.u);
    } else {
        event.kind = BMX_BLE_DESCRIPTOR_DISCOVERY_COMPLETE;
        event.status = error->status == BLE_HS_EDONE ? 0 : error->status;
    }
    bmx_ble_queue(&event);
    return 0;
}

static void bmx_ble_client_read_complete(BMXBLEClientRead *read, int status) {
    BMXBLEEvent event = {0};
    event.kind = BMX_BLE_READ_COMPLETE;
    event.connection_handle = read->connection_handle;
    event.attribute_id = read->attribute_handle;
    event.status = status == BLE_HS_EDONE ? 0 : status;
    event.data_length = read->length;
    if (read->length) memcpy(event.data, read->value, read->length);
    read->active = false;
    bmx_ble_queue(&event);
}

static int bmx_ble_client_read_callback(uint16_t connection_handle,
        const struct ble_gatt_error *error, struct ble_gatt_attr *attribute,
        void *argument) {
    (void)connection_handle;
    BMXBLEClientRead *read = argument;
    if (!read || !read->active) return 0;
    if (error->status == 0 && attribute) {
        uint16_t chunk_length = OS_MBUF_PKTLEN(attribute->om);
        if (attribute->offset < read->start_offset) {
            bmx_ble_client_read_complete(read, BLE_HS_EBADDATA);
            return BLE_HS_EBADDATA;
        }
        uint32_t destination = (uint32_t)attribute->offset - read->start_offset;
        if (destination + chunk_length > BMX_BLE_DATA_CAPACITY) {
            bmx_ble_client_read_complete(read, BLE_HS_EMSGSIZE);
            return BLE_HS_EMSGSIZE;
        }
        uint16_t copied = 0;
        int result = ble_hs_mbuf_to_flat(attribute->om,
            read->value + destination, chunk_length, &copied);
        if (result != 0 || copied != chunk_length) {
            bmx_ble_client_read_complete(read, result ? result : BLE_HS_EBADDATA);
            return result ? result : BLE_HS_EBADDATA;
        }
        uint16_t end = (uint16_t)(destination + chunk_length);
        if (end > read->length) read->length = end;
        return 0;
    }
    bmx_ble_client_read_complete(read, error->status);
    return 0;
}

static int bmx_ble_client_write_callback(uint16_t connection_handle,
        const struct ble_gatt_error *error, struct ble_gatt_attr *attribute,
        void *argument) {
    (void)attribute;
    BMXBLEEvent event = {0};
    event.kind = BMX_BLE_WRITE_COMPLETE;
    event.connection_handle = connection_handle;
    event.attribute_id = (uint16_t)(uintptr_t)argument;
    event.status = error->status;
    bmx_ble_queue(&event);
    return 0;
}

static void bmx_ble_copy_address(BMXBLEEvent *event, const ble_addr_t *address) {
    event->address_type = address->type;
    for (uint32_t index = 0; index < sizeof(event->address); ++index) {
        event->address[index] = address->val[sizeof(event->address) - index - 1u];
    }
}

static BMXBLEGATTCharacteristic *bmx_ble_gatt_characteristic(uint16_t identifier) {
    if (!identifier || identifier > bmx_ble_gatt_characteristic_count) return NULL;
    return &bmx_ble_gatt_characteristics[identifier - 1u];
}

static BMXBLEGATTCharacteristic *bmx_ble_gatt_characteristic_for_handle(uint16_t handle) {
    for (uint16_t index = 0; index < bmx_ble_gatt_characteristic_count; ++index) {
        if (bmx_ble_gatt_characteristics[index].value_handle == handle) {
            return &bmx_ble_gatt_characteristics[index];
        }
    }
    return NULL;
}

static int bmx_ble_parse_uuid(const uint8_t *bytes, uint32_t length,
        ble_uuid_any_t *uuid) {
    if (!bytes || !uuid || !length || length > 36u) return BLE_HS_EINVAL;
    char text[37];
    memcpy(text, bytes, length);
    text[length] = 0;
    return ble_uuid_from_str(uuid, text);
}

static uint32_t bmx_ble_gatt_native_flags(uint32_t flags) {
    uint32_t native_flags = 0;
    if (flags & BMX_BLE_GATT_READ) native_flags |= BLE_GATT_CHR_F_READ;
    if (flags & BMX_BLE_GATT_WRITE_FLAG) native_flags |= BLE_GATT_CHR_F_WRITE;
    if (flags & BMX_BLE_GATT_NOTIFY) native_flags |= BLE_GATT_CHR_F_NOTIFY;
    if (flags & BMX_BLE_GATT_INDICATE) native_flags |= BLE_GATT_CHR_F_INDICATE;
    if (flags & BMX_BLE_GATT_WRITE_NO_RESPONSE) native_flags |= BLE_GATT_CHR_F_WRITE_NO_RSP;
    if (flags & BMX_BLE_GATT_READ_ENCRYPTED) native_flags |= BLE_GATT_CHR_F_READ_ENC;
    if (flags & BMX_BLE_GATT_READ_AUTHENTICATED) native_flags |= BLE_GATT_CHR_F_READ_AUTHEN;
    if (flags & BMX_BLE_GATT_WRITE_ENCRYPTED) native_flags |= BLE_GATT_CHR_F_WRITE_ENC;
    if (flags & BMX_BLE_GATT_WRITE_AUTHENTICATED) native_flags |= BLE_GATT_CHR_F_WRITE_AUTHEN;
    if (flags & BMX_BLE_GATT_SUBSCRIBE_ENCRYPTED) {
        native_flags |= BLE_GATT_CHR_F_NOTIFY_INDICATE_ENC;
    }
    if (flags & BMX_BLE_GATT_SUBSCRIBE_AUTHENTICATED) {
        native_flags |= BLE_GATT_CHR_F_NOTIFY_INDICATE_AUTHEN;
    }
    return native_flags;
}

static int bmx_ble_gatt_access(uint16_t connection_handle, uint16_t attribute_handle,
        struct ble_gatt_access_ctxt *context, void *argument) {
    (void)attribute_handle;
    BMXBLEGATTCharacteristic *characteristic = argument;
    if (!characteristic || !context) return BLE_ATT_ERR_UNLIKELY;
    if (context->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t value[BMX_BLE_DATA_CAPACITY];
        uint16_t length;
        portENTER_CRITICAL(&bmx_ble_lock);
        length = characteristic->length;
        if (length) memcpy(value, characteristic->value, length);
        portEXIT_CRITICAL(&bmx_ble_lock);
        if (context->offset > length) return BLE_ATT_ERR_INVALID_OFFSET;
        uint16_t remaining = length - context->offset;
        return os_mbuf_append(context->om, value + context->offset, remaining) == 0 ? 0 :
            BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (context->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t length = OS_MBUF_PKTLEN(context->om);
        if (length > characteristic->capacity) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        uint8_t value[BMX_BLE_DATA_CAPACITY];
        uint16_t copied = 0;
        if (ble_hs_mbuf_to_flat(context->om, value, length, &copied) != 0 ||
                copied != length) return BLE_ATT_ERR_UNLIKELY;
        portENTER_CRITICAL(&bmx_ble_lock);
        if (length) memcpy(characteristic->value, value, length);
        characteristic->length = length;
        portEXIT_CRITICAL(&bmx_ble_lock);
        BMXBLEEvent event = {0};
        event.kind = BMX_BLE_GATT_WRITE;
        event.connection_handle = connection_handle;
        event.attribute_id = characteristic->identifier;
        event.data_length = length;
        if (event.data_length) memcpy(event.data, value, event.data_length);
        bmx_ble_queue(&event);
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int bmx_ble_start_advertising_internal(void) {
    uint8_t own_address_type;
    int result = ble_hs_id_infer_auto(0, &own_address_type);
    if (result != 0) return result;
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    if (bmx_ble_advertising_service > 0) {
        ble_uuid_any_t *uuid = &bmx_ble_gatt_service_uuids[bmx_ble_advertising_service - 1];
        switch (uuid->u.type) {
            case BLE_UUID_TYPE_16:
                fields.uuids16 = &uuid->u16;
                fields.num_uuids16 = 1;
                fields.uuids16_is_complete = 1;
                break;
            case BLE_UUID_TYPE_32:
                fields.uuids32 = &uuid->u32;
                fields.num_uuids32 = 1;
                fields.uuids32_is_complete = 1;
                break;
            case BLE_UUID_TYPE_128:
                fields.uuids128 = &uuid->u128;
                fields.num_uuids128 = 1;
                fields.uuids128_is_complete = 1;
                break;
            default:
                return BLE_HS_EINVAL;
        }
    }
    result = ble_gap_adv_set_fields(&fields);
    if (result != 0) return result;
    struct ble_hs_adv_fields response = {0};
    const char *name = ble_svc_gap_device_name();
    if (name && *name) {
        size_t name_length = strlen(name);
        response.name = (const uint8_t *)name;
        response.name_len = name_length > UINT8_MAX ? UINT8_MAX : (uint8_t)name_length;
        response.name_is_complete = 1;
    }
    response.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    response.tx_pwr_lvl_is_present = 1;
    result = ble_gap_adv_rsp_set_fields(&response);
    if (result != 0) return result;
    struct ble_gap_adv_params parameters = {0};
    parameters.conn_mode = bmx_ble_advertising_connectable ?
        BLE_GAP_CONN_MODE_UND : BLE_GAP_CONN_MODE_NON;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    int32_t duration = bmx_ble_advertising_duration ?
        (int32_t)bmx_ble_advertising_duration : BLE_HS_FOREVER;
    result = ble_gap_adv_start(own_address_type, NULL, duration, &parameters,
        bmx_ble_gap_event, NULL);
    if (result == 0) bmx_ble_advertising_active = true;
    return result;
}

static int bmx_ble_gap_event(struct ble_gap_event *event, void *argument) {
    (void)argument;
    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            BMXBLEEvent queued = {0};
            queued.kind = BMX_BLE_SCAN_RESULT;
            bmx_ble_copy_address(&queued, &event->disc.addr);
            queued.event_type = event->disc.event_type;
            queued.rssi = event->disc.rssi;
            queued.data_length = event->disc.length_data;
            if (queued.data_length) memcpy(queued.data, event->disc.data, queued.data_length);
            bmx_ble_queue(&queued);
            break;
        }
        case BLE_GAP_EVENT_DISC_COMPLETE: {
            bool was_active = bmx_ble_scan_active;
            bmx_ble_scan_active = false;
            if (was_active) {
                bmx_ble_queue_status(BMX_BLE_SCAN_COMPLETE,
                    event->disc_complete.reason);
            }
            break;
        }
        case BLE_GAP_EVENT_CONNECT: {
            bool outbound = bmx_ble_connecting;
            bmx_ble_connecting = false;
            bmx_ble_advertising_active = false;
            BMXBLEEvent queued = {0};
            queued.kind = BMX_BLE_CONNECTED;
            queued.status = event->connect.status;
            queued.connection_handle = event->connect.conn_handle;
            if (event->connect.status == 0) {
                bmx_ble_add_connection(event->connect.conn_handle);
                struct ble_gap_conn_desc description;
                if (ble_gap_conn_find(event->connect.conn_handle, &description) == 0) {
                    bmx_ble_copy_address(&queued, &description.peer_ota_addr);
                    queued.properties = description.role;
                }
            } else if (outbound) {
                queued.properties = BLE_GAP_ROLE_MASTER;
            }
            bmx_ble_queue(&queued);
            if (event->connect.status != 0 && bmx_ble_advertising_auto_restart) {
                int result = bmx_ble_start_advertising_internal();
                if (result != 0) bmx_ble_queue_status(BMX_BLE_ADV_COMPLETE, result);
            }
            break;
        }
        case BLE_GAP_EVENT_DISCONNECT: {
            BMXBLEEvent queued = {0};
            queued.kind = BMX_BLE_DISCONNECTED;
            queued.status = event->disconnect.reason;
            queued.connection_handle = event->disconnect.conn.conn_handle;
            bmx_ble_remove_connection(event->disconnect.conn.conn_handle);
            bmx_ble_copy_address(&queued, &event->disconnect.conn.peer_ota_addr);
            queued.properties = event->disconnect.conn.role;
            bmx_ble_queue(&queued);
            for (uint32_t index = 0; index < BMX_BLE_MAX_PENDING_READS; ++index) {
                if (bmx_ble_client_reads[index].active &&
                        bmx_ble_client_reads[index].connection_handle ==
                            event->disconnect.conn.conn_handle) {
                    bmx_ble_client_reads[index].active = false;
                }
            }
            bmx_ble_advertising_active = false;
            if (event->disconnect.conn.role == BLE_GAP_ROLE_SLAVE &&
                    bmx_ble_advertising_auto_restart) {
                int result = bmx_ble_start_advertising_internal();
                if (result != 0) bmx_ble_queue_status(BMX_BLE_ADV_COMPLETE, result);
            }
            break;
        }
        case BLE_GAP_EVENT_ADV_COMPLETE:
            bmx_ble_advertising_active = false;
            bmx_ble_queue_status(BMX_BLE_ADV_COMPLETE, event->adv_complete.reason);
            break;
        case BLE_GAP_EVENT_SUBSCRIBE: {
            BMXBLEGATTCharacteristic *characteristic =
                bmx_ble_gatt_characteristic_for_handle(event->subscribe.attr_handle);
            if (characteristic) {
                BMXBLEEvent queued = {0};
                queued.kind = BMX_BLE_GATT_SUBSCRIBE;
                queued.connection_handle = event->subscribe.conn_handle;
                queued.attribute_id = characteristic->identifier;
                queued.notifications = event->subscribe.cur_notify;
                queued.indications = event->subscribe.cur_indicate;
                bmx_ble_queue(&queued);
            }
            break;
        }
        case BLE_GAP_EVENT_NOTIFY_RX: {
            BMXBLEEvent queued = {0};
            queued.kind = BMX_BLE_NOTIFICATION;
            queued.connection_handle = event->notify_rx.conn_handle;
            queued.attribute_id = event->notify_rx.attr_handle;
            queued.indications = event->notify_rx.indication;
            bmx_ble_queue_mbuf(&queued, event->notify_rx.om);
            bmx_ble_queue(&queued);
            break;
        }
        case BLE_GAP_EVENT_MTU: {
            BMXBLEEvent queued = {0};
            queued.kind = BMX_BLE_MTU_CHANGED;
            queued.connection_handle = event->mtu.conn_handle;
            queued.attribute_end = event->mtu.value;
            bmx_ble_queue(&queued);
            break;
        }
        case BLE_GAP_EVENT_NOTIFY_TX: {
            BMXBLEGATTCharacteristic *characteristic =
                bmx_ble_gatt_characteristic_for_handle(event->notify_tx.attr_handle);
            if (characteristic) {
                BMXBLEEvent queued = {0};
                queued.kind = BMX_BLE_GATT_UPDATE_COMPLETE;
                queued.status = event->notify_tx.status == BLE_HS_EDONE ?
                    0 : event->notify_tx.status;
                queued.connection_handle = event->notify_tx.conn_handle;
                queued.attribute_id = characteristic->identifier;
                queued.indications = event->notify_tx.indication;
                queued.properties = event->notify_tx.indication &&
                    event->notify_tx.status == BLE_HS_EDONE;
                bmx_ble_queue(&queued);
            }
            break;
        }
        case BLE_GAP_EVENT_ENC_CHANGE: {
            BMXBLEEvent queued = {0};
            queued.kind = BMX_BLE_SECURITY_CHANGED;
            queued.status = event->enc_change.status;
            queued.connection_handle = event->enc_change.conn_handle;
            struct ble_gap_conn_desc description;
            if (ble_gap_conn_find(event->enc_change.conn_handle, &description) == 0) {
                queued.notifications = description.sec_state.encrypted;
                queued.indications = description.sec_state.authenticated;
                queued.properties = description.sec_state.bonded;
                queued.attribute_end = description.sec_state.key_size;
            }
            bmx_ble_queue(&queued);
            break;
        }
        case BLE_GAP_EVENT_PASSKEY_ACTION: {
            BMXBLEEvent queued = {0};
            queued.kind = BMX_BLE_PASSKEY_ACTION;
            queued.connection_handle = event->passkey.conn_handle;
            queued.event_type = event->passkey.params.action;
            if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
                queued.status = event->passkey.params.numcmp;
            }
            bmx_ble_queue(&queued);
            break;
        }
        case BLE_GAP_EVENT_REPEAT_PAIRING: {
            struct ble_gap_conn_desc description;
            if (ble_gap_conn_find(event->repeat_pairing.conn_handle, &description) != 0) {
                return BLE_GAP_REPEAT_PAIRING_IGNORE;
            }
            if (ble_store_util_delete_peer(&description.peer_id_addr) != 0) {
                return BLE_GAP_REPEAT_PAIRING_IGNORE;
            }
            return BLE_GAP_REPEAT_PAIRING_RETRY;
        }
        case BLE_GAP_EVENT_CONN_UPDATE: {
            BMXBLEEvent queued = {0};
            queued.kind = BMX_BLE_CONNECTION_UPDATED;
            queued.status = event->conn_update.status;
            queued.connection_handle = event->conn_update.conn_handle;
            struct ble_gap_conn_desc description;
            if (ble_gap_conn_find(event->conn_update.conn_handle, &description) == 0) {
                queued.attribute_id = description.conn_itvl;
                queued.attribute_end = description.conn_latency;
                queued.parent_attribute = description.supervision_timeout;
            }
            bmx_ble_queue(&queued);
            break;
        }
        case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE: {
            BMXBLEEvent queued = {0};
            queued.kind = BMX_BLE_PHY_UPDATED;
            queued.status = event->phy_updated.status;
            queued.connection_handle = event->phy_updated.conn_handle;
            queued.event_type = event->phy_updated.tx_phy;
            queued.properties = event->phy_updated.rx_phy;
            bmx_ble_queue(&queued);
            break;
        }
        default:
            break;
    }
    return 0;
}

static void bmx_ble_on_reset(int reason) {
    bmx_ble_ready = false;
    bmx_ble_scan_active = false;
    bmx_ble_advertising_active = false;
    bmx_ble_connecting = false;
    portENTER_CRITICAL(&bmx_ble_lock);
    bmx_ble_connection_count = 0;
    portEXIT_CRITICAL(&bmx_ble_lock);
    bmx_ble_queue_status(BMX_BLE_RESET, reason);
}

static void bmx_ble_on_sync(void) {
    int result = ble_hs_util_ensure_addr(0);
    if (result == 0) {
        bmx_ble_ready = true;
        bmx_ble_queue_status(BMX_BLE_READY, 0);
    } else {
        bmx_ble_queue_status(BMX_BLE_RESET, result);
    }
}

static void bmx_ble_host_task(void *argument) {
    (void)argument;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

int32_t bmx_embedded_ble_initialize(const uint8_t *name, uint32_t name_length) {
    if (bmx_ble_initialized) return ESP_OK;
    if (name_length > 248u || (name_length && !name)) return ESP_ERR_INVALID_ARG;
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        result = nvs_flash_erase();
        if (result == ESP_OK) result = nvs_flash_init();
    }
    if (result != ESP_OK) return result;
    result = nimble_port_init();
    if (result != ESP_OK) return result;
    ble_hs_cfg.reset_cb = bmx_ble_on_reset;
    ble_hs_cfg.sync_cb = bmx_ble_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    if (bmx_ble_security_configured) {
        ble_hs_cfg.sm_io_cap = bmx_ble_security_io_capability;
        ble_hs_cfg.sm_bonding = bmx_ble_security_bonding;
        ble_hs_cfg.sm_mitm = bmx_ble_security_authentication;
        ble_hs_cfg.sm_sc = bmx_ble_security_secure_connections;
        ble_hs_cfg.sm_sc_only = bmx_ble_security_secure_connections_only;
        ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
        ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    }
    ble_svc_gap_init();
    ble_svc_gatt_init();
    if (name_length) {
        char text[249];
        memcpy(text, name, name_length);
        text[name_length] = 0;
        int host_result = ble_svc_gap_device_name_set(text);
        if (host_result != 0) {
            nimble_port_deinit();
            return host_result;
        }
    }
    if (bmx_ble_gatt_service_count) {
        int host_result = ble_gatts_count_cfg(bmx_ble_gatt_services);
        if (host_result == 0) host_result = ble_gatts_add_svcs(bmx_ble_gatt_services);
        if (host_result != 0) {
            nimble_port_deinit();
            return host_result;
        }
    }
    bmx_ble_event_put = bmx_ble_event_get = bmx_ble_dropped = 0;
    memset(bmx_ble_client_reads, 0, sizeof(bmx_ble_client_reads));
    bmx_ble_connection_count = 0;
    bmx_ble_ready = false;
    bmx_ble_scan_active = false;
    bmx_ble_advertising_active = false;
    bmx_ble_advertising_auto_restart = false;
    bmx_ble_initialized = true;
    nimble_port_freertos_init(bmx_ble_host_task);
    return ESP_OK;
}

int32_t bmx_embedded_ble_deinitialize(void) {
    if (!bmx_ble_initialized) return ESP_OK;
    if (bmx_ble_scan_active) {
        int result = ble_gap_disc_cancel();
        if (result != 0 && result != BLE_HS_EALREADY) return result;
    }
    if (bmx_ble_connecting) {
        int result = ble_gap_conn_cancel();
        if (result != 0 && result != BLE_HS_EALREADY) return result;
        bmx_ble_connecting = false;
    }
    bmx_ble_advertising_auto_restart = false;
    if (bmx_ble_advertising_active) {
        int result = ble_gap_adv_stop();
        if (result != 0 && result != BLE_HS_EALREADY) return result;
    }
    int result = nimble_port_stop();
    if (result != 0) return result;
    result = nimble_port_deinit();
    if (result != ESP_OK) return result;
    bmx_ble_initialized = false;
    bmx_ble_ready = false;
    bmx_ble_scan_active = false;
    bmx_ble_advertising_active = false;
    bmx_ble_connecting = false;
    bmx_ble_event_put = bmx_ble_event_get = 0;
    memset(bmx_ble_client_reads, 0, sizeof(bmx_ble_client_reads));
    bmx_ble_connection_count = 0;
    return ESP_OK;
}

int32_t bmx_embedded_ble_initialized(void) { return bmx_ble_initialized; }
int32_t bmx_embedded_ble_ready(void) { return bmx_ble_ready; }
int32_t bmx_embedded_ble_scan_active(void) { return bmx_ble_scan_active; }

int32_t bmx_embedded_ble_start_scan(uint32_t duration, int32_t active,
        int32_t filter_duplicates) {
    if (!bmx_ble_initialized || !bmx_ble_ready) return BLE_HS_ENOTSYNCED;
    if (bmx_ble_scan_active) return BLE_HS_EALREADY;
    uint8_t own_address_type;
    int result = ble_hs_id_infer_auto(0, &own_address_type);
    if (result != 0) return result;
    struct ble_gap_disc_params parameters = {0};
    parameters.passive = active ? 0 : 1;
    parameters.filter_duplicates = filter_duplicates ? 1 : 0;
    int32_t native_duration = duration ? (int32_t)duration : BLE_HS_FOREVER;
    result = ble_gap_disc(own_address_type, native_duration, &parameters,
        bmx_ble_gap_event, NULL);
    if (result == 0) bmx_ble_scan_active = true;
    return result;
}

int32_t bmx_embedded_ble_stop_scan(void) {
    if (!bmx_ble_initialized || !bmx_ble_ready) return BLE_HS_ENOTSYNCED;
    if (!bmx_ble_scan_active) return ESP_OK;
    bmx_ble_scan_active = false;
    int result = ble_gap_disc_cancel();
    if (result == 0) {
        bmx_ble_queue_status(BMX_BLE_SCAN_COMPLETE, 0);
    } else {
        bmx_ble_scan_active = true;
    }
    return result;
}

int32_t bmx_embedded_ble_start_advertising(int32_t service_id, uint32_t duration,
        int32_t connectable, int32_t auto_restart) {
    if (!bmx_ble_initialized || !bmx_ble_ready) return BLE_HS_ENOTSYNCED;
    if (bmx_ble_advertising_active) return BLE_HS_EALREADY;
    if (bmx_ble_scan_active) return BLE_HS_EBUSY;
    if (service_id < 0 || service_id > bmx_ble_gatt_service_count) return BLE_HS_EINVAL;
    bmx_ble_advertising_service = service_id;
    bmx_ble_advertising_duration = duration;
    bmx_ble_advertising_connectable = connectable != 0;
    bmx_ble_advertising_auto_restart = auto_restart != 0;
    int result = bmx_ble_start_advertising_internal();
    if (result != 0) bmx_ble_advertising_auto_restart = false;
    return result;
}

int32_t bmx_embedded_ble_stop_advertising(void) {
    if (!bmx_ble_initialized || !bmx_ble_ready) return BLE_HS_ENOTSYNCED;
    bmx_ble_advertising_auto_restart = false;
    if (!bmx_ble_advertising_active) return ESP_OK;
    return ble_gap_adv_stop();
}

int32_t bmx_embedded_ble_advertising_active(void) {
    return bmx_ble_advertising_active;
}

int32_t bmx_embedded_ble_connect(int32_t address_type, const uint8_t *address,
        uint32_t timeout) {
    if (!bmx_ble_initialized || !bmx_ble_ready) return BLE_HS_ENOTSYNCED;
    if (!address || address_type < BLE_ADDR_PUBLIC || address_type > BLE_ADDR_RANDOM_ID) {
        return BLE_HS_EINVAL;
    }
    if (bmx_ble_connecting) return BLE_HS_EALREADY;
    if (bmx_ble_scan_active || bmx_ble_advertising_active) return BLE_HS_EBUSY;
    uint8_t own_address_type;
    int result = ble_hs_id_infer_auto(0, &own_address_type);
    if (result != 0) return result;
    ble_addr_t peer = {.type = (uint8_t)address_type};
    for (uint32_t index = 0; index < sizeof(peer.val); ++index) {
        peer.val[index] = address[sizeof(peer.val) - index - 1u];
    }
    int32_t duration = timeout ? (int32_t)timeout : BLE_HS_FOREVER;
    result = ble_gap_connect(own_address_type, &peer, duration, NULL,
        bmx_ble_gap_event, NULL);
    if (result == 0) bmx_ble_connecting = true;
    return result;
}

int32_t bmx_embedded_ble_cancel_connect(void) {
    if (!bmx_ble_initialized || !bmx_ble_ready) return BLE_HS_ENOTSYNCED;
    if (!bmx_ble_connecting) return ESP_OK;
    int result = ble_gap_conn_cancel();
    if (result == 0) bmx_ble_connecting = false;
    return result;
}

int32_t bmx_embedded_ble_disconnect(int32_t connection_handle) {
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX) return BLE_HS_EINVAL;
    return ble_gap_terminate((uint16_t)connection_handle, BLE_ERR_REM_USER_CONN_TERM);
}

int32_t bmx_embedded_ble_exchange_mtu(int32_t connection_handle) {
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX) return BLE_HS_EINVAL;
    return ble_gattc_exchange_mtu((uint16_t)connection_handle, NULL, NULL);
}

int32_t bmx_embedded_ble_connection_mtu(int32_t connection_handle) {
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX) return 0;
    return ble_att_mtu((uint16_t)connection_handle);
}

int32_t bmx_embedded_ble_connection_count(void) {
    if (!bmx_ble_initialized || !bmx_ble_ready) return BLE_HS_ENOTSYNCED;
    portENTER_CRITICAL(&bmx_ble_lock);
    int32_t count = bmx_ble_connection_count;
    portEXIT_CRITICAL(&bmx_ble_lock);
    return count;
}

int32_t bmx_embedded_ble_connection_handle(int32_t index,
        int32_t *connection_handle) {
    if (!bmx_ble_initialized || !bmx_ble_ready) return BLE_HS_ENOTSYNCED;
    if (!connection_handle || index < 0) return BLE_HS_EINVAL;
    portENTER_CRITICAL(&bmx_ble_lock);
    if (index >= bmx_ble_connection_count) {
        portEXIT_CRITICAL(&bmx_ble_lock);
        return BLE_HS_ENOENT;
    }
    *connection_handle = bmx_ble_connections[index];
    portEXIT_CRITICAL(&bmx_ble_lock);
    return ESP_OK;
}

int32_t bmx_embedded_ble_connection_info(int32_t connection_handle,
        int32_t *role, int32_t *address_type, uint8_t *address,
        int32_t *interval_units, int32_t *latency,
        int32_t *supervision_timeout_units) {
    if (!bmx_ble_initialized || !bmx_ble_ready || !role || !address_type ||
            !address || !interval_units || !latency ||
            !supervision_timeout_units || connection_handle < 0 ||
            connection_handle > UINT16_MAX) return BLE_HS_EINVAL;
    struct ble_gap_conn_desc description;
    int result = ble_gap_conn_find((uint16_t)connection_handle, &description);
    if (result != 0) return result;
    *role = description.role;
    *address_type = description.peer_ota_addr.type;
    for (uint32_t index = 0; index < sizeof(description.peer_ota_addr.val); ++index) {
        address[index] = description.peer_ota_addr.val[
            sizeof(description.peer_ota_addr.val) - index - 1u];
    }
    *interval_units = description.conn_itvl;
    *latency = description.conn_latency;
    *supervision_timeout_units = description.supervision_timeout;
    return ESP_OK;
}

int32_t bmx_embedded_ble_connection_rssi(int32_t connection_handle,
        int32_t *rssi) {
    if (!bmx_ble_initialized || !bmx_ble_ready || !rssi ||
            connection_handle < 0 || connection_handle > UINT16_MAX) {
        return BLE_HS_EINVAL;
    }
    int8_t native_rssi = 0;
    int result = ble_gap_conn_rssi((uint16_t)connection_handle, &native_rssi);
    if (result == 0) *rssi = native_rssi;
    return result;
}

int32_t bmx_embedded_ble_connection_phy(int32_t connection_handle,
        int32_t *tx_phy, int32_t *rx_phy) {
    if (!bmx_ble_initialized || !bmx_ble_ready || !tx_phy || !rx_phy ||
            connection_handle < 0 || connection_handle > UINT16_MAX) {
        return BLE_HS_EINVAL;
    }
    uint8_t native_tx_phy = 0;
    uint8_t native_rx_phy = 0;
    int result = ble_gap_read_le_phy((uint16_t)connection_handle,
        &native_tx_phy, &native_rx_phy);
    if (result == 0) {
        *tx_phy = native_tx_phy;
        *rx_phy = native_rx_phy;
    }
    return result;
}

int32_t bmx_embedded_ble_update_connection_parameters(int32_t connection_handle,
        int32_t minimum_interval_microseconds,
        int32_t maximum_interval_microseconds, int32_t latency,
        int32_t supervision_timeout_milliseconds) {
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX || minimum_interval_microseconds < 7500 ||
            maximum_interval_microseconds > 4000000 ||
            minimum_interval_microseconds > maximum_interval_microseconds ||
            minimum_interval_microseconds % 1250 != 0 ||
            maximum_interval_microseconds % 1250 != 0 || latency < 0 ||
            latency > 499 || supervision_timeout_milliseconds < 100 ||
            supervision_timeout_milliseconds > 32000 ||
            supervision_timeout_milliseconds % 10 != 0) return BLE_HS_EINVAL;
    struct ble_gap_upd_params parameters = {
        .itvl_min = (uint16_t)(minimum_interval_microseconds / 1250),
        .itvl_max = (uint16_t)(maximum_interval_microseconds / 1250),
        .latency = (uint16_t)latency,
        .supervision_timeout = (uint16_t)(supervision_timeout_milliseconds / 10),
        .min_ce_len = 0,
        .max_ce_len = 0,
    };
    return ble_gap_update_params((uint16_t)connection_handle, &parameters);
}

int32_t bmx_embedded_ble_set_preferred_phy(int32_t connection_handle,
        int32_t tx_phy_mask, int32_t rx_phy_mask, int32_t coded_preference) {
    const int32_t valid_mask = BLE_GAP_LE_PHY_1M_MASK |
        BLE_GAP_LE_PHY_2M_MASK | BLE_GAP_LE_PHY_CODED_MASK;
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX || tx_phy_mask <= 0 ||
            rx_phy_mask <= 0 || (tx_phy_mask & ~valid_mask) ||
            (rx_phy_mask & ~valid_mask) ||
            coded_preference < BLE_GAP_LE_PHY_CODED_ANY ||
            coded_preference > BLE_GAP_LE_PHY_CODED_S8) return BLE_HS_EINVAL;
    return ble_gap_set_prefered_le_phy((uint16_t)connection_handle,
        (uint8_t)tx_phy_mask, (uint8_t)rx_phy_mask,
        (uint16_t)coded_preference);
}

int32_t bmx_embedded_ble_configure_security(int32_t io_capability,
        int32_t bonding, int32_t authentication, int32_t secure_connections,
        int32_t secure_connections_only) {
    if (bmx_ble_initialized) return BLE_HS_EBUSY;
    if (io_capability < BLE_HS_IO_DISPLAY_ONLY ||
            io_capability > BLE_HS_IO_KEYBOARD_DISPLAY) return BLE_HS_EINVAL;
    if (secure_connections_only && !secure_connections) return BLE_HS_EINVAL;
    bmx_ble_security_io_capability = (uint8_t)io_capability;
    bmx_ble_security_bonding = bonding != 0;
    bmx_ble_security_authentication = authentication != 0;
    bmx_ble_security_secure_connections = secure_connections != 0;
    bmx_ble_security_secure_connections_only = secure_connections_only != 0;
    bmx_ble_security_configured = true;
    return ESP_OK;
}

int32_t bmx_embedded_ble_secure_connection(int32_t connection_handle) {
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX) return BLE_HS_EINVAL;
    return ble_gap_security_initiate((uint16_t)connection_handle);
}

int32_t bmx_embedded_ble_security_state(int32_t connection_handle,
        int32_t *encrypted, int32_t *authenticated, int32_t *bonded,
        int32_t *key_size) {
    if (!encrypted || !authenticated || !bonded || !key_size ||
            connection_handle < 0 || connection_handle > UINT16_MAX) {
        return BLE_HS_EINVAL;
    }
    struct ble_gap_conn_desc description;
    int result = ble_gap_conn_find((uint16_t)connection_handle, &description);
    if (result != 0) return result;
    *encrypted = description.sec_state.encrypted;
    *authenticated = description.sec_state.authenticated;
    *bonded = description.sec_state.bonded;
    *key_size = description.sec_state.key_size;
    return ESP_OK;
}

int32_t bmx_embedded_ble_provide_passkey(int32_t connection_handle,
        int32_t action, int32_t passkey) {
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX || passkey < 0 || passkey > 999999 ||
            (action != BLE_SM_IOACT_INPUT && action != BLE_SM_IOACT_DISP)) {
        return BLE_HS_EINVAL;
    }
    struct ble_sm_io response = {0};
    response.action = (uint8_t)action;
    response.passkey = (uint32_t)passkey;
    return ble_sm_inject_io((uint16_t)connection_handle, &response);
}

int32_t bmx_embedded_ble_confirm_passkey(int32_t connection_handle,
        int32_t accept) {
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX) return BLE_HS_EINVAL;
    struct ble_sm_io response = {0};
    response.action = BLE_SM_IOACT_NUMCMP;
    response.numcmp_accept = accept != 0;
    return ble_sm_inject_io((uint16_t)connection_handle, &response);
}

int32_t bmx_embedded_ble_bond_count(void) {
    if (!bmx_ble_initialized || !bmx_ble_ready) return BLE_HS_ENOTSYNCED;
    ble_addr_t peers[16];
    int count = 0;
    int result = ble_store_util_bonded_peers(peers, &count,
        sizeof(peers) / sizeof(peers[0]));
    return result == 0 ? count : result;
}

int32_t bmx_embedded_ble_bond(int32_t index, int32_t *address_type,
        uint8_t *address) {
    if (!bmx_ble_initialized || !bmx_ble_ready) return BLE_HS_ENOTSYNCED;
    if (index < 0 || !address_type || !address) return BLE_HS_EINVAL;
    ble_addr_t peers[16];
    int count = 0;
    int result = ble_store_util_bonded_peers(peers, &count,
        sizeof(peers) / sizeof(peers[0]));
    if (result != 0) return result;
    if (index >= count) return BLE_HS_ENOENT;
    *address_type = peers[index].type;
    for (uint32_t byte = 0; byte < sizeof(peers[index].val); ++byte) {
        address[byte] = peers[index].val[sizeof(peers[index].val) - byte - 1u];
    }
    return ESP_OK;
}

int32_t bmx_embedded_ble_forget_bond(int32_t address_type,
        const uint8_t *address) {
    if (!bmx_ble_initialized || !bmx_ble_ready) return BLE_HS_ENOTSYNCED;
    if (!address || address_type < BLE_ADDR_PUBLIC ||
            address_type > BLE_ADDR_RANDOM_ID) return BLE_HS_EINVAL;
    ble_addr_t peer = {.type = (uint8_t)address_type};
    for (uint32_t index = 0; index < sizeof(peer.val); ++index) {
        peer.val[index] = address[sizeof(peer.val) - index - 1u];
    }
    return ble_gap_unpair(&peer);
}

int32_t bmx_embedded_ble_forget_all_bonds(void) {
    if (!bmx_ble_initialized || !bmx_ble_ready) return BLE_HS_ENOTSYNCED;
    ble_addr_t peers[16];
    int count = 0;
    int result = ble_store_util_bonded_peers(peers, &count,
        sizeof(peers) / sizeof(peers[0]));
    if (result != 0) return result;
    for (int index = 0; index < count; ++index) {
        result = ble_gap_unpair(&peers[index]);
        if (result != 0) return result;
    }
    return ESP_OK;
}

int32_t bmx_embedded_ble_discover_services(int32_t connection_handle) {
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX) return BLE_HS_EINVAL;
    return ble_gattc_disc_all_svcs((uint16_t)connection_handle,
        bmx_ble_client_service_callback, NULL);
}

int32_t bmx_embedded_ble_discover_characteristics(int32_t connection_handle,
        int32_t start_handle, int32_t end_handle) {
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX || start_handle <= 0 ||
            end_handle < start_handle || end_handle > UINT16_MAX) return BLE_HS_EINVAL;
    return ble_gattc_disc_all_chrs((uint16_t)connection_handle,
        (uint16_t)start_handle, (uint16_t)end_handle,
        bmx_ble_client_characteristic_callback, (void *)(uintptr_t)start_handle);
}

int32_t bmx_embedded_ble_discover_descriptors(int32_t connection_handle,
        int32_t value_handle, int32_t end_handle) {
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX || value_handle <= 0 ||
            end_handle <= value_handle || end_handle > UINT16_MAX) return BLE_HS_EINVAL;
    return ble_gattc_disc_all_dscs((uint16_t)connection_handle,
        (uint16_t)value_handle, (uint16_t)end_handle,
        bmx_ble_client_descriptor_callback, NULL);
}

int32_t bmx_embedded_ble_client_read(int32_t connection_handle,
        int32_t attribute_handle, uint32_t offset) {
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX || attribute_handle <= 0 ||
            attribute_handle > UINT16_MAX || offset > UINT16_MAX) return BLE_HS_EINVAL;
    BMXBLEClientRead *read = NULL;
    for (uint32_t index = 0; index < BMX_BLE_MAX_PENDING_READS; ++index) {
        if (!bmx_ble_client_reads[index].active) {
            read = &bmx_ble_client_reads[index];
            break;
        }
    }
    if (!read) return BLE_HS_EBUSY;
    memset(read, 0, sizeof(*read));
    read->active = true;
    read->connection_handle = connection_handle;
    read->attribute_handle = attribute_handle;
    read->start_offset = offset;
    int result = ble_gattc_read_long((uint16_t)connection_handle,
        (uint16_t)attribute_handle, (uint16_t)offset,
        bmx_ble_client_read_callback, read);
    if (result != 0) read->active = false;
    return result;
}

int32_t bmx_embedded_ble_client_write(int32_t connection_handle,
        int32_t attribute_handle, const uint8_t *value, uint32_t value_length,
        int32_t response) {
    if (!bmx_ble_initialized || !bmx_ble_ready || connection_handle < 0 ||
            connection_handle > UINT16_MAX || attribute_handle <= 0 ||
            attribute_handle > UINT16_MAX || value_length > BMX_BLE_DATA_CAPACITY ||
            (value_length && !value)) return BLE_HS_EINVAL;
    uint16_t mtu = ble_att_mtu((uint16_t)connection_handle);
    if (!mtu) return BLE_HS_ENOTCONN;
    uint16_t short_limit = mtu > 3u ? mtu - 3u : 0u;
    if (response && value_length > short_limit) {
        struct os_mbuf *value_mbuf = ble_hs_mbuf_from_flat(value, value_length);
        if (!value_mbuf) return BLE_HS_ENOMEM;
        return ble_gattc_write_long((uint16_t)connection_handle,
            (uint16_t)attribute_handle, 0, value_mbuf,
            bmx_ble_client_write_callback, (void *)(uintptr_t)attribute_handle);
    }
    if (response) {
        return ble_gattc_write_flat((uint16_t)connection_handle,
            (uint16_t)attribute_handle, value, (uint16_t)value_length,
            bmx_ble_client_write_callback, (void *)(uintptr_t)attribute_handle);
    }
    if (value_length > short_limit) return BLE_HS_EMSGSIZE;
    int result = ble_gattc_write_no_rsp_flat((uint16_t)connection_handle,
        (uint16_t)attribute_handle, value, (uint16_t)value_length);
    if (result == 0) {
        BMXBLEEvent event = {0};
        event.kind = BMX_BLE_WRITE_COMPLETE;
        event.connection_handle = connection_handle;
        event.attribute_id = attribute_handle;
        bmx_ble_queue(&event);
    }
    return result;
}

int32_t bmx_embedded_ble_gatt_reset(void) {
    if (bmx_ble_initialized) return BLE_HS_EBUSY;
    memset(bmx_ble_gatt_services, 0, sizeof(bmx_ble_gatt_services));
    memset(bmx_ble_gatt_characteristic_defs, 0, sizeof(bmx_ble_gatt_characteristic_defs));
    memset(bmx_ble_gatt_service_uuids, 0, sizeof(bmx_ble_gatt_service_uuids));
    memset(bmx_ble_gatt_characteristics, 0, sizeof(bmx_ble_gatt_characteristics));
    memset(bmx_ble_gatt_characteristic_counts, 0, sizeof(bmx_ble_gatt_characteristic_counts));
    bmx_ble_gatt_service_count = 0;
    bmx_ble_gatt_characteristic_count = 0;
    return ESP_OK;
}

int32_t bmx_embedded_ble_gatt_add_service(const uint8_t *uuid, uint32_t uuid_length,
        int32_t *service_id) {
    if (!service_id) return BLE_HS_EINVAL;
    if (bmx_ble_initialized) return BLE_HS_EBUSY;
    if (bmx_ble_gatt_service_count >= BMX_BLE_GATT_MAX_SERVICES) return BLE_HS_ENOMEM;
    uint16_t index = bmx_ble_gatt_service_count;
    int result = bmx_ble_parse_uuid(uuid, uuid_length, &bmx_ble_gatt_service_uuids[index]);
    if (result != 0) return result;
    bmx_ble_gatt_services[index].type = BLE_GATT_SVC_TYPE_PRIMARY;
    bmx_ble_gatt_services[index].uuid = &bmx_ble_gatt_service_uuids[index].u;
    bmx_ble_gatt_services[index].characteristics = bmx_ble_gatt_characteristic_defs[index];
    ++bmx_ble_gatt_service_count;
    *service_id = index + 1;
    return ESP_OK;
}

int32_t bmx_embedded_ble_gatt_add_characteristic(int32_t service_id,
        const uint8_t *uuid, uint32_t uuid_length, uint32_t flags,
        const uint8_t *value, uint32_t value_length, uint32_t capacity,
        int32_t *characteristic_id) {
    if (!characteristic_id) return BLE_HS_EINVAL;
    if (bmx_ble_initialized) return BLE_HS_EBUSY;
    uint32_t valid_flags = BMX_BLE_GATT_READ | BMX_BLE_GATT_WRITE_FLAG |
        BMX_BLE_GATT_NOTIFY | BMX_BLE_GATT_INDICATE | BMX_BLE_GATT_WRITE_NO_RESPONSE |
        BMX_BLE_GATT_READ_ENCRYPTED | BMX_BLE_GATT_READ_AUTHENTICATED |
        BMX_BLE_GATT_WRITE_ENCRYPTED | BMX_BLE_GATT_WRITE_AUTHENTICATED |
        BMX_BLE_GATT_SUBSCRIBE_ENCRYPTED | BMX_BLE_GATT_SUBSCRIBE_AUTHENTICATED;
    if (service_id <= 0 || service_id > bmx_ble_gatt_service_count || !flags ||
            (flags & ~valid_flags) ||
            !capacity || capacity > BMX_BLE_DATA_CAPACITY || value_length > capacity ||
            (value_length && !value)) return BLE_HS_EINVAL;
    uint16_t service_index = service_id - 1;
    uint8_t definition_index = bmx_ble_gatt_characteristic_counts[service_index];
    if (definition_index >= BMX_BLE_GATT_MAX_CHARACTERISTICS_PER_SERVICE ||
            bmx_ble_gatt_characteristic_count >= BMX_BLE_GATT_MAX_CHARACTERISTICS) {
        return BLE_HS_ENOMEM;
    }
    BMXBLEGATTCharacteristic *characteristic =
        &bmx_ble_gatt_characteristics[bmx_ble_gatt_characteristic_count];
    int result = bmx_ble_parse_uuid(uuid, uuid_length, &characteristic->uuid);
    if (result != 0) return result;
    characteristic->identifier = bmx_ble_gatt_characteristic_count + 1;
    characteristic->capacity = capacity;
    characteristic->length = value_length;
    characteristic->flags = flags;
    if (value_length) memcpy(characteristic->value, value, value_length);
    struct ble_gatt_chr_def *definition =
        &bmx_ble_gatt_characteristic_defs[service_index][definition_index];
    definition->uuid = &characteristic->uuid.u;
    definition->access_cb = bmx_ble_gatt_access;
    definition->arg = characteristic;
    definition->flags = bmx_ble_gatt_native_flags(flags);
    definition->val_handle = &characteristic->value_handle;
    ++bmx_ble_gatt_characteristic_counts[service_index];
    ++bmx_ble_gatt_characteristic_count;
    *characteristic_id = characteristic->identifier;
    return ESP_OK;
}

int32_t bmx_embedded_ble_gatt_set_value(int32_t characteristic_id,
        const uint8_t *value, uint32_t value_length, int32_t notify_subscribers) {
    BMXBLEGATTCharacteristic *characteristic = bmx_ble_gatt_characteristic(characteristic_id);
    if (!characteristic || value_length > characteristic->capacity ||
            (value_length && !value)) return BLE_HS_EINVAL;
    portENTER_CRITICAL(&bmx_ble_lock);
    if (value_length) memcpy(characteristic->value, value, value_length);
    characteristic->length = value_length;
    portEXIT_CRITICAL(&bmx_ble_lock);
    if (notify_subscribers) {
        if (!(characteristic->flags & (BMX_BLE_GATT_NOTIFY | BMX_BLE_GATT_INDICATE))) {
            return BLE_HS_ENOTSUP;
        }
        if (!bmx_ble_initialized || !bmx_ble_ready || !characteristic->value_handle) {
            return BLE_HS_ENOTSYNCED;
        }
        ble_gatts_chr_updated(characteristic->value_handle);
    }
    return ESP_OK;
}

int32_t bmx_embedded_ble_gatt_get_value(int32_t characteristic_id,
        uint8_t *value, uint32_t capacity, int32_t *value_length) {
    BMXBLEGATTCharacteristic *characteristic = bmx_ble_gatt_characteristic(characteristic_id);
    if (!characteristic || !value_length) return BLE_HS_EINVAL;
    portENTER_CRITICAL(&bmx_ble_lock);
    uint16_t length = characteristic->length;
    *value_length = length;
    if (value && capacity >= length && length) memcpy(value, characteristic->value, length);
    portEXIT_CRITICAL(&bmx_ble_lock);
    if (!value) return capacity == 0 ? ESP_OK : BLE_HS_EINVAL;
    return capacity >= length ? ESP_OK : BLE_HS_EMSGSIZE;
}

int32_t bmx_embedded_ble_gatt_notify(int32_t characteristic_id,
        int32_t connection_handle, int32_t indication) {
    BMXBLEGATTCharacteristic *characteristic = bmx_ble_gatt_characteristic(characteristic_id);
    if (!characteristic) return BLE_HS_EINVAL;
    uint32_t required_flag = indication ? BMX_BLE_GATT_INDICATE : BMX_BLE_GATT_NOTIFY;
    if (!(characteristic->flags & required_flag)) {
        return BLE_HS_ENOTSUP;
    }
    if (!bmx_ble_initialized || !bmx_ble_ready || !characteristic->value_handle) {
        return BLE_HS_ENOTSYNCED;
    }
    if (connection_handle < 0) {
        if (indication) return BLE_HS_EINVAL;
        ble_gatts_chr_updated(characteristic->value_handle);
        return ESP_OK;
    }
    if (connection_handle > UINT16_MAX) return BLE_HS_EINVAL;
    uint16_t mtu = ble_att_mtu((uint16_t)connection_handle);
    if (mtu <= 3u) return BLE_HS_ENOTCONN;
    if (characteristic->length > mtu - 3u) {
        return BLE_HS_EMSGSIZE;
    }
    return indication ?
        ble_gatts_indicate((uint16_t)connection_handle, characteristic->value_handle) :
        ble_gatts_notify((uint16_t)connection_handle, characteristic->value_handle);
}

int32_t bmx_embedded_ble_take_event(int32_t *kind, int32_t *status,
        int32_t *address_type, uint8_t *address, int32_t *event_type,
        int32_t *rssi, uint8_t *data, int32_t *data_length,
        int32_t *connection_handle, int32_t *attribute_id,
        int32_t *notifications, int32_t *indications, int32_t *attribute_end,
        int32_t *parent_attribute, int32_t *properties) {
    if (!kind || !status || !address_type || !address || !event_type ||
            !rssi || !data || !data_length || !connection_handle ||
            !attribute_id || !notifications || !indications || !attribute_end ||
            !parent_attribute || !properties) return 0;
    portENTER_CRITICAL(&bmx_ble_lock);
    if (bmx_ble_event_get == bmx_ble_event_put) {
        portEXIT_CRITICAL(&bmx_ble_lock);
        return 0;
    }
    BMXBLEEvent event = bmx_ble_events[bmx_ble_event_get & BMX_BLE_EVENT_MASK];
    ++bmx_ble_event_get;
    portEXIT_CRITICAL(&bmx_ble_lock);
    *kind = event.kind;
    *status = event.status;
    *address_type = event.address_type;
    memcpy(address, event.address, sizeof(event.address));
    *event_type = event.event_type;
    *rssi = event.rssi;
    *data_length = event.data_length;
    memcpy(data, event.data, sizeof(event.data));
    *connection_handle = event.connection_handle;
    *attribute_id = event.attribute_id;
    *notifications = event.notifications;
    *indications = event.indications;
    *attribute_end = event.attribute_end;
    *parent_attribute = event.parent_attribute;
    *properties = event.properties;
    return 1;
}

uint32_t bmx_embedded_ble_dropped_events(void) { return bmx_ble_dropped; }

const BMXEmbeddedString *bmx_esp32_ble_result_name(int32_t result) {
    if (result == 0) return bmx_embedded_string_from_ascii("OK", 2);
    const char *host_name = NULL;
    switch (result) {
        case BLE_HS_EAGAIN: host_name = "BLE_HS_EAGAIN"; break;
        case BLE_HS_EALREADY: host_name = "BLE_HS_EALREADY"; break;
        case BLE_HS_EINVAL: host_name = "BLE_HS_EINVAL"; break;
        case BLE_HS_EMSGSIZE: host_name = "BLE_HS_EMSGSIZE"; break;
        case BLE_HS_ENOENT: host_name = "BLE_HS_ENOENT"; break;
        case BLE_HS_ENOMEM: host_name = "BLE_HS_ENOMEM"; break;
        case BLE_HS_ENOTCONN: host_name = "BLE_HS_ENOTCONN"; break;
        case BLE_HS_ENOTSUP: host_name = "BLE_HS_ENOTSUP"; break;
        case BLE_HS_EAPP: host_name = "BLE_HS_EAPP"; break;
        case BLE_HS_EBADDATA: host_name = "BLE_HS_EBADDATA"; break;
        case BLE_HS_EOS: host_name = "BLE_HS_EOS"; break;
        case BLE_HS_ECONTROLLER: host_name = "BLE_HS_ECONTROLLER"; break;
        case BLE_HS_ETIMEOUT: host_name = "BLE_HS_ETIMEOUT"; break;
        case BLE_HS_EDONE: host_name = "BLE_HS_EDONE"; break;
        case BLE_HS_EBUSY: host_name = "BLE_HS_EBUSY"; break;
        case BLE_HS_EREJECT: host_name = "BLE_HS_EREJECT"; break;
        case BLE_HS_EUNKNOWN: host_name = "BLE_HS_EUNKNOWN"; break;
        case BLE_HS_EROLE: host_name = "BLE_HS_EROLE"; break;
        case BLE_HS_ETIMEOUT_HCI: host_name = "BLE_HS_ETIMEOUT_HCI"; break;
        case BLE_HS_ENOMEM_EVT: host_name = "BLE_HS_ENOMEM_EVT"; break;
        case BLE_HS_ENOADDR: host_name = "BLE_HS_ENOADDR"; break;
        case BLE_HS_ENOTSYNCED: host_name = "BLE_HS_ENOTSYNCED"; break;
        case BLE_HS_EAUTHEN: host_name = "BLE_HS_EAUTHEN"; break;
        case BLE_HS_EAUTHOR: host_name = "BLE_HS_EAUTHOR"; break;
        case BLE_HS_EENCRYPT: host_name = "BLE_HS_EENCRYPT"; break;
        case BLE_HS_EENCRYPT_KEY_SZ: host_name = "BLE_HS_EENCRYPT_KEY_SZ"; break;
        case BLE_HS_ESTORE_CAP: host_name = "BLE_HS_ESTORE_CAP"; break;
        case BLE_HS_ESTORE_FAIL: host_name = "BLE_HS_ESTORE_FAIL"; break;
        case BLE_HS_EPREEMPTED: host_name = "BLE_HS_EPREEMPTED"; break;
        case BLE_HS_EDISABLED: host_name = "BLE_HS_EDISABLED"; break;
        case BLE_HS_ESTALLED: host_name = "BLE_HS_ESTALLED"; break;
    }
    if (host_name) {
        return bmx_embedded_string_from_ascii(host_name, (int32_t)strlen(host_name));
    }
    const char *name = esp_err_to_name((esp_err_t)result);
    return bmx_embedded_string_from_ascii(name, (int32_t)strlen(name));
}
