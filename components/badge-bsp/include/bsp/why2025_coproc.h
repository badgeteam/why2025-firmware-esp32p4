
// SPDX-License-Identifier: MIT

#pragma once

#include "esp_err.h"
#include "hardware/why2025.h"



// Initialise the co-processor drivers.
esp_err_t bsp_why2025_coproc_init();

// Turn on the ESP32-C6.
void bsp_c6_poweron();
// Turn off the ESP32-C6.
void bsp_c6_poweroff();
