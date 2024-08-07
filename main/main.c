
// SPDX-License-Identifier: MIT

#include "bsp.h"
#include "bsp/why2025_coproc.h"
#include "bsp_device.h"
#include "ch32v203prog.h"
#include "hardware/why2025.h"
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



void display_version() {
    esp_app_desc_t const *app_description = esp_app_get_description();
    printf("BADGE.TEAM %s launcher firmware v%s\n", app_description->project_name, app_description->version);
}

bsp_input_devtree_t const input_tree = {
    .common = {
        .type = BSP_EP_INPUT_WHY2025_CH32,
    },
    .category           = BSP_INPUT_CAT_KEYBOARD,
    .keymap             = &bsp_keymap_why2025,
    .backlight_endpoint = 0,
    .backlight_index    = 1,
};
bsp_input_devtree_t const input2_tree = {
    .common   = {
        .type = BSP_EP_INPUT_GPIO,
    },
    .category = BSP_INPUT_CAT_GENERIC,
    .pinmap   = &(bsp_pinmap_t const) {
        .pins_len  = 1,
        .pins      = (uint8_t const[]) {35},
        .activelow = false,
    },
};
bsp_led_devtree_t const led_tree = {
    .common   = {
        .type = BSP_EP_LED_WHY2025_CH32,
    },
    .num_leds = 2,
    .ledfmt   = {
        .color = BSP_PIXFMT_16_GREY,
    }
};
bsp_display_devtree_t const disp_tree = {
    .common = {
        .type      = BSP_EP_DISP_ST7701,
        .reset_pin = 0,
    },
    .pixfmt = {BSP_PIXFMT_16_565RGB, false},
    .h_fp   = BSP_DSI_LCD_HFP,
    .width  = BSP_DSI_LCD_H_RES,
    .h_bp   = BSP_DSI_LCD_HBP,
    .h_sync = BSP_DSI_LCD_HSYNC,
    .v_fp   = BSP_DSI_LCD_VFP,
    .height = BSP_DSI_LCD_V_RES,
    .v_bp   = BSP_DSI_LCD_VBP,
    .v_sync = BSP_DSI_LCD_VSYNC,
    .backlight_endpoint = 0,
    .backlight_index    = 0,
};
bsp_devtree_t const tree = {
    .input_count = 2,
    .input_dev = (bsp_input_devtree_t const *const[]) {
        &input_tree,
        &input2_tree,
    },
    .led_count = 1,
    .led_dev = (bsp_led_devtree_t const *const[]){
        &led_tree,
    },
    .disp_count = 1,
    .disp_dev   = (bsp_display_devtree_t const *const[]) {
        &disp_tree,
    },
};

pgui_grid_t gui = PGUI_NEW_GRID(
    10,
    10,
    216,
    100,
    2,
    3,

    &PGUI_NEW_LABEL("Row 1"),
    &PGUI_NEW_TEXTBOX(),

    &PGUI_NEW_LABEL("Row 2"),
    &PGUI_NEW_BUTTON("Hello,"),

    &PGUI_NEW_LABEL("Row 2"),
    &PGUI_NEW_BUTTON("World!")
);

extern void rvswd_test();

pax_buf_t gfx;
void      draw_text(char const *text) {
    pax_background(&gfx, 0);
    pax_center_text(&gfx, 0xff000000, pax_font_saira_condensed, pax_font_saira_condensed->default_size, 400, 240, text);
}

void app_main(void) {
    display_version();
    bsp_init();

    pax_buf_init(&gfx, NULL, BSP_DSI_LCD_H_RES, BSP_DSI_LCD_V_RES, PAX_BUF_16_565RGB);
    pax_buf_set_orientation(&gfx, PAX_O_ROT_CW);
    pax_background(&gfx, 0);
    pax_buf_reversed(&gfx, false);

    uint32_t dev_id = bsp_dev_register(&tree);
    bsp_disp_backlight(dev_id, 0, 65535);

    rvswd_test();
    bsp_disp_backlight(dev_id, 0, 65535);

    pgui_calc_layout(pax_buf_get_dims(&gfx), (pgui_elem_t *)&gui, NULL);
    pax_background(&gfx, pgui_theme_default.bg_col);
    pgui_draw(&gfx, (pgui_elem_t *)&gui, NULL);
    bsp_disp_update(dev_id, 0, pax_buf_get_pixels(&gfx));

    while (true) {
        bsp_event_t event;
        if (bsp_event_wait(&event, UINT64_MAX)) {
            pgui_event_t p_event = {
                .type    = event.input.type,
                .input   = event.input.nav_input,
                .value   = event.input.text_input,
                .modkeys = event.input.modkeys,
            };
            pgui_resp_t resp = pgui_event(pax_buf_get_dims(&gfx), (pgui_elem_t *)&gui, NULL, p_event);
            if (resp) {
                pgui_redraw(&gfx, (pgui_elem_t *)&gui, NULL);
                bsp_disp_update(dev_id, 0, pax_buf_get_pixels(&gfx));
            }
        }
    }
}
