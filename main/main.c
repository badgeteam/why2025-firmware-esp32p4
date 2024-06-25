// SPDX-License-Identifier: MIT

#include <stdio.h>
#include "bsp.h"
#include "bsp/es8156.h"
#include "freertos/FreeRTOS.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/i2s_std.h"
#include "bsp/why2025_coproc.h"
#include "display_test.h"
#include "techdemo.h"
#include "sid.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "fonts/chakrapetchmedium.h"
#include "rvswd.h"
#include <string.h>

extern const uint8_t ch32_firmware_start[] asm("_binary_ch32_firmware_bin_start");
extern const uint8_t ch32_firmware_end[] asm("_binary_ch32_firmware_bin_end");

static char const *TAG = "main";

void display_version() {
    esp_app_desc_t const *app_description = esp_app_get_description();
    printf("BADGE.TEAM %s launcher firmware v%s\n", app_description->project_name, app_description->version);
}

bool start_demo = false;

i2s_chan_handle_t i2s_handle = NULL;

uint8_t volume = 120;
uint8_t screen_brightness = 51;
uint8_t keyboard_brightness = 51;

esp_err_t i2s_test() {
    // I2S audio
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(0, I2S_ROLE_MASTER);

    esp_err_t res = i2s_new_channel(&chan_cfg, &i2s_handle, NULL);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing I2S channel failed");
        return res;
    }

    i2s_std_config_t i2s_config = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg =
            {
                .mclk = GPIO_NUM_30,
                .bclk = GPIO_NUM_29,
                .ws   = GPIO_NUM_31,
                .dout = GPIO_NUM_28,
                .din  = I2S_GPIO_UNUSED,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv   = false,
                    },
            },
    };

    res = i2s_channel_init_std_mode(i2s_handle, &i2s_config);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Configuring I2S channel failed");
        return res;
    }

    res = i2s_channel_enable(i2s_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Enabling I2S channel failed");
        return res;
    }

    res = es8156_init();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing codec failed");
        return res;
    }

    res = es8156_codec_set_voice_volume(volume);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Set volume on codec failed");
        return res;
    }

    res = sid_init(i2s_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Initializing SID emulator failed");
        return res;
    }

    res = bsp_amplifier_control(true);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Enabling amplifier failed");
        return res;
    }

    return ESP_OK;
}

