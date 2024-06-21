
// SPDX-License-Identifier: MIT

#include "bsp.h"
#include "bsp_device.h"
#include "hardware/why2025.h"
#include "pax_gfx.h"

#include <stdio.h>

#include <esp_app_desc.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>



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
bsp_display_devtree_t const disp_tree = {
    .common = {
        .type = BSP_EP_DISP_ST7701,
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
    .input_count = 1,
    .input_dev = (bsp_input_devtree_t const *const[]) {
        &input_tree,
    },
    .disp_count = 1,
    .disp_dev   = (bsp_display_devtree_t const *const[]) {
        &disp_tree,
    },
};

void app_main(void) {
    display_version();
    bsp_init();

    pax_buf_t gfx;
    pax_buf_init(&gfx, NULL, BSP_DSI_LCD_H_RES, BSP_DSI_LCD_V_RES, PAX_BUF_16_565RGB);
    pax_buf_set_orientation(&gfx, PAX_O_ROT_CW);
    pax_background(&gfx, 0);
    pax_draw_text(&gfx, 0xffffffff, pax_font_sky, 36, 0, 0, "Julian Wuz Here");

    uint32_t dev_id = bsp_dev_register(&tree);
    bsp_disp_update(dev_id, 0, pax_buf_get_pixels(&gfx));

    bsp_disp_backlight(dev_id, 0, 65535);
    bsp_input_backlight(dev_id, 0, 16384);

    while (true) {
        bsp_event_t event;
        if (bsp_event_wait(&event, UINT64_MAX)) {
            char const *const tab[] = {
                [BSP_INPUT_EVENT_PRESS]   = "pressed",
                [BSP_INPUT_EVENT_HOLD]    = "held",
                [BSP_INPUT_EVENT_RELEASE] = "released",
            };
            ESP_LOGI(
                "main",
                "Dev %" PRIu32 " ep %" PRIu8 " input %d raw input %d %s",
                event.input.dev_id,
                event.input.endpoint,
                event.input.input,
                event.input.raw_input,
                tab[event.input.type]
            );
        }
    }
}
