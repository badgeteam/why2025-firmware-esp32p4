
// SPDX-License-Identifier: MIT

#include "bsp/disp_st7701.h"

#include "bsp/disp_mipi_dsi.h"

#include <stdlib.h>

#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_lcd_panel_commands.h>
#include <esp_lcd_panel_interface.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/cdefs.h>

#define ST7701_CMD_OPCODE (0x22)

#define ST7701_GS_PANEL  (1)
#define ST7701_SS_PANEL  (1 << 1)
#define ST7701_REV_PANEL (1 << 2)
#define ST7701_BGR_PANEL (1 << 3)

static char const TAG[] = "bsp-st7701";

static esp_err_t bsp_st7701_del(esp_lcd_panel_t *panel);
static esp_err_t bsp_st7701_reset(esp_lcd_panel_t *panel);
static esp_err_t bsp_st7701_init(esp_lcd_panel_t *panel);
static esp_err_t bsp_st7701_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t bsp_st7701_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t bsp_st7701_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t bsp_st7701_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t bsp_st7701_disp_on_off(esp_lcd_panel_t *panel, bool off);
static esp_err_t bsp_st7701_sleep(esp_lcd_panel_t *panel, bool sleep);

typedef struct {
    int         cmd;        /*<! The specific LCD command */
    void const *data;       /*<! Buffer that holds the command specific data */
    size_t      data_bytes; /*<! Size of `data` in memory, in bytes */
} st7701_lcd_init_cmd_t;

// clang-format off
static const st7701_lcd_init_cmd_t init_sequence[] = {
    // {cmd, { data }, data_size}
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5},                                                                       // Bank 3
    {0xEF, (uint8_t[]){0x08}, 1},                                                                                               //
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5},                                                                       // Bank 0
    {0xC0, (uint8_t[]){0x63, 0x00}, 2},                                                                                         // LNESET (Display Line Setting): (0x63+1)*8 = 800 lines
    {0xC1, (uint8_t[]){0x10, 0x02}, 2},                                                                                         // PORCTRL (Porch Control): VBP = 16, VFP = 2
    {0xC2, (uint8_t[]){0x37, 0x08}, 2},                                                                                         // INVSET (Inversion sel. & frame rate control): PCLK=512+(8*16) = 640
    {0xCC, (uint8_t[]){0x30}, 1},                                                                                               //
    {0xB0, (uint8_t[]){0x40, 0xC9, 0x8F, 0x0F, 0x17, 0x0B, 0x05, 0x0C, 0x0A, 0x23, 0x07, 0x5A, 0x17, 0xEA, 0x33, 0xDF}, 16},    // PVGAMCTRL
    {0xB1, (uint8_t[]){0x40, 0xCB, 0xD3, 0x0C, 0x89, 0x00, 0x00, 0x02, 0x03, 0x1C, 0x02, 0x4B, 0x0A, 0x69, 0xF3, 0xDF}, 16},    // NVGAMCTRL
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5},                                                                       // Bank 1
    {0xB0, (uint8_t[]){0x65}, 1},                                                                                               // VRHS
    {0xB1, (uint8_t[]){0x62}, 1},                                                                                               // VCOMS
    {0xB2, (uint8_t[]){0x87}, 1},                                                                                               // VGH
    {0xB3, (uint8_t[]){0x80}, 1},                                                                                               // TESTCMD
    {0xB5, (uint8_t[]){0x42}, 1},                                                                                               // VGLS
    {0xB7, (uint8_t[]){0x85}, 1},                                                                                               // PWCTRL1
    {0xB8, (uint8_t[]){0x20}, 1},                                                                                               // PWCTRL2
    {0xB9, (uint8_t[]){0x10}, 1},                                                                                               // DGMLUTR
    {0xC1, (uint8_t[]){0x78}, 1},                                                                                               // SPD1
    {0xC2, (uint8_t[]){0x78}, 1},                                                                                               // SPD2
    {0xD0, (uint8_t[]){0x88}, 1},                                                                                               // MIPISET1
    {0xE0, (uint8_t[]){0x00, 0x19, 0x02}, 3},                                                                                   //
    {0xE1, (uint8_t[]){0x05, 0xA0, 0x07, 0xA0, 0x04, 0xA0, 0x06, 0xA0, 0x00, 0x44, 0x44}, 11},                                  //
    {0xE2, (uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 13},                      //
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 5},                                                                             //
    {0xE4, (uint8_t[]){0x44, 0x44}, 2},                                                                                         //
    {0xE5, (uint8_t[]){0x0D, 0x31, 0xC8, 0xAF, 0x0F, 0x33, 0xC8, 0xAF, 0x09, 0x2D, 0xC8, 0xAF, 0x0B, 0x2F, 0xC8, 0xAF}, 16},    //
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4},                                                                             //
    {0xE7, (uint8_t[]){0x44, 0x44}, 2},                                                                                         //
    {0xE8, (uint8_t[]){0x0C, 0x30, 0xC8, 0xAF, 0x0E, 0x32, 0xC8, 0xAF, 0x08, 0x2C, 0xC8, 0xAF, 0x0A, 0x2E, 0xC8, 0xAF}, 16},    //
    {0xEB, (uint8_t[]){0x02, 0x00, 0xE4, 0xE4, 0x44, 0x00, 0x40}, 7},                                                           //
    {0xEC, (uint8_t[]){0x3C, 0x00}, 2},                                                                                         //
    {0xED, (uint8_t[]){0xAB, 0x89, 0x76, 0x54, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x10, 0x45, 0x67, 0x98, 0xBA}, 16},    //
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6},                                                                 //
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5},                                                                       // Bank 0
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5},                                                                       // Bank 3, enable BK function of command 2
    {0xE8, (uint8_t[]){0x00, 0x0E}, 2},                                                                                         //
    {0x3A, (uint8_t[]){0x50}, 1},                                                                                               // COLMOD (RGB 565)
    {0x11, (uint8_t[]){0x00}, 0},                                                                                               // SLPOUT
};
// clang-format on

