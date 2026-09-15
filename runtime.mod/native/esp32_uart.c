#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "blitzmax/embedded_events.h"
#include "blitzmax/embedded_uart.h"
#include "blitzmax/esp32_uart.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "soc/soc_caps.h"

#define BMX_ESP32_UART_ERROR_FRAMING 1u
#define BMX_ESP32_UART_ERROR_PARITY  2u
#define BMX_ESP32_UART_ERROR_BREAK   4u
#define BMX_ESP32_UART_ERROR_OVERRUN 8u
#define BMX_ESP32_UART_INVALID_ARGUMENT (-5)
#define BMX_ESP32_UART_BUFFER_SIZE 256
#define BMX_ESP32_UART_EVENT_CAPACITY 16

typedef struct BMXESP32UARTState {
    QueueHandle_t event_queue;
    uint32_t errors;
    int32_t tx_pin;
    uint8_t initialized;
    uint8_t translate_crlf;
    uint8_t pins_configured;
    uint8_t break_asserted;
    uint8_t *async_rx_buffer;
    uint8_t *async_tx_buffer;
    uint32_t async_rx_capacity;
    uint32_t async_tx_capacity;
    uint32_t async_rx_put;
    uint32_t async_rx_get;
    uint32_t async_tx_put;
    uint32_t async_tx_get;
    uint32_t async_rx_dropped;
    uint32_t async_rx_token;
    uint32_t async_tx_token;
    uint32_t async_error_token;
    TaskHandle_t async_task;
    uint8_t async_open;
    uint8_t async_tx_active;
} BMXESP32UARTState;

static BMXESP32UARTState bmx_esp32_uart_states[SOC_UART_NUM];
static portMUX_TYPE bmx_esp32_uart_async_lock = portMUX_INITIALIZER_UNLOCKED;

static int32_t bmx_esp32_uart_valid(int32_t controller) {
    return controller >= 0 && controller < SOC_UART_NUM;
}

static BMXESP32UARTState *bmx_esp32_uart_state(int32_t controller) {
    if (!bmx_esp32_uart_valid(controller)) return NULL;
    return &bmx_esp32_uart_states[controller];
}

static BMXESP32UARTState *bmx_esp32_uart_initialized(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_state(controller);
    return state && state->initialized ? state : NULL;
}

static int32_t bmx_esp32_uart_buffer_valid(BMXESP32UARTState *state,
        const void *buffer, int32_t length) {
    return state && length >= 0 && (length == 0 || buffer);
}

static void bmx_esp32_uart_collect_events(BMXESP32UARTState *state) {
    if (!state || !state->event_queue || state->async_open) return;
    uart_event_t event;
    while (xQueueReceive(state->event_queue, &event, 0) == pdTRUE) {
        switch (event.type) {
            case UART_FRAME_ERR:
                state->errors |= BMX_ESP32_UART_ERROR_FRAMING;
                break;
            case UART_PARITY_ERR:
                state->errors |= BMX_ESP32_UART_ERROR_PARITY;
                break;
            case UART_BREAK:
                state->errors |= BMX_ESP32_UART_ERROR_BREAK;
                break;
            case UART_FIFO_OVF:
            case UART_BUFFER_FULL:
                state->errors |= BMX_ESP32_UART_ERROR_OVERRUN;
                break;
            default:
                break;
        }
    }
}

static int32_t bmx_esp32_uart_write_raw(int32_t controller,
        const uint8_t *source, int32_t length) {
    if (!length) return 0;
    if (bmx_esp32_uart_states[controller].break_asserted) return -1;
    int written = uart_write_bytes((uart_port_t)controller, source, (size_t)length);
    return written == length ? length : -1;
}

int32_t bmx_esp32_uart_controller_count(void) {
    return SOC_UART_NUM;
}

