// Copyright (c) 2026 Bruce A Henderson and contributors
// SPDX-License-Identifier: Zlib

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/rmt_common.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_err.h"
#include "soc/soc_caps.h"

#define BMX_RMT_MAX_SYMBOLS 16384u

typedef struct BMXRMTTransmitter {
    rmt_channel_handle_t channel;
    rmt_encoder_handle_t copy_encoder;
} BMXRMTTransmitter;

typedef struct BMXRMTReceiver {
    rmt_channel_handle_t channel;
    rmt_symbol_word_t *symbols;
    uint32_t capacity;
    atomic_uint count;
    atomic_bool ready;
    bool receiving;
} BMXRMTReceiver;

static int32_t bmx_rmt_last_create_error = ESP_OK;

int32_t bmx_esp32_rmt_last_create_error(void) {
    return bmx_rmt_last_create_error;
}

int32_t bmx_esp32_rmt_supported(void) {
    return SOC_RMT_SUPPORTED;
}

static bool bmx_rmt_valid_symbol(uint32_t word) {
    return (word & 0x7fffu) != 0 && ((word >> 16) & 0x7fffu) != 0;
}

static rmt_symbol_word_t bmx_rmt_symbol(uint32_t word) {
    rmt_symbol_word_t symbol = {.val = word};
    return symbol;
}

static esp_err_t bmx_rmt_set_carrier(rmt_channel_handle_t channel,
        uint32_t frequency_hz, uint32_t duty_per_mille, int32_t active_low) {
    if (!channel) return ESP_ERR_INVALID_ARG;
    if (!frequency_hz) return rmt_apply_carrier(channel, NULL);
    if (duty_per_mille == 0 || duty_per_mille >= 1000) return ESP_ERR_INVALID_ARG;
    rmt_carrier_config_t config = {
        .frequency_hz = frequency_hz,
        .duty_cycle = duty_per_mille / 10.0f,
        .flags.polarity_active_low = active_low != 0,
    };
    return rmt_apply_carrier(channel, &config);
}

void *bmx_esp32_rmt_new_tx(uint32_t pin, uint32_t resolution_hz,
        int32_t invert_output) {
    bmx_rmt_last_create_error = ESP_ERR_INVALID_ARG;
    if (pin > INT32_MAX || !GPIO_IS_VALID_OUTPUT_GPIO((int32_t)pin) ||
            !resolution_hz) return NULL;
    BMXRMTTransmitter *state = calloc(1, sizeof(*state));
    if (!state) {
        bmx_rmt_last_create_error = ESP_ERR_NO_MEM;
        return NULL;
    }
    rmt_tx_channel_config_t config = {
        .gpio_num = (gpio_num_t)pin,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = resolution_hz,
        .mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL,
        .trans_queue_depth = 1,
        .flags.invert_out = invert_output != 0,
    };
    esp_err_t result = rmt_new_tx_channel(&config, &state->channel);
    if (result == ESP_OK) {
        rmt_copy_encoder_config_t encoder_config = {};
        result = rmt_new_copy_encoder(&encoder_config, &state->copy_encoder);
    }
    if (result == ESP_OK) result = rmt_enable(state->channel);
    if (result != ESP_OK) {
        if (state->copy_encoder) rmt_del_encoder(state->copy_encoder);
        if (state->channel) rmt_del_channel(state->channel);
        free(state);
        bmx_rmt_last_create_error = result;
        return NULL;
    }
    bmx_rmt_last_create_error = ESP_OK;
    return state;
}

void bmx_esp32_rmt_delete_tx(void *handle) {
    BMXRMTTransmitter *state = handle;
    if (!state) return;
    if (state->channel) {
        (void)rmt_disable(state->channel);
        (void)rmt_del_channel(state->channel);
    }
    if (state->copy_encoder) (void)rmt_del_encoder(state->copy_encoder);
    free(state);
}

