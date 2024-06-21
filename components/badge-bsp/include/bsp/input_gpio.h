
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp_device.h"



// GPIO input init function.
bool bsp_input_gpio_init(bsp_device_t *dev, uint8_t endpoint);
// GPIO input deinit function.
bool bsp_input_gpio_deinit(bsp_device_t *dev, uint8_t endpoint);
// Get current input value by raw input number.
bool bsp_input_gpio_get_raw(bsp_device_t *dev, uint8_t endpoint, uint16_t raw_input);
