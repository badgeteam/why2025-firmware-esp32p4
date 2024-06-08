/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_st7701.h"
#include "esp_ldo_regulator.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "pax_gfx.h"
#include "fonts/chakrapetchmedium.h"

static const char *TAG = "example";
pax_buf_t fb;

// Colors
pax_col_t color_background = 0xFFEEEAEE;
pax_col_t color_primary    = 0xFF01BC99;
pax_col_t color_secondary  = 0xFFFFCF53;
pax_col_t color_tertiary   = 0xFFFF017F;
pax_col_t color_quaternary = 0xFF340132;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD Spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define EXAMPLE_MIPI_DSI_DPI_CLK_MHZ  30
#define EXAMPLE_MIPI_DSI_LCD_H_RES    480
#define EXAMPLE_MIPI_DSI_LCD_V_RES    800
#define EXAMPLE_MIPI_DSI_LCD_HSYNC    40
#define EXAMPLE_MIPI_DSI_LCD_HBP      40
#define EXAMPLE_MIPI_DSI_LCD_HFP      30
#define EXAMPLE_MIPI_DSI_LCD_VSYNC    16
#define EXAMPLE_MIPI_DSI_LCD_VBP      16
#define EXAMPLE_MIPI_DSI_LCD_VFP      16

#define EXAMPLE_MIPI_DSI_LANE_NUM          2
#define EXAMPLE_MIPI_DSI_LANE_BITRATE_MBPS 250

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your Board Design //////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The "VDD_MIPI_DPHY" should be supplied with 2.5V, it can source from the internal LDO regulator or from external LDO chip
#define EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN       3  // LDO_VO3 is connected to VDD_MIPI_DPHY
#define EXAMPLE_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV 2500
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL           1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL          !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_BK_LIGHT                -1
#define EXAMPLE_PIN_NUM_LCD_RST                 0

static void example_bsp_enable_dsi_phy_power(void)
{
    // Turn on the power for MIPI DSI PHY, so it can go from "No Power" state to "Shutdown" state
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
#ifdef EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = EXAMPLE_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
#endif
}

static void example_bsp_init_lcd_backlight(void)
{
#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif
}

static void example_bsp_set_lcd_backlight(uint32_t level)
{
#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, level);
#endif
}

