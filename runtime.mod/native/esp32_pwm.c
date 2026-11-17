#include <limits.h>
#include <stdint.h>

#include "blitzmax/embedded_pwm.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_attr.h"
#include "esp_clk_tree.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"

typedef struct BMXESP32PWMChannel {
    uint8_t configured;
    uint8_t enabled;
    uint8_t inverted;
    uint8_t timer;
    uint8_t resolution_bits;
    uint8_t sleep_mode;
    uint8_t duty_pending;
    volatile uint8_t fading;
    uint32_t gpio;
    uint32_t duty;
    volatile uint32_t fade_target_native;
} BMXESP32PWMChannel;

typedef struct BMXESP32PWMTimer {
    uint8_t used;
    uint8_t references;
    uint8_t resolution_bits;
    ledc_clk_cfg_t clock_source;
    uint32_t requested_frequency;
    uint32_t achieved_frequency;
} BMXESP32PWMTimer;

static BMXESP32PWMChannel bmx_esp32_pwm_channels[LEDC_CHANNEL_MAX];
static BMXESP32PWMTimer bmx_esp32_pwm_timers[LEDC_TIMER_MAX];
static uint8_t bmx_esp32_pwm_fade_service_installed;

static ledc_clk_cfg_t bmx_esp32_pwm_clock_source(void) {
#if CONFIG_IDF_TARGET_ESP32
    /*
     * Classic ESP32 uses one low-speed source for the whole peripheral.
     * Reconfiguring an active timer between REF_TICK and RC_FAST can leave
     * the low-speed update handshake wedged.  RC_FAST also remains available
     * during light sleep, so use it consistently on this target.
     */
    return LEDC_USE_RC_FAST_CLK;
#elif SOC_LEDC_SUPPORT_XTAL_CLOCK
    return LEDC_USE_XTAL_CLK;
#elif SOC_LEDC_SUPPORT_REF_TICK
    return LEDC_USE_REF_TICK;
#else
    return LEDC_USE_RC_FAST_CLK;
#endif
}

static ledc_clk_cfg_t bmx_esp32_pwm_sleep_clock_source(void) {
#if SOC_LEDC_SUPPORT_XTAL_CLOCK
    return LEDC_USE_XTAL_CLK;
#else
    return LEDC_USE_RC_FAST_CLK;
#endif
}

static uint32_t bmx_esp32_pwm_clock_frequency(ledc_clk_cfg_t clock_source) {
    uint32_t frequency = 0;
    if (esp_clk_tree_src_get_freq_hz((soc_module_clk_t)clock_source,
            ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &frequency) != ESP_OK) return 0;
    return frequency;
}

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

static uint32_t bmx_esp32_pwm_configure_timer(int32_t timer, uint32_t frequency,
        ledc_clk_cfg_t clock_source) {
    uint32_t clock_frequency = bmx_esp32_pwm_clock_frequency(clock_source);
    if (!clock_frequency) return 0;
    uint32_t resolution = ledc_find_suitable_duty_resolution(clock_frequency, frequency);
    if (!resolution) return 0;
    ledc_timer_config_t config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)resolution,
        .timer_num = (ledc_timer_t)timer,
        .freq_hz = frequency,
        .clk_cfg = clock_source,
    };
    if (ledc_timer_config(&config) != ESP_OK) return 0;
    uint32_t achieved = ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)timer);
    if (!achieved) return 0;
    bmx_esp32_pwm_timers[timer].used = 1;
    bmx_esp32_pwm_timers[timer].resolution_bits = (uint8_t)resolution;
    bmx_esp32_pwm_timers[timer].clock_source = clock_source;
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
        .sleep_mode = (ledc_sleep_mode_t)state->sleep_mode,
        .flags.output_invert = state->inverted,
    };
    if (ledc_channel_config(&config) != ESP_OK) return 0;
    state->duty_pending = 1;
    return 1;
}

static int32_t bmx_esp32_pwm_apply_duty(int32_t channel) {
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel,
            bmx_esp32_pwm_native_duty(state)) != ESP_OK) return 0;
    if (ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel) != ESP_OK) return 0;
    state->duty_pending = 1;
    return 1;
}

static void bmx_esp32_pwm_wait_for_duty_latch(BMXESP32PWMChannel *state) {
    if (!state->duty_pending) return;
    uint32_t frequency = bmx_esp32_pwm_timers[state->timer].achieved_frequency;
    uint32_t milliseconds = frequency ? (1000u + frequency - 1u) / frequency + 1u : 2u;
    TickType_t ticks = pdMS_TO_TICKS(milliseconds);
    vTaskDelay(ticks ? ticks : 1);
    state->duty_pending = 0;
}

