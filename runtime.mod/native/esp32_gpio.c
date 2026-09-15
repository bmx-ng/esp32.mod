#include <stdint.h>

#include "blitzmax/embedded_events.h"
#include "blitzmax/embedded_gpio.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "soc/soc_caps.h"

#define BMX_EMBEDDED_GPIO_IRQ_LEVEL_LOW 1u
#define BMX_EMBEDDED_GPIO_IRQ_LEVEL_HIGH 2u
#define BMX_EMBEDDED_GPIO_IRQ_EDGE_FALL 4u
#define BMX_EMBEDDED_GPIO_IRQ_EDGE_RISE 8u

static uint8_t bmx_esp32_gpio_direction[GPIO_NUM_MAX];
static uint8_t bmx_esp32_gpio_output[GPIO_NUM_MAX];
static uint8_t bmx_esp32_gpio_pull_up[GPIO_NUM_MAX];
static uint8_t bmx_esp32_gpio_pull_down[GPIO_NUM_MAX];
static volatile uint32_t bmx_esp32_gpio_irq_events[GPIO_NUM_MAX];
static volatile uint32_t bmx_esp32_gpio_event_tokens[GPIO_NUM_MAX];
static volatile uint32_t bmx_esp32_gpio_irq_masks[GPIO_NUM_MAX];
static portMUX_TYPE bmx_esp32_gpio_irq_lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t bmx_esp32_gpio_isr_service_ready;

int32_t bmx_embedded_gpio_is_valid(uint32_t gpio) {
    return gpio <= INT32_MAX && GPIO_IS_VALID_GPIO((int32_t)gpio);
}

int32_t bmx_embedded_gpio_is_output_capable(uint32_t gpio) {
    return gpio <= INT32_MAX && GPIO_IS_VALID_OUTPUT_GPIO((int32_t)gpio);
}

int32_t bmx_embedded_gpio_is_pull_capable(uint32_t gpio) {
    return bmx_embedded_gpio_is_output_capable(gpio);
}

int32_t bmx_embedded_gpio_init(uint32_t gpio) {
    if (!bmx_embedded_gpio_is_valid(gpio) || gpio_reset_pin((gpio_num_t)gpio) != ESP_OK) return 0;
    if (bmx_embedded_gpio_is_output_capable(gpio) &&
            gpio_set_level((gpio_num_t)gpio, 0) != ESP_OK) return 0;
    if (bmx_embedded_gpio_is_pull_capable(gpio) &&
            gpio_set_pull_mode((gpio_num_t)gpio, GPIO_FLOATING) != ESP_OK) return 0;
    if (gpio_set_direction((gpio_num_t)gpio, GPIO_MODE_INPUT) != ESP_OK) return 0;
    bmx_esp32_gpio_direction[gpio] = 0;
    bmx_esp32_gpio_output[gpio] = 0;
    bmx_esp32_gpio_pull_up[gpio] = 0;
    bmx_esp32_gpio_pull_down[gpio] = 0;
    return 1;
}

int32_t bmx_embedded_gpio_set_direction(uint32_t gpio, int32_t direction) {
    if (!bmx_embedded_gpio_is_valid(gpio) || (direction != 0 && direction != 1)) return 0;
    if (direction && !bmx_embedded_gpio_is_output_capable(gpio)) return 0;
    gpio_mode_t mode = direction ? GPIO_MODE_INPUT_OUTPUT : GPIO_MODE_INPUT;
    if (gpio_set_direction((gpio_num_t)gpio, mode) != ESP_OK) return 0;
    bmx_esp32_gpio_direction[gpio] = direction != 0;
    return 1;
}

int32_t bmx_embedded_gpio_get_direction(uint32_t gpio) {
    if (!bmx_embedded_gpio_is_valid(gpio)) return 0;
    return bmx_esp32_gpio_direction[gpio] != 0;
}

int32_t bmx_embedded_gpio_set_input(uint32_t gpio) {
    return bmx_embedded_gpio_set_direction(gpio, 0);
}

int32_t bmx_embedded_gpio_set_output(uint32_t gpio) {
    return bmx_embedded_gpio_set_direction(gpio, 1);
}

int32_t bmx_embedded_gpio_get(uint32_t gpio) {
    if (!bmx_embedded_gpio_is_valid(gpio)) return 0;
    return gpio_get_level((gpio_num_t)gpio) != 0;
}

