
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>
#include "bsp_input.h"


// Keyboard scancode to input map.
typedef struct {
    // Maximum scan code.
    uint16_t        max_scancode;
    // Scan code to BSP input table.
    uint16_t const *keymap;
} keymap_t;

// WHY2025 badge built-in keyboard.
extern keymap_t const bsp_keymap_why2025;

bsp_input_t why2025_map_keys(uint32_t scancode);
