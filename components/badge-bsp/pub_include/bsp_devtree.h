
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp_color.h"
#include "bsp_input.h"
#include "bsp_keymap.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <refcount.h>



// Display orientation settings.
typedef enum {
    // No change in orientation.
    BSP_O_UPRIGHT,
    // Counter-clockwise rotation.
    BSP_O_ROT_CCW,
    // Half turn rotation.
    BSP_O_ROT_HALF,
    // Clockwise rotation.
    BSP_O_ROT_CW,

    // Flip horizontally.
    BSP_O_FLIP_H,
    // Counter-clockwise rotation then flip horizontally.
    BSP_O_ROT_CCW_FLIP_H,
    // Half turn rotation then flip horizontally.
    BSP_O_ROT_HALF_FLIP_H,
    // Clockwise rotation then flip horizontally.
    BSP_O_ROT_CW_FLIP_H,
} bsp_orient_t;

// Endpoint types.
typedef enum {
    // Input devices.
    BSP_EP_INPUT,
    // LED devices.
    BSP_EP_LED,
    // Display devices.
    BSP_EP_DISP,
    // Audio devices.
    BSP_EP_AUDIO,
    // Number of endpoint types.
    BSP_EP_TYPE_COUNT,
} bsp_ep_type_t;

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
    BSP_BUS_I2C,
    BSP_BUS_SPI,
    BSP_BUS_I2S,
    BSP_BUS_MIPIDSI,
} bsp_bus_type_t;

// Input endpoint types.
typedef enum {
    BSP_EP_INPUT_GPIO,
    BSP_EP_INPUT_WHY2025_CH32,
} bsp_ep_input_type_t;

// LED endpoint types.
typedef enum {
    BSP_EP_LED_WS2812,
    BSP_EP_LED_WHY2025_CH32,
} bsp_ep_led_type_t;

// Display endpoint types.
typedef enum {
    BSP_EP_DISP_ST7701,
    BSP_EP_DISP_EK79007,
} bsp_ep_disp_type_t;

// Audio endpoint types.
typedef enum {
    BSP_EP_AUDIO_ES8156,
} bsp_ep_audio_type_t;

// Common device tree data.
typedef struct bsp_devtree_common  bsp_devtree_common_t;
// GPIO pin mappings.
typedef struct bsp_pinmap          bsp_pinmap_t;
// Input device tree data.
typedef struct bsp_input_devtree   bsp_input_devtree_t;
// Display / LED pixel format.
typedef struct bsp_led_desc        bsp_led_desc_t;
// LED device tree data.
typedef struct bsp_led_devtree     bsp_led_devtree_t;
// Display device tree data.
typedef struct bsp_display_devtree bsp_display_devtree_t;
// Audio device tree data.
typedef struct bsp_audio_devtree   bsp_audio_devtree_t;
// Device descriptor.
typedef union bsp_devtree          bsp_devtree_t;

// Common driver functions.
typedef struct bsp_driver_common bsp_driver_common_t;
// Input driver functions.
typedef struct bsp_input_driver  bsp_input_driver_t;
// LED driver functions.
typedef struct bsp_led_driver    bsp_led_driver_t;
// Display driver functions.
typedef struct bsp_disp_driver   bsp_disp_driver_t;
// Audio driver functions.
typedef struct bsp_audio_driver  bsp_audio_driver_t;

// Device address.
typedef struct bsp_addr   bsp_addr_t;
// Registered device.
typedef struct bsp_device bsp_device_t;

// Device init / deinit functions.
typedef bool (*bsp_dev_initfun_t)(bsp_device_t *dev, uint8_t endpoint);
// Get current input value.
typedef bool (*bsp_input_get_t)(bsp_device_t *dev, uint8_t endpoint, bsp_input_t input);
// Get current input value by raw input number.
typedef bool (*bsp_input_get_raw_t)(bsp_device_t *dev, uint8_t endpoint, uint16_t raw_input);
// Set the color of a single LED from raw data.
typedef void (*bsp_led_set_raw_t)(bsp_device_t *dev, uint8_t endpoint, uint16_t led, uint64_t data);
// Get the color of a single LED as raw data.
typedef uint64_t (*bsp_led_get_raw_t)(bsp_device_t *dev, uint8_t endpoint, uint16_t led);
// Send new color data to an LED array.
typedef void (*bsp_led_update_t)(bsp_device_t *dev, uint8_t endpoint);
// Send new image data to a device's display.
typedef void (*bsp_disp_update_t)(bsp_device_t *dev, uint8_t endpoint, void const *framebuffer);
// Send new image data to part of a device's display.
typedef void (*bsp_disp_update_part_t)(
    bsp_device_t *dev, uint8_t endpoint, void const *framebuffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h
);



// Device address.
struct bsp_addr {
    // Bus type.
    bsp_bus_type_t bus;
    // Controller ID / index.
    uint16_t       controller;
    // Device ID / index.
    uint16_t       device;
};

// Common device tree data.
struct bsp_devtree_common {
    // Initialization priority, lowest happens first.
    int        init_prio;
    // Endpoint address.
    bsp_addr_t addr;
    // Endpoint type used for driver selection.
    int        type;
    // Reset pin number, or -1 if none.
    int        reset_pin;
};

// GPIO pin mappings.
struct bsp_pinmap {
    // Active-low logic.
    bool           activelow;
    // Number of pins.
    uint8_t        pins_len;
    // GPIO pins assigned to raw inputs/outputs.
    uint8_t const *pins;
};