int32_t bmx_esp32_uart_default_controller(void) {
#if defined(CONFIG_ESP_CONSOLE_UART) && CONFIG_ESP_CONSOLE_UART
    return CONFIG_ESP_CONSOLE_UART_NUM;
#else
    return -1;
#endif
}

uint32_t bmx_esp32_uart_default_baudrate(void) {
#if defined(CONFIG_ESP_CONSOLE_UART) && CONFIG_ESP_CONSOLE_UART
    return CONFIG_ESP_CONSOLE_UART_BAUDRATE;
#else
    return 0;
#endif
}

int32_t bmx_esp32_uart_supports_flexible_pin_mappings(void) {
    return 1;
}

int32_t bmx_esp32_uart_configure_pins(int32_t controller, uint32_t tx_pin,
        uint32_t rx_pin) {
    BMXESP32UARTState *state = bmx_esp32_uart_state(controller);
    if (!state || state->break_asserted || tx_pin > INT32_MAX ||
            rx_pin > INT32_MAX || !GPIO_IS_VALID_OUTPUT_GPIO((int32_t)tx_pin) ||
            !GPIO_IS_VALID_GPIO((int32_t)rx_pin)) return 0;
    if (uart_set_pin((uart_port_t)controller, (int32_t)tx_pin,
            (int32_t)rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
        return 0;
    state->tx_pin = (int32_t)tx_pin;
    state->pins_configured = 1;
    return 1;
}

int32_t bmx_esp32_uart_configure_flow_pins(int32_t controller,
        uint32_t cts_pin, uint32_t rts_pin) {
    if (!bmx_esp32_uart_valid(controller) || cts_pin > INT32_MAX ||
            rts_pin > INT32_MAX || !GPIO_IS_VALID_GPIO((int32_t)cts_pin) ||
            !GPIO_IS_VALID_OUTPUT_GPIO((int32_t)rts_pin)) return 0;
    return uart_set_pin((uart_port_t)controller, UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE, (int32_t)rts_pin, (int32_t)cts_pin) == ESP_OK;
}

uint32_t bmx_esp32_uart_init(int32_t controller, uint32_t baudrate) {
    BMXESP32UARTState *state = bmx_esp32_uart_state(controller);
    if (!state || !baudrate || state->initialized ||
            uart_is_driver_installed((uart_port_t)controller)) return 0;
    const uart_config_t config = {
        .baud_rate = (int)baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = { .allow_pd = 0, .backup_before_sleep = 0 }
    };
    if (uart_param_config((uart_port_t)controller, &config) != ESP_OK) return 0;
    QueueHandle_t queue = NULL;
    if (uart_driver_install((uart_port_t)controller, BMX_ESP32_UART_BUFFER_SIZE,
            BMX_ESP32_UART_BUFFER_SIZE, BMX_ESP32_UART_EVENT_CAPACITY,
            &queue, 0) != ESP_OK) return 0;
    state->event_queue = queue;
    state->errors = 0;
    state->translate_crlf = 0;
    state->break_asserted = 0;
    state->initialized = 1;
    return bmx_esp32_uart_get_baudrate(controller);
}

int32_t bmx_esp32_uart_deinit(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!state || state->async_open) return 0;
    if (state->break_asserted && !bmx_esp32_uart_set_break(controller, 0)) return 0;
    (void)uart_wait_tx_idle_polling((uart_port_t)controller);
    if (uart_driver_delete((uart_port_t)controller) != ESP_OK) return 0;
    state->event_queue = NULL;
    state->errors = 0;
    state->translate_crlf = 0;
    state->break_asserted = 0;
    state->initialized = 0;
    return 1;
}

uint32_t bmx_esp32_uart_set_baudrate(int32_t controller, uint32_t baudrate) {
    if (!bmx_esp32_uart_initialized(controller) || !baudrate ||
            uart_set_baudrate((uart_port_t)controller, baudrate) != ESP_OK) return 0;
    return bmx_esp32_uart_get_baudrate(controller);
}

uint32_t bmx_esp32_uart_get_baudrate(int32_t controller) {
    if (!bmx_esp32_uart_initialized(controller)) return 0;
    uint32_t baudrate = 0;
    return uart_get_baudrate((uart_port_t)controller, &baudrate) == ESP_OK
        ? baudrate : 0;
}

int32_t bmx_esp32_uart_set_format(int32_t controller, uint32_t data_bits,
        uint32_t stop_bits, uint32_t parity) {
    if (!bmx_esp32_uart_initialized(controller) || data_bits < 5u ||
            data_bits > 8u || (stop_bits != 1u && stop_bits != 2u) ||
            parity > 2u) return 0;
    uart_word_length_t word_length = (uart_word_length_t)(data_bits - 5u);
    uart_stop_bits_t stops = stop_bits == 1u ? UART_STOP_BITS_1 : UART_STOP_BITS_2;
    uart_parity_t parity_mode = UART_PARITY_DISABLE;
    if (parity == 1u) parity_mode = UART_PARITY_EVEN;
    if (parity == 2u) parity_mode = UART_PARITY_ODD;
    return uart_set_word_length((uart_port_t)controller, word_length) == ESP_OK &&
        uart_set_stop_bits((uart_port_t)controller, stops) == ESP_OK &&
        uart_set_parity((uart_port_t)controller, parity_mode) == ESP_OK;
}

int32_t bmx_esp32_uart_set_flow_control(int32_t controller, int32_t cts,
        int32_t rts) {
    if (!bmx_esp32_uart_initialized(controller)) return 0;
    uart_hw_flowcontrol_t mode = UART_HW_FLOWCTRL_DISABLE;
    if (cts && rts) mode = UART_HW_FLOWCTRL_CTS_RTS;
    else if (cts) mode = UART_HW_FLOWCTRL_CTS;
    else if (rts) mode = UART_HW_FLOWCTRL_RTS;
    uint8_t threshold = UART_HW_FIFO_LEN(controller) > 16
        ? (uint8_t)(UART_HW_FIFO_LEN(controller) - 16) : 1u;
    return uart_set_hw_flow_ctrl((uart_port_t)controller, mode, threshold) == ESP_OK;
}

int32_t bmx_esp32_uart_set_fifo_enabled(int32_t controller, int32_t enabled) {
    return bmx_esp32_uart_initialized(controller) && enabled != 0;
}

int32_t bmx_esp32_uart_is_enabled(int32_t controller) {
    return bmx_esp32_uart_initialized(controller) != NULL;
}

int32_t bmx_esp32_uart_is_writable(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!state || state->break_asserted) return 0;
    size_t available = 0;
    return uart_get_tx_buffer_free_size((uart_port_t)controller, &available) == ESP_OK &&
        available != 0;
}

