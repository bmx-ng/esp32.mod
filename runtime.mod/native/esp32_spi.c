#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "blitzmax_esp32_board.h"
#include <string.h>

#include "blitzmax/embedded_spi.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "soc/soc_caps.h"

#define BMX_SPI_CONTROLLER_COUNT (SOC_SPI_PERIPH_NUM - 1)
#define BMX_SPI_ERROR_GENERIC (-1)
#define BMX_SPI_ERROR_INVALID_ARGUMENT (-5)

typedef struct BMXESP32SPIState {
    spi_device_handle_t device;
    uint32_t baudrate;
    int32_t rx_pin;
    int32_t tx_pin;
    int32_t sck_pin;
    uint8_t configured;
    uint8_t initialized;
    uint8_t data_bits;
    uint8_t polarity;
    uint8_t phase;
    uint8_t bit_order;
} BMXESP32SPIState;

static BMXESP32SPIState bmx_esp32_spi[BMX_SPI_CONTROLLER_COUNT];

static BMXESP32SPIState *bmx_esp32_spi_state(int32_t controller) {
    if (controller < 0 || controller >= BMX_SPI_CONTROLLER_COUNT) return NULL;
    return &bmx_esp32_spi[controller];
}

static spi_host_device_t bmx_esp32_spi_host(int32_t controller) {
    return (spi_host_device_t)(controller + 1);
}

static int32_t bmx_esp32_spi_add_device(int32_t controller, BMXESP32SPIState *state) {
    spi_device_interface_config_t config = {
        .mode = (uint8_t)((state->polarity << 1u) | state->phase),
        .clock_speed_hz = (int)state->baudrate,
        .spics_io_num = -1,
        .flags = state->bit_order == 0 ? SPI_DEVICE_BIT_LSBFIRST : 0,
        .queue_size = 1,
    };
    return spi_bus_add_device(bmx_esp32_spi_host(controller), &config,
        &state->device) == ESP_OK;
}

static int32_t bmx_esp32_spi_reconfigure(int32_t controller,
        BMXESP32SPIState *state) {
    if (!state->initialized) return 0;
    if (state->device && spi_bus_remove_device(state->device) != ESP_OK) return 0;
    state->device = NULL;
    return bmx_esp32_spi_add_device(controller, state);
}

static int32_t bmx_esp32_spi_transfer(int32_t controller, const void *source,
        void *destination, int32_t byte_length) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    if (!state || !state->device || byte_length < 0 ||
            (byte_length && !source && !destination)) return BMX_SPI_ERROR_INVALID_ARGUMENT;
    const uint8_t *tx = (const uint8_t *)source;
    uint8_t *rx = (uint8_t *)destination;
    int32_t transferred = 0;
    while (transferred < byte_length) {
        int32_t chunk = byte_length - transferred;
        if (chunk > 4092) chunk = 4092;
        spi_transaction_t transaction = {
            .length = (size_t)chunk * 8u,
            .tx_buffer = tx ? tx + transferred : NULL,
            .rx_buffer = rx ? rx + transferred : NULL,
        };
        if (spi_device_polling_transmit(state->device, &transaction) != ESP_OK)
            return BMX_SPI_ERROR_GENERIC;
        transferred += chunk;
    }
    return byte_length;
}

int32_t bmx_embedded_spi_controller_count(void) {
    return BMX_SPI_CONTROLLER_COUNT;
}

int32_t bmx_embedded_spi_default_controller(void) {
#ifdef BMX_ESP32_BOARD_SPI_CONTROLLER
    return BMX_ESP32_BOARD_SPI_CONTROLLER;
#else
    return -1;
#endif
}

uint32_t bmx_embedded_spi_default_rx_pin(void) {
#ifdef BMX_ESP32_BOARD_SPI_MISO
    return BMX_ESP32_BOARD_SPI_MISO;
#else
    return UINT32_MAX;
#endif
}

uint32_t bmx_embedded_spi_default_tx_pin(void) {
#ifdef BMX_ESP32_BOARD_SPI_MOSI
    return BMX_ESP32_BOARD_SPI_MOSI;
#else
    return UINT32_MAX;
#endif
}

