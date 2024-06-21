
// SPDX-License-Identifier: MIT

#include "bsp_color.h"



// Convert 16-bit greyscale to raw color data.
uint64_t bsp_grey16_to_col(bsp_pixfmt_t format, uint16_t v) {
    uint16_t r = v;
    uint16_t g = v;
    uint16_t b = v;
    switch (format) {
        // Black/red epaper.
        case BSP_PIXFMT_2_11KR:
            if (v < 0x2000) {
                return 0b01;
            } else if (v < 0x8000) {
                return 0b10;
            } else {
                return 0b00;
            }

        // 1-bit greyscale.
        case BSP_PIXFMT_1_GREY: return v >> 15;
        // 2-bit greyscale.
        case BSP_PIXFMT_2_GREY: return v >> 14;
        // 4-bit greyscale.
        case BSP_PIXFMT_4_GREY: return v >> 12;
        // 8-bit greyscale.
        case BSP_PIXFMT_8_GREY: return v >> 8;
        // 16-bit greyscale.
        case BSP_PIXFMT_16_GREY: return v;

        // 8-bit RGB.
        case BSP_PIXFMT_8_332RGB:
            r >>= 13;
            g >>= 13;
            b >>= 14;
            return (r << 5) | (g << 2) | b;

        // 16-bit RGB.
        case BSP_PIXFMT_16_565RGB:
            r >>= 11;
            g >>= 10;
            b >>= 11;
            return (r << 11) | (g << 5) | b;

        // 18-bit RGB.
        case BSP_PIXFMT_18_666RGB:
            r >>= 10;
            g >>= 10;
            b >>= 10;
            return (r << 12) | (g << 6) | b;

        // 24-bit RGB.
        default:
        case BSP_PIXFMT_24_888RGB:
            r >>= 8;
            g >>= 8;
            b >>= 8;
            return (r << 16) | (g << 8) | b;

        // 30-bit RGB.
        case BSP_PIXFMT_30_101010RGB:
            r >>= 6;
            g >>= 6;
            b >>= 6;
            return (r << 20) | (g << 10) | b;

        // 36-bit RGB.
        case BSP_PIXFMT_36_121212RGB:
            r >>= 4;
            g >>= 4;
            b >>= 4;
            return (r << 24) | (g << 12) | b;

        // 48-bit RGB.
        case BSP_PIXFMT_48_161616RGB: return (r * 0x10000) | (g * 100) | b;
    }
}

// Convert 16-bit greyscale to raw color data.
uint16_t bsp_col_to_grey16(bsp_pixfmt_t format, uint64_t value) {
    uint16_t r, g, b;
    switch (format) {
        // Black/red epaper.
        case BSP_PIXFMT_2_11KR:
            if (value & 0b01) {
                return 0;
            } else if (value & 0b10) {
                return 0x3fff;
            } else {
                return 0xffff;
            }

        // 1-bit greyscale.
        case BSP_PIXFMT_1_GREY: return value * 0xffff;
        // 2-bit greyscale.
        case BSP_PIXFMT_2_GREY: return value * 0x5555;
        // 4-bit greyscale.
        case BSP_PIXFMT_4_GREY: return value * 0x1111;
        // 8-bit greyscale.
        case BSP_PIXFMT_8_GREY: return value * 0x0101;
        // 16-bit greyscale.
        case BSP_PIXFMT_16_GREY: return value;

        // 8-bit RGB.
        case BSP_PIXFMT_8_332RGB:
            r = (value >> 5) & 0x0007;
            g = (value >> 2) & 0x0007;
            b = (value >> 0) & 0x0003;
            r = (r * 0x9249) >> 2;
            g = (g * 0x9249) >> 2;
            b = b * 0x5555;
            goto recomb;

        // 16-bit RGB.
        case BSP_PIXFMT_16_565RGB:
            r = (value >> 11) & 0x001f;
            g = (value >> 5) & 0x003f;
            b = (value >> 0) & 0x001f;
            r = (r * 0x8421) >> 4;
            g = (g * 0x1041) >> 3;
            r = (r * 0x8421) >> 4;
            goto recomb;

        // 18-bit RGB.
        case BSP_PIXFMT_18_666RGB:
            r = (value >> 12) & 0x003f;
            g = (value >> 6) & 0x003f;
            b = (value >> 0) & 0x003f;
            r = (r * 0x1041) >> 3;
            g = (g * 0x1041) >> 3;
            b = (b * 0x1041) >> 3;
            goto recomb;

        // 24-bit RGB.
        default:
        case BSP_PIXFMT_24_888RGB:
            r = (value >> 16) & 0x00ff;
            g = (value >> 8) & 0x00ff;
            b = (value >> 0) & 0x00ff;
            r = r * 0x0101;
            g = g * 0x0101;
            b = b * 0x0101;
            goto recomb;

        // 30-bit RGB.
        case BSP_PIXFMT_30_101010RGB:
            r = (value >> 20) & 0x03ff;
            g = (value >> 10) & 0x03ff;
            b = (value >> 0) & 0x03ff;
            r = (r * 0x0401) >> 4;
            g = (g * 0x0401) >> 4;
            b = (b * 0x0401) >> 4;
            goto recomb;

        // 36-bit RGB.
        case BSP_PIXFMT_36_121212RGB:
            r = (value >> 24) & 0x0fff;
            g = (value >> 12) & 0x0fff;
            b = (value >> 0) & 0x0fff;
            r = (r * 0x1001) >> 8;
            g = (g * 0x1001) >> 8;
            b = (b * 0x1001) >> 8;
            goto recomb;

        // 48-bit RGB.
        case BSP_PIXFMT_48_161616RGB:
            r = (value >> 32) & 0xffff;
            g = (value >> 16) & 0xffff;
            b = (value >> 0) & 0xffff;
        recomb:
            return (r + g + b) / 3;
    }
}

