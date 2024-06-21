
// SPDX-License-Identifier: MIT

#include "bsp/disp_mipi_dsi.h"

#include "hardware/why2025.h"

#include <esp_err.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_dev.h>
#include <esp_lcd_panel_ops.h>
#include <esp_ldo_regulator.h>
#include <esp_log.h>

static char const TAG[] = "bsp-dsi";



// Data for MIPI DSI driver.
typedef struct {
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_dsi_bus_handle_t  bus_handle;
    esp_lcd_panel_handle_t    ctrl_handle;
    esp_lcd_panel_handle_t    disp_handle;
} bsp_disp_dsi_t;



// LDO regulator handle.
static esp_ldo_channel_handle_t ldo_handle = NULL;

// Power on Mipi DSI PHY.
static esp_err_t dsi_phy_poweron() {
    // Turn on the power for MIPI DSI PHY, so it can go from "No Power" state to "Shutdown" state
    esp_ldo_channel_config_t ldo_config = {
        .chan_id    = BSP_DSI_LDO_CHAN,
        .voltage_mv = BSP_DSI_LDO_VOLTAGE_MV,
    };
    esp_err_t res = esp_ldo_acquire_channel(&ldo_config, &ldo_handle);
    if (res == ESP_OK) {
        ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
    }
    return res;
}

// Power off Mipi DSI PHY.
static esp_err_t dsi_phy_poweroff() {
    esp_err_t res = esp_ldo_release_channel(ldo_handle);
    if (res == ESP_OK) {
        ESP_LOGI(TAG, "MIPI DSI PHY Powered off");
    }
    return res;
}



// Initialize MIPI DSI driver.
bool bsp_disp_dsi_init(bsp_device_t *dev, uint8_t endpoint, bsp_disp_dsi_new_t new_fun) {
    // Allocate data structures.
    dev->disp_aux[endpoint] = malloc(sizeof(bsp_disp_dsi_t));
    if (!dev->disp_aux[endpoint]) {
        ESP_LOGE(TAG, "Failed to initialize DSI display: %s", "out of memory");
        return false;
    }
    bsp_disp_dsi_t *disp = dev->disp_aux[endpoint];

    esp_err_t res = dsi_phy_poweron();
    if (res != ESP_OK) {
        goto error;
    }

    // Create Mipi DSI bus.
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id             = 0,
        .num_data_lanes     = BSP_DSI_LANES,
        .phy_clk_src        = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bit_rate_mbps = BSP_DSI_BITRATE_MBPS,
    };
    res = esp_lcd_new_dsi_bus(&bus_config, &disp->bus_handle);
    if (res != ESP_OK) {
        dsi_phy_poweroff();
        goto error;
    }

    // Create control channel.
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits    = 8,
        .lcd_param_bits  = 8,
    };
    ESP_LOGI(TAG, "%s:%d", __FILENAME__, __LINE__);
    res = esp_lcd_new_panel_io_dbi(disp->bus_handle, &dbi_config, &disp->io_handle);
    if (res != ESP_OK) {
        goto error2;
    }

    // Initialise panel.
    esp_lcd_panel_dev_config_t lcd_config = {
        .bits_per_pixel = 16,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .reset_gpio_num = BSP_LCD_RESET_PIN,
        .flags = {
            .reset_active_high = false,
        },
    };
    ESP_LOGI(TAG, "%s:%d", __FILENAME__, __LINE__);
    if ((res = new_fun(disp->io_handle, &lcd_config, &disp->ctrl_handle)) != ESP_OK) {
        goto error3;
    }

    // Turn on the display.
    ESP_LOGI(TAG, "%s:%d", __FILENAME__, __LINE__);
    if ((res = esp_lcd_panel_reset(disp->ctrl_handle)) != ESP_OK) {
        goto error3;
    }
    ESP_LOGI(TAG, "%s:%d", __FILENAME__, __LINE__);
    if ((res = esp_lcd_panel_init(disp->ctrl_handle)) != ESP_OK) {
        goto error3;
    }
    ESP_LOGI(TAG, "%s:%d", __FILENAME__, __LINE__);
    if ((res = esp_lcd_panel_disp_on_off(disp->ctrl_handle, true)) != ESP_OK) {
        goto error3;
    }

    // Create data channel.
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel    = 0,
        .dpi_clk_src        = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = BSP_DSI_DPI_CLK_MHZ,
        .pixel_format       = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .video_timing = {
            .h_size            = dev->tree->disp_dev[endpoint]->width,
            .v_size            = dev->tree->disp_dev[endpoint]->height,
            .hsync_back_porch  = dev->tree->disp_dev[endpoint]->h_bp,
            .hsync_pulse_width = dev->tree->disp_dev[endpoint]->h_sync,
            .hsync_front_porch = dev->tree->disp_dev[endpoint]->h_fp,
            .vsync_back_porch  = dev->tree->disp_dev[endpoint]->v_bp,
            .vsync_pulse_width = dev->tree->disp_dev[endpoint]->v_sync,
            .vsync_front_porch = dev->tree->disp_dev[endpoint]->v_fp,
        },
        // TODO: Figure out what this is and when to use it.
        // .flags.use_dma2d = true,
    };
    if ((res = esp_lcd_new_panel_dpi(disp->bus_handle, &dpi_config, &disp->disp_handle)) != ESP_OK) {
        goto error4;
    }
    if ((res = esp_lcd_panel_init(disp->disp_handle)) != ESP_OK) {
        esp_lcd_panel_del(disp->disp_handle);
        goto error4;
    }

    ESP_LOGI(TAG, "Successfully initialized DSI display");
    return true;

    // Error handling.
