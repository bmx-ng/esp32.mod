#include <limits.h>
#include <stdint.h>

#include "blitzmax/embedded_adc.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "hal/adc_types.h"
#include "soc/soc_caps.h"

static adc_oneshot_unit_handle_t bmx_esp32_adc_units[SOC_ADC_PERIPH_NUM];
static uint8_t bmx_esp32_adc_configured[SOC_ADC_PERIPH_NUM][SOC_ADC_MAX_CHANNEL_NUM];
static uint8_t bmx_esp32_adc_attenuation[SOC_ADC_PERIPH_NUM][SOC_ADC_MAX_CHANNEL_NUM];
static adc_cali_handle_t bmx_esp32_adc_calibration[SOC_ADC_PERIPH_NUM][SOC_ADC_MAX_CHANNEL_NUM];
static uint8_t bmx_esp32_adc_calibration_scheme[SOC_ADC_PERIPH_NUM][SOC_ADC_MAX_CHANNEL_NUM];

extern int32_t bmx_esp32_random_internal_entropy_enabled(void);

int32_t bmx_esp32_adc_any_initialized(void) {
    for (uint32_t unit = 0; unit < SOC_ADC_PERIPH_NUM; ++unit) {
        if (bmx_esp32_adc_units[unit]) return 1;
    }
    return 0;
}

static int32_t bmx_esp32_adc_pin_mapping(uint32_t gpio, adc_unit_t *unit,
        adc_channel_t *channel) {
    if (gpio > INT_MAX || adc_oneshot_io_to_channel((int)gpio, unit, channel) != ESP_OK)
        return 0;
    return *unit >= ADC_UNIT_1 && *unit < SOC_ADC_PERIPH_NUM &&
        *channel >= ADC_CHANNEL_0 && *channel < SOC_ADC_MAX_CHANNEL_NUM;
}

int32_t bmx_embedded_adc_is_valid_pin(uint32_t gpio) {
    adc_unit_t unit;
    adc_channel_t channel;
    return bmx_esp32_adc_pin_mapping(gpio, &unit, &channel);
}

static int32_t bmx_esp32_adc_ensure_unit(adc_unit_t unit) {
    if (bmx_esp32_adc_units[unit]) return 1;
    adc_oneshot_unit_init_cfg_t config = {
        .unit_id = unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    return adc_oneshot_new_unit(&config, &bmx_esp32_adc_units[unit]) == ESP_OK;
}

static int32_t bmx_esp32_adc_attenuation_value(int32_t attenuation) {
    switch (attenuation) {
        case 0: return ADC_ATTEN_DB_0;
        case 1: return ADC_ATTEN_DB_2_5;
        case 2: return ADC_ATTEN_DB_6;
        case 3: return ADC_ATTEN_DB_12;
        default: return -1;
    }
}

static int32_t bmx_esp32_adc_configure(adc_unit_t unit, adc_channel_t channel,
        int32_t attenuation) {
    int32_t native_attenuation = bmx_esp32_adc_attenuation_value(attenuation);
    if (native_attenuation < 0 || !bmx_esp32_adc_ensure_unit(unit))
        return 0;
    adc_oneshot_chan_cfg_t config = {
        .atten = (adc_atten_t)native_attenuation,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_oneshot_config_channel(bmx_esp32_adc_units[unit], channel, &config) != ESP_OK)
        return 0;
    bmx_esp32_adc_configured[unit][channel] = 1;
    bmx_esp32_adc_attenuation[unit][channel] = (uint8_t)attenuation;
    return 1;
}

static void bmx_esp32_adc_delete_calibration(adc_unit_t unit,
        adc_channel_t channel) {
    adc_cali_handle_t handle = bmx_esp32_adc_calibration[unit][channel];
    if (!handle) return;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (bmx_esp32_adc_calibration_scheme[unit][channel] == 1)
        (void)adc_cali_delete_scheme_curve_fitting(handle);
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (bmx_esp32_adc_calibration_scheme[unit][channel] == 2)
        (void)adc_cali_delete_scheme_line_fitting(handle);
#endif
    bmx_esp32_adc_calibration[unit][channel] = NULL;
    bmx_esp32_adc_calibration_scheme[unit][channel] = 0;
}

static int32_t bmx_esp32_adc_ensure_calibration(adc_unit_t unit,
        adc_channel_t channel) {
    if (bmx_esp32_adc_calibration[unit][channel]) return 1;
    adc_atten_t attenuation = bmx_esp32_adc_attenuation_value(
        bmx_esp32_adc_attenuation[unit][channel]);
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t curve = {
        .unit_id = unit,
        .chan = channel,
        .atten = attenuation,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&curve,
            &bmx_esp32_adc_calibration[unit][channel]) == ESP_OK) {
        bmx_esp32_adc_calibration_scheme[unit][channel] = 1;
        return 1;
    }
#endif
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t line = {
        .unit_id = unit,
        .atten = attenuation,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
#if CONFIG_IDF_TARGET_ESP32
        .default_vref = 1100,
#endif
    };
    if (adc_cali_create_scheme_line_fitting(&line,
            &bmx_esp32_adc_calibration[unit][channel]) == ESP_OK) {
        bmx_esp32_adc_calibration_scheme[unit][channel] = 2;
        return 1;
    }
#endif
    return 0;
}

