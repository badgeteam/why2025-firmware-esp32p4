
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

// Supported bus types.
typedef enum {
    // MIPI DSI.
    BSP_ADDR_BUS_DSI
} bsp_bus_type_t;

// Display types.
typedef enum {
    BSP_DISP_ST7701,
} bsp_disp_type_t;

// Display / LED format descriptor.
typedef enum {
    // Black/red epaper display.
    BSP_LED_2_11KR,

    // 1-bit greyscale display.
    BSP_LED_1_GREY,
    // 2-bit greyscale display.
    BSP_LED_2_GREY,
    // 4-bit greyscale display.
    BSP_LED_4_GREY,
    // 8-bit greyscale display.
    BSP_LED_8_GREY,

    // 8-bit RGB display.
    BSP_LED_8_332RGB,
    // 16-bit RGB display.
    BSP_LED_16_565RGB,
    // 24-bit RGB display.
    BSP_LED_24_888RGB,
} bsp_led_col_t;

// Get the bits per pixel for the given LED type.
#define BSP_LED_GET_BPP(type)    ((type) & 0xff)
// Reflects whether the LED type is greyscale.
#define BSP_LED_IS_GREY(type)    (((type) & 0xf0000000) == 0x10000000)
// Reflects whether the LED type is paletted.
#define BSP_LED_IS_PALETTE(type) (((type) & 0xf0000000) == 0x20000000)
// Reflects whether the LED type is color.
#define BSP_LED_IS_COLOR(type)   (((type) & 0xf0000000) == 0x00000000)

// Input device.
typedef struct bsp_input_devtree   bsp_input_devtree_t;
// Display / LED pixel format.
typedef struct bsp_led_desc        bsp_led_desc_t;
// LED device.
typedef struct bsp_led_devtree     bsp_led_devtree_t;
// Display device.
typedef struct bsp_display_devtree bsp_display_devtree_t;
// Device descriptor.
typedef struct bsp_devtree         bsp_devtree_t;

// Input driver functions.
typedef struct bsp_input_driver bsp_input_driver_t;
// Display driver functions.
typedef struct bsp_disp_driver  bsp_disp_driver_t;
// Device address.
typedef struct bsp_addr         bsp_addr_t;
// Registered device.
typedef struct bsp_device       bsp_device_t;

// Device init / deinit functions.
typedef bool (*bsp_dev_initfun_t)(bsp_device_t *dev);
// Get current input value.
typedef bool (*bsp_input_get_t)(bsp_device_t *dev, bsp_input_t input);
// Get current input value by raw input number.
typedef bool (*bsp_input_get_raw_t)(bsp_device_t *dev, uint16_t raw_input);
// Set a device's input backlight.
typedef void (*bsp_input_backlight_t)(bsp_device_t *dev, uint16_t pwm);
// Send new image data to a device's display.
typedef void (*bsp_disp_update_t)(bsp_device_t *dev, void const *framebuffer);
// Send new image data to part of a device's display.
typedef void (*bsp_disp_update_part_t)(
    bsp_device_t *dev, void const *framebuffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h
);
// Set a device's display backlight.
typedef void (*bsp_disp_backlight_t)(bsp_device_t *dev, uint16_t pwm);



// Input device.
struct bsp_input_devtree {
    // Initialization priority, lowest happens first.
    int             init_prio;
    // Input device category.
    bsp_input_cat_t type;
    // Button / key map for input devices.
    keymap_t const *keymap;
    // Whether this device has a backlight.
    bool            has_backlight;
    // Number of player indicators.
    uint8_t         players_len;
};

// Display / LED pixel format.
struct bsp_led_desc {
    // Color type.
    bsp_led_col_t color;
    // Reverse channel order (e.g. RGB -> BGR).
    bool          reversed;
};

// LED device.
struct bsp_led_devtree {
    // Initialization priority, lowest happens first.
    int            init_prio;
    // LED type.
    bsp_led_desc_t ledfmt;
    // Number of LEDs on this device.
    uint16_t       num_leds;
};

// Display device.
struct bsp_display_devtree {
    // Initialization priority, lowest happens first.
    int             init_prio;
    // Display type.
    bsp_disp_type_t type;
    // Pixel format.
    bsp_led_desc_t  pixfmt;

    // Horizontal front porch width.
    uint8_t  h_fp;
    // Display width.
    uint16_t width;
    // Horizontal back porch width.
    uint8_t  h_bp;
    // Horizontal sync width.
    uint8_t  h_sync;

    // Horizontal front porch height.
    uint8_t  v_fp;
    // Display height.
    uint16_t height;
    // Horizontal back porch height.
    uint8_t  v_bp;
    // Horizontal sync height.
    uint8_t  v_sync;
};

// Device descriptor.
struct bsp_devtree {
    // Input device.
    bsp_input_devtree_t const   *input_dev;
    // LED string device.
    bsp_led_devtree_t const     *led_dev;
    // Display device.
    bsp_display_devtree_t const *disp_dev;
};



// Input driver functions.
struct bsp_input_driver {
    // Initialize the input device.
    bsp_dev_initfun_t     init;
    // Deinitialize the input device.
    bsp_dev_initfun_t     deinit;
    // Get current input value.
    bsp_input_get_t       get;
    // Get current input value by raw input number.
    bsp_input_get_raw_t   get_raw;
    // Set a device's input backlight.
    bsp_input_backlight_t backlight;
};

// Display driver functions.
struct bsp_disp_driver {
    // Initialize the display.
    bsp_dev_initfun_t      init;
    // Deinitialize the display.
    bsp_dev_initfun_t      deinit;
    // Send new image data to a device's display.
    bsp_disp_update_t      update;
    // Send new image data to part of a device's display.
    bsp_disp_update_part_t update_part;
    // Set a device's display backlight.
    bsp_disp_backlight_t   backlight;
};

// Device address.
struct bsp_addr {
    // Bus type.
    bsp_bus_type_t bus;
    // Controller ID / index.
    uint16_t       controller;
    // Device ID / index.
    uint16_t       device;
};

// Registered device.
struct bsp_device {
    // BSP device ID.
    uint32_t                  id;
    // Device tree.
    bsp_devtree_t const      *tree;
    // Drivers for input devices.
    bsp_input_driver_t const *input_driver;
    // Drivers for display devices.
    bsp_disp_driver_t const  *disp_driver;
    // Auxiliary driver data.
    void                     *input_aux, *disp_aux;
};



// Register a new device and assign an ID to it.
uint32_t bsp_dev_register(bsp_devtree_t const *tree);
// Unregister an existing device.
bool     bsp_dev_unregister(uint32_t dev_id);

// Get current input value.
bool bsp_input_get(uint32_t dev_id, bsp_input_t input);
// Get current input value by raw input number.
bool bsp_input_get_raw(uint32_t dev_id, uint16_t raw_input);
// Set a device's input backlight.
void bsp_input_backlight(uint32_t dev_id, uint16_t pwm);
// Send new image data to a device's display.
void bsp_disp_update(uint32_t dev_id, void const *framebuffer);
// Send new image data to part of a device's display.
void bsp_disp_update_part(uint32_t dev_id, void const *framebuffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
// Set a device's display backlight.
void bsp_disp_backlight(uint32_t dev_id, uint16_t pwm);