esp_err_t sd_test(sd_pwr_ctrl_handle_t pwr_ctrl_handle) {
    esp_err_t res;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    const char mount_point[] = "/sdcard";
    ESP_LOGI(TAG, "Initializing SD card");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.pwr_ctrl_handle = pwr_ctrl_handle;

    sdmmc_slot_config_t slot_config = {
        .clk = GPIO_NUM_43,
        .cmd = GPIO_NUM_44,
        .d0 = GPIO_NUM_39,
        .d1 = GPIO_NUM_40,
        .d2 = GPIO_NUM_41,
        .d3 = GPIO_NUM_42,
        .d4 = GPIO_NUM_NC,
        .d5 = GPIO_NUM_NC,
        .d6 = GPIO_NUM_NC,
        .d7 = GPIO_NUM_NC,
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 4,
        .flags = 0,
    };

    ESP_LOGI(TAG, "Mounting filesystem");
    res = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (res != ESP_OK) {
        if (res == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount SD card filesystem.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the SD card (%s). ", esp_err_to_name(res));
        }
        return res;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    sdmmc_card_print_info(stdout, card);
    return ESP_OK;
}

// Colors
pax_col_t color_background = 0xFFEEEAEE;
pax_col_t color_primary    = 0xFF01BC99;
pax_col_t color_secondary  = 0xFFFFCF53;
pax_col_t color_tertiary   = 0xFFFF017F;
pax_col_t color_quaternary = 0xFF340132;

void draw_test() {
    pax_buf_t* fb = display_test_get_buf();

    pax_background(fb, color_background);

    // Footer test
    int footer_height      = 32;
    int footer_vmarign     = 7;
    int footer_hmarign     = 20;
    int footer_text_height = 16;
    int footer_text_hmarign = 20;
    pax_font_t* footer_text_font = &chakrapetchmedium;

    int footer_box_height = footer_height + (footer_vmarign * 2);
    pax_draw_line(fb, color_quaternary, footer_hmarign, pax_buf_get_height(fb) - footer_box_height, pax_buf_get_width(fb) - footer_hmarign, pax_buf_get_height(fb) - footer_box_height);
    pax_right_text(fb, color_quaternary, footer_text_font, footer_text_height, pax_buf_get_width(fb) - footer_hmarign - footer_text_hmarign, pax_buf_get_height(fb) - footer_box_height + (footer_box_height - footer_text_height) / 2, "↑ / ↓ / ← / → Navigate   🅰 Start");


    // Font test
    /*pax_draw_text(fb, color_quaternary, pax_font_sky, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 1, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(fb, color_quaternary, pax_font_sky_mono, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 2, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(fb, color_quaternary, pax_font_marker, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 3, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(fb, color_quaternary, pax_font_saira_condensed, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 4, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(fb, color_quaternary, pax_font_saira_regular, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 5, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");
    pax_draw_text(fb, color_quaternary, &chakrapetchmedium, footer_text_height, footer_hmarign + footer_text_hmarign,  (footer_vmarign + footer_text_height) * 6, "Test test test 🅰🅱🅷🅼🆂🅴↑↓←→⤓");*/

    int circle_count = 6;
    int circle_spacing = 68;
    int circle_offset = (pax_buf_get_width(fb) - circle_spacing * (circle_count - 1)) / 2;

    for (int i = 0; i < circle_count; i++) {
        pax_draw_circle(fb, 0xFFFFFFFF, circle_offset + circle_spacing * i, 375, 28);
        pax_outline_circle(fb, 0xFFE4E4E4, circle_offset + circle_spacing * i, 375, 28);
    }

    int square_count = 5;
    int square_voffset = 116;
    int square_hoffset = 62;
    int square_size = 170;
    int square_selected = 0;
    int square_spacing = 1;
    int square_marign = 2;

    pax_col_t square_colors[] = {
        0xFFFFFFFF,
        color_secondary,
        color_tertiary,
        color_quaternary,
        color_primary,
    };

    for (int i = 0; i < square_count; i++) {
        if (i == square_selected) {
            pax_draw_rect(fb, color_primary, square_hoffset + (square_size + square_spacing) * i, square_voffset, square_size, square_size);
        }
        pax_draw_rect(fb, square_colors[i], square_hoffset + (square_size + square_spacing) * i + square_marign, square_voffset + square_marign, square_size - square_marign * 2, square_size - square_marign * 2);
    }

    int title_text_voffset = square_voffset - 32;
    int title_text_hoffset = square_hoffset - 2;
    int title_text_height = 20;
    pax_font_t* title_text_font = &chakrapetchmedium;

    pax_draw_text(fb, color_primary, title_text_font, title_text_height, title_text_hoffset, title_text_voffset, "Example Application Title");

    pax_right_text(fb, color_quaternary, title_text_font, 20, pax_buf_get_width(fb) - 40, 32, "23:37 :D");

    pax_push_2d(fb);
    pax_apply_2d(fb, matrix_2d_translate(pax_buf_get_width(fb) / 2, pax_buf_get_height(fb) / 2));
    pax_apply_2d(fb, matrix_2d_rotate(180 * 0.01745329252));
    pax_center_text(fb, 0xFF000000, pax_font_marker, 100, 0, -150, "Renze");
    pax_pop_2d(fb);
    display_test_flush();
}

void draw_key(bsp_input_t input) {
    pax_buf_t* fb = display_test_get_buf();
    pax_background(fb, color_background);
    pax_push_2d(fb);
    pax_apply_2d(fb, matrix_2d_translate(pax_buf_get_width(fb) / 2, pax_buf_get_height(fb) / 2));

    char text[128];
    sprintf(text, "Pressed key %04x", input);

    pax_center_text(fb, 0xFF000000, &chakrapetchmedium, 50, 0, -25, text);
    pax_pop_2d(fb);
    display_test_flush();
}

void draw_text(char* text) {
    pax_buf_t* fb = display_test_get_buf();
    pax_background(fb, color_background);
    pax_push_2d(fb);
    pax_apply_2d(fb, matrix_2d_translate(pax_buf_get_width(fb) / 2, pax_buf_get_height(fb) / 2));
    pax_center_text(fb, 0xFF000000, &chakrapetchmedium, 50, 0, -25, text);
    pax_pop_2d(fb);
    display_test_flush();
}

int current_key = -1;
static SemaphoreHandle_t demo_semaphore;

void blink_keyboard() {
    for (uint8_t i = 0; i < 3; i++) {
        ch32_set_keyboard_backlight(255);
        vTaskDelay(pdMS_TO_TICKS(100));
        ch32_set_keyboard_backlight(0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

#define CH32_REG_DEBUG_DATA0        0x04  // Data register 0, can be used for temporary storage of data
#define CH32_REG_DEBUG_DATA1        0x05  // Data register 1, can be used for temporary storage of data
#define CH32_REG_DEBUG_DMCONTROL    0x10  // Debug module control register
#define CH32_REG_DEBUG_DMSTATUS     0x11  // Debug module status register
#define CH32_REG_DEBUG_HARTINFO     0x12  // Microprocessor status register
#define CH32_REG_DEBUG_ABSTRACTCS   0x16  // Abstract command status register
#define CH32_REG_DEBUG_COMMAND      0x17  // Astract command register
#define CH32_REG_DEBUG_ABSTRACTAUTO 0x18  // Abstract command auto-executtion
#define CH32_REG_DEBUG_PROGBUF0     0x20  // Instruction cache register 0
#define CH32_REG_DEBUG_PROGBUF1     0x21  // Instruction cache register 1
#define CH32_REG_DEBUG_PROGBUF2     0x22  // Instruction cache register 2
#define CH32_REG_DEBUG_PROGBUF3     0x23  // Instruction cache register 3
#define CH32_REG_DEBUG_PROGBUF4     0x24  // Instruction cache register 4
#define CH32_REG_DEBUG_PROGBUF5     0x25  // Instruction cache register 5
#define CH32_REG_DEBUG_PROGBUF6     0x26  // Instruction cache register 6
#define CH32_REG_DEBUG_PROGBUF7     0x27  // Instruction cache register 7
#define CH32_REG_DEBUG_HALTSUM0     0x40  // Halt status register
#define CH32_REG_DEBUG_CPBR         0x7C  // Capability register
#define CH32_REG_DEBUG_CFGR         0x7D  // Configuration register
#define CH32_REG_DEBUG_SHDWCFGR     0x7E  // Shadow configuration register

#define CH32_REGS_CSR 0x0000  // Offsets for accessing CSRs.
#define CH32_REGS_GPR 0x1000  // Offsets for accessing general-purpose (x)registers.

#define CH32_CFGR_KEY   0x5aa50000
#define CH32_CFGR_OUTEN (1 << 10)

// The start of CH32 CODE FLASH region.
#define CH32_CODE_BEGIN 0x08000000
// the end of the CH32 CODE FLASH region.
#define CH32_CODE_END   0x08004000

// FLASH status register.
#define CH32_FLASH_STATR 0x4002200C
// FLASH configuration register.
#define CH32_FLASH_CTLR  0x40022010
// FLASH address register.
#define CH32_FLASH_ADDR  0x40022014

// FLASH is busy writing or erasing.
#define CH32_FLASH_STATR_BUSY (1 << 0)
// FLASH is busy writing
#define CH32_FLASH_STATR_WRBUSY (1 << 1)
// FLASH is finished with the operation.
#define CH32_FLASH_STATR_EOP  (1 << 5)

// Perform standard programming operation.
#define CH32_FLASH_CTLR_PG      (1 << 0)
// Perform 1K sector erase.
#define CH32_FLASH_CTLR_PER     (1 << 1)
// Perform full FLASH erase.
#define CH32_FLASH_CTLR_MER     (1 << 2)
// Perform user-selected word program.
#define CH32_FLASH_CTLR_OBG     (1 << 4)
// Perform user-selected word erasure.
#define CH32_FLASH_CTLR_OBER    (1 << 5)
// Start an erase operation.
#define CH32_FLASH_CTLR_STRT    (1 << 6)
// Lock the FLASH.
#define CH32_FLASH_CTLR_LOCK    (1 << 7)
// Start a fast page programming operation (256 bytes).
#define CH32_FLASH_CTLR_FTPG    (1 << 16)
// Start a fast page erase operation (256 bytes).
#define CH32_FLASH_CTLR_FTER    (1 << 17)
// Start a page programming operation (256 bytes).
#define CH32_FLASH_CTLR_PGSTRT  (1 << 21)

const uint8_t ch32_readmem[] = {
  0x88, 0x41, 0x02, 0x90
};

const uint8_t ch32_writemem[] = {
  0x88, 0xc1, 0x02, 0x90
};

rvswd_result_t ch32_halt_microprocessor(rvswd_handle_t* handle) {
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Make the debug module work properly
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Initiate a halt request

    // Get the debug module status information, check rdata[9:8], if the value is 0b11,
    // it means the processor enters the halt state normally. Otherwise try again.
    uint8_t timeout = 5;
    while (1) {
        uint32_t value;
        rvswd_read(handle, CH32_REG_DEBUG_DMSTATUS, &value);
        if (((value >> 8) & 0b11) == 0b11) {  // Check that processor has entered halted state
            break;
        }
        if (timeout == 0) {
            ESP_LOGE(TAG, "Failed to halt microprocessor, DMSTATUS=%" PRIx32, value);
            return false;
        }
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the halt request
    ESP_LOGI(TAG, "Microprocessor halted");
    return RVSWD_OK;
}

rvswd_result_t ch32_resume_microprocessor(rvswd_handle_t* handle) {
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Make the debug module work properly
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Initiate a halt request
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the halt request
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x40000001);  // Initiate a resume request

    // Get the debug module status information, check rdata[17:16],
    // if the value is 0b11, it means the processor has recovered.
    uint8_t timeout = 5;
    while (1) {
        uint32_t value;
        rvswd_read(handle, CH32_REG_DEBUG_DMSTATUS, &value);
        if ((((value >> 10) & 0b11) == 0b11)) {
            break;
        }
        if (timeout == 0) {
            ESP_LOGE(TAG, "Failed to resume microprocessor, DMSTATUS=%" PRIx32, value);
            return RVSWD_FAIL;
        }
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return RVSWD_OK;
}

rvswd_result_t ch32_reset_microprocessor_and_run(rvswd_handle_t* handle) {
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Make the debug module work properly
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x80000001);  // Initiate a halt request
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the halt request
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000003);  // Initiate a core reset request

    uint8_t timeout = 5;
    while (1) {
        uint32_t value;
        rvswd_read(handle, CH32_REG_DEBUG_DMSTATUS, &value);
        if (((value >> 18) & 0b11) == 0b11) {  // Check that processor has been reset
            break;
        }
        if (timeout == 0) {
            ESP_LOGE(TAG, "Failed to reset microprocessor");
            return RVSWD_FAIL;
        }
        timeout--;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the core reset request
    vTaskDelay(pdMS_TO_TICKS(10));
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x10000001);  // Clear the core reset status signal
    vTaskDelay(pdMS_TO_TICKS(10));
    rvswd_write(handle, CH32_REG_DEBUG_DMCONTROL, 0x00000001);  // Clear the core reset status signal clear request
    vTaskDelay(pdMS_TO_TICKS(10));

    return RVSWD_OK;
}

bool ch32_write_cpu_reg(rvswd_handle_t* handle, uint16_t regno, uint32_t value) {
    uint32_t command = regno         // Register to access.
                       | (1 << 16)   // Write access.
                       | (1 << 17)   // Perform transfer.
                       | (2 << 20)   // 32-bit register access.
                       | (0 << 24);  // Access register command.

    rvswd_write(handle, CH32_REG_DEBUG_DATA0, value);
    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);
    return true;
}

bool ch32_read_cpu_reg(rvswd_handle_t* handle, uint16_t regno, uint32_t* value_out) {
    uint32_t command = regno         // Register to access.
                       | (0 << 16)   // Read access.
                       | (1 << 17)   // Perform transfer.
                       | (2 << 20)   // 32-bit register access.
                       | (0 << 24);  // Access register command.

    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);
    rvswd_read(handle, CH32_REG_DEBUG_DATA0, value_out);
    return true;
}

bool ch32_run_debug_code(rvswd_handle_t* handle, const void* code, size_t code_size) {
    if (code_size > 8 * 4) {
        ESP_LOGE(TAG, "Debug program is too long (%zd/%zd)", code_size, (size_t)8 * 4);
        return false;
    } else if (code_size & 1) {
        ESP_LOGE(TAG, "Debug program size must be a multiple of 2 (%zd)", code_size);
        return false;
    }

    // Copy into program buffer.
    uint32_t tmp[8] = {0};
    memcpy(tmp, code, code_size);
    for (size_t i = 0; i < 8; i++) {
        rvswd_write(handle, CH32_REG_DEBUG_PROGBUF0 + i, tmp[i]);
    }

    // Run program buffer.
    uint32_t command = (0 << 17)     // Do not perform transfer.
                       | (1 << 18)   // Run program buffer afterwards.
                       | (2 << 20)   // 32-bit register access.
                       | (0 << 24);  // Access register command.
    rvswd_write(handle, CH32_REG_DEBUG_COMMAND, command);

    return true;
}

bool ch32_read_memory_word(rvswd_handle_t* handle, uint32_t address, uint32_t* value_out) {
    ch32_write_cpu_reg(handle, CH32_REGS_GPR + 11, address);
    ch32_run_debug_code(handle, ch32_readmem, sizeof(ch32_readmem));
    ch32_read_cpu_reg(handle, CH32_REGS_GPR + 10, value_out);
    return true;
}

bool ch32_write_memory_word(rvswd_handle_t* handle, uint32_t address, uint32_t value) {
    ch32_write_cpu_reg(handle, CH32_REGS_GPR + 10, value);
    ch32_write_cpu_reg(handle, CH32_REGS_GPR + 11, address);
    ch32_run_debug_code(handle, ch32_writemem, sizeof(ch32_writemem));
    return true;
}

// Wait for the FLASH chip to finish its current operation.
static void ch32_wait_flash(rvswd_handle_t* handle) {
    uint32_t value = 0;
    ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);

    printf("Wait for operation to complete...\r\n");

    while (value & CH32_FLASH_STATR_BUSY) {
        vTaskDelay(1);
        ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);
    }
}

