#include <limits.h>
#include <stdint.h>

#include "blitzmax/embedded_pwm.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_private/esp_clk.h"

typedef struct BMXESP32PWMChannel {
    uint8_t configured;
    uint8_t enabled;
    uint8_t inverted;
    uint8_t timer;
    uint8_t resolution_bits;
    uint32_t gpio;
    uint32_t duty;
} BMXESP32PWMChannel;

typedef struct BMXESP32PWMTimer {
    uint8_t used;
    uint8_t references;
    uint8_t resolution_bits;
    uint32_t requested_frequency;
    uint32_t achieved_frequency;
} BMXESP32PWMTimer;

static BMXESP32PWMChannel bmx_esp32_pwm_channels[LEDC_CHANNEL_MAX];
static BMXESP32PWMTimer bmx_esp32_pwm_timers[LEDC_TIMER_MAX];

int32_t bmx_embedded_pwm_is_valid_pin(uint32_t gpio) {
    return gpio <= INT_MAX && GPIO_IS_VALID_OUTPUT_GPIO((int)gpio);
}

static int32_t bmx_esp32_pwm_channel_for_gpio(uint32_t gpio) {
    for (int32_t channel = 0; channel < LEDC_CHANNEL_MAX; ++channel) {
        if (bmx_esp32_pwm_channels[channel].configured &&
                bmx_esp32_pwm_channels[channel].gpio == gpio) return channel;
    }
    return -1;
}

static int32_t bmx_esp32_pwm_free_channel(void) {
    for (int32_t channel = 0; channel < LEDC_CHANNEL_MAX; ++channel) {
        if (!bmx_esp32_pwm_channels[channel].configured) return channel;
    }
    return -1;
}

static int32_t bmx_esp32_pwm_timer_for_frequency(uint32_t frequency) {
    for (int32_t timer = 0; timer < LEDC_TIMER_MAX; ++timer) {
        if (bmx_esp32_pwm_timers[timer].used &&
                bmx_esp32_pwm_timers[timer].requested_frequency == frequency) return timer;
    }
    for (int32_t timer = 0; timer < LEDC_TIMER_MAX; ++timer) {
        if (!bmx_esp32_pwm_timers[timer].used) return timer;
    }
    return -1;
}

static uint32_t bmx_esp32_pwm_configure_timer(int32_t timer, uint32_t frequency) {
    uint32_t resolution = ledc_find_suitable_duty_resolution(
        (uint32_t)esp_clk_apb_freq(), frequency);
    if (!resolution) return 0;
    ledc_timer_config_t config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)resolution,
        .timer_num = (ledc_timer_t)timer,
        .freq_hz = frequency,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&config) != ESP_OK) return 0;
    uint32_t achieved = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)timer);
    if (!achieved) return 0;
    bmx_esp32_pwm_timers[timer].used = 1;
    bmx_esp32_pwm_timers[timer].resolution_bits = (uint8_t)resolution;
    bmx_esp32_pwm_timers[timer].requested_frequency = frequency;
    bmx_esp32_pwm_timers[timer].achieved_frequency = achieved;
    return achieved;
}

static uint32_t bmx_esp32_pwm_native_duty(const BMXESP32PWMChannel *channel) {
    if (!channel->enabled) return 0;
    uint32_t maximum = (UINT32_C(1) << channel->resolution_bits) - 1u;
    return (uint32_t)(((uint64_t)channel->duty * maximum +
        BMX_EMBEDDED_PWM_DUTY_MAXIMUM / 2u) / BMX_EMBEDDED_PWM_DUTY_MAXIMUM);
}

static int32_t bmx_esp32_pwm_apply_channel(int32_t channel) {
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    ledc_channel_config_t config = {
        .gpio_num = (int)state->gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)channel,
        .timer_sel = (ledc_timer_t)state->timer,
        .duty = bmx_esp32_pwm_native_duty(state),
        .hpoint = 0,
        .flags.output_invert = state->inverted,
    };
    return ledc_channel_config(&config) == ESP_OK;
}

static int32_t bmx_esp32_pwm_apply_duty(int32_t channel) {
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel,
            bmx_esp32_pwm_native_duty(state)) != ESP_OK) return 0;
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel) == ESP_OK;
}

static int32_t bmx_esp32_pwm_release_channel(int32_t channel) {
    ledc_channel_config_t config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)channel,
        .deconfigure = true,
    };
    return ledc_channel_config(&config) == ESP_OK;
}

