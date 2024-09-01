
// SPDX-License-Identifier: MIT

#include "menus/apps.h"

#include "app.h"
#include "main.h"
#include "pax_gui.h"



static pgui_elem_t *grid;

static void start_app_cb(pgui_elem_t *elem) {
    app_start(pgui_get_userdata(elem));
}

void menu_apps_open() {
    app_detect();
    grid = pgui_new_grid2(2, app_list_len);
    pgui_enable_flags(grid, PGUI_FLAG_NOBACKGROUND | PGUI_FLAG_NOBORDER | PGUI_FLAG_NOSEPARATOR);
    for (size_t i = 0; i < app_list_len; i++) {
        pgui_elem_t *button = pgui_new_button("start", start_app_cb);
        pgui_set_userdata(button, app_list + i);
        pgui_child_append(grid, pgui_new_text(app_list[i].id));
        pgui_child_append(grid, button);
    }
    menu_push((menu_entry_t){
        .root     = grid,
        .on_close = (menu_close_t)menu_apps_close,
    });
}

void menu_apps_close() {
    pgui_delete_recursive(grid);
    app_list_clear();
}
