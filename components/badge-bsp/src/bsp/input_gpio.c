
// SPDX-License-Identifier: MIT

#include "bsp/input_gpio.h"

#include "bsp.h"
#include "bsp_device.h"

#include <stdatomic.h>

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

static char const TAG[] = "bsp-gpio";



// GPIO interrupt config.
typedef struct {
    // Device ID.
    uint32_t dev_id;
    // Device endpoint.
    uint8_t  dev_ep;
    // GPIO pin.
    uint8_t  pin;
    // Active-low pin.
    bool     activelow;
    // Raw input.
    int      input;
} gpio_irq_cfg_t;

// ISR for GPIO input devices.
static void gpio_input_isr(void *arg) {
    gpio_irq_cfg_t *cfg = arg;

    if (gpio_get_level(cfg->pin) ^ cfg->activelow) {
        bsp_raw_button_pressed_from_isr(cfg->dev_id, cfg->dev_ep, cfg->input);
    } else {
        bsp_raw_button_released_from_isr(cfg->dev_id, cfg->dev_ep, cfg->input);
    }
}

// GPIO input init function.
bool bsp_input_gpio_init(bsp_device_t *dev, uint8_t endpoint) {
    bsp_input_devtree_t const *tree = bsp_dev_get_tree_raw(dev)->input_dev[endpoint];

    // Verify input pins.
    for (uint8_t i = 0; i < tree->pinmap->pins_len; i++) {
        uint8_t pin = tree->pinmap->pins[i];
#pragma GCC diagnostic ignored "-Wtype-limits"
        if (!GPIO_IS_VALID_GPIO(pin)) {
            ESP_LOGE(TAG, "Invalid GPIO pin %" PRId8, pin);
            return false;
        }
    }

    // Allocate memory.
    gpio_irq_cfg_t *cfg = dev->input_aux[endpoint] = calloc(tree->pinmap->pins_len, sizeof(gpio_irq_cfg_t));
    if (!cfg) {
        ESP_LOGE(TAG, "Out of memory");
        return false;
    }

    // Map input pins and install ISR.
    uint8_t   pin;
    esp_err_t res;
    for (uint8_t i = 0; i < tree->pinmap->pins_len; i++) {
        pin = tree->pinmap->pins[i];

        cfg[i] = (gpio_irq_cfg_t){
            .dev_id    = dev->id,
            .dev_ep    = endpoint,
            .pin       = pin,
            .activelow = tree->pinmap->activelow,
            .input     = i,
        };

        if ((res = gpio_set_direction(pin, GPIO_MODE_INPUT)) != ESP_OK) {
            goto pinerr;
        }
        if ((res = gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE)) != ESP_OK) {
            goto pinerr;
        }
        if ((res = gpio_intr_enable(pin)) != ESP_OK) {
            goto pinerr;
        }
        if ((res = gpio_isr_handler_add(pin, gpio_input_isr, cfg + i)) != ESP_OK) {
            goto pinerr;
        }
    }

    return true;
pinerr:
    ESP_LOGE(TAG, "Pin %" PRIu8 " error: %s", pin, esp_err_to_name(res));
    for (uint8_t i = 0; i < tree->pinmap->pins_len; i++) {
        uint8_t pin = tree->pinmap->pins[i];
        gpio_intr_disable(pin);
        gpio_isr_handler_remove(pin);
    }
    return false;
}

// GPIO input deinit function.
bool bsp_input_gpio_deinit(bsp_device_t *dev, uint8_t endpoint) {
    bsp_input_devtree_t const *tree = bsp_dev_get_tree_raw(dev)->input_dev[endpoint];

    // Remove interrupts from pins.
    for (uint8_t i = 0; i < tree->pinmap->pins_len; i++) {
        uint8_t pin = tree->pinmap->pins[i];
        gpio_intr_disable(pin);
        gpio_isr_handler_remove(pin);
    }

    return true;
}

// Get current input value by raw input number.
bool bsp_input_gpio_get_raw(bsp_device_t *dev, uint8_t endpoint, uint16_t raw_input) {
    bsp_input_devtree_t const *tree = bsp_dev_get_tree_raw(dev)->input_dev[endpoint];
    return gpio_get_level(tree->pinmap->pins[raw_input]);
}
