
// SPDX-License-Identifier: MIT

#include "hardware/p4devkit.h"

#include "bsp_device.h"



bsp_display_devtree_t const disp_tree = {
    .common = {
#if CONFIG_BSP_PLATFORM_P4DEVKIT01_ST7701
        .type      = BSP_EP_DISP_ST7701,
#else
        .type      = BSP_EP_DISP_EK79007,
#endif
        .reset_pin = BSP_DSI_LCD_RESET_PIN,
    },
    .pixfmt = {BSP_PIXFMT_16_565RGB, false},
    .h_fp   = BSP_DSI_LCD_HFP,
    .width  = BSP_DSI_LCD_H_RES,
    .h_bp   = BSP_DSI_LCD_HBP,
    .h_sync = BSP_DSI_LCD_HSYNC,
    .v_fp   = BSP_DSI_LCD_VFP,
    .height = BSP_DSI_LCD_V_RES,
    .v_bp   = BSP_DSI_LCD_VBP,
    .v_sync = BSP_DSI_LCD_VSYNC,
    .backlight_endpoint = 0,
    .backlight_index    = 0,
#if CONFIG_BSP_PLATFORM_P4DEVKIT01_ST7701
    .orientation        = BSP_O_ROT_CW,
#else
    .orientation        = BSP_O_UPRIGHT,
#endif
};

bsp_devtree_t const tree = {
    .disp_count = 1,
    .disp_dev = (bsp_display_devtree_t const *const[]) {
        &disp_tree,
    },
};



// Platform-specific BSP init code.
void bsp_platform_preinit() {
}

// Platform-specific BSP init code.
void bsp_platform_init() {
    // Register BSP device tree.
    bsp_dev_register(&tree, true);
}
