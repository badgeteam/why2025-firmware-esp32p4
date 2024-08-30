
// SPDX-License-Identifier: MIT

#include "bsp_pax.h"



// Table for converting BSP color formats to PAX color formats.
pax_buf_type_t const pixfmt_map[] = {
    // Black/red epaper.
    [BSP_PIXFMT_2_11KR] = PAX_BUF_2_PAL,

    // 1-bit greyscale.
    [BSP_PIXFMT_1_GREY] = PAX_BUF_1_GREY,
    // 2-bit greyscale.
    [BSP_PIXFMT_2_GREY] = PAX_BUF_2_GREY,
    // 4-bit greyscale.
    [BSP_PIXFMT_4_GREY] = PAX_BUF_4_GREY,
    // 8-bit greyscale.
    [BSP_PIXFMT_8_GREY] = PAX_BUF_8_GREY,

    // 8-bit RGB.
    [BSP_PIXFMT_8_332RGB]  = PAX_BUF_8_332RGB,
    // 16-bit RGB.
    [BSP_PIXFMT_16_565RGB] = PAX_BUF_16_565RGB,
    // 24-bit RGB.
    [BSP_PIXFMT_24_888RGB] = PAX_BUF_24_888RGB,
};
size_t const pixfmt_map_len = sizeof(pixfmt_map) / sizeof(pax_buf_type_t);