// Input device tree data.
struct bsp_input_devtree {
    // Common device tree data.
    bsp_devtree_common_t common;
    // Input device category.
    bsp_input_cat_t      category;
    // GPIO pin descriptors.
    bsp_pinmap_t const  *pinmap;
    // Button / key map for input devices.
    bsp_keymap_t const  *keymap;
    // Backlight endpoint.
    uint8_t              backlight_endpoint;
    // Backlight index.
    uint16_t             backlight_index;
};

// Display / LED pixel format.
struct bsp_led_desc {
    // Color type.
    bsp_pixfmt_t color;
    // Reverse channel order (e.g. RGB -> BGR).
    bool         reversed;
};

// LED device tree data.
struct bsp_led_devtree {
    // Common device tree data.
    bsp_devtree_common_t common;
    // LED type.
    bsp_led_desc_t       ledfmt;
    // Number of LEDs on this device.
    uint16_t             num_leds;
};

// Display device tree data.
struct bsp_display_devtree {
    // Common device tree data.
    bsp_devtree_common_t common;
    // Backlight endpoint.
    uint8_t              backlight_endpoint;
    // Backlight index.
    uint16_t             backlight_index;
    // Pixel format.
    bsp_led_desc_t       pixfmt;
    // Display orientation.
    bsp_orient_t         orientation;

    // Horizontal front porch width.
    uint16_t h_fp;
    // Display width.
    uint16_t width;
    // Horizontal back porch width.
    uint16_t h_bp;
    // Horizontal sync width.
    uint16_t h_sync;

    // Horizontal front porch height.
    uint16_t v_fp;
    // Display height.
    uint16_t height;
    // Horizontal back porch height.
    uint16_t v_bp;
    // Horizontal sync height.
    uint16_t v_sync;
};

// Audio device tree data.
struct bsp_audio_devtree {
    // Common device tree data.
    bsp_devtree_common_t common;
};

// Device descriptor.
union bsp_devtree {
    struct {
        // Number of input endpoints.
        uint8_t                             input_count;
        // Number of LED endpoints.
        uint8_t                             led_count;
        // Number of display endpoints.
        uint8_t                             disp_count;
        // Number of audio endpoints.
        uint8_t                             audio_count;
        // Input endpoint trees.
        bsp_input_devtree_t const *const   *input_dev;
        // LED endpoint trees.
        bsp_led_devtree_t const *const     *led_dev;
        // Display endpoint trees.
        bsp_display_devtree_t const *const *disp_dev;
        // Audio endpoint trees.
        bsp_audio_devtree_t const *const   *audio_dev;
    };
    struct {
        // Endpoint counts by type.
        uint8_t                            ep_counts[BSP_EP_TYPE_COUNT];
        // Endpoint descriptors by type.
        bsp_devtree_common_t const *const *ep_trees[BSP_EP_TYPE_COUNT];
    };
};



// Common driver functions.
struct bsp_driver_common {
    // Initialize the endpoint.
    bsp_dev_initfun_t init;
    // Deinitialize the endpoint.
    bsp_dev_initfun_t deinit;
};

// Input driver functions.
struct bsp_input_driver {
    // Common driver functions.
    bsp_driver_common_t common;
    // Get current input value.
    bsp_input_get_t     get;
    // Get current input value by raw input number.
    bsp_input_get_raw_t get_raw;
};

// LED driver functions.
struct bsp_led_driver {
    // Common driver functions.
    bsp_driver_common_t common;
    // Set the color of a single LED from raw data.
    bsp_led_set_raw_t   set_raw;
    // Get the color of a single LED as raw data.
    bsp_led_get_raw_t   get_raw;
    // Send new color data to an LED array.
    bsp_led_update_t    update;
};

// Display driver functions.
struct bsp_disp_driver {
    // Common driver functions.
    bsp_driver_common_t    common;
    // Send new image data to a device's display.
    bsp_disp_update_t      update;
    // Send new image data to part of a device's display.
    bsp_disp_update_part_t update_part;
};

// Audio driver functions.
struct bsp_audio_driver {
    // Common driver functions.
    bsp_driver_common_t common;
};

// Registered device.
struct bsp_device {
    // BSP device ID.
    uint32_t id;
    // Device tree.
    rc_t     tree;
    union {
        struct {
            // Drivers for input endpoints.
            bsp_input_driver_t const **input_drivers;
            // Drivers for LED endpoints.
            bsp_led_driver_t const   **led_drivers;
            // Drivers for display endpoints.
            bsp_disp_driver_t const  **disp_drivers;
            // Drivers for audio endpoints.
            bsp_audio_driver_t const **audio_drivers;
        };
        // Drivers per endpoint type.
        bsp_driver_common_t const **ep_drivers[BSP_EP_TYPE_COUNT];
    };
    union {
        struct {
            // Auxiliary driver data for input endpoints.
            void **input_aux;
            // Auxiliary driver data for LED endpoints.
            void **led_aux;
            // Auxiliary driver data for display endpoints.
            void **disp_aux;
            // Auxiliary driver data for audio endpoints.
            void **audio_aux;
        };
        // Auxiliary driver data per endpoint type.
        void **ep_aux[BSP_EP_TYPE_COUNT];
    };
};



// Create a copy of the device tree that can be cleaned up with `free()`.
bsp_devtree_t *bsp_devtree_clone(bsp_devtree_t const *devtree);