static int32_t bmx_esp32_pwm_release_channel(int32_t channel) {
    ledc_channel_config_t config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)channel,
        .deconfigure = true,
    };
    return ledc_channel_config(&config) == ESP_OK;
}

static int32_t bmx_esp32_pwm_timer_is_fading(int32_t timer) {
    for (int32_t channel = 0; channel < LEDC_CHANNEL_MAX; ++channel) {
        BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
        if (state->configured && state->timer == timer && state->fading) return 1;
    }
    return 0;
}

static int32_t bmx_esp32_pwm_timer_has_keep_alive(int32_t timer) {
    for (int32_t channel = 0; channel < LEDC_CHANNEL_MAX; ++channel) {
        BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
        if (state->configured && state->timer == timer &&
                state->sleep_mode == LEDC_SLEEP_MODE_KEEP_ALIVE) return 1;
    }
    return 0;
}

static int32_t bmx_esp32_pwm_set_timer_clock(int32_t timer,
        ledc_clk_cfg_t clock_source) {
    BMXESP32PWMTimer *timer_state = &bmx_esp32_pwm_timers[timer];
    if (timer_state->clock_source == clock_source) return 1;
    if (bmx_esp32_pwm_timer_is_fading(timer)) return 0;
    ledc_clk_cfg_t previous = timer_state->clock_source;
    if (!bmx_esp32_pwm_configure_timer(timer, timer_state->requested_frequency,
            clock_source)) return 0;
    for (int32_t channel = 0; channel < LEDC_CHANNEL_MAX; ++channel) {
        BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
        if (!state->configured || state->timer != timer) continue;
        state->resolution_bits = timer_state->resolution_bits;
        if (state->enabled && !bmx_esp32_pwm_apply_duty(channel)) {
            (void)bmx_esp32_pwm_configure_timer(timer,
                timer_state->requested_frequency, previous);
            for (int32_t restore = 0; restore < LEDC_CHANNEL_MAX; ++restore) {
                BMXESP32PWMChannel *restore_state = &bmx_esp32_pwm_channels[restore];
                if (restore_state->configured && restore_state->timer == timer) {
                    restore_state->resolution_bits = timer_state->resolution_bits;
                    if (restore_state->enabled) (void)bmx_esp32_pwm_apply_duty(restore);
                }
            }
            return 0;
        }
    }
    return 1;
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
    if (!timer_state->used && !bmx_esp32_pwm_configure_timer(timer, frequency,
            bmx_esp32_pwm_clock_source())) return 0;
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    state->configured = 1;
    state->enabled = 1;
    state->inverted = inverted != 0;
    state->timer = (uint8_t)timer;
    state->resolution_bits = timer_state->resolution_bits;
    state->sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    state->fading = 0;
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
    if (state->fading) return 0;
    BMXESP32PWMTimer *timer = &bmx_esp32_pwm_timers[state->timer];
    (void)ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel,
        state->inverted ? 1u : 0u);
    if (!bmx_esp32_pwm_release_channel(channel)) return 0;
    (void)gpio_reset_pin((gpio_num_t)gpio);
    state->configured = 0;
    if (timer->references) --timer->references;
    if (!timer->references) {
        timer->used = 0;
    } else if (!bmx_esp32_pwm_timer_has_keep_alive(state->timer)) {
        (void)bmx_esp32_pwm_set_timer_clock(state->timer,
            bmx_esp32_pwm_clock_source());
    }
    return 1;
}

uint32_t bmx_embedded_pwm_set_pin_frequency(uint32_t gpio, uint32_t frequency) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    if (channel < 0 || !frequency) return 0;
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    if (state->fading) return 0;
    BMXESP32PWMTimer *timer = &bmx_esp32_pwm_timers[state->timer];
    if (timer->requested_frequency == frequency) return timer->achieved_frequency;
    if (timer->references > 1) return 0;
    uint32_t achieved = bmx_esp32_pwm_configure_timer(state->timer, frequency,
        timer->clock_source);
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
    if (state->fading) return 0;
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
    if (state->fading) return 0;
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
    if (state->fading) return 0;
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

