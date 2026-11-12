/* Copyright (c) 2026 Bruce A Henderson and contributors
 * SPDX-License-Identifier: Zlib
 */

#include <stdint.h>
#include "esp_memory_utils.h"
#include "soc/soc_caps.h"

#if SOC_PPA_SUPPORTED
#include "driver/ppa.h"

/* The fixed cost of a blocking PPA transaction outweighs software for very
 * small rectangles. This conservative threshold can be tuned independently
 * without changing the shared display API. */
#define BMX_ESP32_PPA_FILL_MIN_PIXELS 256

static ppa_client_handle_t bmx_esp32_ppa_fill_client;
static ppa_client_handle_t bmx_esp32_ppa_srm_client;

static int bmx_esp32_ppa_ensure_fill_client(void) {
    if (bmx_esp32_ppa_fill_client) return 1;
    const ppa_client_config_t config = {
        .oper_type = PPA_OPERATION_FILL,
        .max_pending_trans_num = 1,
    };
    return ppa_register_client(&config, &bmx_esp32_ppa_fill_client) == ESP_OK;
}

static int bmx_esp32_ppa_ensure_srm_client(void) {
    if (bmx_esp32_ppa_srm_client) return 1;
    const ppa_client_config_t config = {
        .oper_type = PPA_OPERATION_SRM,
        .max_pending_trans_num = 1,
    };
    return ppa_register_client(&config, &bmx_esp32_ppa_srm_client) == ESP_OK;
}

static int bmx_esp32_ppa_exact_scale(int32_t source, int32_t destination,
        float *scale) {
    const uint32_t fixed = (uint32_t)(((uint64_t)(uint32_t)destination * 16U +
        (uint32_t)source - 1U) / (uint32_t)source);
    if (!fixed || fixed >= 4096U ||
            ((uint64_t)(uint32_t)source * fixed) / 16U !=
                (uint32_t)destination) return 0;
    *scale = (float)fixed / 16.0f;
    return 1;
}
#endif

typedef int32_t (*bmx_embedded_display_fill_rgb565_accelerator_t)(
    uint8_t *pixels, int32_t stride, int32_t buffer_width,
    int32_t buffer_height, int32_t x, int32_t y, int32_t width,
    int32_t height, uint32_t color);

extern void bmx_embedded_display_set_fill_rgb565_accelerator(
    bmx_embedded_display_fill_rgb565_accelerator_t accelerator);

typedef int32_t (*bmx_embedded_display_blit_rgb565_accelerator_t)(
    uint8_t *target, int32_t target_stride, int32_t target_width,
    int32_t target_height, const uint8_t *source, int32_t source_stride,
    int32_t source_width, int32_t source_height, int32_t destination_x,
    int32_t destination_y, int32_t destination_width,
    int32_t destination_height, int32_t clip_x, int32_t clip_y,
    int32_t clip_width, int32_t clip_height, int32_t draw_flags);

extern void bmx_embedded_display_set_blit_rgb565_accelerator(
    bmx_embedded_display_blit_rgb565_accelerator_t accelerator)
    __attribute__((weak));

int32_t bmx_embedded_display_accelerate_fill_rgb565(uint8_t *pixels,
    int32_t stride, int32_t buffer_width, int32_t buffer_height,
    int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color);

int32_t bmx_embedded_display_accelerate_blit_rgb565(uint8_t *target,
    int32_t target_stride, int32_t target_width, int32_t target_height,
    const uint8_t *source, int32_t source_stride, int32_t source_width,
    int32_t source_height, int32_t destination_x, int32_t destination_y,
    int32_t destination_width, int32_t destination_height, int32_t clip_x,
    int32_t clip_y, int32_t clip_width, int32_t clip_height,
    int32_t draw_flags);

int32_t bmx_esp32_display_acceleration_initialize(void) {
#if SOC_PPA_SUPPORTED
    bmx_embedded_display_set_fill_rgb565_accelerator(
        bmx_embedded_display_accelerate_fill_rgb565);
    if (bmx_embedded_display_set_blit_rgb565_accelerator) {
        bmx_embedded_display_set_blit_rgb565_accelerator(
            bmx_embedded_display_accelerate_blit_rgb565);
    }
    return 1;
#else
    return 0;
#endif
}