static void ch32_wait_flash_write(rvswd_handle_t* handle) {
    uint32_t value = 0;
    ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);
    while (value & CH32_FLASH_STATR_WRBUSY) {
        vTaskDelay(1);
        ch32_read_memory_word(handle, CH32_FLASH_STATR, &value);
    }
}

// Unlock the FLASH if not already unlocked.
bool ch32_unlock_flash(rvswd_handle_t* handle) {
    uint32_t ctlr;
    ch32_read_memory_word(handle, CH32_FLASH_CTLR, &ctlr);

    printf("CTLR before unlock: %08" PRIx32 "\r\n", ctlr);

    if (!(ctlr & 0x8080)) {
        // FLASH already unlocked.
        printf("Already unlocked\r\n");
        return true;
    }

    // Enter the unlock keys.
    ch32_write_memory_word(handle, 0x40022004, 0x45670123);
    ch32_write_memory_word(handle, 0x40022004, 0xCDEF89AB);
    ch32_write_memory_word(handle, 0x40022008, 0x45670123);
    ch32_write_memory_word(handle, 0x40022008, 0xCDEF89AB);
    ch32_write_memory_word(handle, 0x40022024, 0x45670123);
    ch32_write_memory_word(handle, 0x40022024, 0xCDEF89AB);

    // Check again if FLASH is unlocked.
    ch32_read_memory_word(handle, CH32_FLASH_CTLR, &ctlr);

    printf("CTLR after unlock: %08" PRIx32 "\r\n", ctlr);

    return !(ctlr & 0x8080);
}

