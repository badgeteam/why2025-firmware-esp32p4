
// SPDX-License-Identifier: MIT

#include "bsp/why2025_coproc.h"

#include "bsp.h"

#include <driver/gpio.h>
#include "driver/i2c_master.h"
#include <driver/sdmmc_host.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <sdmmc_cmd.h>
#include <string.h>


static char const TAG[] = "coprocessor";

static SemaphoreHandle_t ch32_int_semaphore;
static SemaphoreHandle_t ch32_semaphore;
static TaskHandle_t      ch32_thread_handle;
static void              ch32_thread();
static void              ch32_isr(void *arg);
static uint32_t          ch32_input_dev_id;
static uint32_t          ch32_input_dev_ep;
static sdmmc_card_t      c6_card;
static uint8_t           c6_cis_buf[256];
static sdmmc_host_t      sdmmc_host = SDMMC_HOST_DEFAULT();
static i2c_master_dev_handle_t dev_handle;


/* ==== platform-specific functions ==== */

// Get the C6 version.
esp_err_t bsp_ch32_version(uint16_t *ver) {
    if (!why2025_enable_i2cint)
        return ESP_ERR_TIMEOUT;
    xSemaphoreTake(ch32_semaphore, portMAX_DELAY);
    esp_err_t res = i2c_master_transmit_receive(dev_handle, (uint8_t[]){0}, 1, (void*)(ver), 2, 100);
    xSemaphoreGive(ch32_semaphore);
    return res;
}