int32_t bmx_esp32_uart_is_readable(int32_t controller) {
    if (!bmx_esp32_uart_initialized(controller)) return 0;
    size_t available = 0;
    return uart_get_buffered_data_len((uart_port_t)controller, &available) == ESP_OK &&
        available != 0;
}

int32_t bmx_esp32_uart_is_readable_within_us(int32_t controller,
        uint32_t timeout_us) {
    if (!bmx_esp32_uart_initialized(controller)) return 0;
    int64_t deadline = esp_timer_get_time() + timeout_us;
    do {
        if (bmx_esp32_uart_is_readable(controller)) return 1;
        if (esp_timer_get_time() >= deadline) break;
        taskYIELD();
    } while (1);
    return 0;
}

int32_t bmx_esp32_uart_write_blocking(int32_t controller, const void *source,
        int32_t length) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!bmx_esp32_uart_buffer_valid(state, source, length))
        return BMX_ESP32_UART_INVALID_ARGUMENT;
    if (!length) return 0;
    const uint8_t *bytes = (const uint8_t *)source;
    if (!state->translate_crlf) return bmx_esp32_uart_write_raw(controller, bytes, length);
    int32_t start = 0;
    for (int32_t index = 0; index < length; ++index) {
        if (bytes[index] != '\n') continue;
        if (index > start && bmx_esp32_uart_write_raw(controller,
                bytes + start, index - start) < 0) return -1;
        static const uint8_t crlf[2] = { '\r', '\n' };
        if (bmx_esp32_uart_write_raw(controller, crlf, 2) < 0) return -1;
        start = index + 1;
    }
    if (start < length && bmx_esp32_uart_write_raw(controller,
            bytes + start, length - start) < 0) return -1;
    return length;
}