int32_t bmx_embedded_gpio_put(uint32_t gpio, int32_t value) {
    if (!bmx_embedded_gpio_is_output_capable(gpio) ||
            gpio_set_level((gpio_num_t)gpio, value != 0) != ESP_OK) return 0;
    bmx_esp32_gpio_output[gpio] = value != 0;
    return 1;
}

int32_t bmx_embedded_gpio_get_output(uint32_t gpio) {
    if (!bmx_embedded_gpio_is_output_capable(gpio)) return 0;
    return bmx_esp32_gpio_output[gpio] != 0;
}

int32_t bmx_embedded_gpio_set_pulls(uint32_t gpio, int32_t pull_up, int32_t pull_down) {
    if (!bmx_embedded_gpio_is_pull_capable(gpio)) return 0;
    gpio_pull_mode_t mode;
    if (pull_up && pull_down) {
        mode = GPIO_PULLUP_PULLDOWN;
    } else if (pull_up) {
        mode = GPIO_PULLUP_ONLY;
    } else if (pull_down) {
        mode = GPIO_PULLDOWN_ONLY;
    } else {
        mode = GPIO_FLOATING;
    }
    if (gpio_set_pull_mode((gpio_num_t)gpio, mode) != ESP_OK) return 0;
    bmx_esp32_gpio_pull_up[gpio] = pull_up != 0;
    bmx_esp32_gpio_pull_down[gpio] = pull_down != 0;
    return 1;
}

int32_t bmx_embedded_gpio_pull_up(uint32_t gpio) {
    return bmx_embedded_gpio_set_pulls(gpio, 1, 0);
}

int32_t bmx_embedded_gpio_pull_down(uint32_t gpio) {
    return bmx_embedded_gpio_set_pulls(gpio, 0, 1);
}

int32_t bmx_embedded_gpio_disable_pulls(uint32_t gpio) {
    return bmx_embedded_gpio_set_pulls(gpio, 0, 0);
}

int32_t bmx_embedded_gpio_is_pulled_up(uint32_t gpio) {
    if (!bmx_embedded_gpio_is_valid(gpio)) return 0;
    return bmx_esp32_gpio_pull_up[gpio] != 0;
}

int32_t bmx_embedded_gpio_is_pulled_down(uint32_t gpio) {
    if (!bmx_embedded_gpio_is_valid(gpio)) return 0;
    return bmx_esp32_gpio_pull_down[gpio] != 0;
}

int32_t bmx_embedded_gpio_set_drive_strength(uint32_t gpio, int32_t drive_strength) {
    if (!bmx_embedded_gpio_is_output_capable(gpio) || drive_strength < 0 || drive_strength > 3) return 0;
    return gpio_set_drive_capability((gpio_num_t)gpio,
        (gpio_drive_cap_t)drive_strength) == ESP_OK;
}

int32_t bmx_embedded_gpio_get_drive_strength(uint32_t gpio) {
    gpio_drive_cap_t drive_strength;
    if (!bmx_embedded_gpio_is_output_capable(gpio) ||
            gpio_get_drive_capability((gpio_num_t)gpio, &drive_strength) != ESP_OK) return 0;
    return (int32_t)drive_strength;
}

static gpio_int_type_t bmx_esp32_gpio_interrupt_type(uint32_t event_mask) {
    if (event_mask == BMX_EMBEDDED_GPIO_IRQ_LEVEL_LOW) return GPIO_INTR_LOW_LEVEL;
    if (event_mask == BMX_EMBEDDED_GPIO_IRQ_LEVEL_HIGH) return GPIO_INTR_HIGH_LEVEL;
    if (event_mask == BMX_EMBEDDED_GPIO_IRQ_EDGE_FALL) return GPIO_INTR_NEGEDGE;
    if (event_mask == BMX_EMBEDDED_GPIO_IRQ_EDGE_RISE) return GPIO_INTR_POSEDGE;
    if (event_mask == (BMX_EMBEDDED_GPIO_IRQ_EDGE_FALL |
            BMX_EMBEDDED_GPIO_IRQ_EDGE_RISE)) return GPIO_INTR_ANYEDGE;
    return GPIO_INTR_MAX;
}

