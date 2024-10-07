
// SPDX-License-Identifier: MIT

#include "menus/root.h"

#include "main.h"
#include "menus/apps.h"
#include "pax_gui.h"

void camera_test_open(void);

void menu_root_init() {
    pgui_elem_t *grid = pgui_new_grid2(1, 4);
    pgui_enable_flags(grid, PGUI_FLAG_NOBACKGROUND | PGUI_FLAG_NOBORDER | PGUI_FLAG_NOSEPARATOR);
    pgui_child_append(grid, pgui_new_button("Applications", (pgui_callback_t)menu_apps_open));
    pgui_elem_t *button = pgui_new_button("Camera test", (pgui_callback_t)camera_test_open);
    pgui_set_variant(button, PGUI_VARIANT_ACCEPT);
    pgui_child_append(grid, button);
    pgui_elem_t *button2 = pgui_new_button("Useless button 2", NULL);
    pgui_set_variant(button2, PGUI_VARIANT_CANCEL);
    pgui_child_append(grid, button2);
    pgui_child_append(grid, pgui_new_button("LMAO button", NULL));
    menu_set_root((menu_entry_t){
        .root        = grid,
        .bottom_text = "Bottom text",
    });
}