int32_t bmx_esp32_uart_read_blocking(int32_t controller, void *destination,
        int32_t length) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!bmx_esp32_uart_buffer_valid(state, destination, length))
        return BMX_ESP32_UART_INVALID_ARGUMENT;
    if (!length) return 0;
    int32_t count = 0;
    while (count < length) {
        int result = uart_read_bytes((uart_port_t)controller,
            (uint8_t *)destination + count, (uint32_t)(length - count), portMAX_DELAY);
        if (result < 0) return -1;
        count += result;
    }
    bmx_esp32_uart_collect_events(state);
    return count;
}

int32_t bmx_esp32_uart_read_timeout_us(int32_t controller, void *destination,
        int32_t length, uint32_t timeout_us) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!bmx_esp32_uart_buffer_valid(state, destination, length))
        return BMX_ESP32_UART_INVALID_ARGUMENT;
    int64_t deadline = esp_timer_get_time() + timeout_us;
    int32_t count = 0;
    while (count < length) {
        int result = uart_read_bytes((uart_port_t)controller,
            (uint8_t *)destination + count, (uint32_t)(length - count), 0);
        if (result < 0) return -1;
        count += result;
        if (count == length || esp_timer_get_time() >= deadline) break;
        taskYIELD();
    }
    bmx_esp32_uart_collect_events(state);
    return count;
}

int32_t bmx_esp32_uart_read_available(int32_t controller, void *destination,
        int32_t capacity) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!bmx_esp32_uart_buffer_valid(state, destination, capacity))
        return BMX_ESP32_UART_INVALID_ARGUMENT;
    if (!capacity) return 0;
    int result = uart_read_bytes((uart_port_t)controller, destination,
        (uint32_t)capacity, 0);
    bmx_esp32_uart_collect_events(state);
    return result;
}

int32_t bmx_esp32_uart_put_byte(int32_t controller, uint32_t value) {
    if (value > 0xffu) return 0;
    uint8_t byte = (uint8_t)value;
    return bmx_esp32_uart_write_blocking(controller, &byte, 1) == 1;
}

void bmx_esp32_uart_tx_wait_blocking(int32_t controller) {
    if (bmx_esp32_uart_initialized(controller))
        (void)uart_wait_tx_idle_polling((uart_port_t)controller);
}

int32_t bmx_esp32_uart_write_with_break(int32_t controller, const void *source,
        int32_t length, uint32_t break_bits) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!bmx_esp32_uart_buffer_valid(state, source, length) || length == 0 ||
            break_bits == 0 || break_bits > 255u || state->break_asserted)
        return BMX_ESP32_UART_INVALID_ARGUMENT;
    int written = uart_write_bytes_with_break((uart_port_t)controller, source,
        (size_t)length, (int)break_bits);
    return written == length ? length : -1;
}