typedef struct {
    esp_lcd_panel_t           base;
    esp_lcd_panel_io_handle_t io;
    int                       reset_gpio_num;
    int                       x_gap;
    int                       y_gap;
    uint8_t                   madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t                   colmod_val; // save current value of LCD_CMD_COLMOD register
    uint16_t                  init_cmds_size;
    bool                      reset_level;
} st7701_panel_t;

static esp_err_t bsp_disp_st7701_new(
    esp_lcd_panel_io_handle_t const   io,
    esp_lcd_panel_dev_config_t const *panel_dev_config,
    esp_lcd_panel_handle_t           *ret_panel
) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    st7701_panel_t *st7701 = (st7701_panel_t *)calloc(1, sizeof(st7701_panel_t));
    ESP_RETURN_ON_FALSE(st7701, ESP_ERR_NO_MEM, TAG, "no mem for st7701 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode         = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_ele_order) {
        case LCD_RGB_ELEMENT_ORDER_RGB: st7701->madctl_val = 0; break;
        case LCD_RGB_ELEMENT_ORDER_BGR: st7701->madctl_val |= LCD_CMD_BGR_BIT; break;
        default: ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported rgb element order"); break;
    }

    switch (panel_dev_config->bits_per_pixel) {
        case 16: // RGB565
            st7701->colmod_val = 0x55;
            break;
        case 18: // RGB666
            st7701->colmod_val = 0x66;
            break;
        case 24: // RGB888
            st7701->colmod_val = 0x77;
            break;
        default: ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width"); break;
    }

    st7701->io                = io;
    st7701->reset_gpio_num    = panel_dev_config->reset_gpio_num;
    st7701->reset_level       = panel_dev_config->flags.reset_active_high;
    st7701->base.del          = bsp_st7701_del;
    st7701->base.reset        = bsp_st7701_reset;
    st7701->base.init         = bsp_st7701_init;
    st7701->base.invert_color = bsp_st7701_invert_color;
    st7701->base.set_gap      = bsp_st7701_set_gap;
    st7701->base.mirror       = bsp_st7701_mirror;
    st7701->base.swap_xy      = bsp_st7701_swap_xy;
    st7701->base.disp_on_off  = bsp_st7701_disp_on_off;
    st7701->base.disp_sleep   = bsp_st7701_sleep;
    *ret_panel                = &st7701->base;

    return ESP_OK;

