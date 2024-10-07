
// SPDX-License-Identifier: MIT

#pragma once

#include "pax_gui.h"

#define EXAMPLE_RGB565_BITS_PER_PIXEL           16
#define EXAMPLE_MIPI_IDI_CLOCK_RATE             (50000000)
#define EXAMPLE_MIPI_CSI_LANE_BITRATE_MBPS      200 //line_rate = pclk * 4

// GUI on close callback.
typedef void (*menu_close_t)(void *cookie);
// GUI menu entry.
typedef struct {
    // Menu's root element.
    pgui_elem_t *root;
    // Hide top bar.
    bool         hide_top;
    // Bottom bar text.
    char const  *bottom_text;
    // On close callback.
    menu_close_t on_close;
    // Cookie for on close callback.
    void        *on_close_cookie;
} menu_entry_t;

// Set the top-level menu screen.
void menu_set_root(menu_entry_t root);
// Enter new menu screen.
void menu_push(menu_entry_t menu);
// Exit current menu screen.
void menu_pop();