int32_t bmx_esp32_uart_set_break(int32_t controller, int32_t enabled) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!state || !state->pins_configured) return 0;
    if (enabled) {
        if (state->break_asserted) return 1;
        if (uart_wait_tx_idle_polling((uart_port_t)controller) != ESP_OK ||
                gpio_reset_pin((gpio_num_t)state->tx_pin) != ESP_OK) return 0;
        if (gpio_set_level((gpio_num_t)state->tx_pin, 0) != ESP_OK ||
                gpio_set_direction((gpio_num_t)state->tx_pin,
                    GPIO_MODE_OUTPUT) != ESP_OK) {
            (void)uart_set_pin((uart_port_t)controller, state->tx_pin,
                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
            return 0;
        }
        state->break_asserted = 1;
        return 1;
    }
    if (!state->break_asserted) return 1;
    if (uart_set_pin((uart_port_t)controller, state->tx_pin,
            UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE) != ESP_OK) return 0;
    state->break_asserted = 0;
    return 1;
}

int32_t bmx_esp32_uart_is_break_asserted(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    return state && state->break_asserted;
}

int32_t bmx_esp32_uart_set_translate_crlf(int32_t controller, int32_t enabled) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!state) return 0;
    state->translate_crlf = enabled != 0;
    return 1;
}

uint32_t bmx_esp32_uart_get_errors(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!state) return 0;
    bmx_esp32_uart_collect_events(state);
    return state->errors;
}

void bmx_esp32_uart_clear_errors(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!state) return;
    bmx_esp32_uart_collect_events(state);
    state->errors = 0;
}

int32_t bmx_esp32_uart_flush_input(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!state || uart_flush_input((uart_port_t)controller) != ESP_OK) return 0;
    bmx_esp32_uart_collect_events(state);
    return 1;
}

int32_t bmx_esp32_uart_set_loopback(int32_t controller, int32_t enabled) {
    if (!bmx_esp32_uart_initialized(controller)) return 0;
    return uart_set_loop_back((uart_port_t)controller, enabled != 0) == ESP_OK;
}

int32_t bmx_embedded_uart_controller_count(void) {
    return bmx_esp32_uart_controller_count();
}

int32_t bmx_embedded_uart_default_controller(void) {
    return bmx_esp32_uart_default_controller();
}

uint32_t bmx_embedded_uart_default_baudrate(void) {
    return bmx_esp32_uart_default_baudrate();
}

int32_t bmx_embedded_uart_configure_pins(int32_t controller, uint32_t tx_pin,
        uint32_t rx_pin) {
    return bmx_esp32_uart_configure_pins(controller, tx_pin, rx_pin);
}

int32_t bmx_embedded_uart_configure_flow_pins(int32_t controller,
        uint32_t cts_pin, uint32_t rts_pin) {
    return bmx_esp32_uart_configure_flow_pins(controller, cts_pin, rts_pin);
}

uint32_t bmx_embedded_uart_init(int32_t controller, uint32_t baudrate) {
    return bmx_esp32_uart_init(controller, baudrate);
}

int32_t bmx_embedded_uart_deinit(int32_t controller) {
    return bmx_esp32_uart_deinit(controller);
}

uint32_t bmx_embedded_uart_set_baudrate(int32_t controller, uint32_t baudrate) {
    return bmx_esp32_uart_set_baudrate(controller, baudrate);
}

int32_t bmx_embedded_uart_set_format(int32_t controller, uint32_t data_bits,
        uint32_t stop_bits, uint32_t parity) {
    return bmx_esp32_uart_set_format(controller, data_bits, stop_bits, parity);
}

int32_t bmx_embedded_uart_set_flow_control(int32_t controller, int32_t cts,
        int32_t rts) {
    return bmx_esp32_uart_set_flow_control(controller, cts, rts);
}

int32_t bmx_embedded_uart_is_enabled(int32_t controller) {
    return bmx_esp32_uart_is_enabled(controller);
}

int32_t bmx_embedded_uart_is_writable(int32_t controller) {
    return bmx_esp32_uart_is_writable(controller);
}

int32_t bmx_embedded_uart_is_readable(int32_t controller) {
    return bmx_esp32_uart_is_readable(controller);
}

int32_t bmx_embedded_uart_is_readable_within_us(int32_t controller,
        uint32_t timeout_us) {
    return bmx_esp32_uart_is_readable_within_us(controller, timeout_us);
}

