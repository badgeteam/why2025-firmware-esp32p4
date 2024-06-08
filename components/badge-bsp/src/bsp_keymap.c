
// SPDX-License-Identifier: MIT

#include "bsp_keymap.h"

#include "bsp_input.h"


// WHY2025 badge built-in keyboard.
keymap_t const bsp_keymap_why2025 = {
    // The keyboard is a 8x9 grid.
    .max_scancode = 72,
    // Scancode = row*8+col.
    // clang-format off
    .keymap       = (uint16_t const[]){
            /*       col 0       */ /*       col 1       */ /*       col 2       */ /*       col 3       */
            /*       col 4       */ /*       col 5       */ /*       col 6       */ /*       col 7       */
/* row 0 */ BSP_INPUT_ESCAPE,       BSP_INPUT_F1,           BSP_INPUT_F2,           BSP_INPUT_F3,
            BSP_INPUT_KB_TILDA,     BSP_INPUT_KB_1,         BSP_INPUT_KB_2,         BSP_INPUT_KB_3,
/* row 1 */ BSP_INPUT_TAB,          BSP_INPUT_KB_Q,         BSP_INPUT_KB_W,         BSP_INPUT_KB_E,
            BSP_INPUT_FUNCTION,     BSP_INPUT_KB_A,         BSP_INPUT_KB_S,         BSP_INPUT_KB_D,
/* row 2 */ BSP_INPUT_L_SHIFT,      BSP_INPUT_KB_Z,         BSP_INPUT_KB_X,         BSP_INPUT_KB_C,
            BSP_INPUT_L_CTRL,       BSP_INPUT_L_SUPER,      BSP_INPUT_L_ALT,        BSP_INPUT_KB_BACKSLASH,
/* row 3 */ BSP_INPUT_KB_4,         BSP_INPUT_KB_5,         BSP_INPUT_KB_6,         BSP_INPUT_KB_7,
            BSP_INPUT_KB_R,         BSP_INPUT_KB_T,         BSP_INPUT_KB_Y,         BSP_INPUT_KB_U,
/* row 4 */ BSP_INPUT_KB_F,         BSP_INPUT_KB_G,         BSP_INPUT_KB_H,         BSP_INPUT_KB_J,
            BSP_INPUT_KB_V,         BSP_INPUT_KB_B,         BSP_INPUT_KB_N,         BSP_INPUT_KB_M,
/* row 5 */ BSP_INPUT_F4,           BSP_INPUT_F5,           BSP_INPUT_F6,           BSP_INPUT_BACKSPACE,
            BSP_INPUT_KB_9,         BSP_INPUT_KB_0,         BSP_INPUT_KB_MINUS,     BSP_INPUT_KB_EQUALS,
/* row 6 */ BSP_INPUT_KB_O,         BSP_INPUT_KB_P,         BSP_INPUT_KB_LBRAC,     BSP_INPUT_KB_RBRAC,
            BSP_INPUT_KB_L,         BSP_INPUT_KB_SEMICOLON, BSP_INPUT_KB_QUOTE,     BSP_INPUT_ENTER,
/* row 7 */ BSP_INPUT_KB_DOT,       BSP_INPUT_KB_SLASH,     BSP_INPUT_UP,           BSP_INPUT_R_SHIFT,
            BSP_INPUT_R_ALT,        BSP_INPUT_LEFT,         BSP_INPUT_DOWN,         BSP_INPUT_RIGHT,
/* row 8 */ BSP_INPUT_KB_8,         BSP_INPUT_KB_I,         BSP_INPUT_KB_K,         BSP_INPUT_KB_COMMA,
            BSP_INPUT_SPACE,        BSP_INPUT_SPACE,        BSP_INPUT_SPACE,        BSP_INPUT_F7,
    },
    // clang-format on
};
