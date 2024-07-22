
// SPDX-License-Identifier: MIT

#include "hardware/why2025.h"

#include "bsp/why2025_coproc.h"

#include <driver/i2c.h>
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>



// SDIO bus configuration.
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

// Platform-specific BSP init code.
void bsp_platform_preinit() {
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

// Platform-specific BSP init code.
void bsp_platform_init() {
    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_c6_control(true, true));

    // Enable SDIO bus.
    ESP_ERROR_CHECK_WITHOUT_ABORT(sdmmc_host_init());
    ESP_ERROR_CHECK_WITHOUT_ABORT(sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &why2025_sdio_config));

    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_c6_init());
}
