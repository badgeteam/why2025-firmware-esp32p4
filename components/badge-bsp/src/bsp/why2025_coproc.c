
// SPDX-License-Identifier: MIT

#include "bsp/why2025_coproc.h"

#include "bsp.h"
#include "driver/i2c_master.h"
#include "hardware/why2025.h"
#include "tanmatsu_coprocessor.h"

#include <driver/gpio.h>
#include <driver/sdmmc_host.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <sdmmc_cmd.h>
#include <string.h>


static char const TAG[] = "coprocessor";

SemaphoreHandle_t             i2c_concurrency_semaphore = NULL;
tanmatsu_coprocessor_handle_t coprocessor_handle        = NULL;
static uint32_t          ch32_input_dev_id;
static uint32_t          ch32_input_dev_ep;
static sdmmc_card_t      c6_card;
static uint8_t           c6_cis_buf[256];
static sdmmc_host_t      sdmmc_host = SDMMC_HOST_DEFAULT();



/* ==== platform-specific functions ==== */

// Get the CH32 version.
esp_err_t bsp_ch32_version(uint16_t *ver) {
    return tanmatsu_coprocessor_get_firmware_version(coprocessor_handle, ver);
}

void coprocessor_keyboard_callback(
    tanmatsu_coprocessor_handle_t handle, tanmatsu_coprocessor_keys_t *prev_keys, tanmatsu_coprocessor_keys_t *keys
) {
    uint8_t *buttons      = keys->raw;
    uint8_t *prev_buttons = prev_keys->raw;

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
}

void coprocessor_input_callback(
    tanmatsu_coprocessor_handle_t  handle,
    tanmatsu_coprocessor_inputs_t *prev_inputs,
    tanmatsu_coprocessor_inputs_t *inputs
) {
    if ((inputs->sd_card_detect) != (prev_inputs->sd_card_detect)) {
        // SD card detect changed
        ESP_LOGW(TAG, "SD card %s", (inputs->sd_card_detect) ? "inserted" : "removed");
    }

    if ((inputs->headphone_detect) != (prev_inputs->headphone_detect)) {
        // Headphone detect changed
        ESP_LOGW(TAG, "Audio jack %s", (inputs->headphone_detect) ? "inserted" : "removed");
    }
}

// Initialise the co-processor drivers.
esp_err_t bsp_why2025_coproc_init() {
    esp_err_t res;

    i2c_concurrency_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(i2c_concurrency_semaphore);

    i2c_master_bus_config_t i2c_master_config = {
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .i2c_port                     = BSP_I2CINT_NUM,
        .scl_io_num                   = BSP_I2CINT_SCL_PIN,
        .sda_io_num                   = BSP_I2CINT_SDA_PIN,
        .glitch_ignore_cnt            = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t i2c_bus_handle = NULL;

    res = i2c_new_master_bus(&i2c_master_config, &i2c_bus_handle);
    if (res != ESP_OK) {
        return res;
    }

    tanmatsu_coprocessor_config_t coprocessor_config = {
        .int_io_num            = BSP_CH32_IRQ_PIN,
        .i2c_bus               = i2c_bus_handle,
        .i2c_address           = BSP_CH32_ADDR,
        .concurrency_semaphore = i2c_concurrency_semaphore,
        .on_keyboard_change    = coprocessor_keyboard_callback,
        .on_input_change       = coprocessor_input_callback,
    };

    res = tanmatsu_coprocessor_initialize(&coprocessor_config, &coprocessor_handle);
    if (res != ESP_OK) {
        return res;
    }

    return res;
}

// Set the display backlight value.
esp_err_t ch32_set_display_backlight(uint8_t value) {
    return tanmatsu_coprocessor_set_display_backlight(coprocessor_handle, value);
}

// Set the keyboard backlight value.
esp_err_t ch32_set_keyboard_backlight(uint8_t value) {
    return tanmatsu_coprocessor_set_keyboard_backlight(coprocessor_handle, value);
}

// Get the display backlight value.
esp_err_t ch32_get_display_backlight(uint8_t *value) {
    return tanmatsu_coprocessor_get_display_backlight(coprocessor_handle, value);
}

// Get the keyboard backlight value.
esp_err_t ch32_get_keyboard_backlight(uint8_t *value) {
    return tanmatsu_coprocessor_get_keyboard_backlight(coprocessor_handle, value);
}

// Enable/disable the audio amplifier.
esp_err_t bsp_amplifier_control(bool enable) {
    return tanmatsu_coprocessor_set_amplifier_enable(coprocessor_handle, enable);
}

// Enable/disable the ESP32-C6 via RESET and BOOT pins.
esp_err_t bsp_c6_control(bool enable, bool boot) {
    return tanmatsu_coprocessor_set_radio_state(coprocessor_handle, (enable & 1) | ((boot & 1) << 1));
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
    tanmatsu_coprocessor_keys_t keys;
    tanmatsu_coprocessor_get_keyboard_keys(coprocessor_handle, &keys);

		uint16_t row = raw_input / 8;
		uint16_t col = raw_input % 8;

    return (keys.raw[row] >> col) & 1;
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