err:
    if (st7701) {
        bsp_st7701_del(&st7701->base);
    }
    return ret;
}

bool bsp_disp_st7701_init(bsp_device_t *dev) {
    return bsp_disp_dsi_init(dev, bsp_disp_st7701_new);
}

static esp_err_t bsp_st7701_del(esp_lcd_panel_t *panel) {
    st7701_panel_t *st7701 = __containerof(panel, st7701_panel_t, base);

    if (st7701->reset_gpio_num >= 0) {
        gpio_reset_pin(st7701->reset_gpio_num);
    }
    free(st7701);
    return ESP_OK;
}

static esp_err_t bsp_st7701_reset(esp_lcd_panel_t *panel) {
    st7701_panel_t *st7701 = __containerof(panel, st7701_panel_t, base);
    // perform hardware reset
    if (st7701->reset_gpio_num >= 0) {
        gpio_set_level(st7701->reset_gpio_num, st7701->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(st7701->reset_gpio_num, !st7701->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else { // perform software reset
        // Not supported yet
        // esp_lcd_panel_io_handle_t io = st7701->io;
        // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5ms before sending new command
    }
    return ESP_OK;
}

static esp_err_t bsp_st7701_init(esp_lcd_panel_t *panel) {
    st7701_panel_t           *st7701 = __containerof(panel, st7701_panel_t, base);
    esp_lcd_panel_io_handle_t io     = st7701->io;

    st7701_lcd_init_cmd_t const *init_cmds      = init_sequence;
    uint16_t                     init_cmds_size = sizeof(init_sequence) / sizeof(st7701_lcd_init_cmd_t);

    for (int i = 0; i < init_cmds_size; i++) {
        ESP_RETURN_ON_ERROR(
            esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes),
            TAG,
            "send command failed"
        );
    }

    // TODO: What are these?
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xE8, (uint8_t[]){0x00, 0x0C}, 2), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xE8, (uint8_t[]){0x00, 0x00}, 2), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_tx_param(io, 0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5),
        TAG,
        "send command failed"
    );
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x29, (uint8_t[]){0x00}, 0), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x13, (uint8_t[]){0x00}, 0), TAG, "send command failed");

    return ESP_OK;
}

static esp_err_t bsp_st7701_invert_color(esp_lcd_panel_t *panel, bool invert_color_data) {
    ESP_LOGW(TAG, "Invert color is not supported in ST7701 driver. Please use SW color inversion.");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t bsp_st7701_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y) {
    ESP_LOGW(TAG, "Mirror is not supported in ST7701 driver. Please use SW mirroring.");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t bsp_st7701_swap_xy(esp_lcd_panel_t *panel, bool swap_axes) {
    ESP_LOGW(TAG, "Swap XY is not supported in ST7701 driver. Please use SW rotation.");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t bsp_st7701_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap) {
    st7701_panel_t *st7701 = __containerof(panel, st7701_panel_t, base);
    st7701->x_gap          = x_gap;
    st7701->y_gap          = y_gap;
    return ESP_OK;
}

static esp_err_t bsp_st7701_disp_on_off(esp_lcd_panel_t *panel, bool on_off) {
    ESP_LOGI(TAG, "Display %s\n", on_off ? "on" : "off");
    st7701_panel_t           *st7701  = __containerof(panel, st7701_panel_t, base);
    esp_lcd_panel_io_handle_t io      = st7701->io;
    int                       command = 0;

    if (on_off) {
        command = 0x29;
    } else {
        command = 0x28;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t bsp_st7701_sleep(esp_lcd_panel_t *panel, bool sleep) {
    ESP_LOGI(TAG, "Display %s\n", sleep ? "wakeup" : "sleep");
    st7701_panel_t           *st7701  = __containerof(panel, st7701_panel_t, base);
    esp_lcd_panel_io_handle_t io      = st7701->io;
    int                       command = 0;
    if (sleep) {
        command = 0x10;
    } else {
        command = 0x11;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "io tx param failed");
    return ESP_OK;
}