int32_t bmx_esp32_rmt_tx_set_carrier(void *handle, uint32_t frequency_hz,
        uint32_t duty_per_mille, int32_t active_low) {
    BMXRMTTransmitter *state = handle;
    return bmx_rmt_set_carrier(state ? state->channel : NULL, frequency_hz,
        duty_per_mille, active_low);
}

int32_t bmx_esp32_rmt_transmit_symbols(void *handle, const void *words,
        uint32_t count, int32_t end_level) {
    BMXRMTTransmitter *state = handle;
    if (!state || !words || !count || count > BMX_RMT_MAX_SYMBOLS ||
            (end_level != 0 && end_level != 1)) return ESP_ERR_INVALID_ARG;
    rmt_symbol_word_t *symbols = malloc((size_t)count * sizeof(*symbols));
    if (!symbols) return ESP_ERR_NO_MEM;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t word;
        memcpy(&word, (const uint8_t *)words + (size_t)i * sizeof(word),
            sizeof(word));
        if (!bmx_rmt_valid_symbol(word)) {
            free(symbols);
            return ESP_ERR_INVALID_ARG;
        }
        symbols[i] = bmx_rmt_symbol(word);
    }
    rmt_transmit_config_t config = {.flags.eot_level = (unsigned)end_level};
    esp_err_t result = rmt_transmit(state->channel, state->copy_encoder,
        symbols, (size_t)count * sizeof(*symbols), &config);
    if (result == ESP_OK) result = rmt_tx_wait_all_done(state->channel, -1);
    free(symbols);
    return result;
}

int32_t bmx_esp32_rmt_transmit_bytes(void *handle, const void *bytes,
        uint32_t count, uint32_t zero_symbol, uint32_t one_symbol,
        int32_t msb_first, int32_t end_level) {
    BMXRMTTransmitter *state = handle;
    if (!state || !bytes || !count || count > BMX_RMT_MAX_SYMBOLS ||
            !bmx_rmt_valid_symbol(zero_symbol) ||
            !bmx_rmt_valid_symbol(one_symbol) ||
            (end_level != 0 && end_level != 1)) return ESP_ERR_INVALID_ARG;
    uint8_t *payload = malloc(count);
    if (!payload) return ESP_ERR_NO_MEM;
    memcpy(payload, bytes, count);
    rmt_bytes_encoder_config_t encoder_config = {
        .bit0 = bmx_rmt_symbol(zero_symbol),
        .bit1 = bmx_rmt_symbol(one_symbol),
        .flags.msb_first = msb_first != 0,
    };
    rmt_encoder_handle_t encoder = NULL;
    esp_err_t result = rmt_new_bytes_encoder(&encoder_config, &encoder);
    if (result == ESP_OK) {
        rmt_transmit_config_t config = {.flags.eot_level = (unsigned)end_level};
        result = rmt_transmit(state->channel, encoder, payload, count, &config);
        if (result == ESP_OK) result = rmt_tx_wait_all_done(state->channel, -1);
    }
    if (encoder) (void)rmt_del_encoder(encoder);
    free(payload);
    return result;
}

static bool bmx_rmt_receive_done(rmt_channel_handle_t channel,
        const rmt_rx_done_event_data_t *event, void *context) {
    (void)channel;
    BMXRMTReceiver *state = context;
    unsigned count = (unsigned)event->num_symbols;
    if (count > state->capacity) count = state->capacity;
    atomic_store_explicit(&state->count, count,
        memory_order_relaxed);
    atomic_store_explicit(&state->ready, true, memory_order_release);
    return false;
}

