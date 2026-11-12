/* Copyright (c) 2026 Bruce A Henderson and contributors
   SPDX-License-Identifier: Zlib */

#include <stdint.h>
#include <stdlib.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt1151.h"

#define BMX_GT1151_MAX_CONTACTS 10

typedef struct {
    i2c_master_bus_handle_t bus;
    esp_lcd_panel_io_handle_t io;
    esp_lcd_touch_handle_t touch;
    esp_lcd_touch_point_data_t contacts[BMX_GT1151_MAX_CONTACTS];
    uint8_t contact_count;
} bmx_gt1151_t;

static int bmx_gt1151_last_error;

void *bmx_esp32_gt1151_create(int width, int height, int i2c_port, int sda_pin,
        int scl_pin, unsigned int frequency_hz, int reset_pin, int interrupt_pin,
        int swap_xy, int mirror_x, int mirror_y) {
    if (width <= 0 || height <= 0 || sda_pin < 0 || scl_pin < 0) {
        bmx_gt1151_last_error = ESP_ERR_INVALID_ARG;
        return NULL;
    }

    bmx_gt1151_t *state = calloc(1, sizeof(*state));
    if (!state) {
        bmx_gt1151_last_error = ESP_ERR_NO_MEM;
        return NULL;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = i2c_port,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 1,
    };
    esp_err_t error = i2c_new_master_bus(&bus_config, &state->bus);
    if (error != ESP_OK) goto fail;

    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT1151_CONFIG();
    io_config.scl_speed_hz = frequency_hz ? frequency_hz : 400000;
    error = esp_lcd_new_panel_io_i2c(state->bus, &io_config, &state->io);
    if (error != ESP_OK) goto fail;

    esp_lcd_touch_config_t touch_config = {
        .x_max = width,
        .y_max = height,
        .rst_gpio_num = reset_pin,
        .int_gpio_num = interrupt_pin,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = swap_xy != 0,
            .mirror_x = mirror_x != 0,
            .mirror_y = mirror_y != 0,
        },
    };
    error = esp_lcd_touch_new_i2c_gt1151(state->io, &touch_config, &state->touch);
    if (error != ESP_OK) goto fail;

    bmx_gt1151_last_error = ESP_OK;
    return state;

fail:
    bmx_gt1151_last_error = error;
    if (state->touch) esp_lcd_touch_del(state->touch);
    if (state->io) esp_lcd_panel_io_del(state->io);
    if (state->bus) i2c_del_master_bus(state->bus);
    free(state);
    return NULL;
}

void bmx_esp32_gt1151_delete(void *handle) {
    bmx_gt1151_t *state = handle;
    if (!state) return;
    if (state->touch) esp_lcd_touch_del(state->touch);
    if (state->io) esp_lcd_panel_io_del(state->io);
    if (state->bus) i2c_del_master_bus(state->bus);
    free(state);
}

int bmx_esp32_gt1151_poll(void *handle) {
    bmx_gt1151_t *state = handle;
    if (!state || !state->touch) return -1;
    esp_err_t error = esp_lcd_touch_read_data(state->touch);
    if (error != ESP_OK) {
        bmx_gt1151_last_error = error;
        return -1;
    }
    state->contact_count = 0;
    error = esp_lcd_touch_get_data(state->touch, state->contacts,
        &state->contact_count, BMX_GT1151_MAX_CONTACTS);
    if (error != ESP_OK) {
        bmx_gt1151_last_error = error;
        return -1;
    }
    return state->contact_count;
}

static esp_lcd_touch_point_data_t *bmx_gt1151_contact(bmx_gt1151_t *state, int index) {
    if (!state || index < 0 || index >= state->contact_count) return NULL;
    return &state->contacts[index];
}

int bmx_esp32_gt1151_contact_id(void *handle, int index) {
    esp_lcd_touch_point_data_t *point = bmx_gt1151_contact(handle, index);
    return point ? point->track_id : -1;
}

int bmx_esp32_gt1151_contact_x(void *handle, int index) {
    esp_lcd_touch_point_data_t *point = bmx_gt1151_contact(handle, index);
    return point ? point->x : 0;
}

int bmx_esp32_gt1151_contact_y(void *handle, int index) {
    esp_lcd_touch_point_data_t *point = bmx_gt1151_contact(handle, index);
    return point ? point->y : 0;
}

int bmx_esp32_gt1151_last_error(void) {
    return bmx_gt1151_last_error;
}
