
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "hardware/why2025.h"



// Initialise the co-processor drivers.
esp_err_t bsp_why2025_coproc_init();

esp_err_t ch32_set_display_backlight(uint16_t value);
esp_err_t ch32_set_keyboard_backlight(uint16_t value);
esp_err_t bsp_c6_control(bool enable, bool boot);
esp_err_t bsp_amplifier_control(bool enable);
