
// SPDX-License-Identifier: MIT


// ESPRESSIF MIT License

// Copyright (c) 2021 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>

// Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
// it is free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "bsp/es8156.h"
#include "bsp.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

#define ES8156_ADDR 0x08

// RESET Control
#define ES8156_RESET_REG00             0x00

// Clock Managerment
#define ES8156_MAINCLOCK_CTL_REG01     0x01
#define ES8156_SCLK_MODE_REG02         0x02
#define ES8156_LRCLK_DIV_H_REG03       0x03
#define ES8156_LRCLK_DIV_L_REG04       0x04
#define ES8156_SCLK_DIV_REG05          0x05
#define ES8156_NFS_CONFIG_REG06        0x06
#define ES8156_MISC_CONTROL1_REG07     0x07
#define ES8156_CLOCK_ON_OFF_REG08      0x08
#define ES8156_MISC_CONTROL2_REG09     0x09
#define ES8156_TIME_CONTROL1_REG0A     0x0a
#define ES8156_TIME_CONTROL2_REG0B     0x0b

// System Control
#define ES8156_CHIP_STATUS_REG0C       0x0c
#define ES8156_P2S_CONTROL_REG0D       0x0d
#define ES8156_DAC_OSR_COUNTER_REG10   0x10

// SDP Control
#define ES8156_DAC_SDP_REG11           0x11
#define ES8156_AUTOMUTE_SET_REG12      0x12
#define ES8156_DAC_MUTE_REG13          0x13
#define ES8156_VOLUME_CONTROL_REG14    0x14

// ALC Control
#define ES8156_ALC_CONFIG1_REG15       0x15
#define ES8156_ALC_CONFIG2_REG16       0x16
#define ES8156_ALC_CONFIG3_REG17       0x17
#define ES8156_MISC_CONTROL3_REG18     0x18
#define ES8156_EQ_CONTROL1_REG19       0x19
#define ES8156_EQ_CONTROL2_REG1A       0x1a

// Analog System Control
#define ES8156_ANALOG_SYS1_REG20       0x20
#define ES8156_ANALOG_SYS2_REG21       0x21
#define ES8156_ANALOG_SYS3_REG22       0x22
#define ES8156_ANALOG_SYS4_REG23       0x23
#define ES8156_ANALOG_LP_REG24         0x24
#define ES8156_ANALOG_SYS5_REG25       0x25

// Chip Information
#define ES8156_I2C_PAGESEL_REGFC       0xFC
#define ES8156_CHIPID1_REGFD           0xFD
#define ES8156_CHIPID0_REGFE           0xFE
#define ES8156_CHIP_VERSION_REGFF      0xFF

static SemaphoreHandle_t es8156_semaphore;

static esp_err_t es8156_write_reg(uint8_t address, uint8_t data) {
    uint8_t buffer[2];
    buffer[0] = address;
    buffer[1] = data;
    return i2c_master_write_to_device(BSP_I2CINT_NUM, ES8156_ADDR, buffer, sizeof(buffer), pdMS_TO_TICKS(50));
}

static esp_err_t es8156_read_reg(uint8_t address, uint8_t* data) {
    return i2c_master_write_read_device(BSP_I2CINT_NUM, BSP_CH32_ADDR, &address, 1, data, 1, pdMS_TO_TICKS(50));
}

esp_err_t es8156_init() {
    es8156_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(es8156_semaphore);

    if (es8156_semaphore == NULL) {
        return ESP_ERR_NO_MEM;
    }

    es8156_write_reg(ES8156_SCLK_MODE_REG02, 0x04);
    es8156_write_reg(ES8156_ANALOG_SYS1_REG20, 0x2A);
    es8156_write_reg(ES8156_ANALOG_SYS2_REG21, 0x3C);
    es8156_write_reg(ES8156_ANALOG_SYS3_REG22, 0x00);
    es8156_write_reg(ES8156_ANALOG_LP_REG24, 0x07);
    es8156_write_reg(ES8156_ANALOG_SYS4_REG23, 0b100);

    es8156_write_reg(0x0A, 0x01);
    es8156_write_reg(0x0B, 0x01);
    es8156_write_reg(ES8156_DAC_SDP_REG11, 0x00);
    es8156_write_reg(ES8156_VOLUME_CONTROL_REG14, 179);  // volume 70%

    es8156_write_reg(0x0D, 0x14);
    es8156_write_reg(ES8156_MISC_CONTROL3_REG18, 0x00);
    es8156_write_reg(ES8156_CLOCK_ON_OFF_REG08, 0x3F);
    es8156_write_reg(ES8156_RESET_REG00, 0x02);
    es8156_write_reg(ES8156_RESET_REG00, 0x03);
    es8156_write_reg(ES8156_ANALOG_SYS5_REG25, 0x20);
    return ESP_OK;
}

esp_err_t es8156_standby(void) {
    esp_err_t ret = 0;
    ret = es8156_write_reg(ES8156_VOLUME_CONTROL_REG14, 0x00);
    ret |= es8156_write_reg(ES8156_EQ_CONTROL1_REG19, 0x02);
    ret |= es8156_write_reg(ES8156_ANALOG_SYS2_REG21, 0x1F);
    ret |= es8156_write_reg(ES8156_ANALOG_SYS3_REG22, 0x02);
    ret |= es8156_write_reg(ES8156_ANALOG_SYS5_REG25, 0x21);
    ret |= es8156_write_reg(ES8156_ANALOG_SYS5_REG25, 0xA1);
    ret |= es8156_write_reg(ES8156_MISC_CONTROL3_REG18, 0x01);
    ret |= es8156_write_reg(ES8156_MISC_CONTROL2_REG09, 0x02);
    ret |= es8156_write_reg(ES8156_MISC_CONTROL2_REG09, 0x01);
    ret |= es8156_write_reg(ES8156_CLOCK_ON_OFF_REG08, 0x00);
    return ret;
}

esp_err_t es8156_resume(void) {
    esp_err_t ret = 0;
    ret |= es8156_write_reg(ES8156_CLOCK_ON_OFF_REG08, 0x3F);
    ret |= es8156_write_reg(ES8156_MISC_CONTROL2_REG09, 0x00);
    ret |= es8156_write_reg(ES8156_MISC_CONTROL3_REG18, 0x00);
    ret |= es8156_write_reg(ES8156_ANALOG_SYS5_REG25, 0x20);
    ret |= es8156_write_reg(ES8156_ANALOG_SYS3_REG22, 0x00);
    ret |= es8156_write_reg(ES8156_ANALOG_SYS2_REG21, 0x3C);
    ret |= es8156_write_reg(ES8156_EQ_CONTROL1_REG19, 0x20);
    ret |= es8156_write_reg(ES8156_VOLUME_CONTROL_REG14, 179);
    return ret;
}

esp_err_t es8156_codec_set_voice_mute(bool enable)
{
    uint8_t regv;
    esp_err_t res = es8156_read_reg(ES8156_DAC_MUTE_REG13, &regv);
    if (res != ESP_OK) {
        return res;
    }
    if (enable) {
        regv = regv | BIT(1) | BIT(2);
    } else {
        regv = regv & (~(BIT(1) | BIT(2))) ;
    }
    es8156_write_reg(ES8156_DAC_MUTE_REG13, regv);
    return ESP_OK;
}

// Register values. 0x00: -95.5 dB, 0x5B: -50 dB, 0xBF: 0 dB, 0xFF: 32 dB
// Accuracy of gain is 0.5 dB
esp_err_t es8156_codec_set_voice_volume(uint8_t value) {
    return es8156_write_reg(ES8156_VOLUME_CONTROL_REG14, value);
}
