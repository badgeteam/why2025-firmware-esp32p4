
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp_device.h"

#include <esp_err.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_dev.h>
#include <esp_lcd_panel_ops.h>



// MIPI DSI display-specific init functions.
typedef esp_err_t (*bsp_disp_dsi_new_t)(
    esp_lcd_panel_io_handle_t const   io,
    esp_lcd_panel_dev_config_t const *panel_dev_config,
    esp_lcd_panel_handle_t           *ret_panel
);

// Initialize MIPI DSI driver.
bool bsp_disp_dsi_init(bsp_device_t *dev, uint8_t endpoint, bsp_disp_dsi_new_t new_fun);
// Deinitalize MIPI DSI driver.
bool bsp_disp_dsi_deinit(bsp_device_t *dev, uint8_t endpoint);

// Send new image data to a device's display.
void bsp_disp_dsi_update(bsp_device_t *dev, uint8_t endpoint, void const *framebuffer);
// Send new image data to part of a device's display.
void bsp_disp_dsi_update_part(
    bsp_device_t *dev, uint8_t endpoint, void const *framebuffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h
);