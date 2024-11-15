
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp_event.h"
#include "bsp_input.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>



// Pre-init function; initialize BSP but not external devices.
// Optional; called implicitly when `bsp_init()` is called.
void bsp_preinit();
// Initialise the BSP, should be called early on in `app_main`.
void bsp_init();

// Get current input value.
bool bsp_input_get(uint32_t dev_id, uint8_t endpoint, bsp_input_t input);
// Get current input value by raw input number.
bool bsp_input_get_raw(uint32_t dev_id, uint8_t endpoint, uint16_t raw_input);
// Set a device's input backlight.
void bsp_input_backlight(uint32_t dev_id, uint8_t endpoint, uint16_t pwm);

// Set the color of a single LED from 16-bit greyscale.
void     bsp_led_set_grey16(uint32_t dev_id, uint8_t endpoint, uint16_t led, uint16_t value);
// Get the color of a single LED as 16-bit greyscale.
uint16_t bsp_led_get_grey16(uint32_t dev_id, uint8_t endpoint, uint16_t led);
// Set the color of a single LED from 8-bit greyscale.
void     bsp_led_set_grey8(uint32_t dev_id, uint8_t endpoint, uint16_t led, uint16_t value);
// Get the color of a single LED as 8-bit greyscale.
uint16_t bsp_led_get_grey8(uint32_t dev_id, uint8_t endpoint, uint16_t led);
// Set the color of a single LED from 48-bit RGB.
void     bsp_led_set_rgb48(uint32_t dev_id, uint8_t endpoint, uint16_t led, uint64_t rgb);
// Get the color of a single LED as 48-bit RGB.
uint64_t bsp_led_get_rgb48(uint32_t dev_id, uint8_t endpoint, uint16_t led);
// Set the color of a single LED from 24-bit RGB.
void     bsp_led_set_rgb(uint32_t dev_id, uint8_t endpoint, uint16_t led, uint32_t rgb);
// Get the color of a single LED as 24-bit RGB.
uint32_t bsp_led_get_rgb(uint32_t dev_id, uint8_t endpoint, uint16_t led);

// Set the color of a single LED from raw data.
void     bsp_led_set_raw(uint32_t dev_id, uint8_t endpoint, uint16_t led, uint64_t data);
// Get the color of a single LED as raw data.
uint64_t bsp_led_get_raw(uint32_t dev_id, uint8_t endpoint, uint16_t led);
// Send new color data to an LED array.
void     bsp_led_update(uint32_t dev_id, uint8_t endpoint);

// Send new image data to a device's display.
void bsp_disp_update(uint32_t dev_id, uint8_t endpoint, void const *framebuffer);
// Send new image data to part of a device's display.
void bsp_disp_update_part(
    uint32_t dev_id, uint8_t endpoint, void const *framebuffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h
);
// Set a device's display backlight.
void bsp_disp_backlight(uint32_t dev_id, uint8_t endpoint, uint16_t pwm);