int32_t bmx_embedded_display_accelerate_blit_rgb565(uint8_t *target,
        int32_t target_stride, int32_t target_width, int32_t target_height,
        const uint8_t *source, int32_t source_stride, int32_t source_width,
        int32_t source_height, int32_t destination_x, int32_t destination_y,
        int32_t destination_width, int32_t destination_height,
        int32_t clip_x, int32_t clip_y, int32_t clip_width,
        int32_t clip_height, int32_t draw_flags) {
#if SOC_PPA_SUPPORTED
    float scale_x;
    float scale_y;
    if (!target || !source || target_stride != target_width * 2 ||
            source_stride != source_width * 2 || destination_x < 0 ||
            destination_y < 0 || destination_x + destination_width > target_width ||
            destination_y + destination_height > target_height ||
            clip_x != destination_x || clip_y != destination_y ||
            clip_width != destination_width || clip_height != destination_height ||
            !(esp_ptr_dma_capable(source) || esp_ptr_external_ram(source)) ||
            !bmx_esp32_ppa_exact_scale(source_width, destination_width, &scale_x) ||
            !bmx_esp32_ppa_exact_scale(source_height, destination_height, &scale_y) ||
            !bmx_esp32_ppa_ensure_srm_client()) return 0;

    const ppa_srm_oper_config_t config = {
        .in = {
            .buffer = source,
            .pic_w = (uint32_t)source_width,
            .pic_h = (uint32_t)source_height,
            .block_w = (uint32_t)source_width,
            .block_h = (uint32_t)source_height,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .out = {
            .buffer = target,
            .buffer_size = (uint32_t)((uint64_t)(uint32_t)target_stride *
                (uint32_t)target_height),
            .pic_w = (uint32_t)target_width,
            .pic_h = (uint32_t)target_height,
            .block_offset_x = (uint32_t)destination_x,
            .block_offset_y = (uint32_t)destination_y,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = scale_x,
        .scale_y = scale_y,
        .mirror_x = (draw_flags & 1) != 0,
        .mirror_y = (draw_flags & 2) != 0,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    return ppa_do_scale_rotate_mirror(bmx_esp32_ppa_srm_client, &config) == ESP_OK;
#else
    (void)target;
    (void)target_stride;
    (void)target_width;
    (void)target_height;
    (void)source;
    (void)source_stride;
    (void)source_width;
    (void)source_height;
    (void)destination_x;
    (void)destination_y;
    (void)destination_width;
    (void)destination_height;
    (void)clip_x;
    (void)clip_y;
    (void)clip_width;
    (void)clip_height;
    (void)draw_flags;
    return 0;
#endif
}

int32_t bmx_embedded_display_accelerate_fill_rgb565(uint8_t *pixels,
        int32_t stride, int32_t buffer_width, int32_t buffer_height,
        int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color) {
#if SOC_PPA_SUPPORTED
    if (!pixels || stride != buffer_width * 2 || buffer_width <= 0 ||
            buffer_height <= 0 || x < 0 || y < 0 || width <= 0 || height <= 0 ||
            (uint64_t)(uint32_t)width * (uint32_t)height < BMX_ESP32_PPA_FILL_MIN_PIXELS ||
            !bmx_esp32_ppa_ensure_fill_client()) return 0;

    const ppa_fill_oper_config_t config = {
        .out = {
            .buffer = pixels,
            .buffer_size = (uint32_t)((uint64_t)(uint32_t)stride *
                (uint32_t)buffer_height),
            .pic_w = (uint32_t)buffer_width,
            .pic_h = (uint32_t)buffer_height,
            .block_offset_x = (uint32_t)x,
            .block_offset_y = (uint32_t)y,
            .fill_cm = PPA_FILL_COLOR_MODE_RGB565,
        },
        .fill_block_w = (uint32_t)width,
        .fill_block_h = (uint32_t)height,
        .fill_argb_color = {
            .a = 0xff,
            .r = (uint8_t)(color >> 16),
            .g = (uint8_t)(color >> 8),
            .b = (uint8_t)color,
        },
        .mode = PPA_TRANS_MODE_BLOCKING,
    };
    return ppa_do_fill(bmx_esp32_ppa_fill_client, &config) == ESP_OK;
#else
    (void)pixels;
    (void)stride;
    (void)buffer_width;
    (void)buffer_height;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
    return 0;
#endif
}
