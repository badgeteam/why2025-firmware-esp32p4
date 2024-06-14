/**
 * Copyright (c) 2023 Nicolai Electronics
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "driver/i2s_std.h"
#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

esp_err_t sid_init(i2s_chan_handle_t i2s_handle);

#ifdef __cplusplus
}
#endif //__cplusplus