// If unlocked: Erase a 256-byte block of FLASH.
bool ch32_erase_flash_block(rvswd_handle_t* handle, uint32_t addr) {
    if (addr % 256)
        return false;
    ch32_wait_flash(handle);
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, CH32_FLASH_CTLR_FTER);
    ch32_write_memory_word(handle, CH32_FLASH_ADDR, addr);
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, CH32_FLASH_CTLR_FTER | CH32_FLASH_CTLR_STRT);
    ch32_wait_flash(handle);
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, 0);
    return true;
}

// If unlocked: Write a 256-byte block of FLASH.
bool ch32_write_flash_block(rvswd_handle_t* handle, uint32_t addr, const void* data) {
    if (addr % 256)
        return false;

    ch32_wait_flash(handle);
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, CH32_FLASH_CTLR_FTPG);

    ch32_write_memory_word(handle, CH32_FLASH_ADDR, addr);

    uint32_t wdata[64];
    memcpy(wdata, data, sizeof(wdata));
    for (size_t i = 0; i < 64; i++) {
        printf("Writing 0x%08" PRIx32 ": 0x%08" PRIx32 "...\r\n", addr + i * 4, wdata[i]);
        ch32_write_memory_word(handle, addr + i * 4, wdata[i]);
        ch32_wait_flash_write(handle);
    }

    printf("Start programming...\r\n");
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, CH32_FLASH_CTLR_FTPG | CH32_FLASH_CTLR_PGSTRT);
    ch32_wait_flash(handle);
    ch32_write_memory_word(handle, CH32_FLASH_CTLR, 0);
    printf("Done programming\r\n");
    vTaskDelay(1);

    uint32_t rdata[64];
    for (size_t i = 0; i < 64; i++) {
        vTaskDelay(0);
        ch32_read_memory_word(handle, addr + i * 4, &rdata[i]);
    }
    if (memcmp(wdata, rdata, sizeof(wdata))) {
        ESP_LOGE(TAG, "Write block mismatch at %08" PRIx32, addr);
        ESP_LOGE(TAG, "Write:");
        for (size_t i = 0; i < 64; i++) {
            ESP_LOGE(TAG, "%zx: %08" PRIx32, i, wdata[i]);
        }
        ESP_LOGE(TAG, "Read:");
        for (size_t i = 0; i < 64; i++) {
            ESP_LOGE(TAG, "%zx: %08" PRIx32, i, rdata[i]);
        }
        return false;
    }

    return true;
}

