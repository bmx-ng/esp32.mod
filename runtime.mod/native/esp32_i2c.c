#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "blitzmax_esp32_board.h"

#include "blitzmax/embedded_i2c.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "soc/soc_caps.h"

#define BMX_I2C_ERROR_GENERIC (-1)
#define BMX_I2C_ERROR_TIMEOUT (-2)
#define BMX_I2C_ERROR_INVALID_ARGUMENT (-5)

typedef struct BMXESP32I2CState {
    i2c_master_bus_handle_t bus;
    uint32_t baudrate;
    int32_t sda_pin;
    int32_t scl_pin;
    uint8_t configured;
    uint8_t pull_ups;
} BMXESP32I2CState;

static BMXESP32I2CState bmx_esp32_i2c[SOC_I2C_NUM];

static BMXESP32I2CState *bmx_esp32_i2c_state(int32_t controller) {
    if (controller < 0 || controller >= SOC_I2C_NUM) return NULL;
    return &bmx_esp32_i2c[controller];
}

static int32_t bmx_esp32_i2c_error(esp_err_t error) {
    if (error == ESP_OK) return 0;
    if (error == ESP_ERR_TIMEOUT) return BMX_I2C_ERROR_TIMEOUT;
    if (error == ESP_ERR_INVALID_ARG) return BMX_I2C_ERROR_INVALID_ARGUMENT;
    return BMX_I2C_ERROR_GENERIC;
}

static int bmx_esp32_i2c_timeout_ms(uint32_t timeout_us) {
    uint64_t milliseconds = ((uint64_t)timeout_us + 999u) / 1000u;
    return milliseconds > INT_MAX ? INT_MAX : (int)milliseconds;
}

static int32_t bmx_esp32_i2c_transfer_valid(BMXESP32I2CState *state,
        uint32_t address, const void *data, int32_t length) {
    return state && state->bus && address <= 0x7fu && data && length > 0;
}

static esp_err_t bmx_esp32_i2c_add_device(BMXESP32I2CState *state,
        uint32_t address, i2c_master_dev_handle_t *device) {
    i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = (uint16_t)address,
        .scl_speed_hz = state->baudrate,
    };
    return i2c_master_bus_add_device(state->bus, &config, device);
}

int32_t bmx_embedded_i2c_controller_count(void) {
    return SOC_I2C_NUM;
}

int32_t bmx_embedded_i2c_default_controller(void) {
#ifdef BMX_ESP32_BOARD_I2C_CONTROLLER
    return BMX_ESP32_BOARD_I2C_CONTROLLER;
#else
    return -1;
#endif
}

uint32_t bmx_embedded_i2c_default_sda_pin(void) {
#ifdef BMX_ESP32_BOARD_I2C_SDA
    return BMX_ESP32_BOARD_I2C_SDA;
#else
    return UINT32_MAX;
#endif
}

uint32_t bmx_embedded_i2c_default_scl_pin(void) {
#ifdef BMX_ESP32_BOARD_I2C_SCL
    return BMX_ESP32_BOARD_I2C_SCL;
#else
    return UINT32_MAX;
#endif
}

int32_t bmx_embedded_i2c_configure_pins(int32_t controller, uint32_t sda_pin,
        uint32_t scl_pin, int32_t pull_ups) {
    BMXESP32I2CState *state = bmx_esp32_i2c_state(controller);
    if (!state || state->bus || sda_pin > INT32_MAX || scl_pin > INT32_MAX ||
            sda_pin == scl_pin || !GPIO_IS_VALID_GPIO((int32_t)sda_pin) ||
            !GPIO_IS_VALID_GPIO((int32_t)scl_pin)) return 0;
    state->sda_pin = (int32_t)sda_pin;
    state->scl_pin = (int32_t)scl_pin;
    state->pull_ups = pull_ups != 0;
    state->configured = 1;
    return 1;
}

uint32_t bmx_embedded_i2c_init(int32_t controller, uint32_t baudrate) {
    BMXESP32I2CState *state = bmx_esp32_i2c_state(controller);
    if (!state || !state->configured || state->bus || !baudrate) return 0;
    i2c_master_bus_config_t config = {
        .i2c_port = (i2c_port_num_t)controller,
        .sda_io_num = (gpio_num_t)state->sda_pin,
        .scl_io_num = (gpio_num_t)state->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = state->pull_ups,
    };
    if (i2c_new_master_bus(&config, &state->bus) != ESP_OK) {
        state->bus = NULL;
        return 0;
    }
    state->baudrate = baudrate;
    return baudrate;
}

