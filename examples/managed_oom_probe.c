#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_runtime.h"

#define BMX_ESP32_OOM_BLOCK_COUNT 256u
#define BMX_ESP32_OOM_BLOCK_SIZE 256u
#define BMX_ESP32_OOM_REUSE_SIZE 128u
#define BMX_ESP32_LANGUAGE_OOM_BLOCK_COUNT 1024u

extern int32_t bmx_esp32_runtime_core(void);

static void *bmx_esp32_language_oom_blocks[BMX_ESP32_LANGUAGE_OOM_BLOCK_COUNT];
static uint32_t bmx_esp32_language_oom_block_count;

void bmx_esp32_language_oom_end(void);

int32_t bmx_esp32_language_oom_begin(void) {
    static const size_t sizes[] = { 256u, 64u, 16u };
    if (bmx_esp32_language_oom_block_count) return 0;
    for (size_t size_index = 0u; size_index < sizeof(sizes) / sizeof(sizes[0]); ++size_index) {
        while (bmx_esp32_language_oom_block_count < BMX_ESP32_LANGUAGE_OOM_BLOCK_COUNT) {
            void *block = bbMemAlloc(sizes[size_index]);
            if (!block) break;
            bmx_esp32_language_oom_blocks[bmx_esp32_language_oom_block_count++] = block;
        }
    }
    const int32_t ready = bmx_esp32_language_oom_block_count > 0u &&
        bmx_esp32_language_oom_block_count < BMX_ESP32_LANGUAGE_OOM_BLOCK_COUNT &&
        bmx_embedded_heap_integrity_valid();
    if (!ready) bmx_esp32_language_oom_end();
    return ready;
}

void bmx_esp32_language_oom_end(void) {
    while (bmx_esp32_language_oom_block_count) {
        bbMemFree(bmx_esp32_language_oom_blocks[--bmx_esp32_language_oom_block_count]);
        bmx_esp32_language_oom_blocks[bmx_esp32_language_oom_block_count] = NULL;
    }
}

static int32_t bmx_esp32_oom_pattern_valid(void *memory, size_t size, uint8_t pattern) {
    const uint8_t *bytes = (const uint8_t *)memory;
    for (size_t index = 0; index < size; ++index) {
        if (bytes[index] != pattern) return 0;
    }
    return 1;
}

int32_t bmx_esp32_managed_oom_probe(void) {
    void *blocks[BMX_ESP32_OOM_BLOCK_COUNT] = { NULL };
    const uint32_t failures_before = bmx_embedded_arena_failure_count();
    const uint32_t automatic_before = bmx_embedded_automatic_collection_count();
    uint32_t block_count = 0u;
    void *small = NULL;
    void *extended = NULL;
    int32_t passed = 0;

    if (bmx_esp32_runtime_core() != 0 || !bmx_embedded_heap_integrity_valid()) goto cleanup;

    for (; block_count < BMX_ESP32_OOM_BLOCK_COUNT; ++block_count) {
        blocks[block_count] = bbMemAlloc(BMX_ESP32_OOM_BLOCK_SIZE);
        if (!blocks[block_count]) break;
        memset(blocks[block_count], (uint8_t)(0x5au ^ block_count), BMX_ESP32_OOM_BLOCK_SIZE);
    }

    if (block_count < 4u || block_count == BMX_ESP32_OOM_BLOCK_COUNT ||
        bmx_embedded_arena_failure_count() != failures_before + 1u ||
        bmx_embedded_automatic_collection_count() <= automatic_before ||
        !bmx_embedded_heap_integrity_valid()) goto cleanup;

    for (uint32_t index = 0u; index < block_count; ++index) {
        if (!bmx_esp32_oom_pattern_valid(blocks[index], BMX_ESP32_OOM_BLOCK_SIZE,
            (uint8_t)(0x5au ^ index))) goto cleanup;
    }

    for (uint32_t index = 1u; index < block_count; index += 2u) {
        bbMemFree(blocks[index]);
        blocks[index] = NULL;
    }
    if (!bmx_embedded_heap_reusable_bytes() || !bmx_embedded_heap_integrity_valid()) goto cleanup;

    for (uint32_t index = 1u; index < block_count; index += 2u) {
        blocks[index] = bbMemAlloc(BMX_ESP32_OOM_REUSE_SIZE);
        if (!blocks[index]) goto cleanup;
        memset(blocks[index], (uint8_t)(0xa5u ^ index), BMX_ESP32_OOM_REUSE_SIZE);
    }
    for (uint32_t index = 0u; index < block_count; index += 2u) {
        if (!bmx_esp32_oom_pattern_valid(blocks[index], BMX_ESP32_OOM_BLOCK_SIZE,
            (uint8_t)(0x5au ^ index))) goto cleanup;
    }

    for (uint32_t index = 0u; index < block_count; ++index) {
        bbMemFree(blocks[index]);
        blocks[index] = NULL;
    }
    if (bmx_embedded_heap_reusable_bytes() < block_count * BMX_ESP32_OOM_BLOCK_SIZE ||
        bmx_embedded_heap_largest_free_block() <= BMX_ESP32_OOM_BLOCK_SIZE ||
        !bmx_embedded_heap_integrity_valid()) goto cleanup;

    small = bbMemAlloc(64u);
    if (!small) goto cleanup;
    memset(small, 0x3cu, 64u);

    const uint32_t extend_failures_before = bmx_embedded_arena_failure_count();
    const uint32_t extend_automatic_before = bmx_embedded_automatic_collection_count();
    extended = bbMemExtend(small, 64u, (size_t)bmx_embedded_arena_capacity() + 16u);
    if (extended ||
        bmx_embedded_arena_failure_count() != extend_failures_before + 1u ||
        bmx_embedded_automatic_collection_count() <= extend_automatic_before ||
        !bmx_esp32_oom_pattern_valid(small, 64u, 0x3cu) ||
        !bmx_embedded_heap_integrity_valid()) goto cleanup;

    extended = bbMemExtend(small, 64u, 160u);
    if (!extended || !bmx_esp32_oom_pattern_valid(extended, 64u, 0x3cu)) goto cleanup;
    small = NULL;
    memset((uint8_t *)extended + 64u, 0xc3, 96u);
    bbMemFree(extended);
    extended = NULL;

    passed = bmx_embedded_arena_failure_count() == failures_before + 2u &&
        bmx_embedded_heap_integrity_valid();

cleanup:
    if (extended) bbMemFree(extended);
    if (small) bbMemFree(small);
    for (uint32_t index = 0u; index < block_count; ++index) {
        if (blocks[index]) bbMemFree(blocks[index]);
    }
    return passed && bmx_embedded_heap_integrity_valid();
}