// If unlocked: Erase and write a range of FLASH memory.
bool ch32_write_flash(rvswd_handle_t* handle, uint32_t addr, const void* _data, size_t data_len) {
    if (addr % 64) {
        return false;
    }

    const uint8_t* data = _data;

    for (size_t i = 0; i < data_len; i += 256) {
        vTaskDelay(0);
        printf("Erasing 0x%08" PRIx32"...\r\n", addr + i);
        if (!ch32_erase_flash_block(handle, addr + i)) {
            ESP_LOGE(TAG, "Error: Failed to erase FLASH at %08" PRIx32, addr + i);
            return false;
        }

        printf("Writing 0x%08" PRIx32"...\r\n", addr + i);
        if (!ch32_write_flash_block(handle, addr + i, data + i)) {
            ESP_LOGE(TAG, "Error: Failed to write FLASH at %08" PRIx32, addr + i);
            return false;
        }
    }

    return true;
}

void rvswd_test(void) {
    rvswd_result_t res;

    rvswd_handle_t handle = {
        .swdio = 22,
        .swclk = 23,
    };

    res = rvswd_init(&handle);

    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "Init error %u!", res);
        return;
    }

    res = rvswd_reset(&handle);

    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "Reset error %u!", res);
        return;
    }

    res = ch32_halt_microprocessor(&handle);
    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "Failed to halt");
        return;
    }

    bool bool_res = ch32_unlock_flash(&handle);

    printf("Unlock: %s\r\n", bool_res ? "yes" : "no");

    bool_res = ch32_write_flash(&handle, 0x08000000, ch32_firmware_start, ch32_firmware_end - ch32_firmware_start);
    if (!bool_res) {
        ESP_LOGE(TAG, "Failed to write flash");
        return;
    }

    res = ch32_reset_microprocessor_and_run(&handle);
    if (res != RVSWD_OK) {
        ESP_LOGE(TAG, "Failed to reset and run");
        return;
    }

    ESP_LOGI(TAG, "Okay!");

    /*while (1) {

        // Halt
        rvswd_write(&handle, 0x10, 0x80000001);
        rvswd_write(&handle, 0x10, 0x80000001);

        // Read status
        uint32_t value = 0;
        res = rvswd_read(&handle, 0x11, &value);
        if (res == RVSWD_OK) {
            printf("Status after halt: %08" PRIx32 "\n", value);
        } else {
            ESP_LOGE(TAG, "Status error %u!", res);
        }

        // Dump some memory

        for (uint32_t address = 0x0; address < 0x10; address++) {
            uint32_t value = 0;
            ch32_read_memory_word(&handle, address, &value);
            printf("0x%08" PRIx32 "  0x%08" PRIx32 "\r\n", address, value);
        }
        printf("\r\n");

        // Resume
        rvswd_write(&handle, 0x10, 0x40000001);
        rvswd_write(&handle, 0x10, 0x40000001);

        // Read status
        value = 0;
        res = rvswd_read(&handle, 0x11, &value);
        if (res == RVSWD_OK) {
            printf("Status after resume: %08" PRIx32 "\n", value);
        } else {
            ESP_LOGE(TAG, "Status error %u!", res);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }*/

}