int32_t bmx_embedded_adc_init_pin(uint32_t gpio) {
    adc_unit_t unit;
    adc_channel_t channel;
    if (!bmx_esp32_adc_pin_mapping(gpio, &unit, &channel) ||
            bmx_esp32_random_internal_entropy_enabled()) return 0;
    if (bmx_esp32_adc_configured[unit][channel]) return 1;
    return bmx_esp32_adc_configure(unit, channel, 3);
}

int32_t bmx_embedded_adc_deinit_pin(uint32_t gpio) {
    adc_unit_t unit;
    adc_channel_t channel;
    if (!bmx_esp32_adc_pin_mapping(gpio, &unit, &channel) ||
            !bmx_esp32_adc_configured[unit][channel]) return 0;
    bmx_esp32_adc_delete_calibration(unit, channel);
    bmx_esp32_adc_configured[unit][channel] = 0;
    for (uint32_t candidate = 0; candidate < SOC_ADC_MAX_CHANNEL_NUM; ++candidate) {
        if (bmx_esp32_adc_configured[unit][candidate]) return 1;
    }
    if (adc_oneshot_del_unit(bmx_esp32_adc_units[unit]) != ESP_OK) {
        bmx_esp32_adc_configured[unit][channel] = 1;
        return 0;
    }
    bmx_esp32_adc_units[unit] = NULL;
    return 1;
}

int32_t bmx_embedded_adc_read_raw(uint32_t gpio, uint32_t *value) {
    adc_unit_t unit;
    adc_channel_t channel;
    int reading;
    if (!value || bmx_esp32_random_internal_entropy_enabled() ||
            !bmx_esp32_adc_pin_mapping(gpio, &unit, &channel) ||
            !bmx_esp32_adc_configured[unit][channel]) return 0;
    if (adc_oneshot_read(bmx_esp32_adc_units[unit], channel, &reading) != ESP_OK)
        return 0;
    *value = (uint32_t)reading;
    return 1;
}

uint32_t bmx_embedded_adc_resolution_bits(uint32_t gpio) {
    return bmx_embedded_adc_is_valid_pin(gpio) ? SOC_ADC_RTC_MAX_BITWIDTH : 0u;
}

uint32_t bmx_embedded_adc_maximum_value(uint32_t gpio) {
    uint32_t bits = bmx_embedded_adc_resolution_bits(gpio);
    return bits && bits < 32u ? (UINT32_C(1) << bits) - 1u : 0u;
}

int32_t bmx_esp32_adc_unit_for_pin(uint32_t gpio) {
    adc_unit_t unit;
    adc_channel_t channel;
    return bmx_esp32_adc_pin_mapping(gpio, &unit, &channel) ? (int32_t)unit : -1;
}

int32_t bmx_esp32_adc_channel_for_pin(uint32_t gpio) {
    adc_unit_t unit;
    adc_channel_t channel;
    return bmx_esp32_adc_pin_mapping(gpio, &unit, &channel) ? (int32_t)channel : -1;
}

int32_t bmx_esp32_adc_set_attenuation(uint32_t gpio, int32_t attenuation) {
    adc_unit_t unit;
    adc_channel_t channel;
    if (bmx_esp32_random_internal_entropy_enabled() ||
            !bmx_esp32_adc_pin_mapping(gpio, &unit, &channel) ||
            !bmx_esp32_adc_configured[unit][channel]) return 0;
    if (!bmx_esp32_adc_configure(unit, channel, attenuation)) return 0;
    bmx_esp32_adc_delete_calibration(unit, channel);
    return 1;
}

int32_t bmx_esp32_adc_get_attenuation(uint32_t gpio) {
    adc_unit_t unit;
    adc_channel_t channel;
    if (!bmx_esp32_adc_pin_mapping(gpio, &unit, &channel) ||
            !bmx_esp32_adc_configured[unit][channel]) return -1;
    return bmx_esp32_adc_attenuation[unit][channel];
}

int32_t bmx_esp32_adc_read_millivolts(uint32_t gpio, int32_t *millivolts) {
    adc_unit_t unit;
    adc_channel_t channel;
    int reading;
    int voltage;
    if (!millivolts || bmx_esp32_random_internal_entropy_enabled() ||
            !bmx_esp32_adc_pin_mapping(gpio, &unit, &channel) ||
            !bmx_esp32_adc_configured[unit][channel] ||
            !bmx_esp32_adc_ensure_calibration(unit, channel) ||
            adc_oneshot_read(bmx_esp32_adc_units[unit], channel, &reading) != ESP_OK)
        return 0;
    if (adc_cali_raw_to_voltage(bmx_esp32_adc_calibration[unit][channel],
            reading, &voltage) != ESP_OK) return 0;
    *millivolts = (int32_t)voltage;
    return 1;
}