uint32_t bmx_embedded_spi_default_sck_pin(void) {
#ifdef BMX_ESP32_BOARD_SPI_CLOCK
    return BMX_ESP32_BOARD_SPI_CLOCK;
#else
    return UINT32_MAX;
#endif
}

uint32_t bmx_embedded_spi_default_csn_pin(void) {
#ifdef BMX_ESP32_BOARD_SPI_CHIP_SELECT
    return BMX_ESP32_BOARD_SPI_CHIP_SELECT;
#else
    return UINT32_MAX;
#endif
}

int32_t bmx_embedded_spi_configure_pins(int32_t controller, uint32_t rx_pin,
        uint32_t tx_pin, uint32_t sck_pin) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    if (!state || state->initialized || rx_pin > INT32_MAX || tx_pin > INT32_MAX ||
            sck_pin > INT32_MAX || rx_pin == tx_pin || rx_pin == sck_pin ||
            tx_pin == sck_pin || !GPIO_IS_VALID_GPIO((int32_t)rx_pin) ||
            !GPIO_IS_VALID_OUTPUT_GPIO((int32_t)tx_pin) ||
            !GPIO_IS_VALID_OUTPUT_GPIO((int32_t)sck_pin)) return 0;
    state->rx_pin = (int32_t)rx_pin;
    state->tx_pin = (int32_t)tx_pin;
    state->sck_pin = (int32_t)sck_pin;
    state->configured = 1;
    return 1;
}

uint32_t bmx_embedded_spi_init(int32_t controller, uint32_t baudrate) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    if (!state || !state->configured || state->initialized || !baudrate ||
            baudrate > INT_MAX) return 0;
    spi_bus_config_t bus_config = {
        .mosi_io_num = state->tx_pin,
        .miso_io_num = state->rx_pin,
        .sclk_io_num = state->sck_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = 4092,
    };
    if (spi_bus_initialize(bmx_esp32_spi_host(controller), &bus_config,
            SPI_DMA_CH_AUTO) != ESP_OK) return 0;
    state->baudrate = baudrate;
    state->data_bits = 8;
    state->polarity = 0;
    state->phase = 0;
    state->bit_order = 1;
    state->initialized = 1;
    if (!bmx_esp32_spi_add_device(controller, state)) {
        state->initialized = 0;
        (void)spi_bus_free(bmx_esp32_spi_host(controller));
        return 0;
    }
    return bmx_embedded_spi_get_baudrate(controller);
}

int32_t bmx_embedded_spi_deinit(int32_t controller) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    if (!state || !state->initialized) return 0;
    if (state->device && spi_bus_remove_device(state->device) != ESP_OK) return 0;
    state->device = NULL;
    if (spi_bus_free(bmx_esp32_spi_host(controller)) != ESP_OK) return 0;
    state->initialized = 0;
    state->baudrate = 0;
    return 1;
}

uint32_t bmx_embedded_spi_set_baudrate(int32_t controller, uint32_t baudrate) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    if (!state || !state->initialized || !baudrate || baudrate > INT_MAX) return 0;
    uint32_t previous = state->baudrate;
    state->baudrate = baudrate;
    if (!bmx_esp32_spi_reconfigure(controller, state)) {
        state->baudrate = previous;
        (void)bmx_esp32_spi_add_device(controller, state);
        return 0;
    }
    return bmx_embedded_spi_get_baudrate(controller);
}

uint32_t bmx_embedded_spi_get_baudrate(int32_t controller) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    int frequency_khz = 0;
    if (!state || !state->device ||
            spi_device_get_actual_freq(state->device, &frequency_khz) != ESP_OK) return 0;
    return frequency_khz > 0 ? (uint32_t)frequency_khz * 1000u : 0;
}

