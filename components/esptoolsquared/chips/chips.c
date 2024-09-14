
// SPDX-License-Identifier: MIT

#include "chips.h"



#ifdef CONFIG_ET2_SUPPORT_ESP32C2
et2_chip_t const et2_chip_esp32c2 = {
    .stub        = &stub_esp32c2,
    .flash_enc   = true,
    .ram_block   = DEFAULT_RAM_BLOCK,
    .flash_block = DEFAULT_FLASH_BLOCK,
};
#endif

#ifdef CONFIG_ET2_SUPPORT_ESP32C3
et2_chip_t const et2_chip_esp32c3 = {
    .stub        = &stub_esp32c3,
    .flash_enc   = true,
    .ram_block   = DEFAULT_RAM_BLOCK,
    .flash_block = DEFAULT_FLASH_BLOCK,
};
#endif

#ifdef CONFIG_ET2_SUPPORT_ESP32C6
et2_chip_t const et2_chip_esp32c6 = {
    .stub        = &stub_esp32c6,
    .flash_enc   = true,
    .ram_block   = DEFAULT_RAM_BLOCK,
    .flash_block = DEFAULT_FLASH_BLOCK,
};
#endif

#ifdef CONFIG_ET2_SUPPORT_ESP32P4
et2_chip_t const et2_chip_esp32p4 = {
    .stub        = &stub_esp32p4,
    .flash_enc   = true,
    .ram_block   = DEFAULT_RAM_BLOCK,
    .flash_block = DEFAULT_FLASH_BLOCK,
};
#endif

#ifdef CONFIG_ET2_SUPPORT_ESP32S2
et2_chip_t const et2_chip_esp32s2 = {
    .stub        = &stub_esp32s2,
    .flash_enc   = true,
    .ram_block   = DEFAULT_RAM_BLOCK,
    .flash_block = DEFAULT_FLASH_BLOCK,
};
#endif

#ifdef CONFIG_ET2_SUPPORT_ESP32S3
et2_chip_t const et2_chip_esp32s3 = {
    .stub        = &stub_esp32s3,
    .flash_enc   = true,
    .ram_block   = DEFAULT_RAM_BLOCK,
    .flash_block = DEFAULT_FLASH_BLOCK,
};
#endif
