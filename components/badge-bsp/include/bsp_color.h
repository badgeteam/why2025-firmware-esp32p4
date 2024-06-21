
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>



// Display / LED pixel format descriptor.
typedef enum {
    // Black/red epaper.
    BSP_PIXFMT_2_11KR,

    // 1-bit greyscale.
    BSP_PIXFMT_1_GREY,
    // 2-bit greyscale.
    BSP_PIXFMT_2_GREY,
    // 4-bit greyscale.
    BSP_PIXFMT_4_GREY,
    // 8-bit greyscale.
    BSP_PIXFMT_8_GREY,
    // 16-bit greyscale.
    BSP_PIXFMT_16_GREY,

    // 8-bit RGB.
    BSP_PIXFMT_8_332RGB,
    // 16-bit RGB.
    BSP_PIXFMT_16_565RGB,
    // 18-bit RGB.
    BSP_PIXFMT_18_666RGB,
    // 24-bit RGB.
    BSP_PIXFMT_24_888RGB,
    // 30-bit RGB.
    BSP_PIXFMT_30_101010RGB,
    // 36-bit RGB.
    BSP_PIXFMT_36_121212RGB,
    // 48-bit RGB.
    BSP_PIXFMT_48_161616RGB,
} bsp_pixfmt_t;

// Convert 16-bit greyscale to raw color data.
uint64_t bsp_grey16_to_col(bsp_pixfmt_t format, uint16_t value);
// Convert 16-bit greyscale to raw color data.
uint16_t bsp_col_to_grey16(bsp_pixfmt_t format, uint64_t value);

// Convert 16-bit greyscale to raw color data.
static inline uint64_t bsp_grey8_to_col(bsp_pixfmt_t format, uint8_t value) {
    return bsp_grey16_to_col(format, value * 0x0101);
}
// Convert 16-bit greyscale to raw color data.
static inline uint8_t bsp_col_to_grey8(bsp_pixfmt_t format, uint64_t value) {
    return bsp_col_to_grey16(format, value) >> 8;
}

// Convert 48-bit RGB to raw color data.
uint64_t bsp_rgb48_to_col(bsp_pixfmt_t format, uint64_t rgb);
// Convert raw color data to 48-bit RGB.
uint64_t bsp_col_to_rgb48(bsp_pixfmt_t format, uint64_t value);

// Convert 24-bit RGB to raw color data.
uint64_t bsp_rgb_to_col(bsp_pixfmt_t format, uint32_t rgb);
// Convert raw color data to 24-bit RGB.
uint32_t bsp_col_to_rgb(bsp_pixfmt_t format, uint64_t value);