int32_t bmx_embedded_spi_set_format(int32_t controller, uint32_t data_bits,
        uint32_t polarity, uint32_t phase, uint32_t bit_order) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    if (!state || !state->initialized || (data_bits != 8 && data_bits != 16) ||
            polarity > 1 || phase > 1 || bit_order > 1) return 0;
    uint8_t old_bits = state->data_bits;
    uint8_t old_polarity = state->polarity;
    uint8_t old_phase = state->phase;
    uint8_t old_order = state->bit_order;
    state->data_bits = (uint8_t)data_bits;
    state->polarity = (uint8_t)polarity;
    state->phase = (uint8_t)phase;
    state->bit_order = (uint8_t)bit_order;
    if (!bmx_esp32_spi_reconfigure(controller, state)) {
        state->data_bits = old_bits;
        state->polarity = old_polarity;
        state->phase = old_phase;
        state->bit_order = old_order;
        (void)bmx_esp32_spi_add_device(controller, state);
        return 0;
    }
    return 1;
}

int32_t bmx_embedded_spi_write_read_blocking(int32_t controller, void *source,
        void *destination, int32_t length) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    if (!state || state->data_bits != 8 || length < 0 ||
            (length && (!source || !destination))) return BMX_SPI_ERROR_INVALID_ARGUMENT;
    return bmx_esp32_spi_transfer(controller, source, destination, length);
}

int32_t bmx_embedded_spi_write_blocking(int32_t controller, void *source, int32_t length) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    if (!state || state->data_bits != 8 || length < 0 || (length && !source))
        return BMX_SPI_ERROR_INVALID_ARGUMENT;
    return bmx_esp32_spi_transfer(controller, source, NULL, length);
}

int32_t bmx_embedded_spi_read_blocking(int32_t controller, uint32_t repeated_data,
        void *destination, int32_t length) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    if (!state || state->data_bits != 8 || repeated_data > 0xffu || length < 0 ||
            (length && !destination)) return BMX_SPI_ERROR_INVALID_ARGUMENT;
    uint8_t repeated[64];
    memset(repeated, (uint8_t)repeated_data, sizeof(repeated));
    uint8_t *output = (uint8_t *)destination;
    int32_t transferred = 0;
    while (transferred < length) {
        int32_t chunk = length - transferred;
        if (chunk > (int32_t)sizeof(repeated)) chunk = (int32_t)sizeof(repeated);
        if (bmx_esp32_spi_transfer(controller, repeated, output + transferred, chunk) < 0)
            return BMX_SPI_ERROR_GENERIC;
        transferred += chunk;
    }
    return length;
}

int32_t bmx_embedded_spi_write16_read16_blocking(int32_t controller, uint16_t *source,
        uint16_t *destination, int32_t length) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    if (!state || state->data_bits != 16 || length < 0 ||
            (length && (!source || !destination)) || length > INT32_MAX / 2)
        return BMX_SPI_ERROR_INVALID_ARGUMENT;
    int32_t result = bmx_esp32_spi_transfer(controller, source, destination, length * 2);
    return result < 0 ? result : length;
}

int32_t bmx_embedded_spi_write16_blocking(int32_t controller, uint16_t *source,
        int32_t length) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    if (!state || state->data_bits != 16 || length < 0 || (length && !source) ||
            length > INT32_MAX / 2) return BMX_SPI_ERROR_INVALID_ARGUMENT;
    int32_t result = bmx_esp32_spi_transfer(controller, source, NULL, length * 2);
    return result < 0 ? result : length;
}

int32_t bmx_embedded_spi_read16_blocking(int32_t controller, uint32_t repeated_data,
        uint16_t *destination, int32_t length) {
    BMXESP32SPIState *state = bmx_esp32_spi_state(controller);
    if (!state || state->data_bits != 16 || repeated_data > 0xffffu || length < 0 ||
            (length && !destination)) return BMX_SPI_ERROR_INVALID_ARGUMENT;
    uint16_t repeated[32];
    for (size_t index = 0; index < sizeof(repeated) / sizeof(repeated[0]); ++index)
        repeated[index] = (uint16_t)repeated_data;
    int32_t transferred = 0;
    while (transferred < length) {
        int32_t chunk = length - transferred;
        if (chunk > 32) chunk = 32;
        int32_t result = bmx_esp32_spi_transfer(controller, repeated,
            destination + transferred, chunk * 2);
        if (result < 0) return result;
        transferred += chunk;
    }
    return length;
}
