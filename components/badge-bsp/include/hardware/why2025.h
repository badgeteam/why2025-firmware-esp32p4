
// SPDX-License-Identifier: MIT

#pragma once

// I²C peripheral used for internal I²C bus.
#define BSP_I2CINT_NUM     0
// Internal I²C bus SDA pin.
#define BSP_I2CINT_SDA_PIN 52
// Internal I²C bus SCL pin.
#define BSP_I2CINT_SCL_PIN 53

// I²S peripheral number
#define BSP_I2S_NUM        0
// I²S MCLK Pin
#define I2S_MCK_IO         30
// I²S BCLK Pin
#define I2S_BCK_IO         29
// I²S WS Pin
#define I2S_WS_IO          31
// I²S DO Pin
#define I2S_DO_IO          28


// Interrupt pin from the CH32V203 to the ESP32P4.
#define BSP_CH32_IRQ_PIN 49
// I²C address for the CH32V203.
#define BSP_CH32_ADDR    0x5f

/* ==== Mipi DSI parameters ==== */
#define BSP_DSI_DPI_CLK_MHZ    30
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