// Initialise the co-processor drivers.
esp_err_t bsp_why2025_coproc_init(i2c_master_bus_handle_t i2c_bus_handle) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BSP_CH32_ADDR,
        .scl_speed_hz = 100000,
    };

    esp_err_t res = i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle);
    if (res != ESP_OK) {
        return res;
    }

    ch32_int_semaphore = xSemaphoreCreateBinary();
    ch32_semaphore     = xSemaphoreCreateBinary();
    xSemaphoreGive(ch32_semaphore);

    if (ch32_int_semaphore == NULL || ch32_semaphore == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(ch32_thread, "CH32 driver", 2048, NULL, 0, &ch32_thread_handle) != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }

    res = gpio_isr_handler_add(BSP_CH32_IRQ_PIN, ch32_isr, NULL);
    if (res != ESP_OK) {
        return res;
    }

    gpio_config_t irq_cfg = {
        .pin_bit_mask = BIT64(BSP_CH32_IRQ_PIN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = false,
        .pull_down_en = false,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    res = gpio_config(&irq_cfg);
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

        // When an interrupt occurs, read buttons from I²C.
        xSemaphoreTake(ch32_semaphore, portMAX_DELAY);
        esp_err_t res = i2c_master_transmit_receive(dev_handle, (uint8_t[]){2}, 1, buttons, 9, 500);

        if (res != ESP_OK) {
            xSemaphoreGive(ch32_semaphore);
            continue;
        }

        // Read input status
        res = i2c_master_transmit_receive(dev_handle, (uint8_t[]){15}, 1, &inputs, 1, 500);

        xSemaphoreGive(ch32_semaphore);
        if (res != ESP_OK) {
            continue;
        }

        // Fire button changed events.
        for (int row = 0; row < 9; row++) {
            for (int col = 0; col < 8; col++) {
                if (((buttons[row] & ~prev_buttons[row]) >> col) & 1) {
                    bsp_raw_button_pressed(ch32_input_dev_id, ch32_input_dev_ep, row * 8 + col);
                } else if (((~buttons[row] & prev_buttons[row]) >> col) & 1) {
                    bsp_raw_button_released(ch32_input_dev_id, ch32_input_dev_ep, row * 8 + col);
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

// Set the display backlight value.
esp_err_t ch32_set_display_backlight(uint16_t value) {
    xSemaphoreTake(ch32_semaphore, portMAX_DELAY);
    uint8_t buffer[3];
    buffer[0] = 11; // I2C_REG_DISPLAY_BACKLIGHT_0
    buffer[1] = value & 0xFF;
    buffer[2] = value >> 8;
    esp_err_t res = i2c_master_transmit(dev_handle, buffer, sizeof(buffer), 100);
    xSemaphoreGive(ch32_semaphore);
    return res;
}

// Set the keyboard backlight value.
esp_err_t ch32_set_keyboard_backlight(uint16_t value) {
    xSemaphoreTake(ch32_semaphore, portMAX_DELAY);
    uint8_t buffer[3];
    buffer[0] = 13; // I2C_REG_KEYBOARD_BACKLIGHT_0
    buffer[1] = value & 0xFF;
    buffer[2] = value >> 8;
    esp_err_t res = i2c_master_transmit(dev_handle, buffer, sizeof(buffer), 100);
    xSemaphoreGive(ch32_semaphore);
    return res;
}

// Get the display backlight value.
esp_err_t ch32_get_display_backlight(uint16_t *value) {
    if (!value)
        return ESP_OK;
    esp_err_t res = i2c_master_transmit_receive(dev_handle, (uint8_t[]){13}, 1, (void*)(value), 2, 100); // I2C_REG_DISPLAY_BACKLIGHT_0
    xSemaphoreGive(ch32_semaphore);
    return res;
}

// Get the keyboard backlight value.
esp_err_t ch32_get_keyboard_backlight(uint16_t *value) {
    if (!value)
        return ESP_OK;
    xSemaphoreTake(ch32_semaphore, portMAX_DELAY);
    esp_err_t res = i2c_master_transmit_receive(dev_handle, (uint8_t[]){13}, 1, (void*)(value), 2, 100);
    xSemaphoreGive(ch32_semaphore);
    return res;
}

// Enable/disable the audio amplifier and camera.
esp_err_t bsp_output_control(bool amplifier, bool camera) {
    xSemaphoreTake(ch32_semaphore, portMAX_DELAY);
    uint8_t buffer[2];
    buffer[0]     = 16; // I2C_REG_AMPLIFIER_ENABLE
    buffer[1]     = (amplifier ? 1 : 0);
    buffer[1]    |= (camera ? 1 : 0) << 1;
    esp_err_t res = i2c_master_transmit(dev_handle, buffer, sizeof(buffer), 100);
    xSemaphoreGive(ch32_semaphore);
    return res;
}

// Enable/disable the ESP32-C6 via RESET and BOOT pins.
esp_err_t bsp_c6_control(bool enable, bool boot) {
    xSemaphoreTake(ch32_semaphore, portMAX_DELAY);
    uint8_t buffer[2];
    buffer[0]     = 17; // I2C_REG_RADIO_CONTROL
    buffer[1]     = (enable & 1) | ((boot & 1) << 1);
    esp_err_t res = i2c_master_transmit(dev_handle, buffer, sizeof(buffer), 100);
    xSemaphoreGive(ch32_semaphore);
    return res;
}

// Initialize the ESP32-C6 after it was (re-)enabled.
esp_err_t bsp_c6_init() {
    // Find ESP32-C6 on SDIO bus.
    sdmmc_host.flags         = SDMMC_HOST_FLAG_4BIT;
    sdmmc_host.max_freq_khz  = SDMMC_FREQ_HIGHSPEED;
    sdmmc_host.flags        |= SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF;
    esp_err_t res;
    while (1) {
        res = sdmmc_card_init(&sdmmc_host, &c6_card);
        if (!res) {
            break;
        }
        ESP_LOGE(TAG, "SDIO error: %s", esp_err_to_name(res));
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Print card info.
    sdmmc_card_print_info(stdout, &c6_card);
    size_t cis_size = 0;
    sdmmc_io_get_cis_data(&c6_card, c6_cis_buf, sizeof(c6_cis_buf), &cis_size);
    sdmmc_io_print_cis_info(c6_cis_buf, cis_size, NULL);

    return ESP_OK;
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
    i2c_master_transmit_receive(dev_handle, (uint8_t[]){2 + raw_input / 8}, 1, &buttons, 1, 100);
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
    // if (!led) {
    //     ch32_get_display_backlight(&value);
    // } else {
    //     ch32_get_keyboard_backlight(&value);
    // }
    return value;
}

// Send new color data to an LED array.
void bsp_led_why2025ch32_update(bsp_device_t *dev, uint8_t endpoint) {
    (void)dev;
    (void)endpoint;
}
