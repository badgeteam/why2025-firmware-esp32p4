
// SPDX-License-Identifier: MIT

#include "main.h"

#include "appfs2.h"
#include "arrays.h"
#include "bsp.h"
#include "bsp/why2025_coproc.h"
#include "bsp_device.h"
#include "bsp_pax.h"
#include "ch32v203prog.h"
#include "menus/root.h"
#include "pax_gfx.h"
#include "pax_gui.h"

#include <stdio.h>

#include <esp_app_desc.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/stat.h>

char const TAG[] = "main";

#define MAX_MENU_DEPTH 16



extern uint8_t const ch32_firmware_start[] asm("_binary_ch32_firmware_bin_start");
extern uint8_t const ch32_firmware_end[] asm("_binary_ch32_firmware_bin_end");

// Current GUI root element.
static pgui_elem_t *gui;
// GUI top bar.
static pgui_elem_t *top_bar;
// GUI bottom bar.
static pgui_elem_t *bottom_bar;
// Global framebuffer.
static pax_buf_t   *gfx;
// Top-level menu screen GUI root element.
static menu_entry_t root_menu;
// Menu stack.
static menu_entry_t menu_stack[MAX_MENU_DEPTH];
// Number of menus on the stack.
static size_t       menu_stack_len;
// Previsous value of `menu_stack_len`; used to clean up.
static size_t       menu_stack_prev;
// Need to update active menu.
static bool         menu_change = true;



// Set current screen.
static void menu_enable(menu_entry_t menu) {
    if (menu.hide_top) {
        pgui_enable_flags(top_bar, PGUI_FLAG_HIDDEN);
    } else {
        pgui_disable_flags(top_bar, PGUI_FLAG_HIDDEN);
    }
    if (menu.bottom_text) {
        pgui_set_text(bottom_bar, menu.bottom_text);
        pgui_disable_flags(bottom_bar, PGUI_FLAG_HIDDEN);
    } else {
        pgui_enable_flags(bottom_bar, PGUI_FLAG_HIDDEN);
    }
    pgui_enable_flags(menu.root, PGUI_FLAG_TOPLEVEL);
    pgui_child_replace(gui, 1, menu.root);
    pgui_set_selection(gui, 1);
}

// Set the top-level menu screen.
void menu_set_root(menu_entry_t root) {
    root_menu = root;
}

// Enter new menu screen.
void menu_push(menu_entry_t menu) {
    if (menu_stack_len >= MAX_MENU_DEPTH) {
        ESP_LOGE(TAG, "Menu stack is full! Please increase MAX_MENU_DEPTH.");
        return;
    }
    menu_stack[menu_stack_len++] = menu;
    menu_change                  = true;
}

// Exit current menu screen.
void menu_pop() {
    if (menu_stack_len) {
        menu_stack_len--;
        menu_change = true;
    }
}