void *bmx_esp32_rmt_new_rx(uint32_t pin, uint32_t resolution_hz,
        uint32_t capacity, int32_t invert_input) {
    bmx_rmt_last_create_error = ESP_ERR_INVALID_ARG;
    if (pin > INT32_MAX || !GPIO_IS_VALID_GPIO((int32_t)pin) ||
            !resolution_hz || !capacity || capacity > BMX_RMT_MAX_SYMBOLS)
        return NULL;
    BMXRMTReceiver *state = calloc(1, sizeof(*state));
    if (!state) {
        bmx_rmt_last_create_error = ESP_ERR_NO_MEM;
        return NULL;
    }
    state->symbols = calloc(capacity, sizeof(*state->symbols));
    if (!state->symbols) {
        free(state);
        bmx_rmt_last_create_error = ESP_ERR_NO_MEM;
        return NULL;
    }
    state->capacity = capacity;
    atomic_init(&state->count, 0);
    atomic_init(&state->ready, false);
    rmt_rx_channel_config_t config = {
        .gpio_num = (gpio_num_t)pin,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = resolution_hz,
        .mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL,
        .flags.invert_in = invert_input != 0,
    };
    esp_err_t result = rmt_new_rx_channel(&config, &state->channel);
    if (result == ESP_OK) {
        rmt_rx_event_callbacks_t callbacks = {.on_recv_done = bmx_rmt_receive_done};
        result = rmt_rx_register_event_callbacks(state->channel, &callbacks, state);
    }
    if (result == ESP_OK) result = rmt_enable(state->channel);
    if (result != ESP_OK) {
        if (state->channel) (void)rmt_del_channel(state->channel);
        free(state->symbols);
        free(state);
        bmx_rmt_last_create_error = result;
        return NULL;
    }
    bmx_rmt_last_create_error = ESP_OK;
    return state;
}

void bmx_esp32_rmt_delete_rx(void *handle) {
    BMXRMTReceiver *state = handle;
    if (!state) return;
    if (state->channel) {
        (void)rmt_disable(state->channel);
        (void)rmt_del_channel(state->channel);
    }
    free(state->symbols);
    free(state);
}

int32_t bmx_esp32_rmt_rx_set_carrier(void *handle, uint32_t frequency_hz,
        uint32_t duty_per_mille, int32_t active_low) {
    BMXRMTReceiver *state = handle;
    return bmx_rmt_set_carrier(state ? state->channel : NULL, frequency_hz,
        duty_per_mille, active_low);
}

int32_t bmx_esp32_rmt_start_receive(void *handle, uint32_t minimum_ns,
        uint32_t maximum_ns) {
    BMXRMTReceiver *state = handle;
    if (!state || state->receiving || !minimum_ns || maximum_ns <= minimum_ns)
        return ESP_ERR_INVALID_ARG;
    rmt_receive_config_t config = {
        .signal_range_min_ns = minimum_ns,
        .signal_range_max_ns = maximum_ns,
    };
    atomic_store_explicit(&state->count, 0, memory_order_relaxed);
    atomic_store_explicit(&state->ready, false, memory_order_release);
    state->receiving = true;
    esp_err_t result = rmt_receive(state->channel, state->symbols,
        (size_t)state->capacity * sizeof(*state->symbols), &config);
    if (result != ESP_OK) state->receiving = false;
    return result;
}

int32_t bmx_esp32_rmt_receive_ready(void *handle) {
    BMXRMTReceiver *state = handle;
    return state && atomic_load_explicit(&state->ready, memory_order_acquire);
}

uint32_t bmx_esp32_rmt_received_count(void *handle) {
    BMXRMTReceiver *state = handle;
    if (!state || !atomic_load_explicit(&state->ready, memory_order_acquire))
        return 0;
    return atomic_load_explicit(&state->count, memory_order_relaxed);
}

int32_t bmx_esp32_rmt_read_received(void *handle, void *words,
        uint32_t capacity) {
    BMXRMTReceiver *state = handle;
    if (!state || !atomic_load_explicit(&state->ready, memory_order_acquire))
        return ESP_ERR_INVALID_STATE;
    uint32_t count = atomic_load_explicit(&state->count, memory_order_relaxed);
    if (count > state->capacity || count > capacity || (count && !words))
        return ESP_ERR_INVALID_SIZE;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t word = state->symbols[i].val;
        memcpy((uint8_t *)words + (size_t)i * sizeof(word), &word,
            sizeof(word));
    }
    state->receiving = false;
    atomic_store_explicit(&state->ready, false, memory_order_release);
    return ESP_OK;
}
