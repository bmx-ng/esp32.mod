#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_runtime.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

static const esp_partition_t *bmx_esp32_partition_at(
    uint32_t wanted, int32_t type, int32_t subtype) {
    esp_partition_iterator_t iterator = esp_partition_find(
        (esp_partition_type_t)(uint8_t)type,
        (esp_partition_subtype_t)(uint8_t)subtype, NULL);
    const esp_partition_t *result = NULL;
    uint32_t index = 0;
    while (iterator) {
        if (index++ == wanted) {
            result = esp_partition_get(iterator);
            break;
        }
        iterator = esp_partition_next(iterator);
    }
    esp_partition_iterator_release(iterator);
    return result;
}

uint32_t bmx_esp32_partition_count(int32_t type, int32_t subtype) {
    esp_partition_iterator_t iterator = esp_partition_find(
        (esp_partition_type_t)(uint8_t)type,
        (esp_partition_subtype_t)(uint8_t)subtype, NULL);
    uint32_t count = 0;
    while (iterator) {
        ++count;
        iterator = esp_partition_next(iterator);
    }
    esp_partition_iterator_release(iterator);
    return count;
}

const BMXEmbeddedString *bmx_esp32_partition_label(uint32_t index, int32_t type, int32_t subtype) {
    const esp_partition_t *partition = bmx_esp32_partition_at(index, type, subtype);
    return partition ? bmx_embedded_string_from_ascii(partition->label,
        (int32_t)strnlen(partition->label, sizeof(partition->label))) : &bmx_embedded_empty_string;
}

int32_t bmx_esp32_partition_type(uint32_t index, int32_t type, int32_t subtype) {
    const esp_partition_t *partition = bmx_esp32_partition_at(index, type, subtype);
    return partition ? (int32_t)partition->type : -1;
}

int32_t bmx_esp32_partition_subtype(uint32_t index, int32_t type, int32_t subtype) {
    const esp_partition_t *partition = bmx_esp32_partition_at(index, type, subtype);
    return partition ? (int32_t)partition->subtype : -1;
}

uint32_t bmx_esp32_partition_address(uint32_t index, int32_t type, int32_t subtype) {
    const esp_partition_t *partition = bmx_esp32_partition_at(index, type, subtype);
    return partition ? partition->address : 0;
}

uint32_t bmx_esp32_partition_size(uint32_t index, int32_t type, int32_t subtype) {
    const esp_partition_t *partition = bmx_esp32_partition_at(index, type, subtype);
    return partition ? partition->size : 0;
}

uint32_t bmx_esp32_partition_erase_size(uint32_t index, int32_t type, int32_t subtype) {
    const esp_partition_t *partition = bmx_esp32_partition_at(index, type, subtype);
    return partition ? partition->erase_size : 0;
}

int32_t bmx_esp32_partition_encrypted(uint32_t index, int32_t type, int32_t subtype) {
    const esp_partition_t *partition = bmx_esp32_partition_at(index, type, subtype);
    return partition && partition->encrypted;
}

int32_t bmx_esp32_partition_read_only(uint32_t index, int32_t type, int32_t subtype) {
    const esp_partition_t *partition = bmx_esp32_partition_at(index, type, subtype);
    return partition && partition->readonly;
}

int32_t bmx_esp32_partition_running(uint32_t index, int32_t type, int32_t subtype) {
    const esp_partition_t *partition = bmx_esp32_partition_at(index, type, subtype);
    const esp_partition_t *running = esp_ota_get_running_partition();
    return partition && running && partition->address == running->address &&
        partition->size == running->size && partition->type == running->type &&
        partition->subtype == running->subtype;
}