error4:
    ESP_LOGI(TAG, "error4");
    esp_lcd_panel_disp_on_off(disp->ctrl_handle, false);
error3:
    ESP_LOGI(TAG, "error3");
    esp_lcd_panel_del(disp->ctrl_handle);
error2:
    ESP_LOGI(TAG, "error2");
    esp_lcd_del_dsi_bus(disp->bus_handle);
    dsi_phy_poweroff();
error:
    ESP_LOGI(TAG, "error1");
    free(dev->disp_aux[endpoint]);
    dev->disp_aux[endpoint] = NULL;
    ESP_LOGE(TAG, "Failed to initialize DSI display: %s", esp_err_to_name(res));
    return false;
}

// Deinitalize MIPI DSI driver.
bool bsp_disp_dsi_deinit(bsp_device_t *dev, uint8_t endpoint) {
    bsp_disp_dsi_t *disp = dev->disp_aux[endpoint];
    esp_lcd_panel_disp_on_off(disp->ctrl_handle, false);
    esp_lcd_panel_del(disp->disp_handle);
    esp_lcd_panel_del(disp->ctrl_handle);
    esp_lcd_del_dsi_bus(disp->bus_handle);
    dsi_phy_poweroff();
    return true;
}



// Send new image data to a device's display.
void bsp_disp_dsi_update(bsp_device_t *dev, uint8_t endpoint, void const *framebuffer) {
    bsp_disp_dsi_t *disp = dev->disp_aux[endpoint];
    esp_lcd_panel_draw_bitmap(
        disp->disp_handle,
        0,
        0,
        dev->tree->disp_dev[endpoint]->width,
        dev->tree->disp_dev[endpoint]->height,
        framebuffer
    );
}

// Send new image data to part of a device's display.
void bsp_disp_dsi_update_part(
    bsp_device_t *dev, uint8_t endpoint, void const *framebuffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h
) {
    bsp_disp_dsi_t *disp = dev->disp_aux[endpoint];
    esp_lcd_panel_draw_bitmap(disp->disp_handle, x, y, w, h, framebuffer);
}