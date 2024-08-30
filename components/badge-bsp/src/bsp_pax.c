
// SPDX-License-Identifier: MIT

#include "bsp_pax.h"



// Convert BSP color formats to PAX color formats.
static pax_buf_type_t pixfmt_conv(bsp_pixfmt_t fmt) {
    switch (fmt) {
        default: return -1;

        // Black/red epaper.
        case BSP_PIXFMT_2_11KR: return PAX_BUF_2_PAL;

        // 1-bit greyscale.
        case BSP_PIXFMT_1_GREY: return PAX_BUF_1_GREY;
        // 2-bit greyscale.
        case BSP_PIXFMT_2_GREY: return PAX_BUF_2_GREY;
        // 4-bit greyscale.
        case BSP_PIXFMT_4_GREY: return PAX_BUF_4_GREY;
        // 8-bit greyscale.
        case BSP_PIXFMT_8_GREY: return PAX_BUF_8_GREY;

        // 8-bit RGB.
        case BSP_PIXFMT_8_332RGB: return PAX_BUF_8_332RGB;
        // 16-bit RGB.
        case BSP_PIXFMT_16_565RGB: return PAX_BUF_16_565RGB;
        // 24-bit RGB.
        case BSP_PIXFMT_24_888RGB: return PAX_BUF_24_888RGB;
    }
}

// Black/red epaper palette.
static pax_col_t const palette_11kr[3] = {
    0xffffffff,
    0xff000000,
    0xffff0000,
};

// Convert BSP color formats to PAX palettes.
static void pixfmt_apply_palette(pax_buf_t *buf, bsp_pixfmt_t fmt) {
    switch (fmt) {
        default: break;

        // Black/red epaper.
        case BSP_PIXFMT_2_11KR: pax_buf_set_palette_rom(buf, palette_11kr, 3); break;
    }
}



// Create an appropriate PAX buffer given display endpoint.
pax_buf_t *pax_pax_buf_from_ep(uint32_t dev_id, uint8_t endpoint) {
    rc_t           dt   = bsp_dev_get_devtree(dev_id);
    bsp_devtree_t *tree = dt->data;
    if (tree->disp_count <= endpoint) {
        return NULL;
    }
    pax_buf_t *buf = bsp_pax_buf_from_tree(tree->disp_dev[endpoint]);
    rc_delete(dt);
    return buf;
}

// Create an appropriate PAX buffer given display devtree.
pax_buf_t *bsp_pax_buf_from_tree(bsp_display_devtree_t const *tree) {
    pax_buf_type_t pixfmt = pixfmt_conv(tree->pixfmt.color);
    if (pixfmt == -1 || tree->pixfmt.reversed) {
        return NULL;
    }
    pax_buf_t *buf = pax_buf_init(NULL, tree->width, tree->height, pixfmt);
    pixfmt_apply_palette(buf, tree->pixfmt.color);
    pax_buf_set_orientation(buf, tree->orientation);
    return buf;
}