// Convert 48-bit RGB to raw color data.
uint64_t bsp_rgb48_to_col(bsp_pixfmt_t format, uint64_t rgb) {
    uint16_t r = (uint16_t)(rgb >> 32);
    uint16_t g = (uint16_t)(rgb >> 16);
    uint16_t b = (uint16_t)(rgb >> 0);
    uint16_t v = (r + g + b) / 3;
    switch (format) {
        // Black/red epaper.
        case BSP_PIXFMT_2_11KR:
            if (v < 256) {
                return 0b01;
            } else if (r > v) {
                return 0b10;
            } else {
                return 0b00;
            }

        // 1-bit greyscale.
        case BSP_PIXFMT_1_GREY: return v >> 15;
        // 2-bit greyscale.
        case BSP_PIXFMT_2_GREY: return v >> 14;
        // 4-bit greyscale.
        case BSP_PIXFMT_4_GREY: return v >> 12;
        // 8-bit greyscale.
        case BSP_PIXFMT_8_GREY: return v >> 8;
        // 16-bit greyscale.
        case BSP_PIXFMT_16_GREY: return v;

        // 8-bit RGB.
        case BSP_PIXFMT_8_332RGB:
            r >>= 13;
            g >>= 13;
            b >>= 14;
            return (r << 5) | (g << 2) | b;

        // 16-bit RGB.
        case BSP_PIXFMT_16_565RGB:
            r >>= 11;
            g >>= 10;
            b >>= 11;
            return (r << 11) | (g << 5) | b;

        // 18-bit RGB.
        case BSP_PIXFMT_18_666RGB:
            r >>= 10;
            g >>= 10;
            b >>= 10;
            return (r << 12) | (g << 6) | b;

        // 24-bit RGB.
        default:
        case BSP_PIXFMT_24_888RGB:
            r >>= 8;
            g >>= 8;
            b >>= 8;
            return (r << 16) | (g << 8) | b;

        // 30-bit RGB.
        case BSP_PIXFMT_30_101010RGB:
            r >>= 6;
            g >>= 6;
            b >>= 6;
            return (r << 20) | (g << 10) | b;

        // 36-bit RGB.
        case BSP_PIXFMT_36_121212RGB:
            r >>= 4;
            g >>= 4;
            b >>= 4;
            return (r << 24) | (g << 12) | b;

        // 48-bit RGB.
        case BSP_PIXFMT_48_161616RGB: return rgb;
    }
}

