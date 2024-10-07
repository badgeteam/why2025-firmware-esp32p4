
// SPDX-License-Identifier: MIT

#include "hardware/why2025.h"

#include "bsp/why2025_coproc.h"

#include <driver/i2c.h>
#include <driver/sdmmc_host.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

static char const TAG[] = "why2025";



static bsp_input_devtree_t const input_tree = {
    .common = {
        .type = BSP_EP_INPUT_WHY2025_CH32,
    },
    .category           = BSP_INPUT_CAT_KEYBOARD,
    .keymap             = &bsp_keymap_why2025,
    .backlight_endpoint = 0,
    .backlight_index    = 1,
};

static bsp_input_devtree_t const input2_tree = {
    .common   = {
        .type = BSP_EP_INPUT_GPIO,
    },
    .category = BSP_INPUT_CAT_GENERIC,
    .pinmap   = &(bsp_pinmap_t const) {
        .pins_len  = 1,
        .pins      = (uint8_t const[]) {35},
        .activelow = false,
    },
};

static bsp_led_devtree_t const led_tree = {
    .common   = {
        .type = BSP_EP_LED_WHY2025_CH32,
    },
    .num_leds = 2,
    .ledfmt   = {
        .color = BSP_PIXFMT_16_GREY,
    }
};

static bsp_display_devtree_t const disp_tree = {
    .common = {
        .type      = BSP_EP_DISP_ST7701,
        .reset_pin = 0,
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
    .orientation        = BSP_O_ROT_CW,
};

static bsp_devtree_t const tree = {
    .input_count = 2,
    .input_dev = (bsp_input_devtree_t const *const[]) {
        &input_tree,
        &input2_tree,
    },
    .led_count = 1,
    .led_dev = (bsp_led_devtree_t const *const[]){
        &led_tree,
    },
    .disp_count = 1,
    .disp_dev   = (bsp_display_devtree_t const *const[]) {
        &disp_tree,
    },
};



// Enable the WHY2025 badge internal I²C bus.
bool                 why2025_enable_i2cint = true;
// SD-card SDIO handle thing.
static sdmmc_card_t *sd_card;
// SDMMC host.
sdmmc_host_t         sdcard_host = SDMMC_HOST_DEFAULT();

// ESP32-C6 SDIO bus configuration.
sdmmc_slot_config_t const why2025_sdio_config = {
    .clk   = BSP_SDIO_CLK,
    .cmd   = BSP_SDIO_CMD,
    .d0    = BSP_SDIO_D0,
    .d1    = BSP_SDIO_D1,
    .d2    = BSP_SDIO_D2,
    .d3    = BSP_SDIO_D3,
    .d4    = BSP_SDIO_D4,
    .d5    = BSP_SDIO_D5,
    .d6    = BSP_SDIO_D6,
    .d7    = BSP_SDIO_D7,
    .cd    = BSP_SDIO_CD,
    .wp    = BSP_SDIO_WP,
    .width = BSP_SDIO_WIDTH,
    .flags = BSP_SDIO_FLAGS,
};

// SD-card SDIO bus configuration.
sdmmc_slot_config_t const why2025_sdcard_config = {
    .clk   = 0,
    .cmd   = 0,
    .d0    = 0,
    .d1    = 0,
    .d2    = 0,
    .d3    = 0,
    .d4    = 0,
    .d5    = 0,
    .d6    = 0,
    .d7    = 0,
    .cd    = 0,
    .wp    = 0,
    .width = BSP_SDCARD_WIDTH,
    .flags = BSP_SDCARD_FLAGS,
};

// Platform-specific BSP init code.
void bsp_platform_preinit() {
    // Check I²C pin levels.
    gpio_set_direction(BSP_I2CINT_SCL_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(BSP_I2CINT_SDA_PIN, GPIO_MODE_INPUT);
    esp_rom_delay_us(100);
    if (!gpio_get_level(BSP_I2CINT_SCL_PIN)) {
        ESP_LOGW(TAG, "SCL pin is being held LOW");
        why2025_enable_i2cint = false;
        return;
    }
    if (!gpio_get_level(BSP_I2CINT_SDA_PIN)) {
        ESP_LOGW(TAG, "SDA pin is being held LOW");
        why2025_enable_i2cint = false;
        return;
    }

    // Enable GPIO interrupts.
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_install_isr_service(0));

    // Enable internal I²C bus.
    i2c_config_t i2c_conf = {
        .mode       = I2C_MODE_MASTER,
        .master = {
            .clk_speed = 400000,
        },
        .scl_io_num = BSP_I2CINT_SCL_PIN,
        .sda_io_num = BSP_I2CINT_SDA_PIN,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_param_config(BSP_I2CINT_NUM, &i2c_conf));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_driver_install(BSP_I2CINT_NUM, I2C_MODE_MASTER, 0, 0, 0));

    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_why2025_coproc_init());
}

// Try to mount SDcard.
static void bsp_mount_sdcard() {
    sdcard_host.slot                     = SDMMC_HOST_SLOT_0;
    esp_vfs_fat_mount_config_t mount_cfg = VFS_FAT_MOUNT_DEFAULT_CONFIG();
    esp_err_t res = esp_vfs_fat_sdmmc_mount("/sd", &sdcard_host, &why2025_sdcard_config, &mount_cfg, &sd_card);
    if (res) {
        ESP_LOGE(TAG, "SDcard mount error %s (%d)", esp_err_to_name(res), res);
    }
}

// Try to mount internal FAT filesystem.
static void bsp_mount_fatfs() {
    esp_vfs_fat_mount_config_t mount_cfg = VFS_FAT_MOUNT_DEFAULT_CONFIG();
    mount_cfg.format_if_mount_failed     = true;
    wl_handle_t wl_handle                = WL_INVALID_HANDLE;
    esp_err_t   res                      = esp_vfs_fat_spiflash_mount_rw_wl("/int", NULL, &mount_cfg, &wl_handle);
    if (res) {
        ESP_LOGE(TAG, "FAT mount error %s (%d)", esp_err_to_name(res), res);
    } else {
        ESP_LOGI(TAG, "FAT filesystem mounted");
    }
}

// Platform-specific BSP init code.
void bsp_platform_init() {
    // Try to mount SDcard.
    bsp_mount_sdcard();

    // Enable C6.
    //ESP_ERROR_CHECK_WITHOUT_ABORT(sdmmc_host_init());
    //ESP_ERROR_CHECK_WITHOUT_ABORT(sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &why2025_sdio_config));
    //ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_c6_init());

    // Try to mount internal FAT filesystem.
    bsp_mount_fatfs();

    // Register BSP device tree.
    bsp_dev_register(&tree, true);
}
