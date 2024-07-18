
// SPDX-License-Identifier: MIT

#include "bsp.h"
#include "bsp/why2025_coproc.h"
#include "bsp_device.h"
#include "hardware/why2025.h"
#include "pax_gfx.h"
#include "pax_gui.h"

#include <stdio.h>

#include <driver/sdmmc_host.h>
#include <esp_app_desc.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdmmc_cmd.h>

static char const TAG[] = "main";



static esp_err_t print_sdio_cis_information(sdmmc_card_t *card) {
    size_t const cis_buffer_size = 256;
    uint8_t      cis_buffer[cis_buffer_size];
    size_t       cis_data_len = 1024; // specify maximum searching range to avoid infinite loop
    esp_err_t    ret          = ESP_OK;

    ret = sdmmc_io_get_cis_data(card, cis_buffer, cis_buffer_size, &cis_data_len);
    if (ret == ESP_ERR_INVALID_SIZE) {
        int      temp_buf_size = cis_data_len;
        uint8_t *temp_buf      = malloc(temp_buf_size);
        assert(temp_buf);

        ESP_LOGW(TAG, "CIS data longer than expected, temporary buffer allocated.");
        ret = sdmmc_io_get_cis_data(card, temp_buf, temp_buf_size, &cis_data_len);
        ESP_ERROR_CHECK(ret);

        sdmmc_io_print_cis_info(temp_buf, cis_data_len, NULL);

        free(temp_buf);
    } else if (ret == ESP_OK) {
        sdmmc_io_print_cis_info(cis_buffer, cis_data_len, NULL);
    } else {
        // including ESP_ERR_NOT_FOUND
        ESP_LOGE(TAG, "failed to get the entire CIS data.");
        abort();
    }
    return ESP_OK;
}

esp_err_t sdio_test() {
    sdmmc_host_t host  = SDMMC_HOST_DEFAULT();
    host.flags         = SDMMC_HOST_FLAG_4BIT;
    host.max_freq_khz  = SDMMC_FREQ_HIGHSPEED;
    host.flags        |= SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF;

    sdmmc_slot_config_t slot_config = {
        .clk   = GPIO_NUM_17,
        .cmd   = GPIO_NUM_16,
        .d0    = GPIO_NUM_18,
        .d1    = GPIO_NUM_19,
        .d2    = GPIO_NUM_20,
        .d3    = GPIO_NUM_21,
        .d4    = GPIO_NUM_NC,
        .d5    = GPIO_NUM_NC,
        .d6    = GPIO_NUM_NC,
        .d7    = GPIO_NUM_NC,
        .cd    = SDMMC_SLOT_NO_CD,
        .wp    = SDMMC_SLOT_NO_WP,
        .width = 4,
        .flags = 0,
    };

    esp_err_t res = sdmmc_host_init();
    if (res != ESP_OK)
        return res;

    res = sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);
    if (res != ESP_OK)
        return res;

    sdmmc_card_t *card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));

    for (;;) {
        if (sdmmc_card_init(&host, card) == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "slave init failed, retry...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    sdmmc_card_print_info(stdout, card);

    print_sdio_cis_information(card);

    return res;
}



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

pax_buf_t gfx;
void      app_main(void) {
    display_version();
    bsp_init();

    // esp_err_t res = sdio_test();
    // if (res) {
    //     ESP_LOGW(TAG, "SDIO error: %s", esp_err_to_name(res));
    // }

    pax_buf_init(&gfx, NULL, BSP_DSI_LCD_H_RES, BSP_DSI_LCD_V_RES, PAX_BUF_16_565RGB);
    pax_buf_set_orientation(&gfx, PAX_O_ROT_CW);
    pax_background(&gfx, 0);
    pax_buf_reversed(&gfx, false);

    uint32_t dev_id = bsp_dev_register(&tree);
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
