/* Copyright (c) 2026 Bruce A Henderson and contributors. SPDX-License-Identifier: Zlib */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "soc/soc_caps.h"

#if SOC_LCD_RGB_SUPPORTED
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_types.h"

typedef struct {
    esp_lcd_panel_handle_t panel;
    uint8_t *framebuffer;
    int32_t width;
    int32_t height;
    int32_t stride;
} bmx_esp32_rgb_display_t;

static int32_t last_error;

int32_t bmx_esp32_rgb_display_supported(void) { return 1; }
int32_t bmx_esp32_rgb_display_last_error(void) { return last_error; }

void *bmx_esp32_rgb_display_create(int32_t width, int32_t height,
        uint32_t pixel_clock_hz, int32_t hsync_pulse_width,
        int32_t hsync_back_porch, int32_t hsync_front_porch,
        int32_t vsync_pulse_width, int32_t vsync_back_porch,
        int32_t vsync_front_porch, int32_t hsync_pin, int32_t vsync_pin,
        int32_t data_enable_pin, int32_t pixel_clock_pin,
        int32_t display_enable_pin, const int32_t *data_pins,
        int32_t pixel_clock_active_negative,
        int32_t display_enable_active_low) {
    last_error = ESP_ERR_INVALID_ARG;
    if (width <= 0 || height <= 0 || !pixel_clock_hz || !data_pins) return NULL;
    bmx_esp32_rgb_display_t *display = calloc(1, sizeof(*display));
    if (!display) { last_error = ESP_ERR_NO_MEM; return NULL; }
    esp_lcd_rgb_panel_config_t config = {
        .clk_src = LCD_CLK_SRC_PLL160M,
        .timings = {
            .pclk_hz = pixel_clock_hz,
            .h_res = width,
            .v_res = height,
            .hsync_pulse_width = hsync_pulse_width,
            .hsync_back_porch = hsync_back_porch,
            .hsync_front_porch = hsync_front_porch,
            .vsync_pulse_width = vsync_pulse_width,
            .vsync_back_porch = vsync_back_porch,
            .vsync_front_porch = vsync_front_porch,
            .flags.pclk_active_neg = !!pixel_clock_active_negative,
        },
        .data_width = 16,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 1,
        .dma_burst_size = 64,
        .hsync_gpio_num = hsync_pin,
        .vsync_gpio_num = vsync_pin,
        .de_gpio_num = data_enable_pin,
        .pclk_gpio_num = pixel_clock_pin,
        .disp_gpio_num = display_enable_pin,
        .flags.disp_active_low = !!display_enable_active_low,
        .flags.fb_in_psram = 1,
    };
    for (int index = 0; index < 16; ++index) config.data_gpio_nums[index] = data_pins[index];
    esp_err_t result = esp_lcd_new_rgb_panel(&config, &display->panel);
    if (result == ESP_OK) result = esp_lcd_panel_reset(display->panel);
    if (result == ESP_OK) result = esp_lcd_panel_init(display->panel);
    if (result == ESP_OK) result = esp_lcd_rgb_panel_get_frame_buffer(
        display->panel, 1, (void **)&display->framebuffer);
    if (result == ESP_OK) result = esp_lcd_panel_disp_on_off(display->panel, true);
    if (result != ESP_OK || !display->framebuffer) {
        if (display->panel) esp_lcd_panel_del(display->panel);
        free(display);
        last_error = result != ESP_OK ? result : ESP_ERR_NO_MEM;
        return NULL;
    }
    display->width = width;
    display->height = height;
    display->stride = width * 2;
    memset(display->framebuffer, 0, (size_t)display->stride * (size_t)height);
    result = esp_lcd_panel_draw_bitmap(display->panel, 0, 0, width, height,
        display->framebuffer);
    if (result != ESP_OK) {
        esp_lcd_panel_del(display->panel);
        free(display);
        last_error = result;
        return NULL;
    }
    last_error = ESP_OK;
    return display;
}

void bmx_esp32_rgb_display_delete(void *handle) {
    bmx_esp32_rgb_display_t *display = handle;
    if (!display) return;
    if (display->panel) esp_lcd_panel_del(display->panel);
    free(display);
}

uint8_t *bmx_esp32_rgb_display_framebuffer(void *handle) {
    bmx_esp32_rgb_display_t *display = handle;
    return display ? display->framebuffer : NULL;
}

int32_t bmx_esp32_rgb_display_present(void *handle, uint8_t *pixels,
        int32_t stride, int32_t x, int32_t y, int32_t width, int32_t height) {
    bmx_esp32_rgb_display_t *display = handle;
    if (!display || !pixels || stride != display->stride || width <= 0 || height <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    int32_t x0 = x < 0 ? 0 : x;
    int32_t y0 = y < 0 ? 0 : y;
    int32_t x1 = x + width;
    int32_t y1 = y + height;
    if (x1 > display->width) x1 = display->width;
    if (y1 > display->height) y1 = display->height;
    if (x0 >= x1 || y0 >= y1) return ESP_ERR_INVALID_ARG;
    if (pixels != display->framebuffer &&
            (x0 != 0 || y0 != 0 || x1 != display->width || y1 != display->height)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return esp_lcd_panel_draw_bitmap(display->panel, x0, y0, x1, y1, pixels);
}

int32_t bmx_esp32_rgb_display_set_enabled(void *handle, int32_t enabled) {
    bmx_esp32_rgb_display_t *display = handle;
    return display ? esp_lcd_panel_disp_on_off(display->panel, !!enabled) : ESP_ERR_INVALID_ARG;
}

#else
static int32_t last_error = -1;
int32_t bmx_esp32_rgb_display_supported(void) { return 0; }
int32_t bmx_esp32_rgb_display_last_error(void) { return last_error; }
void *bmx_esp32_rgb_display_create(int32_t width, int32_t height,
        uint32_t pixel_clock_hz, int32_t hsync_pulse_width,
        int32_t hsync_back_porch, int32_t hsync_front_porch,
        int32_t vsync_pulse_width, int32_t vsync_back_porch,
        int32_t vsync_front_porch, int32_t hsync_pin, int32_t vsync_pin,
        int32_t data_enable_pin, int32_t pixel_clock_pin,
        int32_t display_enable_pin, const int32_t *data_pins,
        int32_t pixel_clock_active_negative, int32_t display_enable_active_low) {
    (void)width; (void)height; (void)pixel_clock_hz;
    (void)hsync_pulse_width; (void)hsync_back_porch; (void)hsync_front_porch;
    (void)vsync_pulse_width; (void)vsync_back_porch; (void)vsync_front_porch;
    (void)hsync_pin; (void)vsync_pin; (void)data_enable_pin; (void)pixel_clock_pin;
    (void)display_enable_pin; (void)data_pins; (void)pixel_clock_active_negative;
    (void)display_enable_active_low; return NULL;
}
void bmx_esp32_rgb_display_delete(void *handle) { (void)handle; }
uint8_t *bmx_esp32_rgb_display_framebuffer(void *handle) { (void)handle; return NULL; }
int32_t bmx_esp32_rgb_display_present(void *handle, uint8_t *pixels,
        int32_t stride, int32_t x, int32_t y, int32_t width, int32_t height) {
    (void)handle; (void)pixels; (void)stride; (void)x; (void)y; (void)width; (void)height;
    return -1;
}
int32_t bmx_esp32_rgb_display_set_enabled(void *handle, int32_t enabled) {
    (void)handle; (void)enabled; return -1;
}
#endif