static void bmx_esp32_gpio_interrupt(void *context) {
    uint32_t gpio = (uint32_t)(uintptr_t)context;
    if (gpio >= GPIO_NUM_MAX) return;
    uint32_t event_mask = bmx_esp32_gpio_irq_masks[gpio];
    uint32_t events = event_mask;
    if (event_mask == (BMX_EMBEDDED_GPIO_IRQ_EDGE_FALL |
            BMX_EMBEDDED_GPIO_IRQ_EDGE_RISE)) {
        events = gpio_get_level((gpio_num_t)gpio)
            ? BMX_EMBEDDED_GPIO_IRQ_EDGE_RISE
            : BMX_EMBEDDED_GPIO_IRQ_EDGE_FALL;
    }
    uint64_t captured_time = (uint64_t)esp_timer_get_time();
    portENTER_CRITICAL_ISR(&bmx_esp32_gpio_irq_lock);
    bmx_esp32_gpio_irq_events[gpio] |= events;
    uint32_t token = bmx_esp32_gpio_event_tokens[gpio];
    portEXIT_CRITICAL_ISR(&bmx_esp32_gpio_irq_lock);
    (void)bmx_embedded_event_post_from_isr_ex(token, events, gpio,
        (uint32_t)captured_time, (uint32_t)(captured_time >> 32u));
}

int32_t bmx_embedded_gpio_set_irq_enabled(uint32_t gpio, uint32_t event_mask,
        int32_t enabled) {
    if (!bmx_embedded_gpio_is_valid(gpio) || !event_mask ||
            bmx_esp32_gpio_interrupt_type(event_mask) == GPIO_INTR_MAX) return 0;
    if (!enabled) {
        if (gpio_intr_disable((gpio_num_t)gpio) != ESP_OK) return 0;
        if (bmx_esp32_gpio_isr_service_ready)
            (void)gpio_isr_handler_remove((gpio_num_t)gpio);
        portENTER_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
        bmx_esp32_gpio_irq_masks[gpio] = 0;
        portEXIT_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
        return 1;
    }
    if (!bmx_esp32_gpio_isr_service_ready) {
        esp_err_t installed = gpio_install_isr_service(0);
        if (installed != ESP_OK && installed != ESP_ERR_INVALID_STATE) return 0;
        bmx_esp32_gpio_isr_service_ready = 1;
    }
    if (gpio_set_intr_type((gpio_num_t)gpio,
            bmx_esp32_gpio_interrupt_type(event_mask)) != ESP_OK) return 0;
    portENTER_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
    bmx_esp32_gpio_irq_masks[gpio] = event_mask;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
    if (gpio_isr_handler_add((gpio_num_t)gpio, bmx_esp32_gpio_interrupt,
            (void *)(uintptr_t)gpio) != ESP_OK) {
        portENTER_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
        bmx_esp32_gpio_irq_masks[gpio] = 0;
        portEXIT_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
        return 0;
    }
    if (gpio_intr_enable((gpio_num_t)gpio) == ESP_OK) return 1;
    (void)gpio_isr_handler_remove((gpio_num_t)gpio);
    portENTER_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
    bmx_esp32_gpio_irq_masks[gpio] = 0;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
    return 0;
}

int32_t bmx_embedded_gpio_set_event_token(uint32_t gpio, uint32_t token) {
    if (!bmx_embedded_gpio_is_valid(gpio)) return 0;
    portENTER_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
    if (token && bmx_esp32_gpio_event_tokens[gpio] &&
            bmx_esp32_gpio_event_tokens[gpio] != token) {
        portEXIT_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
        return 0;
    }
    bmx_esp32_gpio_event_tokens[gpio] = token;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
    return 1;
}

uint32_t bmx_embedded_gpio_pending_irq_events(uint32_t gpio) {
    if (!bmx_embedded_gpio_is_valid(gpio)) return 0;
    portENTER_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
    uint32_t events = bmx_esp32_gpio_irq_events[gpio];
    portEXIT_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
    return events;
}

uint32_t bmx_embedded_gpio_take_irq_events(uint32_t gpio) {
    if (!bmx_embedded_gpio_is_valid(gpio)) return 0;
    portENTER_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
    uint32_t events = bmx_esp32_gpio_irq_events[gpio];
    bmx_esp32_gpio_irq_events[gpio] = 0;
    portEXIT_CRITICAL_SAFE(&bmx_esp32_gpio_irq_lock);
    return events;
}