int32_t bmx_embedded_uart_write_blocking(int32_t controller,
        const void *source, int32_t length) {
    return bmx_esp32_uart_write_blocking(controller, source, length);
}

int32_t bmx_embedded_uart_read_blocking(int32_t controller,
        void *destination, int32_t length) {
    return bmx_esp32_uart_read_blocking(controller, destination, length);
}

int32_t bmx_embedded_uart_read_timeout_us(int32_t controller,
        void *destination, int32_t length, uint32_t timeout_us) {
    return bmx_esp32_uart_read_timeout_us(controller, destination, length,
        timeout_us);
}

int32_t bmx_embedded_uart_read_available(int32_t controller,
        void *destination, int32_t capacity) {
    return bmx_esp32_uart_read_available(controller, destination, capacity);
}

int32_t bmx_embedded_uart_put_byte(int32_t controller, uint32_t value) {
    return bmx_esp32_uart_put_byte(controller, value);
}

int32_t bmx_embedded_uart_tx_wait_blocking(int32_t controller) {
    if (!bmx_esp32_uart_is_enabled(controller)) return 0;
    bmx_esp32_uart_tx_wait_blocking(controller);
    return 1;
}

int32_t bmx_embedded_uart_set_break(int32_t controller, int32_t enabled) {
    return bmx_esp32_uart_set_break(controller, enabled);
}

int32_t bmx_embedded_uart_set_translate_crlf(int32_t controller,
        int32_t enabled) {
    return bmx_esp32_uart_set_translate_crlf(controller, enabled);
}

uint32_t bmx_embedded_uart_get_errors(int32_t controller) {
    return bmx_esp32_uart_get_errors(controller);
}

int32_t bmx_embedded_uart_clear_errors(int32_t controller) {
    if (!bmx_esp32_uart_is_enabled(controller)) return 0;
    bmx_esp32_uart_clear_errors(controller);
    return 1;
}

static uint32_t bmx_esp32_uart_event_error(uart_event_type_t event_type) {
    if (event_type == UART_FRAME_ERR) return BMX_ESP32_UART_ERROR_FRAMING;
    if (event_type == UART_PARITY_ERR) return BMX_ESP32_UART_ERROR_PARITY;
    if (event_type == UART_BREAK) return BMX_ESP32_UART_ERROR_BREAK;
    if (event_type == UART_FIFO_OVF || event_type == UART_BUFFER_FULL)
        return BMX_ESP32_UART_ERROR_OVERRUN;
    return 0;
}

static void bmx_esp32_uart_async_receive(int32_t controller,
        BMXESP32UARTState *state) {
    uint8_t input[64];
    int result;
    do {
        result = uart_read_bytes((uart_port_t)controller, input,
            sizeof(input), 0);
        if (result <= 0) break;
        portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
        int was_empty = state->async_rx_put == state->async_rx_get;
        for (int index = 0; index < result; ++index) {
            uint32_t put = state->async_rx_put;
            if (put - state->async_rx_get >= state->async_rx_capacity) {
                if (state->async_rx_dropped != UINT32_MAX)
                    ++state->async_rx_dropped;
            } else {
                state->async_rx_buffer[put & (state->async_rx_capacity - 1u)] =
                    input[index];
                state->async_rx_put = put + 1u;
            }
        }
        uint32_t available = state->async_rx_put - state->async_rx_get;
        uint32_t token = state->async_rx_token;
        portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
        if (was_empty && available)
            (void)bmx_embedded_event_post(token, available,
                (uint32_t)controller);
    } while (result == (int)sizeof(input));
}

