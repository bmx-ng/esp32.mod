#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_runtime.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

typedef struct BMXESP32NVSHandle {
    nvs_handle_t value;
    esp_err_t last_error;
} BMXESP32NVSHandle;

static esp_err_t bmx_esp32_nvs_open_error = ESP_OK;

static esp_err_t bmx_esp32_nvs_set_error(BMXESP32NVSHandle *handle, esp_err_t error) {
    if (handle) handle->last_error = error;
    return error;
}

static char *bmx_esp32_nvs_utf8(const BMXEmbeddedString *value) {
    return (char *)bmx_embedded_string_to_utf8_string(value);
}

int32_t bmx_esp32_nvs_initialize(void) {
    return (int32_t)nvs_flash_init();
}

int32_t bmx_esp32_nvs_last_open_error(void) {
    return (int32_t)bmx_esp32_nvs_open_error;
}

const BMXEmbeddedString *bmx_esp32_nvs_result_name(int32_t result) {
    const char *name = esp_err_to_name((esp_err_t)result);
    return bmx_embedded_string_from_ascii(name, (int32_t)strlen(name));
}

void *bmx_esp32_nvs_open(const BMXEmbeddedString *name, int32_t mode) {
    char *native_name = bmx_esp32_nvs_utf8(name);
    if (!native_name) {
        bmx_esp32_nvs_open_error = ESP_ERR_NO_MEM;
        return NULL;
    }
    esp_err_t error = nvs_flash_init();
    BMXESP32NVSHandle *result = NULL;
    if (error == ESP_OK) {
        result = (BMXESP32NVSHandle *)bbMemAlloc(sizeof(*result));
        if (!result) {
            error = ESP_ERR_NO_MEM;
        } else {
            error = nvs_open(native_name, mode == NVS_READONLY ? NVS_READONLY : NVS_READWRITE,
                &result->value);
            result->last_error = error;
            if (error != ESP_OK) {
                bbMemFree(result);
                result = NULL;
            }
        }
    }
    bbMemFree(native_name);
    bmx_esp32_nvs_open_error = error;
    return result;
}

void bmx_esp32_nvs_close(void *opaque) {
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque;
    if (!handle) return;
    nvs_close(handle->value);
    handle->value = 0;
    bbMemFree(handle);
}

int32_t bmx_esp32_nvs_last_error(void *opaque) {
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque;
    return handle ? (int32_t)handle->last_error : (int32_t)ESP_ERR_NVS_INVALID_HANDLE;
}

int32_t bmx_esp32_nvs_commit(void *opaque) {
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque;
    return (int32_t)bmx_esp32_nvs_set_error(handle,
        handle ? nvs_commit(handle->value) : ESP_ERR_NVS_INVALID_HANDLE);
}

static char *bmx_esp32_nvs_key(BMXESP32NVSHandle *handle, const BMXEmbeddedString *key) {
    if (!handle) return NULL;
    char *result = bmx_esp32_nvs_utf8(key);
    if (!result) bmx_esp32_nvs_set_error(handle, ESP_ERR_NO_MEM);
    return result;
}

int32_t bmx_esp32_nvs_erase_key(void *opaque, const BMXEmbeddedString *key) {
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque;
    char *native_key = bmx_esp32_nvs_key(handle, key);
    if (!native_key) return handle ? handle->last_error : ESP_ERR_NVS_INVALID_HANDLE;
    esp_err_t error = nvs_erase_key(handle->value, native_key);
    bbMemFree(native_key);
    return (int32_t)bmx_esp32_nvs_set_error(handle, error);
}

int32_t bmx_esp32_nvs_erase_all(void *opaque) {
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque;
    return (int32_t)bmx_esp32_nvs_set_error(handle,
        handle ? nvs_erase_all(handle->value) : ESP_ERR_NVS_INVALID_HANDLE);
}

int32_t bmx_esp32_nvs_value_type(void *opaque, const BMXEmbeddedString *key) {
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque;
    char *native_key = bmx_esp32_nvs_key(handle, key);
    if (!native_key) return -1;
    nvs_type_t type = NVS_TYPE_ANY;
    esp_err_t error = nvs_find_key(handle->value, native_key, &type);
    bbMemFree(native_key);
    bmx_esp32_nvs_set_error(handle, error);
    return error == ESP_OK ? (int32_t)type : -1;
}

