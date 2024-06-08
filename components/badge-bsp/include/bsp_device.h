
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp_input.h"
#include "bsp_keymap.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>



// Input device category.
typedef enum {
    // Generic input devices.
    BSP_INPUT_CAT_GENERIC,
    // Keyboard / numpad devices.
    BSP_INPUT_CAT_KEYBOARD,
    // Game controller devices.
    BSP_INPUT_CAT_CONTROLLER,
} bsp_input_cat_t;

// Input device.
typedef struct {
    // Input device category.
    bsp_input_cat_t type;
    // Button / key map for input devices.
    keymap_t const *keymap;
    // Whether this device has a backlight.
    bool            has_backlight;
    // Number of player indicators.
    uint8_t         players_len;
} bsp_input_devtree_t;

// Display / LED channel color
typedef enum {
    BSP_COLOR_BLACK,
    BSP_COLOR_RED,
    BSP_COLOR_GREEN,
    BSP_COLOR_YELLOW,
    BSP_COLOR_BLUE,
    BSP_COLOR_MAGENTA,
    BSP_COLOR_CYAN,
    BSP_COLOR_WHITE,
} __attribute__((packed)) bsp_color_t;

// Display / LED channels descriptor.
typedef struct {
    // Number of channels per pixel / LED, max. 3.
    uint8_t     channels;
    // Number of bits per channel.
    uint8_t     bits_per_channel[3];
    // Colors per channel.
    bsp_color_t channel_colors[3];
} bsp_led_desc_t;

// LED device.
typedef struct {
    // LED type.
    bsp_led_desc_t ledfmt;
    // Number of LEDs on this device.
    uint16_t       num_leds;
} bsp_led_devtree_t;

// Display device.
typedef struct {
    // Pixel format.
    bsp_led_desc_t pixfmt;
    // Display width.
    uint16_t       width;
    // Display height.
    uint16_t       height;
} bsp_display_devtree_t;

// Device descriptor.
typedef struct {
    // Input device.
    bsp_input_devtree_t const   *input_dev;
    // LED string device.
    bsp_led_devtree_t const     *led_dev;
    // Display device.
    bsp_display_devtree_t const *disp_dev;
} bsp_devtree_t;

// Registered device.
typedef struct {
    // BSP device ID.
    uint32_t             id;
    // Device tree.
    bsp_devtree_t const *tree;
    // Auxiliary driver data.
    void                *input_aux, *led_aux, *disp_aux;
} bsp_device_t;



// Register a new device and assign an ID to it.
void bsp_dev_register(bsp_device_t *dev);
// Unregister an existing device.
void bsp_dev_unregister(uint32_t dev_id);

// Get current input value.
bool bsp_input_get(uint32_t dev_id, bsp_input_t input);
// Get current input value by raw input number.
bool bsp_input_get_raw(uint32_t dev_id, uint16_t raw_input);
// Set a device's input backlight.
void bsp_input_backlight(uint32_t dev_id, uint8_t pwm);
// Send new image data to a device's display.
void bsp_disp_update(uint32_t dev_id, void const *framebuffer);
// Send new image data to part of a device's display.
void bsp_disp_update_part(uint32_t dev_id, void const *framebuffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
// Set a device's display backlight.
void bsp_disp_backlight(uint32_t dev_id, uint8_t pwm);
