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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    esp_lcd_panel_handle_t panel;
    uint8_t *framebuffers[3];
    int32_t framebuffer_count;
    int32_t scan_index;
    int32_t render_index;
    int32_t pending_index;
    int32_t spare_index;
    int32_t width;
    int32_t height;
    int32_t stride;
    SemaphoreHandle_t vsync;
    SemaphoreHandle_t frame_complete;
    volatile int32_t awaiting_frame_complete;
    int32_t previous_dirty_valid;
    int32_t previous_dirty_x0;
    int32_t previous_dirty_y0;
    int32_t previous_dirty_x1;
    int32_t previous_dirty_y1;
} bmx_esp32_rgb_display_t;

static int32_t last_error;

int32_t bmx_esp32_rgb_display_supported(void) { return 1; }
int32_t bmx_esp32_rgb_display_last_error(void) { return last_error; }

static void bmx_esp32_rgb_display_copy_region(
        const bmx_esp32_rgb_display_t *display, uint8_t *destination,
        const uint8_t *source, int32_t x0, int32_t y0,
        int32_t x1, int32_t y1) {
    const size_t row_bytes = (size_t)(x1 - x0) * 2U;
    for (int32_t row = y0; row < y1; ++row) {
        memcpy(destination + (size_t)row * display->stride + (size_t)x0 * 2U,
            source + (size_t)row * display->stride + (size_t)x0 * 2U,
            row_bytes);
    }
}

static void bmx_esp32_rgb_display_preserve_next(
        bmx_esp32_rgb_display_t *display, uint8_t *destination,
        const uint8_t *source, int32_t x0, int32_t y0,
        int32_t x1, int32_t y1, int32_t preserve_contents) {
    if (!preserve_contents) {
        display->previous_dirty_valid = 0;
        return;
    }

    /* A triple-buffered render target is two submitted frames behind the
       source. Bring it through the preceding frame before applying the
       current changes. Double buffering is only one frame behind. */
    if (display->framebuffer_count == 3 && display->previous_dirty_valid) {
        bmx_esp32_rgb_display_copy_region(display, destination, source,
            display->previous_dirty_x0, display->previous_dirty_y0,
            display->previous_dirty_x1, display->previous_dirty_y1);
    }
    bmx_esp32_rgb_display_copy_region(display, destination, source,
        x0, y0, x1, y1);

    if (display->framebuffer_count == 3) {
        display->previous_dirty_x0 = x0;
        display->previous_dirty_y0 = y0;
        display->previous_dirty_x1 = x1;
        display->previous_dirty_y1 = y1;
        display->previous_dirty_valid = 1;
    }
}

static bool IRAM_ATTR bmx_esp32_rgb_display_on_vsync(
        esp_lcd_panel_handle_t panel,
        const esp_lcd_rgb_panel_event_data_t *event_data, void *user_ctx) {
    (void)panel;
    (void)event_data;
    bmx_esp32_rgb_display_t *display = user_ctx;
    BaseType_t task_woken = pdFALSE;
    if (display && display->vsync) {
        xSemaphoreGiveFromISR(display->vsync, &task_woken);
    }
    return task_woken == pdTRUE;
}

static bool IRAM_ATTR bmx_esp32_rgb_display_on_frame_complete(
        esp_lcd_panel_handle_t panel,
        const esp_lcd_rgb_panel_event_data_t *event_data, void *user_ctx) {
    (void)panel;
    (void)event_data;
    bmx_esp32_rgb_display_t *display = user_ctx;
    BaseType_t task_woken = pdFALSE;
    if (display && display->frame_complete && display->awaiting_frame_complete) {
        xSemaphoreGiveFromISR(display->frame_complete, &task_woken);
    }
    return task_woken == pdTRUE;
}

