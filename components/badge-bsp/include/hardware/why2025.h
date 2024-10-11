
// SPDX-License-Identifier: MIT

#pragma once

// I²C peripheral used for internal I²C bus.
#define BSP_I2CINT_NUM     0
// Internal I²C bus SDA pin.
#define BSP_I2CINT_SDA_PIN 9//52
// Internal I²C bus SCL pin.
#define BSP_I2CINT_SCL_PIN 10//53

// Interrupt pin from the CH32V203 to the ESP32P4.
#define BSP_CH32_IRQ_PIN 6//49
// I²C address for the CH32V203.
#define BSP_CH32_ADDR    0x5f
