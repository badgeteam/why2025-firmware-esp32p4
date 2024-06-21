
// SPDX-License-Identifier: MIT

#include "bsp/why2025_coproc.h"

#include "bsp.h"
#include "bsp_device.h"

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>


static char const TAG[] = "coprocessor";

static SemaphoreHandle_t ch32_int_semaphore;
static SemaphoreHandle_t ch32_semaphore;
static TaskHandle_t      ch32_thread_handle;
static void              ch32_thread(void *arg);
static void              ch32_isr(void *arg);
static uint32_t          ch32_input_dev_id;
static uint8_t           ch32_input_dev_ep;



/* ==== platform-specific functions ==== */

// Initialise the co-processor drivers.
// Because this is built-in, this is called from BSP platform init instead of device tree.
esp_err_t bsp_why2025_coproc_init() {
    // Initialise the CH32V203 driver.
    ch32_int_semaphore = xSemaphoreCreateBinary();
    ch32_semaphore     = xSemaphoreCreateBinary();
    xSemaphoreGive(ch32_semaphore);
    if (ch32_int_semaphore == NULL || ch32_semaphore == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(ch32_thread, "CH32 driver", 2048, NULL, 0, &ch32_thread_handle) != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }
    gpio_set_direction(BSP_CH32_IRQ_PIN, GPIO_MODE_INPUT);
    gpio_isr_handler_add(BSP_CH32_IRQ_PIN, ch32_isr, NULL);
    return ESP_OK;
}



// CH32V203 driver thread.
static void ch32_thread(void *arg) {
    (void)arg;
    uint8_t prev_buttons[9] = {0};
    uint8_t buttons[9];
    ESP_LOGI(TAG, "CH32V203 thread started");
    while (true) {
        // Wait for an interrupt to be noticed.
        gpio_intr_enable(BSP_CH32_IRQ_PIN);
        xSemaphoreTake(ch32_int_semaphore, pdMS_TO_TICKS(100));
        if (!ch32_input_dev_id) {
            continue;
        }
        xSemaphoreTake(ch32_semaphore, portMAX_DELAY);

        // When an interrupt occurs, read buttons from I²C.
        esp_err_t res = i2c_master_write_read_device(
            BSP_I2CINT_NUM,
            BSP_CH32_ADDR,
            (uint8_t[]){2}, // I2C_REG_KEYBOARD_0
            1,
            buttons,
            9,
            pdMS_TO_TICKS(50)
        );
        xSemaphoreGive(ch32_semaphore);
        if (res != ESP_OK) {
            continue;
        }

        // Fire button changed events.
        for (int row = 0; row < 9; row++) {
            for (int col = 0; col < 8; col++) {
                if (((buttons[row] & ~prev_buttons[row]) >> col) & 1) {
                    bsp_raw_button_pressed(1, 0, row * 8 + col);
                } else if (((~buttons[row] & prev_buttons[row]) >> col) & 1) {
                    bsp_raw_button_released(1, 0, row * 8 + col);
                }
            }
        }
        memcpy(prev_buttons, buttons, sizeof(buttons));
    }
}

// CH32V203 interrupt handler.
IRAM_ATTR static void ch32_isr(void *arg) {
    (void)arg;
    // Very simple mechanism to notify the CH32 thread.
    gpio_intr_disable(BSP_CH32_IRQ_PIN);
    xSemaphoreGiveFromISR(ch32_int_semaphore, NULL);
    portYIELD_FROM_ISR();
}

esp_err_t ch32_set_display_backlight(uint16_t value) {
    uint8_t buffer[3];
    buffer[0] = 11; // I2C_REG_DISPLAY_BACKLIGHT_0
    buffer[1] = value & 0xFF;
    buffer[2] = value >> 8;
    return i2c_master_write_to_device(BSP_I2CINT_NUM, BSP_CH32_ADDR, buffer, sizeof(buffer), portMAX_DELAY);
}

esp_err_t ch32_set_keyboard_backlight(uint16_t value) {
    uint8_t buffer[3];
    buffer[0] = 13; // I2C_REG_KEYBOARD_BACKLIGHT_0
    buffer[1] = value & 0xFF;
    buffer[2] = value >> 8;
    return i2c_master_write_to_device(BSP_I2CINT_NUM, BSP_CH32_ADDR, buffer, sizeof(buffer), portMAX_DELAY);
}

esp_err_t ch32_get_display_backlight(uint16_t *value) {
    xSemaphoreTake(ch32_semaphore, portMAX_DELAY);
    esp_err_t res = i2c_master_write_read_device(
        BSP_I2CINT_NUM,
        BSP_CH32_ADDR,
        (uint8_t[]){11}, // I2C_REG_DISPLAY_BACKLIGHT_0
        1,
        (void *)value,
        1,
        pdMS_TO_TICKS(50)
    );
    xSemaphoreGive(ch32_semaphore);
    return res;
}

esp_err_t ch32_get_keyboard_backlight(uint16_t *value) {
    xSemaphoreTake(ch32_semaphore, portMAX_DELAY);
    esp_err_t res = i2c_master_write_read_device(
        BSP_I2CINT_NUM,
        BSP_CH32_ADDR,
        (uint8_t[]){13}, // I2C_REG_KEYBOARD_BACKLIGHT_0
        1,
        (void *)value,
        1,
        pdMS_TO_TICKS(50)
    );
    xSemaphoreGive(ch32_semaphore);
    return res;
}

esp_err_t bsp_c6_control(bool enable, bool boot) {
    uint8_t buffer[2];
    buffer[0] = 17; // I2C_REG_RADIO_CONTROL
    buffer[1] = (enable & 1) | ((boot & 1) << 1);
    return i2c_master_write_to_device(BSP_I2CINT_NUM, BSP_CH32_ADDR, buffer, sizeof(buffer), portMAX_DELAY);
}



/* ==== device driver functions ==== */

// GPIO input init function.
bool bsp_input_why2025ch32_init(bsp_device_t *dev, uint8_t endpoint) {
    ch32_input_dev_id = dev->id;
    ch32_input_dev_ep = endpoint;
    return true;
}

// Get current input value by raw input number.
bool bsp_input_why2025ch32_get_raw(bsp_device_t *dev, uint8_t endpoint, uint16_t raw_input) {
    uint8_t buttons = 0;

    // Read buttons from I²C.
    xSemaphoreTake(ch32_semaphore, portMAX_DELAY);
    i2c_master_write_read_device(
        BSP_I2CINT_NUM,
        BSP_CH32_ADDR,
        (uint8_t[]){2 + raw_input / 8}, // I2C_REG_KEYBOARD_0
        1,
        &buttons,
        1,
        pdMS_TO_TICKS(50)
    );
    xSemaphoreGive(ch32_semaphore);

    return (buttons >> (raw_input & 7)) & 1;
}



// Set the color of a single LED from raw data.
void bsp_led_why2025ch32_set_raw(bsp_device_t *dev, uint8_t endpoint, uint16_t led, uint64_t data) {
    if (!led) {
        ch32_set_display_backlight(data);
    } else {
        ch32_set_keyboard_backlight(data);
    }
}

// Get the color of a single LED as raw data.
uint64_t bsp_led_why2025ch32_get_raw(bsp_device_t *dev, uint8_t endpoint, uint16_t led) {
    uint16_t value = 0;
    if (!led) {
        ch32_get_display_backlight(&value);
    } else {
        ch32_get_keyboard_backlight(&value);
    }
    return value;
}

// Send new color data to an LED array.
void bsp_led_why2025ch32_update(bsp_device_t *dev, uint8_t endpoint) {
    (void)dev;
    (void)endpoint;
}
