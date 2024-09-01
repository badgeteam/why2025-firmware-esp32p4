
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
    grid = pgui_new_grid2(1, app_list_len);
    pgui_enable_flags(grid, PGUI_FLAG_NOBACKGROUND | PGUI_FLAG_NOBORDER | PGUI_FLAG_NOSEPARATOR | PGUI_FLAG_NOPADDING);
    pgui_set_pos2(grid, 200, 10);
    for (size_t i = 0; i < app_list_len; i++) {
        pgui_elem_t *button = pgui_new_button(NULL, start_app_cb);
        pgui_child_append(grid, button);
        pgui_set_userdata(button, app_list + i);
        pgui_enable_flags(
            button,
            PGUI_FLAG_FIX_WIDTH | PGUI_FLAG_FIX_HEIGHT | PGUI_FLAG_NOBORDER | PGUI_FLAG_NOPADDING
        );
        pgui_set_size2(button, 400, 38);
        pgui_elem_t *name = pgui_new_text(app_list[i].name ?: app_list[i].id);
        pgui_child_append(button, name);
        pgui_enable_flags(name, PGUI_FLAG_FIX_WIDTH | PGUI_FLAG_FIX_HEIGHT);
        pgui_set_size2(name, 324, 38);
        pgui_set_halign(name, PAX_ALIGN_BEGIN);
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
