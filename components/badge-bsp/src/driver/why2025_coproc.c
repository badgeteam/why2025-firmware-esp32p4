
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
    gpio_set_direction(BSP_CH32_IRQ_PIN, GPIO_MODE_INPUT);
    gpio_isr_handler_add(BSP_CH32_IRQ_PIN, ch32_isr, NULL);
    return ESP_OK;
}



// CH32V203 driver thread.
static void ch32_thread() {
    uint8_t prev_buttons[9] = {0};
    uint8_t buttons[9];
    ESP_LOGI(TAG, "CH32V203 thread started");
    while (true) {
        // Wait for an interrupt to be noticed.
        gpio_intr_enable(BSP_CH32_IRQ_PIN);
        xSemaphoreTake(ch32_int_semaphore, pdMS_TO_TICKS(100));
        xSemaphoreTake(ch32_semaphore, portMAX_DELAY);

        // When an interrupt occurs, read buttons from I²C.
        esp_err_t res = i2c_master_write_read_device(
            BSP_I2CINT_NUM,
            BSP_CH32_ADDR,
            (uint8_t[]){0},
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
                    bsp_raw_button_pressed(0, row * 8 + col);
                } else if (((~buttons[row] & prev_buttons[row]) >> col) & 1) {
                    bsp_raw_button_released(0, row * 8 + col);
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
