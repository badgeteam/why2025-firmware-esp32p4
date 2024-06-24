
// SPDX-License-Identifier: MIT

#include "bsp/why2025_coproc.h"

#include "bsp.h"

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
static void              ch32_thread();
static void              ch32_isr(void *arg);

// Initialise the co-processor drivers.
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

    esp_err_t res = gpio_isr_handler_add(BSP_CH32_IRQ_PIN, ch32_isr, NULL);
    if (res != ESP_OK) {
        return res;
    }

    gpio_config_t sao_cfg = {
        .pin_bit_mask = BIT64(BSP_CH32_IRQ_PIN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = false,
        .pull_down_en = false,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    res = gpio_config(&sao_cfg);
    if (res != ESP_OK) {
        return res;
    }

    xSemaphoreGive(ch32_int_semaphore); // Workaround for firmware bug in CH32V203 firmware, will be fixed soon

    return res;
}



// CH32V203 driver thread.
static void ch32_thread() {
    uint8_t prev_buttons[9] = {0};
    uint8_t buttons[9];
    uint8_t prev_inputs = 0;
    uint8_t inputs;
    ESP_LOGI(TAG, "CH32V203 thread started");
    while (true) {
        // Wait for an interrupt to be noticed.
        xSemaphoreTake(ch32_int_semaphore, portMAX_DELAY);
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

        if (res != ESP_OK) {
            xSemaphoreGive(ch32_semaphore);
            continue;
        }

        // Read input status
        res = i2c_master_write_read_device(
            BSP_I2CINT_NUM,
            BSP_CH32_ADDR,
            (uint8_t[]){15}, // I2C_REG_INPUT
            1,
            &inputs,
            1,
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
                    bsp_raw_button_pressed(0, row * 8 + col);
                } else if (((~buttons[row] & prev_buttons[row]) >> col) & 1) {
                    bsp_raw_button_released(0, row * 8 + col);
                }
            }
        }

        if ((inputs & 1) != (prev_inputs & 1)) {
            // SD card detect changed
            ESP_LOGW(TAG, "SD card %s", (inputs & 1) ? "inserted" : "removed");
        }

        if ((inputs & 2) != (prev_inputs & 2)) {
            // Headphone detect changed
            ESP_LOGW(TAG, "Audio jack %s", (inputs & 2) ? "inserted" : "removed");
        }

        prev_inputs = inputs;

        memcpy(prev_buttons, buttons, sizeof(buttons));
    }
}

// CH32V203 interrupt handler.
IRAM_ATTR static void ch32_isr(void *arg) {
    (void)arg;
    xSemaphoreGiveFromISR(ch32_int_semaphore, NULL);
    portYIELD_FROM_ISR();
}

esp_err_t ch32_set_display_backlight(uint16_t value) {
    uint8_t buffer[3];
    buffer[0] = 11; // I2C_REG_DISPLAY_BACKLIGHT_0
    buffer[1] = value & 0xFF;
    buffer[2] = value >> 8;
    return i2c_master_write_to_device(BSP_I2CINT_NUM, BSP_CH32_ADDR, buffer, sizeof(buffer), pdMS_TO_TICKS(50));
}

esp_err_t ch32_set_keyboard_backlight(uint16_t value) {
    uint8_t buffer[3];
    buffer[0] = 13; // I2C_REG_KEYBOARD_BACKLIGHT_0
    buffer[1] = value & 0xFF;
    buffer[2] = value >> 8;
    return i2c_master_write_to_device(BSP_I2CINT_NUM, BSP_CH32_ADDR, buffer, sizeof(buffer), pdMS_TO_TICKS(50));
}

esp_err_t bsp_c6_control(bool enable, bool boot) {
    uint8_t buffer[2];
    buffer[0] = 17; // I2C_REG_RADIO_CONTROL
    buffer[1] = (enable & 1) | ((boot & 1) << 1);
    return i2c_master_write_to_device(BSP_I2CINT_NUM, BSP_CH32_ADDR, buffer, sizeof(buffer), pdMS_TO_TICKS(50));
}


esp_err_t bsp_amplifier_control(bool enable) {
    uint8_t buffer[2];
    buffer[0] = 16; // I2C_REG_AMPLIFIER_ENABLE
    buffer[1] = enable ? 1 : 0;
    return i2c_master_write_to_device(BSP_I2CINT_NUM, BSP_CH32_ADDR, buffer, sizeof(buffer), pdMS_TO_TICKS(50));
}
