
// SPDX-License-Identifier: MIT

#include "bsp/disp_ek79007.h"

#include "esp_lcd_ek79007.h"



// MIPI DSI display-specific init functions.
esp_err_t bsp_disp_ek79007_new(
    esp_lcd_panel_io_handle_t         io,
    esp_lcd_panel_dev_config_t       *panel_dev_config,
    esp_lcd_panel_handle_t           *ret_panel,
    esp_lcd_dsi_bus_handle_t          dsi_bus,
    esp_lcd_dpi_panel_config_t const *dpi_config
) {
    ek79007_vendor_config_t vendor_config = {
        .flags = {
            .use_mipi_interface = 1,
        },
        .mipi_config = {
            .dsi_bus    = dsi_bus,
            .dpi_config = dpi_config,
        },
    };
    panel_dev_config->vendor_config = &vendor_config;
    return esp_lcd_new_panel_ek79007(io, panel_dev_config, ret_panel);
}

// EK79007 init function.
bool bsp_disp_ek79007_init(bsp_device_t *dev, uint8_t endpoint) {
    return bsp_disp_dsi_init(dev, endpoint, bsp_disp_ek79007_new);
}