int32_t bmx_embedded_i2c_deinit(int32_t controller) {
    BMXESP32I2CState *state = bmx_esp32_i2c_state(controller);
    if (!state || !state->bus || i2c_del_master_bus(state->bus) != ESP_OK) return 0;
    state->bus = NULL;
    state->baudrate = 0;
    return 1;
}

uint32_t bmx_embedded_i2c_set_baudrate(int32_t controller, uint32_t baudrate) {
    BMXESP32I2CState *state = bmx_esp32_i2c_state(controller);
    if (!state || !state->bus || !baudrate) return 0;
    state->baudrate = baudrate;
    return baudrate;
}

static int32_t bmx_esp32_i2c_write(int32_t controller, uint32_t address,
        void *data, int32_t length, int timeout_ms) {
    BMXESP32I2CState *state = bmx_esp32_i2c_state(controller);
    if (!bmx_esp32_i2c_transfer_valid(state, address, data, length))
        return BMX_I2C_ERROR_INVALID_ARGUMENT;
    i2c_master_dev_handle_t device = NULL;
    esp_err_t error = bmx_esp32_i2c_add_device(state, address, &device);
    if (error == ESP_OK)
        error = i2c_master_transmit(device, (const uint8_t *)data, (size_t)length, timeout_ms);
    if (device) (void)i2c_master_bus_rm_device(device);
    return error == ESP_OK ? length : bmx_esp32_i2c_error(error);
}

static int32_t bmx_esp32_i2c_read(int32_t controller, uint32_t address,
        void *data, int32_t length, int timeout_ms) {
    BMXESP32I2CState *state = bmx_esp32_i2c_state(controller);
    if (!bmx_esp32_i2c_transfer_valid(state, address, data, length))
        return BMX_I2C_ERROR_INVALID_ARGUMENT;
    i2c_master_dev_handle_t device = NULL;
    esp_err_t error = bmx_esp32_i2c_add_device(state, address, &device);
    if (error == ESP_OK)
        error = i2c_master_receive(device, (uint8_t *)data, (size_t)length, timeout_ms);
    if (device) (void)i2c_master_bus_rm_device(device);
    return error == ESP_OK ? length : bmx_esp32_i2c_error(error);
}

static int32_t bmx_esp32_i2c_write_read(int32_t controller, uint32_t address,
        void *write_data, int32_t write_length, void *read_data, int32_t read_length,
        int timeout_ms) {
    BMXESP32I2CState *state = bmx_esp32_i2c_state(controller);
    if (!bmx_esp32_i2c_transfer_valid(state, address, write_data, write_length) ||
            !read_data || read_length <= 0) return BMX_I2C_ERROR_INVALID_ARGUMENT;
    i2c_master_dev_handle_t device = NULL;
    esp_err_t error = bmx_esp32_i2c_add_device(state, address, &device);
    if (error == ESP_OK) {
        error = i2c_master_transmit_receive(device, (const uint8_t *)write_data,
            (size_t)write_length, (uint8_t *)read_data, (size_t)read_length, timeout_ms);
    }
    if (device) (void)i2c_master_bus_rm_device(device);
    return error == ESP_OK ? read_length : bmx_esp32_i2c_error(error);
}

int32_t bmx_embedded_i2c_write_blocking(int32_t controller, uint32_t address,
        void *data, int32_t length) {
    return bmx_esp32_i2c_write(controller, address, data, length, -1);
}

int32_t bmx_embedded_i2c_read_blocking(int32_t controller, uint32_t address,
        void *data, int32_t length) {
    return bmx_esp32_i2c_read(controller, address, data, length, -1);
}

int32_t bmx_embedded_i2c_write_timeout_us(int32_t controller, uint32_t address,
        void *data, int32_t length, uint32_t timeout_us) {
    return bmx_esp32_i2c_write(controller, address, data, length,
        bmx_esp32_i2c_timeout_ms(timeout_us));
}

int32_t bmx_embedded_i2c_read_timeout_us(int32_t controller, uint32_t address,
        void *data, int32_t length, uint32_t timeout_us) {
    return bmx_esp32_i2c_read(controller, address, data, length,
        bmx_esp32_i2c_timeout_ms(timeout_us));
}

int32_t bmx_embedded_i2c_write_read_blocking(int32_t controller, uint32_t address,
        void *write_data, int32_t write_length, void *read_data, int32_t read_length) {
    return bmx_esp32_i2c_write_read(controller, address, write_data, write_length,
        read_data, read_length, -1);
}

int32_t bmx_embedded_i2c_write_read_timeout_us(int32_t controller, uint32_t address,
        void *write_data, int32_t write_length, void *read_data, int32_t read_length,
        uint32_t timeout_us) {
    return bmx_esp32_i2c_write_read(controller, address, write_data, write_length,
        read_data, read_length, bmx_esp32_i2c_timeout_ms(timeout_us));
}