static void bmx_esp32_uart_async_transmit(int32_t controller,
        BMXESP32UARTState *state) {
    uint8_t output[64];
    int count = 0;
    int became_empty = 0;
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    while (count < (int)sizeof(output) &&
            state->async_tx_get != state->async_tx_put) {
        output[count++] = state->async_tx_buffer[state->async_tx_get &
            (state->async_tx_capacity - 1u)];
        ++state->async_tx_get;
    }
    if (count && state->async_tx_get == state->async_tx_put) became_empty = 1;
    uint32_t token = state->async_tx_token;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    if (count) {
        if (uart_write_bytes((uart_port_t)controller, output, count) != count) {
            portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
            state->errors |= BMX_ESP32_UART_ERROR_OVERRUN;
            uint32_t error_token = state->async_error_token;
            portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
            (void)bmx_embedded_event_post(error_token,
                BMX_ESP32_UART_ERROR_OVERRUN, (uint32_t)controller);
        } else if (became_empty) {
            (void)bmx_embedded_event_post(token, 0, (uint32_t)controller);
        }
    }
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    int tx_active = state->async_tx_active;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    if (tx_active && uart_wait_tx_done((uart_port_t)controller, 0) == ESP_OK) {
        portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
        state->async_tx_active = 0;
        token = state->async_tx_token;
        portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
        (void)bmx_embedded_event_post(token, 0, (uint32_t)controller);
    }
}

static void bmx_esp32_uart_async_worker(void *context) {
    int32_t controller = (int32_t)(intptr_t)context;
    BMXESP32UARTState *state = bmx_esp32_uart_state(controller);
    for (;;) {
        portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
        int open = state && state->async_open;
        portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
        if (!open) break;
        uart_event_t event;
        if (xQueueReceive(state->event_queue, &event, 1) == pdTRUE) {
            if (event.type == UART_DATA) {
                bmx_esp32_uart_async_receive(controller, state);
            } else {
                uint32_t errors = bmx_esp32_uart_event_error(event.type);
                if (errors) {
                    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
                    state->errors |= errors;
                    uint32_t token = state->async_error_token;
                    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
                    (void)bmx_embedded_event_post(token, errors,
                        (uint32_t)controller);
                }
            }
        }
        bmx_esp32_uart_async_transmit(controller, state);
    }
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    state->async_task = NULL;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    vTaskDelete(NULL);
}

int32_t bmx_embedded_uart_async_open(int32_t controller,
        uint32_t rx_capacity, uint32_t tx_capacity, uint32_t rx_token,
        uint32_t tx_token, uint32_t error_token) {
    BMXESP32UARTState *state = bmx_esp32_uart_initialized(controller);
    if (!state || state->async_open || rx_capacity < 2u || tx_capacity < 2u ||
            (rx_capacity & (rx_capacity - 1u)) ||
            (tx_capacity & (tx_capacity - 1u)) || !rx_token || !tx_token ||
            !error_token) return 0;
    uint8_t *rx_buffer = heap_caps_malloc(rx_capacity, MALLOC_CAP_8BIT);
    if (!rx_buffer) return 0;
    uint8_t *tx_buffer = heap_caps_malloc(tx_capacity, MALLOC_CAP_8BIT);
    if (!tx_buffer) {
        heap_caps_free(rx_buffer);
        return 0;
    }
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    state->async_rx_buffer = rx_buffer;
    state->async_tx_buffer = tx_buffer;
    state->async_rx_capacity = rx_capacity;
    state->async_tx_capacity = tx_capacity;
    state->async_rx_put = 0;
    state->async_rx_get = 0;
    state->async_tx_put = 0;
    state->async_tx_get = 0;
    state->async_rx_dropped = 0;
    state->async_rx_token = rx_token;
    state->async_tx_token = tx_token;
    state->async_error_token = error_token;
    state->async_tx_active = 0;
    state->async_open = 1;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    if (xTaskCreate(bmx_esp32_uart_async_worker, "bmx-uart", 3072,
            (void *)(intptr_t)controller, tskIDLE_PRIORITY + 2,
            &state->async_task) != pdPASS) {
        portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
        state->async_open = 0;
        state->async_rx_buffer = NULL;
        state->async_tx_buffer = NULL;
        portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
        heap_caps_free(rx_buffer);
        heap_caps_free(tx_buffer);
        return 0;
    }
    return 1;
}

