
// SPDX-License-Identifier: MIT

#pragma once

#include "hardware/why2025.h"

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t es8156_init(void);
esp_err_t es8156_standby(void);
esp_err_t es8156_reset(void);
esp_err_t es8156_resume(void);
esp_err_t es8156_codec_set_voice_mute(bool enable);
esp_err_t es8156_codec_set_voice_volume(uint8_t volume);