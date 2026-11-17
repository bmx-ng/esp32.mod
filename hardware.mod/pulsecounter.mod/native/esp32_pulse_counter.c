#include <stdint.h>
#include <stdlib.h>

#include "esp_err.h"
#include "soc/soc_caps.h"

#if SOC_PCNT_SUPPORTED
#include "driver/pulse_cnt.h"
#endif

#define BMX_PCNT_MAX_CHANNELS 8u

static int32_t bmx_esp32_pcnt_create_error = ESP_OK;

#if SOC_PCNT_SUPPORTED
typedef struct {
    pcnt_unit_handle_t unit;
    pcnt_channel_handle_t channels[BMX_PCNT_MAX_CHANNELS];
    uint32_t channel_count;
    uint8_t enabled;
    uint8_t running;
} bmx_esp32_pcnt_t;

static int bmx_esp32_pcnt_valid_edge_action(int32_t action) {
    return action >= PCNT_CHANNEL_EDGE_ACTION_HOLD &&
        action <= PCNT_CHANNEL_EDGE_ACTION_DECREASE;
}

static int bmx_esp32_pcnt_valid_level_action(int32_t action) {
    return action >= PCNT_CHANNEL_LEVEL_ACTION_KEEP &&
        action <= PCNT_CHANNEL_LEVEL_ACTION_HOLD;
}
#endif

int32_t bmx_esp32_pcnt_supported(void) {
#if SOC_PCNT_SUPPORTED
    return 1;
#else
    return 0;
#endif
}

int32_t bmx_esp32_pcnt_last_create_error(void) {
    return bmx_esp32_pcnt_create_error;
}

void *bmx_esp32_pcnt_new(int32_t low_limit, int32_t high_limit,
        int32_t accumulate_overflow, int32_t group_id) {
#if SOC_PCNT_SUPPORTED
    bmx_esp32_pcnt_create_error = ESP_OK;
    if (low_limit >= 0 || high_limit <= 0) {
        bmx_esp32_pcnt_create_error = ESP_ERR_INVALID_ARG;
        return NULL;
    }
    bmx_esp32_pcnt_t *counter = calloc(1, sizeof(*counter));
    if (!counter) {
        bmx_esp32_pcnt_create_error = ESP_ERR_NO_MEM;
        return NULL;
    }
    pcnt_unit_config_t config = {
        .group_id = group_id,
        .clk_src = PCNT_CLK_SRC_DEFAULT,
        .low_limit = low_limit,
        .high_limit = high_limit,
        .intr_priority = 0,
        .flags.accum_count = accumulate_overflow != 0,
    };
    esp_err_t result = pcnt_new_unit(&config, &counter->unit);
    if (result != ESP_OK) {
        bmx_esp32_pcnt_create_error = result;
        free(counter);
        return NULL;
    }
    return counter;
#else
    (void)low_limit;
    (void)high_limit;
    (void)accumulate_overflow;
    (void)group_id;
    bmx_esp32_pcnt_create_error = ESP_ERR_NOT_SUPPORTED;
    return NULL;
#endif
}

void bmx_esp32_pcnt_delete(void *handle) {
#if SOC_PCNT_SUPPORTED
    bmx_esp32_pcnt_t *counter = handle;
    if (!counter) return;
    if (counter->running) (void)pcnt_unit_stop(counter->unit);
    if (counter->enabled) (void)pcnt_unit_disable(counter->unit);
    for (uint32_t index = 0; index < counter->channel_count; ++index) {
        if (counter->channels[index])
            (void)pcnt_del_channel(counter->channels[index]);
    }
    (void)pcnt_del_unit(counter->unit);
    free(counter);
#else
    (void)handle;
#endif
}