int32_t bmx_embedded_uart_async_close(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_state(controller);
    if (!state) return 0;
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    if (!state->async_open) {
        portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
        return 1;
    }
    state->async_open = 0;
    TaskHandle_t task = state->async_task;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    while (task) {
        vTaskDelay(1);
        portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
        task = state->async_task;
        portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    }
    uint8_t *rx_buffer = state->async_rx_buffer;
    uint8_t *tx_buffer = state->async_tx_buffer;
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    state->async_rx_buffer = NULL;
    state->async_tx_buffer = NULL;
    state->async_rx_capacity = 0;
    state->async_tx_capacity = 0;
    state->async_rx_token = 0;
    state->async_tx_token = 0;
    state->async_error_token = 0;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    heap_caps_free(rx_buffer);
    heap_caps_free(tx_buffer);
    return 1;
}

int32_t bmx_embedded_uart_async_is_open(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_state(controller);
    if (!state) return 0;
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    int open = state->async_open;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    return open;
}

int32_t bmx_embedded_uart_async_read(int32_t controller, void *destination,
        int32_t capacity) {
    BMXESP32UARTState *state = bmx_esp32_uart_state(controller);
    if (!state || capacity < 0 || (capacity && !destination))
        return BMX_ESP32_UART_INVALID_ARGUMENT;
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    if (!state->async_open) {
        portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
        return BMX_ESP32_UART_INVALID_ARGUMENT;
    }
    uint8_t *bytes = destination;
    int32_t count = 0;
    while (count < capacity && state->async_rx_get != state->async_rx_put) {
        bytes[count++] = state->async_rx_buffer[state->async_rx_get &
            (state->async_rx_capacity - 1u)];
        ++state->async_rx_get;
    }
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    return count;
}

int32_t bmx_embedded_uart_async_write(int32_t controller,
        const void *source, int32_t length) {
    BMXESP32UARTState *state = bmx_esp32_uart_state(controller);
    if (!state || length < 0 || (length && !source))
        return BMX_ESP32_UART_INVALID_ARGUMENT;
    const uint8_t *bytes = source;
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    if (!state->async_open) {
        portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
        return BMX_ESP32_UART_INVALID_ARGUMENT;
    }
    int32_t count = 0;
    while (count < length && state->async_tx_put - state->async_tx_get <
            state->async_tx_capacity) {
        state->async_tx_buffer[state->async_tx_put &
            (state->async_tx_capacity - 1u)] = bytes[count++];
        ++state->async_tx_put;
    }
    if (count) state->async_tx_active = 1;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    return count;
}

uint32_t bmx_embedded_uart_async_read_available(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_state(controller);
    if (!state) return 0;
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    uint32_t available = state->async_open
        ? state->async_rx_put - state->async_rx_get : 0;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    return available;
}

uint32_t bmx_embedded_uart_async_write_available(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_state(controller);
    if (!state) return 0;
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    uint32_t available = state->async_open ? state->async_tx_capacity -
        (state->async_tx_put - state->async_tx_get) : 0;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    return available;
}

uint32_t bmx_embedded_uart_async_rx_dropped(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_state(controller);
    if (!state) return 0;
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    uint32_t dropped = state->async_rx_dropped;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    return dropped;
}

int32_t bmx_embedded_uart_async_tx_idle(int32_t controller) {
    BMXESP32UARTState *state = bmx_esp32_uart_state(controller);
    if (!state) return 1;
    portENTER_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    int idle = !state->async_open ||
        (state->async_tx_put == state->async_tx_get &&
            !state->async_tx_active);
    portEXIT_CRITICAL_SAFE(&bmx_esp32_uart_async_lock);
    return idle;
}