void app_main(void) {

    demo_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(demo_semaphore);

    display_version();
    bsp_init();

    rvswd_test();

    uint16_t version;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ch32_get_firmware_version(&version);
        printf("Firmware version %"PRIu16"\r\n", version);
    }

    ch32_set_display_backlight(screen_brightness*5);
    ch32_set_keyboard_backlight(keyboard_brightness*5);
    bsp_c6_control(true, true);
    //bsp_c6_control(false, true);
    display_test();


    draw_text("RVSWD TEST");
    ch32_set_keyboard_backlight(0);
    


    pax_buf_t clipbuffer;
    pax_buf_init(&clipbuffer, NULL, 800, 480, PAX_BUF_1_PAL);
    //pax_buf_set_orientation(&clipbuffer, PAX_O_ROT_CW);

    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = 4,
    };

    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;
    esp_err_t res = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &pwr_ctrl_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create a new on-chip LDO power control driver");
        return;
    }

    res = sd_test(pwr_ctrl_handle);
    if (res != ESP_OK) {
        char text[128];
        sprintf(text, "SD card failed:\n%s", esp_err_to_name(res));
        draw_text(text);
    } else {
        draw_text("SD card OK");
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    /*gpio_config_t swio_config = {
        .pin_bit_mask = BIT64(22),
        .mode         = GPIO_MODE_OUTPUT_OD,
        .pull_up_en   = true,
        .pull_down_en = false,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    res = gpio_config(&swio_config);
    if (res != ESP_OK) {
        return;
    }

    gpio_config_t swck_config = {
        .pin_bit_mask = BIT64(23),
        .mode         = GPIO_MODE_OUTPUT_OD,
        .pull_up_en   = true,
        .pull_down_en = false,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    res = gpio_config(&swck_config);
    if (res != ESP_OK) {
        return;
    }

    gpio_set_level(22, 1);
    gpio_set_level(23, 1);*/

    /*ESP_LOGW(TAG, "TEST PINS");
    while (1) {
        gpio_set_level(22, 1);
        gpio_set_level(23, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(22, 0);
        gpio_set_level(23, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }*/

    i2s_test();

    while (1) {
        draw_test();

        start_demo = false;
        while (!start_demo) {
            xSemaphoreTake(demo_semaphore, portMAX_DELAY);
            if (current_key < 0) {
                draw_test();
            } else {
                draw_key(current_key);
            }
        }
        pax_techdemo_init(display_test_get_buf(), &clipbuffer);
        bool finished = false;
        size_t start = esp_timer_get_time() / 1000;
        while (!finished) {
            size_t now = esp_timer_get_time() / 1000;
            finished = pax_techdemo_draw(now - start);
            size_t after = esp_timer_get_time() / 1000;
            display_test_flush();
            int delay = 33 - (after - now);
            if (delay < 0) delay = 0;
            //ESP_LOGI("MAIN", "Delay %d, took %d", delay, after - now);
            vTaskDelay(pdMS_TO_TICKS(delay));
        }
    }
}

