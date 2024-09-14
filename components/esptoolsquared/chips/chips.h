
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sdkconfig.h>

typedef struct {
    uint8_t const *text;
    size_t         text_len;
    size_t         text_start;
    uint8_t const *data;
    size_t         data_len;
    size_t         data_start;
    size_t         bss_start;
    size_t         entry;
} et2_stub_t;

typedef struct {
    // Flasher stub.
    et2_stub_t const *stub;
    // Chip supports FLASH encryption.
    bool              flash_enc;
    // Maximum block size for RAM.
    uint32_t          ram_block;
    // Maximum block size for FLASH.
    uint32_t          flash_block;
} et2_chip_t;

#define DEFAULT_RAM_BLOCK   0x1800
#define DEFAULT_FLASH_BLOCK 0x4000



#ifdef CONFIG_ET2_SUPPORT_ESP32C2
extern et2_stub_t const stub_esp32c2;
extern et2_chip_t const et2_chip_esp32c2;
#endif

#ifdef CONFIG_ET2_SUPPORT_ESP32C3
extern et2_stub_t const stub_esp32c3;
extern et2_chip_t const et2_chip_esp32c3;
#endif

#ifdef CONFIG_ET2_SUPPORT_ESP32C6
extern et2_stub_t const stub_esp32c6;
extern et2_chip_t const et2_chip_esp32c6;
#endif

#ifdef CONFIG_ET2_SUPPORT_ESP32P4
extern et2_stub_t const stub_esp32p4;
extern et2_chip_t const et2_chip_esp32p4;
#endif

#ifdef CONFIG_ET2_SUPPORT_ESP32S2
extern et2_stub_t const stub_esp32s2;
extern et2_chip_t const et2_chip_esp32s2;
#endif

#ifdef CONFIG_ET2_SUPPORT_ESP32S3
extern et2_stub_t const stub_esp32s3;
extern et2_chip_t const et2_chip_esp32s3;
#endif