void app_main(void) {
    esp_err_t res;
    bsp_preinit();

    // Read CH32 version.
    uint16_t version = 0xffff;
    res              = bsp_ch32_version(&version);
    if (res) {
        ESP_LOGW(TAG, "Unable to read CH32 version");
    } else if (version != BSP_CH32_VERSION) {
        ESP_LOGI(
            TAG,
            "CH32 version 0x%04" PRIx16 " too %s (expected %04" PRIx16 ")",
            version,
            version < BSP_CH32_VERSION ? "old" : "new",
            BSP_CH32_VERSION
        );
    } else {
        ESP_LOGI(TAG, "CH32 version 0x%04" PRIx16, version);
    }

    // Program the CH32 if there is a mismatch.
    if (res || version != BSP_CH32_VERSION) {
        ESP_LOGI(TAG, "Programming CH32");
        rvswd_handle_t handle = {
            .swdio = 22,
            .swclk = 23,
        };
        ch32_program(&handle, ch32_firmware_start, ch32_firmware_end - ch32_firmware_start);
        esp_restart();
    }

    // Initialize the hardware.
    bsp_init();
    gfx = pax_pax_buf_from_ep(1, 0);
    if (!gfx) {
        ESP_LOGE(TAG, "Failed to create framebuffer");
        esp_restart();
    }

    // if (mkdir("/int/apps", 0777)) {
    //     ESP_LOGE(TAG, "No /int/apps :c");
    //     return;
    // }
    // if (mkdir("/int/apps/app_ok", 0777)) {
    //     ESP_LOGE(TAG, "No /int/apps/app_ok :c");
    //     return;
    // }
    // FILE *fd = fopen("/int/apps/app_ok/meta.json", "w");
    // if (!fd) {
    //     ESP_LOGE(TAG, "No FD :c");
    //     return;
    // }
    // fputs(
    //     "{\n    \"type\": \"AppFS2\",\n    \"name\": \"Test application\",\n    \"desc\": \"This is a simple "
    //     "metadata entry for testing the application loader\",\n    \"main\": \"/appfs/test\",\n    \"icon\":""
    //     "\"icon.png\"\n}", fd
    // );
    // fclose(fd);
    // return;

    // Initialize AppFS so the app launcher can use it.
    appfsInit(APPFS_PART_TYPE, APPFS_PART_SUBTYPE);

    // Compose top-level GUI.
    gui = pgui_new_grid2(1, 3);
    pgui_enable_flags(
        gui,
        PGUI_FLAG_NOPADDING | PGUI_FLAG_NOBORDER | PGUI_FLAG_NOSEPARATOR | PGUI_FLAG_FIX_WIDTH | PGUI_FLAG_FIX_HEIGHT
    );
    pgui_set_size(gui, pax_buf_get_dims(gfx));
    pgui_set_row_growable(gui, 0, false);
    pgui_set_row_growable(gui, 2, false);
    {
        // Create top bar.
        top_bar = pgui_new_grid2(2, 1);
        pgui_set_variant(top_bar, PGUI_VARIANT_PANEL);
        pgui_enable_flags(top_bar, PGUI_FLAG_NOBORDER | PGUI_FLAG_NOSEPARATOR | PGUI_FLAG_NOROUNDING);
        pgui_child_replace(gui, 0, top_bar);
        {
            // Misc decor.
            pgui_elem_t *top_left = pgui_new_text("13:37");
            pgui_set_variant(top_left, PGUI_VARIANT_PANEL);
            pgui_set_halign(top_left, PAX_ALIGN_BEGIN);
            pgui_child_replace(top_bar, 0, top_left);

            pgui_elem_t *top_right = pgui_new_text("42%");
            pgui_set_variant(top_right, PGUI_VARIANT_PANEL);
            pgui_set_halign(top_right, PAX_ALIGN_END);
            pgui_child_replace(top_bar, 1, top_right);
        }

        // Create bottom bar.
        bottom_bar = pgui_new_text(NULL);
        pgui_enable_flags(bottom_bar, PGUI_FLAG_NOROUNDING);
        pgui_disable_flags(bottom_bar, PGUI_FLAG_NOBACKGROUND);
        pgui_set_variant(bottom_bar, PGUI_VARIANT_PANEL);
        pgui_set_halign(bottom_bar, PAX_ALIGN_BEGIN);
        pgui_child_replace(gui, 2, bottom_bar);
    }

    // Set up the menu screens.
    menu_root_init();
    menu_enable(root_menu);
    bsp_disp_backlight(1, 0, 65535);

    bool needs_draw   = true;
    bool needs_redraw = false;
    while (true) {
        bsp_event_t event;
        if (menu_change) {
            while (menu_stack_prev > menu_stack_len) {
                menu_stack_prev--;
                if (menu_stack[menu_stack_prev].on_close) {
                    menu_stack[menu_stack_prev].on_close(menu_stack[menu_stack_prev].on_close_cookie);
                }
            }
            menu_stack_prev = menu_stack_len;
            menu_enable(menu_stack_len ? menu_stack[menu_stack_len - 1] : root_menu);
            needs_draw = true;
            pgui_calc_layout(pax_buf_get_dims(gfx), gui, NULL);
        }

        if (needs_draw) {
            // Full re-draw required.
            pax_background(gfx, pgui_get_default_theme()->palette[PGUI_VARIANT_DEFAULT].bg_col);
            pgui_draw(gfx, gui, NULL);
            bsp_disp_update(1, 0, pax_buf_get_pixels(gfx));
            needs_draw   = false;
            needs_redraw = false;

        } else if (needs_redraw) {
            // Partial re-draw required.
            pgui_redraw(gfx, gui, NULL);
            bsp_disp_update(1, 0, pax_buf_get_pixels(gfx));
            needs_redraw = false;
        }

        // Run all pending events.
        uint64_t timeout = UINT64_MAX;
        while (bsp_event_wait(&event, timeout)) {
            // Convert BSP event to PGUI event.
            pgui_event_t p_event = {
                .type    = event.input.type,
                .input   = event.input.nav_input,
                .value   = event.input.text_input,
                .modkeys = event.input.modkeys,
            };
            // Run event through GUI.
            pgui_resp_t resp = pgui_event(pax_buf_get_dims(gfx), gui, NULL, p_event);
            if (resp) {
                // Mark as dirty.
                if (resp == PGUI_RESP_CAPTURED_DIRTY) {
                    needs_draw = true;
                }
                needs_redraw = true;
            } else if (p_event.input == PGUI_INPUT_BACK && p_event.type == PGUI_EVENT_TYPE_PRESS) {
                // Exit current screen and go back one level.
                menu_pop();
            }
            timeout = 0;
        }
    }
}