void *bmx_esp32_rgb_display_create(int32_t width, int32_t height,
        uint32_t pixel_clock_hz, int32_t hsync_pulse_width,
        int32_t hsync_back_porch, int32_t hsync_front_porch,
        int32_t vsync_pulse_width, int32_t vsync_back_porch,
        int32_t vsync_front_porch, int32_t hsync_pin, int32_t vsync_pin,
        int32_t data_enable_pin, int32_t pixel_clock_pin,
        int32_t display_enable_pin, const int32_t *data_pins,
        int32_t pixel_clock_active_negative,
        int32_t display_enable_active_low, int32_t framebuffer_count) {
    last_error = ESP_ERR_INVALID_ARG;
    if (width <= 0 || height <= 0 || !pixel_clock_hz || !data_pins ||
            framebuffer_count < 1 || framebuffer_count > 3) return NULL;
    bmx_esp32_rgb_display_t *display = calloc(1, sizeof(*display));
    if (!display) { last_error = ESP_ERR_NO_MEM; return NULL; }
    display->vsync = xSemaphoreCreateBinary();
    display->frame_complete = xSemaphoreCreateBinary();
    if (!display->vsync || !display->frame_complete) {
        if (display->vsync) vSemaphoreDelete(display->vsync);
        if (display->frame_complete) vSemaphoreDelete(display->frame_complete);
        free(display);
        last_error = ESP_ERR_NO_MEM;
        return NULL;
    }
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
        .num_fbs = framebuffer_count,
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
    if (result == ESP_OK) {
        const esp_lcd_rgb_panel_event_callbacks_t callbacks = {
            .on_vsync = bmx_esp32_rgb_display_on_vsync,
            .on_frame_buf_complete = bmx_esp32_rgb_display_on_frame_complete,
        };
        result = esp_lcd_rgb_panel_register_event_callbacks(display->panel,
            &callbacks, display);
    }
    if (result == ESP_OK) result = esp_lcd_panel_reset(display->panel);
    if (result == ESP_OK) result = esp_lcd_panel_init(display->panel);
    if (result == ESP_OK && framebuffer_count == 1) {
        result = esp_lcd_rgb_panel_get_frame_buffer(display->panel, 1,
            (void **)&display->framebuffers[0]);
    } else if (result == ESP_OK && framebuffer_count == 2) {
        result = esp_lcd_rgb_panel_get_frame_buffer(display->panel, 2,
            (void **)&display->framebuffers[0], (void **)&display->framebuffers[1]);
    } else if (result == ESP_OK) {
        result = esp_lcd_rgb_panel_get_frame_buffer(display->panel, 3,
            (void **)&display->framebuffers[0], (void **)&display->framebuffers[1],
            (void **)&display->framebuffers[2]);
    }
    if (result == ESP_OK) result = esp_lcd_panel_disp_on_off(display->panel, true);
    if (result != ESP_OK || !display->framebuffers[0] ||
            (framebuffer_count >= 2 && !display->framebuffers[1]) ||
            (framebuffer_count == 3 && !display->framebuffers[2])) {
        if (display->panel) esp_lcd_panel_del(display->panel);
        vSemaphoreDelete(display->vsync);
        vSemaphoreDelete(display->frame_complete);
        free(display);
        last_error = result != ESP_OK ? result : ESP_ERR_NO_MEM;
        return NULL;
    }
    display->width = width;
    display->height = height;
    display->stride = width * 2;
    display->framebuffer_count = framebuffer_count;
    display->scan_index = 0;
    display->render_index = framebuffer_count >= 2 ? 1 : 0;
    display->pending_index = -1;
    display->spare_index = framebuffer_count == 3 ? 2 : -1;
    memset(display->framebuffers[0], 0, (size_t)display->stride * (size_t)height);
    if (framebuffer_count == 2) {
        memset(display->framebuffers[1], 0, (size_t)display->stride * (size_t)height);
    } else if (framebuffer_count == 3) {
        memset(display->framebuffers[1], 0, (size_t)display->stride * (size_t)height);
        memset(display->framebuffers[2], 0, (size_t)display->stride * (size_t)height);
    }
    result = esp_lcd_panel_draw_bitmap(display->panel, 0, 0, width, height,
        display->framebuffers[display->scan_index]);
    if (result != ESP_OK) {
        esp_lcd_panel_del(display->panel);
        vSemaphoreDelete(display->vsync);
        vSemaphoreDelete(display->frame_complete);
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
    if (display->vsync) vSemaphoreDelete(display->vsync);
    if (display->frame_complete) vSemaphoreDelete(display->frame_complete);
    free(display);
}

uint8_t *bmx_esp32_rgb_display_framebuffer(void *handle) {
    bmx_esp32_rgb_display_t *display = handle;
    return display ? display->framebuffers[display->render_index] : NULL;
}

int32_t bmx_esp32_rgb_display_framebuffer_count(void *handle) {
    bmx_esp32_rgb_display_t *display = handle;
    return display ? display->framebuffer_count : 0;
}

int32_t bmx_esp32_rgb_display_present(void *handle, uint8_t *pixels,
        int32_t stride, int32_t x, int32_t y, int32_t width, int32_t height,
        int32_t preserve_contents) {
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
    if (display->framebuffer_count == 1 && pixels != display->framebuffers[0] &&
            (x0 != 0 || y0 != 0 || x1 != display->width || y1 != display->height)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (display->framebuffer_count == 1) {
        return esp_lcd_panel_draw_bitmap(display->panel, x0, y0, x1, y1, pixels);
    }

    if (display->framebuffer_count == 3 && display->pending_index >= 0) {
        if (xSemaphoreTake(display->frame_complete, pdMS_TO_TICKS(1000)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
        display->awaiting_frame_complete = 0;
        const int32_t old_scan_index = display->scan_index;
        display->scan_index = display->pending_index;
        display->pending_index = -1;
        display->spare_index = old_scan_index;
    }

    uint8_t *render = display->framebuffers[display->render_index];
    if (pixels != render) {
        bmx_esp32_rgb_display_copy_region(display, render, pixels,
            x0, y0, x1, y1);
    }

    xSemaphoreTake(display->frame_complete, 0);
    display->awaiting_frame_complete = 1;
    esp_err_t result = esp_lcd_panel_draw_bitmap(display->panel, x0, y0, x1, y1, render);
    if (result != ESP_OK) {
        display->awaiting_frame_complete = 0;
        return result;
    }

    if (display->framebuffer_count == 3) {
        display->pending_index = display->render_index;
        display->render_index = display->spare_index;
        display->spare_index = -1;
        bmx_esp32_rgb_display_preserve_next(display,
            display->framebuffers[display->render_index], render,
            x0, y0, x1, y1, preserve_contents);
        return ESP_OK;
    }

    if (xSemaphoreTake(display->frame_complete, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    display->awaiting_frame_complete = 0;

    const int32_t old_scan_index = display->scan_index;
    display->scan_index = display->render_index;
    display->render_index = old_scan_index;
    bmx_esp32_rgb_display_preserve_next(display,
        display->framebuffers[display->render_index], render,
        x0, y0, x1, y1, preserve_contents);
    return ESP_OK;
}

int32_t bmx_esp32_rgb_display_present_regions(void *handle, uint8_t *pixels,
        int32_t stride, const int32_t *x, const int32_t *y,
        const int32_t *width, const int32_t *height, int32_t count,
        int32_t preserve_contents) {
    bmx_esp32_rgb_display_t *display = handle;
    if (!display || !pixels || stride != display->stride || !x || !y ||
            !width || !height || count <= 0) return ESP_ERR_INVALID_ARG;
    if (count == 1) {
        return bmx_esp32_rgb_display_present(handle, pixels, stride,
            x[0], y[0], width[0], height[0], preserve_contents);
    }

    for (int32_t index = 0; index < count; ++index) {
        if (x[index] < 0 || y[index] < 0 || width[index] <= 0 || height[index] <= 0 ||
                x[index] + width[index] > display->width ||
                y[index] + height[index] > display->height) return ESP_ERR_INVALID_ARG;
    }

    int32_t dirty_x0 = x[0];
    int32_t dirty_y0 = y[0];
    int32_t dirty_x1 = x[0] + width[0];
    int32_t dirty_y1 = y[0] + height[0];
    for (int32_t index = 1; index < count; ++index) {
        if (x[index] < dirty_x0) dirty_x0 = x[index];
        if (y[index] < dirty_y0) dirty_y0 = y[index];
        if (x[index] + width[index] > dirty_x1) dirty_x1 = x[index] + width[index];
        if (y[index] + height[index] > dirty_y1) dirty_y1 = y[index] + height[index];
    }

    if (display->framebuffer_count == 1) {
        if (pixels != display->framebuffers[0]) return ESP_ERR_NOT_SUPPORTED;
        for (int32_t index = 0; index < count; ++index) {
            esp_err_t result = esp_lcd_panel_draw_bitmap(display->panel,
                x[index], y[index], x[index] + width[index],
                y[index] + height[index], pixels);
            if (result != ESP_OK) return result;
        }
        return ESP_OK;
    }

    if (display->framebuffer_count == 3 && display->pending_index >= 0) {
        if (xSemaphoreTake(display->frame_complete, pdMS_TO_TICKS(1000)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
        display->awaiting_frame_complete = 0;
        const int32_t old_scan_index = display->scan_index;
        display->scan_index = display->pending_index;
        display->pending_index = -1;
        display->spare_index = old_scan_index;
    }

    uint8_t *render = display->framebuffers[display->render_index];
    if (pixels != render) {
        for (int32_t index = 0; index < count; ++index) {
            const size_t row_bytes = (size_t)width[index] * 2U;
            for (int32_t row = y[index]; row < y[index] + height[index]; ++row) {
                memcpy(render + (size_t)row * display->stride + (size_t)x[index] * 2U,
                    pixels + (size_t)row * stride + (size_t)x[index] * 2U, row_bytes);
            }
        }
    }

    xSemaphoreTake(display->frame_complete, 0);
    display->awaiting_frame_complete = 1;
    esp_err_t result = esp_lcd_panel_draw_bitmap(display->panel, 0, 0,
        display->width, display->height, render);
    if (result != ESP_OK) {
        display->awaiting_frame_complete = 0;
        return result;
    }

    if (display->framebuffer_count == 3) {
        display->pending_index = display->render_index;
        display->render_index = display->spare_index;
        display->spare_index = -1;
    } else {
        if (xSemaphoreTake(display->frame_complete, pdMS_TO_TICKS(1000)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
        display->awaiting_frame_complete = 0;
        const int32_t old_scan_index = display->scan_index;
        display->scan_index = display->render_index;
        display->render_index = old_scan_index;
    }

    bmx_esp32_rgb_display_preserve_next(display,
        display->framebuffers[display->render_index], render,
        dirty_x0, dirty_y0, dirty_x1, dirty_y1, preserve_contents);
    return ESP_OK;
}

int32_t bmx_esp32_rgb_display_set_enabled(void *handle, int32_t enabled) {
    bmx_esp32_rgb_display_t *display = handle;
    return display ? esp_lcd_panel_disp_on_off(display->panel, !!enabled) : ESP_ERR_INVALID_ARG;
}

int32_t bmx_esp32_rgb_display_wait_vsync(void *handle, int32_t timeout_ms) {
    bmx_esp32_rgb_display_t *display = handle;
    if (!display || !display->vsync || timeout_ms < 0) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(display->vsync, 0);
    TickType_t ticks = timeout_ms == 0 ? 0 : pdMS_TO_TICKS(timeout_ms);
    if (timeout_ms > 0 && ticks == 0) ticks = 1;
    return xSemaphoreTake(display->vsync, ticks) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
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
        int32_t pixel_clock_active_negative, int32_t display_enable_active_low,
        int32_t framebuffer_count) {
    (void)width; (void)height; (void)pixel_clock_hz;
    (void)hsync_pulse_width; (void)hsync_back_porch; (void)hsync_front_porch;
    (void)vsync_pulse_width; (void)vsync_back_porch; (void)vsync_front_porch;
    (void)hsync_pin; (void)vsync_pin; (void)data_enable_pin; (void)pixel_clock_pin;
    (void)display_enable_pin; (void)data_pins; (void)pixel_clock_active_negative;
    (void)display_enable_active_low; (void)framebuffer_count; return NULL;
}
void bmx_esp32_rgb_display_delete(void *handle) { (void)handle; }
uint8_t *bmx_esp32_rgb_display_framebuffer(void *handle) { (void)handle; return NULL; }
int32_t bmx_esp32_rgb_display_framebuffer_count(void *handle) { (void)handle; return 0; }
int32_t bmx_esp32_rgb_display_present(void *handle, uint8_t *pixels,
        int32_t stride, int32_t x, int32_t y, int32_t width, int32_t height,
        int32_t preserve_contents) {
    (void)handle; (void)pixels; (void)stride; (void)x; (void)y; (void)width; (void)height;
    (void)preserve_contents;
    return -1;
}
int32_t bmx_esp32_rgb_display_present_regions(void *handle, uint8_t *pixels,
        int32_t stride, const int32_t *x, const int32_t *y,
        const int32_t *width, const int32_t *height, int32_t count,
        int32_t preserve_contents) {
    (void)handle; (void)pixels; (void)stride; (void)x; (void)y;
    (void)width; (void)height; (void)count; (void)preserve_contents;
    return -1;
}
int32_t bmx_esp32_rgb_display_set_enabled(void *handle, int32_t enabled) {
    (void)handle; (void)enabled; return -1;
}
int32_t bmx_esp32_rgb_display_wait_vsync(void *handle, int32_t timeout_ms) {
    (void)handle; (void)timeout_ms; return -1;
}
#endif
