
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>



// Keyboard scancode to input map.
typedef struct {
    // Maximum scan code.
    uint16_t        max_scancode;
    // Scan code to BSP input table.
    uint16_t const *keymap;
} bsp_keymap_t;

// WHY2025 badge built-in keyboard.
extern bsp_keymap_t const bsp_keymap_why2025;