int32_t bmx_esp32_pcnt_add_channel(void *handle, int32_t edge_pin,
        int32_t level_pin, int32_t positive_edge, int32_t negative_edge,
        int32_t high_level, int32_t low_level, int32_t invert_edge,
        int32_t invert_level) {
#if SOC_PCNT_SUPPORTED
    bmx_esp32_pcnt_t *counter = handle;
    if (!counter ||
            counter->channel_count >= BMX_PCNT_MAX_CHANNELS ||
            !bmx_esp32_pcnt_valid_edge_action(positive_edge) ||
            !bmx_esp32_pcnt_valid_edge_action(negative_edge) ||
            !bmx_esp32_pcnt_valid_level_action(high_level) ||
            !bmx_esp32_pcnt_valid_level_action(low_level))
        return ESP_ERR_INVALID_ARG;
    if (counter->enabled) return ESP_ERR_INVALID_STATE;

    pcnt_chan_config_t config = {
        .edge_gpio_num = edge_pin,
        .level_gpio_num = level_pin,
        .flags.invert_edge_input = invert_edge != 0,
        .flags.invert_level_input = invert_level != 0,
        .flags.virt_edge_io_level = 0,
        .flags.virt_level_io_level = 0,
    };
    pcnt_channel_handle_t channel = NULL;
    esp_err_t result = pcnt_new_channel(counter->unit, &config, &channel);
    if (result != ESP_OK) return result;
    result = pcnt_channel_set_edge_action(channel,
        (pcnt_channel_edge_action_t)positive_edge,
        (pcnt_channel_edge_action_t)negative_edge);
    if (result == ESP_OK) {
        result = pcnt_channel_set_level_action(channel,
            (pcnt_channel_level_action_t)high_level,
            (pcnt_channel_level_action_t)low_level);
    }
    if (result != ESP_OK) {
        (void)pcnt_del_channel(channel);
        return result;
    }
    counter->channels[counter->channel_count++] = channel;
    return ESP_OK;
#else
    (void)handle; (void)edge_pin; (void)level_pin; (void)positive_edge;
    (void)negative_edge; (void)high_level; (void)low_level;
    (void)invert_edge; (void)invert_level;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

int32_t bmx_esp32_pcnt_set_glitch_filter(void *handle,
        uint32_t maximum_nanoseconds) {
#if SOC_PCNT_SUPPORTED
    bmx_esp32_pcnt_t *counter = handle;
    if (!counter) return ESP_ERR_INVALID_ARG;
    if (!maximum_nanoseconds)
        return pcnt_unit_set_glitch_filter(counter->unit, NULL);
    pcnt_glitch_filter_config_t config = {
        .max_glitch_ns = maximum_nanoseconds,
    };
    return pcnt_unit_set_glitch_filter(counter->unit, &config);
#else
    (void)handle; (void)maximum_nanoseconds;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

int32_t bmx_esp32_pcnt_start(void *handle) {
#if SOC_PCNT_SUPPORTED
    bmx_esp32_pcnt_t *counter = handle;
    if (!counter || !counter->channel_count) return ESP_ERR_INVALID_ARG;
    if (counter->running) return ESP_ERR_INVALID_STATE;
    if (!counter->enabled) {
        esp_err_t result = pcnt_unit_enable(counter->unit);
        if (result != ESP_OK) return result;
        counter->enabled = 1;
    }
    esp_err_t result = pcnt_unit_start(counter->unit);
    if (result == ESP_OK) counter->running = 1;
    return result;
#else
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

int32_t bmx_esp32_pcnt_stop(void *handle) {
#if SOC_PCNT_SUPPORTED
    bmx_esp32_pcnt_t *counter = handle;
    if (!counter || !counter->running) return ESP_ERR_INVALID_STATE;
    esp_err_t result = pcnt_unit_stop(counter->unit);
    if (result == ESP_OK) counter->running = 0;
    return result;
#else
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

int32_t bmx_esp32_pcnt_clear(void *handle) {
#if SOC_PCNT_SUPPORTED
    bmx_esp32_pcnt_t *counter = handle;
    if (!counter) return ESP_ERR_INVALID_ARG;
    return pcnt_unit_clear_count(counter->unit);
#else
    (void)handle;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

int32_t bmx_esp32_pcnt_read(void *handle, int32_t *value) {
#if SOC_PCNT_SUPPORTED
    bmx_esp32_pcnt_t *counter = handle;
    if (!counter || !value) return ESP_ERR_INVALID_ARG;
    int native_value = 0;
    esp_err_t result = pcnt_unit_get_count(counter->unit, &native_value);
    if (result == ESP_OK) *value = (int32_t)native_value;
    return result;
#else
    (void)handle; (void)value;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
