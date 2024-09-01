
// SPDX-License-Identifier: MIT

#pragma once

#include "pax_gui.h"



// GUI on close callback.
typedef void (*menu_close_t)(void *cookie);
// GUI menu entry.
typedef struct {
    // Menu's root element.
    pgui_elem_t *root;
    // On close callback.
    menu_close_t on_close;
    // Cookie for on close callback.
    void        *on_close_cookie;
} menu_entry_t;

// Set the top-level menu screen.
void menu_set_root(pgui_elem_t *root);
// Enter new menu screen.
void menu_push(menu_entry_t menu);
// Exit current menu screen.
void menu_pop();