uint32_t bmx_embedded_pwm_init_pin(uint32_t gpio, uint32_t frequency,
        uint32_t duty, int32_t inverted) {
    if (!bmx_embedded_pwm_is_valid_pin(gpio) || !frequency ||
            duty > BMX_EMBEDDED_PWM_DUTY_MAXIMUM) return 0;
    int32_t existing = bmx_esp32_pwm_channel_for_gpio(gpio);
    if (existing >= 0) {
        if (!bmx_embedded_pwm_set_pin_frequency(gpio, frequency) ||
                !bmx_embedded_pwm_set_pin_polarity(gpio, inverted) ||
                !bmx_embedded_pwm_set_pin_duty(gpio, duty) ||
                !bmx_embedded_pwm_set_pin_enabled(gpio, 1)) return 0;
        return bmx_embedded_pwm_get_pin_frequency(gpio);
    }
    int32_t channel = bmx_esp32_pwm_free_channel();
    int32_t timer = bmx_esp32_pwm_timer_for_frequency(frequency);
    if (channel < 0 || timer < 0) return 0;
    BMXESP32PWMTimer *timer_state = &bmx_esp32_pwm_timers[timer];
    if (!timer_state->used && !bmx_esp32_pwm_configure_timer(timer, frequency)) return 0;
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    state->configured = 1;
    state->enabled = 1;
    state->inverted = inverted != 0;
    state->timer = (uint8_t)timer;
    state->resolution_bits = timer_state->resolution_bits;
    state->gpio = gpio;
    state->duty = duty;
    if (!bmx_esp32_pwm_apply_channel(channel)) {
        state->configured = 0;
        if (!timer_state->references) timer_state->used = 0;
        return 0;
    }
    ++timer_state->references;
    return timer_state->achieved_frequency;
}

int32_t bmx_embedded_pwm_deinit_pin(uint32_t gpio) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    if (channel < 0) return 0;
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    BMXESP32PWMTimer *timer = &bmx_esp32_pwm_timers[state->timer];
    (void)ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel,
        state->inverted ? 1u : 0u);
    if (!bmx_esp32_pwm_release_channel(channel)) return 0;
    (void)gpio_reset_pin((gpio_num_t)gpio);
    state->configured = 0;
    if (timer->references) --timer->references;
    if (!timer->references) timer->used = 0;
    return 1;
}

uint32_t bmx_embedded_pwm_set_pin_frequency(uint32_t gpio, uint32_t frequency) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    if (channel < 0 || !frequency) return 0;
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    BMXESP32PWMTimer *timer = &bmx_esp32_pwm_timers[state->timer];
    if (timer->requested_frequency == frequency) return timer->achieved_frequency;
    if (timer->references > 1) return 0;
    uint32_t achieved = bmx_esp32_pwm_configure_timer(state->timer, frequency);
    if (!achieved) return 0;
    state->resolution_bits = timer->resolution_bits;
    if (state->enabled && !bmx_esp32_pwm_apply_duty(channel)) return 0;
    return achieved;
}

uint32_t bmx_embedded_pwm_get_pin_frequency(uint32_t gpio) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    if (channel < 0) return 0;
    return bmx_esp32_pwm_timers[bmx_esp32_pwm_channels[channel].timer].achieved_frequency;
}

int32_t bmx_embedded_pwm_set_pin_duty(uint32_t gpio, uint32_t duty) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    if (channel < 0 || duty > BMX_EMBEDDED_PWM_DUTY_MAXIMUM) return 0;
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    uint32_t previous = state->duty;
    state->duty = duty;
    if (!state->enabled || bmx_esp32_pwm_apply_duty(channel)) return 1;
    state->duty = previous;
    return 0;
}

uint32_t bmx_embedded_pwm_get_pin_duty(uint32_t gpio) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    return channel >= 0 ? bmx_esp32_pwm_channels[channel].duty : 0u;
}

int32_t bmx_embedded_pwm_set_pin_polarity(uint32_t gpio, int32_t inverted) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    if (channel < 0) return 0;
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    uint8_t previous = state->inverted;
    (void)ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel,
        previous ? 1u : 0u);
    if (!bmx_esp32_pwm_release_channel(channel)) return 0;
    state->inverted = inverted != 0;
    if (bmx_esp32_pwm_apply_channel(channel)) return 1;
    state->inverted = previous;
    (void)bmx_esp32_pwm_apply_channel(channel);
    return 0;
}

int32_t bmx_embedded_pwm_get_pin_polarity(uint32_t gpio) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    return channel >= 0 && bmx_esp32_pwm_channels[channel].inverted;
}

int32_t bmx_embedded_pwm_set_pin_enabled(uint32_t gpio, int32_t enabled) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    if (channel < 0) return 0;
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    uint8_t previous = state->enabled;
    state->enabled = enabled != 0;
    if (state->enabled) {
        if (bmx_esp32_pwm_apply_duty(channel)) return 1;
    } else if (ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel,
            state->inverted ? 1u : 0u) == ESP_OK) {
        return 1;
    }
    state->enabled = previous;
    return 0;
}

int32_t bmx_embedded_pwm_get_pin_enabled(uint32_t gpio) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    return channel >= 0 && bmx_esp32_pwm_channels[channel].enabled;
}

int32_t bmx_esp32_pwm_channel_for_pin(uint32_t gpio) {
    return bmx_esp32_pwm_channel_for_gpio(gpio);
}

int32_t bmx_esp32_pwm_timer_for_pin(uint32_t gpio) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    return channel >= 0 ? bmx_esp32_pwm_channels[channel].timer : -1;
}

uint32_t bmx_esp32_pwm_resolution_bits_for_pin(uint32_t gpio) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    return channel >= 0 ? bmx_esp32_pwm_channels[channel].resolution_bits : 0u;
}