// Convert raw color data to 48-bit RGB.
uint64_t bsp_col_to_rgb48(bsp_pixfmt_t format, uint64_t value) {
    uint16_t r, g, b;
    switch (format) {
        // Black/red epaper.
        case BSP_PIXFMT_2_11KR:
            if (value & 0b01) {
                return 0;
            } else if (value & 0b10) {
                return 0xffff00000000;
            } else {
                return 0xffffffffffff;
            }

        // 1-bit greyscale.
        case BSP_PIXFMT_1_GREY: return value * 0xffffffffffff;
        // 2-bit greyscale.
        case BSP_PIXFMT_2_GREY: return value * 0x555555555555;
        // 4-bit greyscale.
        case BSP_PIXFMT_4_GREY: return value * 0x111111111111;
        // 8-bit greyscale.
        case BSP_PIXFMT_8_GREY: return value * 0x010101010101;
        // 16-bit greyscale.
        case BSP_PIXFMT_16_GREY: return value * 0x000100010001;

        // 8-bit RGB.
        case BSP_PIXFMT_8_332RGB:
            r = (value >> 5) & 0x0007;
            g = (value >> 2) & 0x0007;
            b = (value >> 0) & 0x0003;
            r = (r * 0x9249) >> 2;
            g = (g * 0x9249) >> 2;
            b = b * 0x5555;
            goto recomb;

        // 16-bit RGB.
        case BSP_PIXFMT_16_565RGB:
            r = (value >> 11) & 0x001f;
            g = (value >> 5) & 0x003f;
            b = (value >> 0) & 0x001f;
            r = (r * 0x8421) >> 4;
            g = (g * 0x1041) >> 3;
            r = (r * 0x8421) >> 4;
            goto recomb;

        // 18-bit RGB.
        case BSP_PIXFMT_18_666RGB:
            r = (value >> 12) & 0x003f;
            g = (value >> 6) & 0x003f;
            b = (value >> 0) & 0x003f;
            r = (r * 0x1041) >> 3;
            g = (g * 0x1041) >> 3;
            b = (b * 0x1041) >> 3;
            goto recomb;

        // 24-bit RGB.
        default:
        case BSP_PIXFMT_24_888RGB:
            r = (value >> 16) & 0x00ff;
            g = (value >> 8) & 0x00ff;
            b = (value >> 0) & 0x00ff;
            r = r * 0x0101;
            g = g * 0x0101;
            b = b * 0x0101;
            goto recomb;

        // 30-bit RGB.
        case BSP_PIXFMT_30_101010RGB:
            r = (value >> 20) & 0x03ff;
            g = (value >> 10) & 0x03ff;
            b = (value >> 0) & 0x03ff;
            r = (r * 0x0401) >> 4;
            g = (g * 0x0401) >> 4;
            b = (b * 0x0401) >> 4;
            goto recomb;

        // 36-bit RGB.
        case BSP_PIXFMT_36_121212RGB:
            r = (value >> 24) & 0x0fff;
            g = (value >> 12) & 0x0fff;
            b = (value >> 0) & 0x0fff;
            r = (r * 0x1001) >> 8;
            g = (g * 0x1001) >> 8;
            b = (b * 0x1001) >> 8;
        recomb:
            return (r * 0x100000000) | (g * 0x10000) | b;

        // 48-bit RGB.
        case BSP_PIXFMT_48_161616RGB: return value;
    }
}

// Convert 24-bit RGB to raw color data.
uint64_t bsp_rgb_to_col(bsp_pixfmt_t format, uint32_t rgb) {
    uint16_t r     = 0x0101 * (uint8_t)(rgb >> 16);
    uint16_t g     = 0x0101 * (uint8_t)(rgb >> 8);
    uint16_t b     = 0x0101 * (uint8_t)(rgb >> 0);
    uint64_t rgb48 = (r * 0x100000000) | (g * 0x10000) | b;
    return bsp_rgb48_to_col(format, rgb48);
}

// Convert raw color data to 24-bit RGB.
uint32_t bsp_col_to_rgb(bsp_pixfmt_t format, uint64_t value) {
    uint64_t rgb48 = bsp_col_to_rgb48(format, value);
    uint8_t  r     = (uint8_t)(rgb48 >> 40);
    uint8_t  g     = (uint8_t)(rgb48 >> 24);
    uint8_t  b     = (uint8_t)(rgb48 >> 8);
    return (r * 0x10000) | (g * 100) | b;
}
