
// SPDX-License-Identifier: MIT

#pragma once

#include <sdkconfig.h>


#if CONFIG_BSP_PLATFORM_P4DEVKIT01_ST7701

/* ==== Mipi DSI parameters ==== */
#define BSP_DSI_DPI_CLK_MHZ    30
#define BSP_DSI_LCD_RESET_PIN  0
#define BSP_DSI_LCD_H_RES      480
#define BSP_DSI_LCD_V_RES      800
#define BSP_DSI_LCD_HSYNC      40
#define BSP_DSI_LCD_HBP        40
#define BSP_DSI_LCD_HFP        30
#define BSP_DSI_LCD_VSYNC      16
#define BSP_DSI_LCD_VBP        16
#define BSP_DSI_LCD_VFP        16
#define BSP_DSI_LANES          2
#define BSP_DSI_BITRATE_MBPS   250
#define BSP_DSI_LDO_CHAN       3
#define BSP_DSI_LDO_VOLTAGE_MV 2500
#define BSP_LCD_RESET_PIN      0

#else

/* ==== Mipi DSI parameters ==== */
#define BSP_DSI_DPI_CLK_MHZ       52
#define BSP_DSI_LCD_RESET_PIN     27
#define BSP_DSI_LCD_BACKLIGHT_PIN 26
#define BSP_DSI_LCD_H_RES         1024
#define BSP_DSI_LCD_V_RES         600
#define BSP_DSI_LCD_HSYNC         10
#define BSP_DSI_LCD_HBP           160
#define BSP_DSI_LCD_HFP           160
#define BSP_DSI_LCD_VSYNC         1
#define BSP_DSI_LCD_VBP           23
#define BSP_DSI_LCD_VFP           12
#define BSP_DSI_LANES             2
#define BSP_DSI_BITRATE_MBPS      1000
#define BSP_DSI_LDO_CHAN          3
#define BSP_DSI_LDO_VOLTAGE_MV    2500
#define BSP_LCD_RESET_PIN         0

// /* ==== Mipi DSI parameters ==== */
// #define BSP_DSI_DPI_CLK_MHZ       52
// #define BSP_DSI_LCD_RESET_PIN     27
// #define BSP_DSI_LCD_BACKLIGHT_PIN 26
// #define BSP_DSI_LCD_H_RES         1024
// #define BSP_DSI_LCD_V_RES         600
// #define BSP_DSI_LCD_HSYNC         1344
// #define BSP_DSI_LCD_HBP           160
// #define BSP_DSI_LCD_HFP           160
// #define BSP_DSI_LCD_VSYNC         635
// #define BSP_DSI_LCD_VBP           23
// #define BSP_DSI_LCD_VFP           12
// #define BSP_DSI_LANES             2
// #define BSP_DSI_BITRATE_MBPS      1000
// #define BSP_DSI_LDO_CHAN          3
// #define BSP_DSI_LDO_VOLTAGE_MV    2500
// #define BSP_LCD_RESET_PIN         0

#endif
