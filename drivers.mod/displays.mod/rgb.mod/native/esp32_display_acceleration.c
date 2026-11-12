/* Copyright (c) 2026 Bruce A Henderson and contributors
 * SPDX-License-Identifier: Zlib
 */

#include <stdint.h>
#include "soc/soc_caps.h"

#if SOC_PPA_SUPPORTED
#include "driver/ppa.h"

/* The fixed cost of a blocking PPA transaction outweighs software for very
 * small rectangles. This conservative threshold can be tuned independently
 * without changing the shared display API. */
#define BMX_ESP32_PPA_FILL_MIN_PIXELS 256

static ppa_client_handle_t bmx_esp32_ppa_fill_client;

static int bmx_esp32_ppa_ensure_fill_client(void) {
    if (bmx_esp32_ppa_fill_client) return 1;
    const ppa_client_config_t config = {
        .oper_type = PPA_OPERATION_FILL,
        .max_pending_trans_num = 1,
    };
    return ppa_register_client(&config, &bmx_esp32_ppa_fill_client) == ESP_OK;
}
#endif

typedef int32_t (*bmx_embedded_display_fill_rgb565_accelerator_t)(
    uint8_t *pixels, int32_t stride, int32_t buffer_width,
    int32_t buffer_height, int32_t x, int32_t y, int32_t width,
    int32_t height, uint32_t color);

extern void bmx_embedded_display_set_fill_rgb565_accelerator(
    bmx_embedded_display_fill_rgb565_accelerator_t accelerator);

int32_t bmx_embedded_display_accelerate_fill_rgb565(uint8_t *pixels,
    int32_t stride, int32_t buffer_width, int32_t buffer_height,
    int32_t x, int32_t y, int32_t width, int32_t height, uint32_t color);

int32_t bmx_esp32_display_acceleration_initialize(void) {
#if SOC_PPA_SUPPORTED
    bmx_embedded_display_set_fill_rgb565_accelerator(
        bmx_embedded_display_accelerate_fill_rgb565);
    return 1;
#else
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