void demo_call(bsp_input_t input, bool pressed) {
    if (input == 42 && pressed) {
        start_demo = true;
    } else if (input == 1 && pressed) {
        if (volume > 0x00) {
            volume--;
        }
        es8156_codec_set_voice_volume(volume);
    } else if (input == 2 && pressed) {
        if (volume < 0xFF) {
            volume++;
        }
        es8156_codec_set_voice_volume(volume);
    } else if (input == 5 && pressed) {
        if (screen_brightness > 0) {
            screen_brightness--;
        }
    ch32_set_display_backlight(screen_brightness*5);
    } else if (input == 6 && pressed) {
        if (screen_brightness < 51) {
            screen_brightness++;
        }
        ch32_set_display_backlight(screen_brightness*5);
    } else if (input == 7 && pressed) {
        if (keyboard_brightness > 0) {
            keyboard_brightness--;
        }
        ch32_set_keyboard_backlight(keyboard_brightness*5);
    } else if (input == 0x18 && pressed) {
        if (keyboard_brightness < 51) {
            keyboard_brightness++;
        }
        ch32_set_keyboard_backlight(keyboard_brightness*5);
    } else if (pressed) {
        current_key = input;
    } else {
        current_key = -1;
    }
    xSemaphoreGive(demo_semaphore);
}