void display_test(void)
{
    example_bsp_enable_dsi_phy_power();
    example_bsp_init_lcd_backlight();
    example_bsp_set_lcd_backlight(EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL);

    // create MIPI DSI bus first, it will initialize the DSI PHY as well
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = EXAMPLE_MIPI_DSI_LANE_NUM,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = EXAMPLE_MIPI_DSI_LANE_BITRATE_MBPS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
    esp_lcd_panel_io_handle_t mipi_dbi_io;
    // we use DBI interface to send LCD commands and parameters
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));
    // create ILI9881C control panel
    esp_lcd_panel_handle_t st7701_ctrl_panel;
    esp_lcd_panel_dev_config_t lcd_dev_config = {
        .bits_per_pixel = 24,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .flags = {
            .reset_active_high = false,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(mipi_dbi_io, &lcd_dev_config, &st7701_ctrl_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(st7701_ctrl_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(st7701_ctrl_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(st7701_ctrl_panel, true));

    ESP_LOGI(TAG, "Install MIPI DSI LCD data panel");
    esp_lcd_panel_handle_t mipi_dpi_panel;
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = EXAMPLE_MIPI_DSI_DPI_CLK_MHZ,
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .video_timing = {
            .h_size = EXAMPLE_MIPI_DSI_LCD_H_RES,
            .v_size = EXAMPLE_MIPI_DSI_LCD_V_RES,
            .hsync_back_porch = EXAMPLE_MIPI_DSI_LCD_HBP,
            .hsync_pulse_width = EXAMPLE_MIPI_DSI_LCD_HSYNC,
            .hsync_front_porch = EXAMPLE_MIPI_DSI_LCD_HFP,
            .vsync_back_porch = EXAMPLE_MIPI_DSI_LCD_VBP,
            .vsync_pulse_width = EXAMPLE_MIPI_DSI_LCD_VSYNC,
            .vsync_front_porch = EXAMPLE_MIPI_DSI_LCD_VFP,
        },
#if CONFIG_EXAMPLE_USE_DMA2D_COPY_FRAME
        .flags.use_dma2d = true,
#endif
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_dpi(mipi_dsi_bus, &dpi_config, &mipi_dpi_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(mipi_dpi_panel));
    // turn on backlight
    example_bsp_set_lcd_backlight(EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);

    pax_buf_init(&fb, NULL, EXAMPLE_MIPI_DSI_LCD_H_RES, EXAMPLE_MIPI_DSI_LCD_V_RES, PAX_BUF_16_565RGB);
    pax_buf_reversed(&fb, false);
    pax_buf_set_orientation(&fb, PAX_O_ROT_CW);

        // Fill the buffer with the background color
    pax_background(&fb, color_background);

    //pax_draw_image(&fb, &assets[ASSET_TEMPLATE], 0, 0);

    // Footer test
    int footer_height      = 32;
    int footer_vmarign     = 7;
    int footer_hmarign     = 20;
    int footer_text_height = 16;
    int footer_text_hmarign = 20;
    pax_font_t* footer_text_font = &chakrapetchmedium;

    int footer_box_height = footer_height + (footer_vmarign * 2);
    pax_draw_line(&fb, color_quaternary, footer_hmarign, pax_buf_get_height(&fb) - footer_box_height, pax_buf_get_width(&fb) - footer_hmarign, pax_buf_get_height(&fb) - footer_box_height);
    pax_right_text(&fb, color_quaternary, footer_text_font, footer_text_height, pax_buf_get_width(&fb) - footer_hmarign - footer_text_hmarign, pax_buf_get_height(&fb) - footer_box_height + (footer_box_height - footer_text_height) / 2, "↑ / ↓ / ← / → Navigate   🅰 Start");


    // Font test
    /*pax_draw_text(&fb, color_quaternary, pax_font_sky, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 1, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(&fb, color_quaternary, pax_font_sky_mono, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 2, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(&fb, color_quaternary, pax_font_marker, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 3, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(&fb, color_quaternary, pax_font_saira_condensed, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 4, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(&fb, color_quaternary, pax_font_saira_regular, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 5, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(&fb, color_quaternary, &chakrapetchmedium, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 6, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");*/

    int circle_count = 6;
    int circle_spacing = 68;
    int circle_offset = (pax_buf_get_width(&fb) - circle_spacing * (circle_count - 1)) / 2;

    for (int i = 0; i < circle_count; i++) {
        pax_draw_circle(&fb, 0xFFFFFFFF, circle_offset + circle_spacing * i, 375, 28);
        pax_outline_circle(&fb, 0xFFE4E4E4, circle_offset + circle_spacing * i, 375, 28);
    }

    int square_count = 5;
    int square_voffset = 116;
    int square_hoffset = 62;
    int square_size = 170;
    int square_selected = 0;
    int square_spacing = 1;
    int square_marign = 2;

    pax_col_t square_colors[] = {
        0xFFFFFFFF,
        color_secondary,
        color_tertiary,
        color_quaternary,
        color_primary,
    };

    for (int i = 0; i < square_count; i++) {
        if (i == square_selected) {
            pax_draw_rect(&fb, color_primary, square_hoffset + (square_size + square_spacing) * i, square_voffset, square_size, square_size);
        }
        pax_draw_rect(&fb, square_colors[i], square_hoffset + (square_size + square_spacing) * i + square_marign, square_voffset + square_marign, square_size - square_marign * 2, square_size - square_marign * 2);
    }

    int title_text_voffset = square_voffset - 32;
    int title_text_hoffset = square_hoffset - 2;
    int title_text_height = 20;
    pax_font_t* title_text_font = &chakrapetchmedium;

    pax_draw_text(&fb, color_primary, title_text_font, title_text_height, title_text_hoffset, title_text_voffset, "Example Application Title");

    pax_right_text(&fb, color_quaternary, title_text_font, 20, pax_buf_get_width(&fb) - 40, 32, "23:37 :D");

    esp_lcd_panel_draw_bitmap(mipi_dpi_panel, 0, 0, EXAMPLE_MIPI_DSI_LCD_H_RES, EXAMPLE_MIPI_DSI_LCD_V_RES, fb.buf);
}
