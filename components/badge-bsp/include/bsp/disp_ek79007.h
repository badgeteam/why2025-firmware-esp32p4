
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp/disp_mipi_dsi.h"
#include "bsp_device.h"

#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_dev.h>
#include <esp_lcd_panel_ops.h>



// EK79007 init function.
bool bsp_disp_ek79007_init(bsp_device_t *dev, uint8_t endpoint);
