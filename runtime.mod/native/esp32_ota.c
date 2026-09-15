#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_runtime.h"
#include "esp_err.h"
#include "esp_ota_ops.h"

typedef struct BMXESP32OTAUpdate {
    esp_ota_handle_t handle;
    const esp_partition_t *partition;
    uint32_t expected_size;
    uint32_t written;
    esp_err_t last_error;
    uint8_t active;
    uint8_t finished;
} BMXESP32OTAUpdate;

static esp_err_t bmx_esp32_ota_begin_error = ESP_OK;

static const BMXEmbeddedString *bmx_esp32_ota_string(const char *value) {
    if (!value) value = "";
    return bmx_embedded_string_from_ascii(value, (int32_t)strlen(value));
}

static esp_err_t bmx_esp32_ota_error(BMXESP32OTAUpdate *update, esp_err_t error) {
    if (update) update->last_error = error;
    return error;
}

uint32_t bmx_esp32_ota_slot_count(void) {
    return (uint32_t)esp_ota_get_app_partition_count();
}

int32_t bmx_esp32_ota_supported(void) {
    return esp_ota_get_next_update_partition(NULL) != NULL;
}

int32_t bmx_esp32_ota_last_begin_error(void) {
    return (int32_t)bmx_esp32_ota_begin_error;
}

const BMXEmbeddedString *bmx_esp32_ota_result_name(int32_t result) {
    return bmx_esp32_ota_string(esp_err_to_name((esp_err_t)result));
}

void *bmx_esp32_ota_begin(uint32_t expected_size) {
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    if (!partition) {
        bmx_esp32_ota_begin_error = ESP_ERR_NOT_FOUND;
        return NULL;
    }
    if (expected_size > partition->size) {
        bmx_esp32_ota_begin_error = ESP_ERR_INVALID_SIZE;
        return NULL;
    }

    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)bbMemAlloc(sizeof(*update));
    if (!update) {
        bmx_esp32_ota_begin_error = ESP_ERR_NO_MEM;
        return NULL;
    }
    memset(update, 0, sizeof(*update));
    update->partition = partition;
    update->expected_size = expected_size;
    update->last_error = esp_ota_begin(partition,
        expected_size ? (size_t)expected_size : OTA_SIZE_UNKNOWN, &update->handle);
    if (update->last_error != ESP_OK) {
        bmx_esp32_ota_begin_error = update->last_error;
        bbMemFree(update);
        return NULL;
    }
    update->active = 1;
    bmx_esp32_ota_begin_error = ESP_OK;
    return update;
}

int32_t bmx_esp32_ota_is_open(void *opaque) {
    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)opaque;
    return update && update->active;
}

int32_t bmx_esp32_ota_is_finished(void *opaque) {
    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)opaque;
    return update && update->finished;
}

int32_t bmx_esp32_ota_last_error(void *opaque) {
    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)opaque;
    return update ? (int32_t)update->last_error : (int32_t)ESP_ERR_INVALID_STATE;
}

uint32_t bmx_esp32_ota_written(void *opaque) {
    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)opaque;
    return update ? update->written : 0;
}

uint32_t bmx_esp32_ota_expected_size(void *opaque) {
    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)opaque;
    return update ? update->expected_size : 0;
}

uint32_t bmx_esp32_ota_capacity(void *opaque) {
    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)opaque;
    return update && update->partition ? update->partition->size : 0;
}

const BMXEmbeddedString *bmx_esp32_ota_target_label(void *opaque) {
    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)opaque;
    return bmx_esp32_ota_string(update && update->partition ? update->partition->label : "");
}

int32_t bmx_esp32_ota_write(void *opaque, const void *data, uint32_t size) {
    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)opaque;
    if (!update || !update->active) return (int32_t)bmx_esp32_ota_error(update, ESP_ERR_INVALID_STATE);
    if (!data && size) return (int32_t)bmx_esp32_ota_error(update, ESP_ERR_INVALID_ARG);
    if (size > update->partition->size - update->written)
        return (int32_t)bmx_esp32_ota_error(update, ESP_ERR_INVALID_SIZE);
    if (update->expected_size && size > update->expected_size - update->written)
        return (int32_t)bmx_esp32_ota_error(update, ESP_ERR_INVALID_SIZE);
    esp_err_t error = esp_ota_write(update->handle, data, (size_t)size);
    if (error == ESP_OK) update->written += size;
    return (int32_t)bmx_esp32_ota_error(update, error);
}

int32_t bmx_esp32_ota_finish(void *opaque) {
    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)opaque;
    if (!update || !update->active) return (int32_t)bmx_esp32_ota_error(update, ESP_ERR_INVALID_STATE);
    if (update->expected_size && update->written != update->expected_size) {
        esp_ota_abort(update->handle);
        update->active = 0;
        return (int32_t)bmx_esp32_ota_error(update, ESP_ERR_INVALID_SIZE);
    }
    esp_err_t error = esp_ota_end(update->handle);
    update->active = 0;
    update->finished = error == ESP_OK;
    return (int32_t)bmx_esp32_ota_error(update, error);
}

int32_t bmx_esp32_ota_activate(void *opaque) {
    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)opaque;
    if (!update || !update->finished)
        return (int32_t)bmx_esp32_ota_error(update, ESP_ERR_INVALID_STATE);
    return (int32_t)bmx_esp32_ota_error(update, esp_ota_set_boot_partition(update->partition));
}

int32_t bmx_esp32_ota_abort(void *opaque) {
    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)opaque;
    if (!update) return ESP_ERR_INVALID_STATE;
    esp_err_t error = ESP_OK;
    if (update->active) error = esp_ota_abort(update->handle);
    update->active = 0;
    update->finished = 0;
    return (int32_t)bmx_esp32_ota_error(update, error);
}

void bmx_esp32_ota_destroy(void *opaque) {
    BMXESP32OTAUpdate *update = (BMXESP32OTAUpdate *)opaque;
    if (!update) return;
    if (update->active) esp_ota_abort(update->handle);
    bbMemFree(update);
}

const BMXEmbeddedString *bmx_esp32_ota_running_label(void) {
    const esp_partition_t *partition = esp_ota_get_running_partition();
    return bmx_esp32_ota_string(partition ? partition->label : "");
}

const BMXEmbeddedString *bmx_esp32_ota_boot_label(void) {
    const esp_partition_t *partition = esp_ota_get_boot_partition();
    return bmx_esp32_ota_string(partition ? partition->label : "");
}

int32_t bmx_esp32_ota_running_state(uint32_t *state) {
    if (!state) return ESP_ERR_INVALID_ARG;
    const esp_partition_t *partition = esp_ota_get_running_partition();
    esp_ota_img_states_t native_state;
    esp_err_t error = esp_ota_get_state_partition(partition, &native_state);
    if (error == ESP_OK) *state = (uint32_t)native_state;
    return (int32_t)error;
}

int32_t bmx_esp32_ota_mark_valid(void) {
    return (int32_t)esp_ota_mark_app_valid_cancel_rollback();
}

int32_t bmx_esp32_ota_can_rollback(void) {
    return esp_ota_check_rollback_is_possible();
}

int32_t bmx_esp32_ota_mark_invalid(void) {
    return (int32_t)esp_ota_mark_app_invalid_rollback();
}