#define BMX_NVS_SET(NAME, TYPE, CALL) \
int32_t bmx_esp32_nvs_set_##NAME(void *opaque, const BMXEmbeddedString *key, TYPE value) { \
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque; \
    char *native_key = bmx_esp32_nvs_key(handle, key); \
    if (!native_key) return handle ? handle->last_error : ESP_ERR_NVS_INVALID_HANDLE; \
    esp_err_t error = CALL(handle->value, native_key, value); \
    bbMemFree(native_key); \
    return (int32_t)bmx_esp32_nvs_set_error(handle, error); \
}

BMX_NVS_SET(i32, int32_t, nvs_set_i32)
BMX_NVS_SET(u32, uint32_t, nvs_set_u32)
BMX_NVS_SET(i64, int64_t, nvs_set_i64)
BMX_NVS_SET(u64, uint64_t, nvs_set_u64)
BMX_NVS_SET(float, float, nvs_set_float)
BMX_NVS_SET(double, double, nvs_set_double)

#define BMX_NVS_GET(NAME, TYPE, CALL) \
int32_t bmx_esp32_nvs_get_##NAME(void *opaque, const BMXEmbeddedString *key, TYPE *value) { \
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque; \
    char *native_key = bmx_esp32_nvs_key(handle, key); \
    if (!native_key) return handle ? handle->last_error : ESP_ERR_NVS_INVALID_HANDLE; \
    esp_err_t error = value ? CALL(handle->value, native_key, value) : ESP_ERR_INVALID_ARG; \
    bbMemFree(native_key); \
    return (int32_t)bmx_esp32_nvs_set_error(handle, error); \
}

BMX_NVS_GET(i32, int32_t, nvs_get_i32)
BMX_NVS_GET(u32, uint32_t, nvs_get_u32)
BMX_NVS_GET(i64, int64_t, nvs_get_i64)
BMX_NVS_GET(u64, uint64_t, nvs_get_u64)
BMX_NVS_GET(float, float, nvs_get_float)
BMX_NVS_GET(double, double, nvs_get_double)

int32_t bmx_esp32_nvs_set_string(
    void *opaque, const BMXEmbeddedString *key, const BMXEmbeddedString *value) {
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque;
    char *native_key = bmx_esp32_nvs_key(handle, key);
    char *native_value = bmx_esp32_nvs_utf8(value);
    if (!native_key || !native_value) {
        if (native_key) bbMemFree(native_key);
        if (native_value) bbMemFree(native_value);
        return (int32_t)bmx_esp32_nvs_set_error(handle,
            handle ? ESP_ERR_NO_MEM : ESP_ERR_NVS_INVALID_HANDLE);
    }
    esp_err_t error = nvs_set_str(handle->value, native_key, native_value);
    bbMemFree(native_key);
    bbMemFree(native_value);
    return (int32_t)bmx_esp32_nvs_set_error(handle, error);
}

const BMXEmbeddedString *bmx_esp32_nvs_get_string(
    void *opaque, const BMXEmbeddedString *key) {
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque;
    char *native_key = bmx_esp32_nvs_key(handle, key);
    if (!native_key) return &bmx_embedded_empty_string;
    size_t size = 0;
    esp_err_t error = nvs_get_str(handle->value, native_key, NULL, &size);
    char *value = NULL;
    if (error == ESP_OK) {
        value = (char *)bbMemAlloc(size);
        if (!value) error = ESP_ERR_NO_MEM;
    }
    if (error == ESP_OK) error = nvs_get_str(handle->value, native_key, value, &size);
    bbMemFree(native_key);
    bmx_esp32_nvs_set_error(handle, error);
    if (error != ESP_OK) {
        if (value) bbMemFree(value);
        return &bmx_embedded_empty_string;
    }
    const BMXEmbeddedString *result = bmx_embedded_string_from_utf8_string((const uint8_t *)value);
    bbMemFree(value);
    return result;
}

int32_t bmx_esp32_nvs_set_blob(
    void *opaque, const BMXEmbeddedString *key, BMXEmbeddedArray *value) {
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque;
    char *native_key = bmx_esp32_nvs_key(handle, key);
    if (!native_key) return handle ? handle->last_error : ESP_ERR_NVS_INVALID_HANDLE;
    esp_err_t error = ESP_ERR_INVALID_ARG;
    if (value && value->element_size == sizeof(uint8_t) && value->length >= 0) {
        error = nvs_set_blob(handle->value, native_key, bmx_embedded_array_data(value),
            (size_t)value->length);
    }
    bbMemFree(native_key);
    return (int32_t)bmx_esp32_nvs_set_error(handle, error);
}

