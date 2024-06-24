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

void rvswd_test(void) {
    rvswd_result_t res;

    rvswd_handle_t handle = {
        .swdio = 22,
        .swclk = 23,
    };

    res = rvswd_init(&handle);

    if (res == RVSWD_OK) {
        ESP_LOGI(TAG, "Init OK!");
    } else {
        ESP_LOGE(TAG, "Init error %u!", res);
    }

    res = rvswd_reset(&handle);

    if (res == RVSWD_OK) {
        ESP_LOGI(TAG, "Reset OK!");
    } else {
        ESP_LOGE(TAG, "Reset error %u!", res);
    }

    while (1) {

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
    }

}

void app_main(void) {

    demo_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(demo_semaphore);

    display_version();
    bsp_init();
    ch32_set_display_backlight(screen_brightness*5);
    ch32_set_keyboard_backlight(keyboard_brightness*5);
    bsp_c6_control(true, true);
    //bsp_c6_control(false, true);
    display_test();


    draw_text("RVSWD TEST");
    ch32_set_keyboard_backlight(0);
    rvswd_test();


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
