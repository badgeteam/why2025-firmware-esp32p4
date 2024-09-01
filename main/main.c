
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

char const TAG[] = "main";

#define MAX_MENU_DEPTH 16



extern uint8_t const ch32_firmware_start[] asm("_binary_ch32_firmware_bin_start");
extern uint8_t const ch32_firmware_end[] asm("_binary_ch32_firmware_bin_end");

// Current GUI root element.
static pgui_elem_t *gui;
// Global framebuffer.
static pax_buf_t   *gfx;
// Top-level menu screen GUI root element.
static pgui_elem_t *root_menu;
// Menu stack.
static menu_entry_t menu_stack[MAX_MENU_DEPTH];
// Number of menus on the stack.
static size_t       menu_stack_len;
// Previsous value of `menu_stack_len`; used to clean up.
static size_t       menu_stack_prev;
// Need to update active menu.
static bool         menu_change = false;



// Set the top-level menu screen.
void menu_set_root(pgui_elem_t *root) {
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

    // Initialize AppFS so the app launcher can use it.
    appfsInit(APPFS_PART_TYPE, APPFS_PART_SUBTYPE);

    // Set up the menu screens.
    menu_root_init();
    gui = root_menu;
    pax_background(gfx, pgui_get_default_theme()->palette[PGUI_VARIANT_DEFAULT].bg_col);
    pgui_calc_layout(pax_buf_get_dims(gfx), gui, NULL);
    pgui_draw(gfx, gui, NULL);
    bsp_disp_update(1, 0, pax_buf_get_pixels(gfx));
    bsp_disp_backlight(1, 0, 65535);

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
            gui             = menu_stack_len ? menu_stack[menu_stack_len - 1].root : root_menu;
            pax_background(gfx, pgui_get_default_theme()->palette[PGUI_VARIANT_DEFAULT].bg_col);
            pgui_calc_layout(pax_buf_get_dims(gfx), gui, NULL);
            pgui_draw(gfx, gui, NULL);
            bsp_disp_update(1, 0, pax_buf_get_pixels(gfx));
        }
        if (bsp_event_wait(&event, UINT64_MAX)) {
            pgui_event_t p_event = {
                .type    = event.input.type,
                .input   = event.input.nav_input,
                .value   = event.input.text_input,
                .modkeys = event.input.modkeys,
            };
            pgui_resp_t resp = pgui_event(pax_buf_get_dims(gfx), gui, NULL, p_event);
            if (resp) {
                if (resp == PGUI_RESP_CAPTURED_DIRTY) {
                    pax_background(gfx, pgui_get_default_theme()->palette[PGUI_VARIANT_DEFAULT].bg_col);
                }
                pgui_redraw(gfx, gui, NULL);
                bsp_disp_update(1, 0, pax_buf_get_pixels(gfx));
            } else if (p_event.input == PGUI_INPUT_BACK && p_event.type == PGUI_EVENT_TYPE_PRESS) {
                menu_pop();
            }
        }
    }
}