int32_t bmx_esp32_pwm_set_sleep_mode(uint32_t gpio, int32_t sleep_mode) {
    if (sleep_mode < LEDC_SLEEP_MODE_NO_ALIVE_NO_PD ||
            sleep_mode >= LEDC_SLEEP_MODE_INVALID) return 0;
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    if (channel < 0) return 0;
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    if (state->fading) return 0;
    if (state->sleep_mode == sleep_mode) return 1;
    uint8_t previous = state->sleep_mode;
    if (sleep_mode == LEDC_SLEEP_MODE_KEEP_ALIVE &&
            !bmx_esp32_pwm_set_timer_clock(state->timer,
                bmx_esp32_pwm_sleep_clock_source())) return 0;
    (void)ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel,
        state->inverted ? 1u : 0u);
    if (!bmx_esp32_pwm_release_channel(channel)) {
        if (!bmx_esp32_pwm_timer_has_keep_alive(state->timer))
            (void)bmx_esp32_pwm_set_timer_clock(state->timer, bmx_esp32_pwm_clock_source());
        return 0;
    }
    state->sleep_mode = (uint8_t)sleep_mode;
    if (bmx_esp32_pwm_apply_channel(channel)) {
        if (sleep_mode != LEDC_SLEEP_MODE_KEEP_ALIVE &&
                !bmx_esp32_pwm_timer_has_keep_alive(state->timer) &&
                !bmx_esp32_pwm_set_timer_clock(state->timer,
                    bmx_esp32_pwm_clock_source())) return 0;
        return 1;
    }
    state->sleep_mode = previous;
    (void)bmx_esp32_pwm_apply_channel(channel);
    if (!bmx_esp32_pwm_timer_has_keep_alive(state->timer))
        (void)bmx_esp32_pwm_set_timer_clock(state->timer, bmx_esp32_pwm_clock_source());
    return 0;
}

int32_t bmx_esp32_pwm_get_sleep_mode(uint32_t gpio) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    return channel >= 0 ? bmx_esp32_pwm_channels[channel].sleep_mode : -1;
}

static IRAM_ATTR bool bmx_esp32_pwm_fade_complete(
        const ledc_cb_param_t *parameter, void *user_argument) {
    BMXESP32PWMChannel *state = (BMXESP32PWMChannel *)user_argument;
    if (parameter && parameter->event == LEDC_FADE_END_EVT && state &&
            parameter->duty == state->fade_target_native) state->fading = 0;
    return false;
}

static int32_t bmx_esp32_pwm_prepare_fade(int32_t channel, int32_t asynchronous) {
    if (!bmx_esp32_pwm_fade_service_installed) {
        esp_err_t result = ledc_fade_func_install(0);
        if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return 0;
        bmx_esp32_pwm_fade_service_installed = 1;
    }
    ledc_cbs_t callbacks = {
        .fade_cb = asynchronous ? bmx_esp32_pwm_fade_complete : NULL,
    };
    return ledc_cb_register(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel,
        &callbacks, &bmx_esp32_pwm_channels[channel]) == ESP_OK;
}

int32_t bmx_esp32_pwm_fade_pin_duty(uint32_t gpio, uint32_t duty,
        uint32_t milliseconds, int32_t wait) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    if (channel < 0 || duty > BMX_EMBEDDED_PWM_DUTY_MAXIMUM || !milliseconds) return 0;
    BMXESP32PWMChannel *state = &bmx_esp32_pwm_channels[channel];
    if (!state->enabled || state->fading ||
            !bmx_esp32_pwm_prepare_fade(channel, !wait)) return 0;
    bmx_esp32_pwm_wait_for_duty_latch(state);
    uint32_t previous = state->duty;
    state->duty = duty;
    state->fade_target_native = bmx_esp32_pwm_native_duty(state);
    state->fading = 1;
    esp_err_t result = ledc_set_fade_time_and_start(LEDC_LOW_SPEED_MODE,
        (ledc_channel_t)channel, state->fade_target_native, milliseconds,
        wait ? LEDC_FADE_WAIT_DONE : LEDC_FADE_NO_WAIT);
    if (result != ESP_OK) {
        state->duty = previous;
        state->fading = 0;
        return 0;
    }
    if (wait) state->fading = 0;
    return 1;
}

int32_t bmx_esp32_pwm_is_pin_fading(uint32_t gpio) {
    int32_t channel = bmx_esp32_pwm_channel_for_gpio(gpio);
    return channel >= 0 && bmx_esp32_pwm_channels[channel].fading;
}
