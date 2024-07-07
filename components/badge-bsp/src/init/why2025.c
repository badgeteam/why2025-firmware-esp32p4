
// SPDX-License-Identifier: MIT

#include "hardware/why2025.h"

#include "bsp/why2025_coproc.h"

#include <driver/i2c.h>



// Platform-specific BSP init code.
void bsp_platform_init() {
    // Enable GPIO interrupts.
    gpio_install_isr_service(0);

    // Enable internal I²C bus.
    i2c_config_t i2c_conf = {
        .mode       = I2C_MODE_MASTER,
        .master = {
            .clk_speed = 400000,
        },
        .scl_io_num = BSP_I2CINT_SCL_PIN,
        .sda_io_num = BSP_I2CINT_SDA_PIN,
    };
    i2c_param_config(BSP_I2CINT_NUM, &i2c_conf);
    i2c_driver_install(BSP_I2CINT_NUM, I2C_MODE_MASTER, 0, 0, 0);

    // Install co-processor drivers.
    bsp_why2025_coproc_init();
    bsp_c6_control(true, true);
}