int32_t bmx_esp32_nvs_set_memory(
    void *opaque, const BMXEmbeddedString *key, const void *data, uint32_t size) {
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque;
    char *native_key = bmx_esp32_nvs_key(handle, key);
    if (!native_key) return handle ? handle->last_error : ESP_ERR_NVS_INVALID_HANDLE;
    esp_err_t error = (!data && size) ? ESP_ERR_INVALID_ARG :
        nvs_set_blob(handle->value, native_key, data, (size_t)size);
    bbMemFree(native_key);
    return (int32_t)bmx_esp32_nvs_set_error(handle, error);
}

BMXEmbeddedArray *bmx_esp32_nvs_get_blob(void *opaque, const BMXEmbeddedString *key) {
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque;
    char *native_key = bmx_esp32_nvs_key(handle, key);
    if (!native_key) return &bmx_embedded_empty_array;
    size_t size = 0;
    esp_err_t error = nvs_get_blob(handle->value, native_key, NULL, &size);
    BMXEmbeddedArray *result = &bmx_embedded_empty_array;
    if (error == ESP_OK && size > INT32_MAX) error = ESP_ERR_NVS_VALUE_TOO_LONG;
    if (error == ESP_OK && size) {
        result = bmx_embedded_array_new_1d((int32_t)size, sizeof(uint8_t),
            BMX_EMBEDDED_ARRAY_ELEMENT_VALUE, NULL, NULL);
        if (result == &bmx_embedded_empty_array) error = ESP_ERR_NO_MEM;
    }
    if (error == ESP_OK && size)
        error = nvs_get_blob(handle->value, native_key, bmx_embedded_array_data(result), &size);
    bbMemFree(native_key);
    bmx_esp32_nvs_set_error(handle, error);
    return error == ESP_OK ? result : &bmx_embedded_empty_array;
}

int32_t bmx_esp32_nvs_get_memory(void *opaque, const BMXEmbeddedString *key,
    void *data, uint32_t capacity, uint32_t *actual_size) {
    BMXESP32NVSHandle *handle = (BMXESP32NVSHandle *)opaque;
    char *native_key = bmx_esp32_nvs_key(handle, key);
    if (!native_key) return handle ? handle->last_error : ESP_ERR_NVS_INVALID_HANDLE;
    if (!actual_size) {
        bbMemFree(native_key);
        return (int32_t)bmx_esp32_nvs_set_error(handle, ESP_ERR_INVALID_ARG);
    }
    size_t required = 0;
    esp_err_t error = nvs_get_blob(handle->value, native_key, NULL, &required);
    if (required > UINT32_MAX) {
        error = ESP_ERR_NVS_VALUE_TOO_LONG;
        *actual_size = 0;
    } else {
        *actual_size = (uint32_t)required;
    }
    if (error == ESP_OK && required > capacity) error = ESP_ERR_NVS_INVALID_LENGTH;
    if (error == ESP_OK && required && !data) error = ESP_ERR_INVALID_ARG;
    if (error == ESP_OK && required) {
        size_t read_size = required;
        error = nvs_get_blob(handle->value, native_key, data, &read_size);
        if (read_size <= UINT32_MAX) *actual_size = (uint32_t)read_size;
    }
    bbMemFree(native_key);
    return (int32_t)bmx_esp32_nvs_set_error(handle, error);
}

int32_t bmx_esp32_nvs_statistics(
    uint32_t *used, uint32_t *free_entries, uint32_t *total, uint32_t *namespaces) {
    if (!used || !free_entries || !total || !namespaces) return ESP_ERR_INVALID_ARG;
    esp_err_t error = nvs_flash_init();
    if (error != ESP_OK) return (int32_t)error;
    nvs_stats_t statistics;
    error = nvs_get_stats(NULL, &statistics);
    if (error == ESP_OK) {
        *used = (uint32_t)statistics.used_entries;
        *free_entries = (uint32_t)statistics.free_entries;
        *total = (uint32_t)statistics.total_entries;
        *namespaces = (uint32_t)statistics.namespace_count;
    }
    return (int32_t)error;
}
