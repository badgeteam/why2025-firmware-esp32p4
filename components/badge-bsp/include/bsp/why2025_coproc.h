
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp_device.h"
#include "esp_err.h"
#include "hardware/why2025.h"

#include <stdbool.h>
#include <stdint.h>



/* ==== platform-specific functions ==== */

// Enable the WHY2025 badge internal I²C bus.
extern bool why2025_enable_i2cint;

// Get the C6 version.
esp_err_t bsp_ch32_version(uint16_t *ver);
// Initialise the co-processor drivers.
esp_err_t bsp_why2025_coproc_init();

// Set the display backlight value.
esp_err_t ch32_set_display_backlight(uint16_t value);
// Set the keyboard backlight value.
esp_err_t ch32_set_keyboard_backlight(uint16_t value);
// Get the display backlight value.
esp_err_t ch32_get_display_backlight(uint16_t *value);
// Get the keyboard backlight value.
esp_err_t ch32_get_keyboard_backlight(uint16_t *value);
// Enable/disable the audio amplifier.
esp_err_t bsp_amplifier_control(bool enable);
// Enable/disable the ESP32-C6 via RESET and BOOT pins.
esp_err_t bsp_c6_control(bool enable, bool boot);
// Initialize the ESP32-C6 after it was (re-)enabled.
esp_err_t bsp_c6_init();



/* ==== device driver functions ==== */

// GPIO input init function.
bool bsp_input_why2025ch32_init(bsp_device_t *dev, uint8_t endpoint);
// Get current input value by raw input number.
bool bsp_input_why2025ch32_get_raw(bsp_device_t *dev, uint8_t endpoint, uint16_t raw_input);

// Set the color of a single LED from raw data.
// LED 0 is display, LED 1 is keyboard.
void     bsp_led_why2025ch32_set_raw(bsp_device_t *dev, uint8_t endpoint, uint16_t led, uint64_t data);
// Get the color of a single LED as raw data.
// LED 0 is display, LED 1 is keyboard.
uint64_t bsp_led_why2025ch32_get_raw(bsp_device_t *dev, uint8_t endpoint, uint16_t led);
// Send new color data to an LED array.
void     bsp_led_why2025ch32_update(bsp_device_t *dev, uint8_t endpoint);
