
// SPDX-License-Identifier: MIT

#include "main.h"

#include "appfs.h"
#include "arrays.h"
#include "bsp.h"
#include "bsp/why2025_coproc.h"
#include "bsp_device.h"
#include "bsp_pax.h"
#include "ch32v203prog.h"
#include "menus/root.h"
#include "pax_gfx.h"
#include "pax_gui.h"
/*
#include "driver/isp.h"
#include "esp_cam_ctlr_csi.h"
#include "esp_cam_ctlr.h"
#include "esp_cache.h"
#include "driver/i2c_master.h"
#include "example_sensor_init.h"
*/

#include <stdio.h>

#include <esp_app_desc.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/stat.h>

char const TAG[] = "main";

#define MAX_MENU_DEPTH 16



extern uint8_t const ch32_firmware_start[] asm("_binary_ch32_firmware_bin_start");
extern uint8_t const ch32_firmware_end[] asm("_binary_ch32_firmware_bin_end");

// Current GUI root element.
static pgui_elem_t *gui;
// GUI top bar.
static pgui_elem_t *top_bar;
// GUI bottom bar.
static pgui_elem_t *bottom_bar;
// Global framebuffer.
static pax_buf_t   *gfx;
// Top-level menu screen GUI root element.
static menu_entry_t root_menu;
// Menu stack.
static menu_entry_t menu_stack[MAX_MENU_DEPTH];
// Number of menus on the stack.
static size_t       menu_stack_len;
// Previsous value of `menu_stack_len`; used to clean up.
static size_t       menu_stack_prev;
// Need to update active menu.
static bool         menu_change = true;



// Set current screen.
static void menu_enable(menu_entry_t menu) {
    if (menu.hide_top) {
        pgui_enable_flags(top_bar, PGUI_FLAG_HIDDEN);
    } else {
        pgui_disable_flags(top_bar, PGUI_FLAG_HIDDEN);
    }
    if (menu.bottom_text) {
        pgui_set_text(bottom_bar, menu.bottom_text);
        pgui_disable_flags(bottom_bar, PGUI_FLAG_HIDDEN);
    } else {
        pgui_enable_flags(bottom_bar, PGUI_FLAG_HIDDEN);
    }
    pgui_enable_flags(menu.root, PGUI_FLAG_TOPLEVEL);
    pgui_child_replace(gui, 1, menu.root);
    pgui_set_selection(gui, 1);
}

// Set the top-level menu screen.
void menu_set_root(menu_entry_t root) {
    root_menu = root;
}

// Enter new menu screen.
void menu_push(menu_entry_t menu) {
    if (menu_stack_len >= MAX_MENU_DEPTH) {
        ESP_LOGE(TAG, "Menu stack is full! Please increase MAX_MENU_DEPTH.");
        return;
    }
    menu_stack[menu_stack_len++] = menu;
    menu_change                  = true;
}

// Exit current menu screen.
void menu_pop() {
    if (menu_stack_len) {
        menu_stack_len--;
        menu_change = true;
    }
}



void app_main(void) {
    esp_err_t res;
    bsp_preinit();

    // Read CH32 version.
    uint16_t version = 0xffff;
    res              = bsp_ch32_version(&version);
    if (res) {
        ESP_LOGW(TAG, "Unable to read CH32 version");
    } else if (version != BSP_CH32_VERSION) {
        ESP_LOGI(
            TAG,
            "CH32 version 0x%04" PRIx16 " too %s (expected %04" PRIx16 ")",
            version,
            version < BSP_CH32_VERSION ? "old" : "new",
            BSP_CH32_VERSION
        );
    } else {
        ESP_LOGI(TAG, "CH32 version 0x%04" PRIx16, version);
    }

    // Program the CH32 if there is a mismatch.
    if (res || version != BSP_CH32_VERSION) {
        ESP_LOGI(TAG, "Programming CH32");
        rvswd_handle_t handle = {
            .swdio = 22,
            .swclk = 23,
        };
        ch32_program(&handle, ch32_firmware_start, ch32_firmware_end - ch32_firmware_start);
        esp_restart();
    }

    // Initialize the hardware.
    bsp_init();
    gfx = bsp_pax_buf_from_ep(1, 0);
    if (!gfx) {
        ESP_LOGE(TAG, "Failed to create framebuffer");
        esp_restart();
    }

    pax_background(gfx, 0xFFFFFF00);
    bsp_disp_update(1, 0, pax_buf_get_pixels(gfx));

    /*ESP_LOGW("BSP", "Waiting...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGW("BSP", "Enable C6...");
    ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_c6_control(true, true));*/

    // if (mkdir("/int/apps", 0777)) {
    //     ESP_LOGE(TAG, "No /int/apps :c");
    //     return;
    // }
    // if (mkdir("/int/apps/app_ok", 0777)) {
    //     ESP_LOGE(TAG, "No /int/apps/app_ok :c");
    //     return;
    // }
    // FILE *fd = fopen("/int/apps/app_ok/meta.json", "w");
    // if (!fd) {
    //     ESP_LOGE(TAG, "No FD :c");
    //     return;
    // }
    // fputs(
    //     "{\n    \"type\": \"AppFS2\",\n    \"name\": \"Test application\",\n    \"desc\": \"This is a simple "
    //     "metadata entry for testing the application loader\",\n    \"main\": \"/appfs/test\",\n    \"icon\":""
    //     "\"icon.png\"\n}", fd
    // );
    // fclose(fd);
    // return;

    // Initialize AppFS so the app launcher can use it.
    appfsInit(APPFS_PART_TYPE, APPFS_PART_SUBTYPE);
    fopen("/appfs/app_ok", "rb");

    // Compose top-level GUI.
    gui = pgui_new_grid2(1, 3);
    pgui_enable_flags(
        gui,
        PGUI_FLAG_NOPADDING | PGUI_FLAG_NOBORDER | PGUI_FLAG_NOSEPARATOR | PGUI_FLAG_FIX_WIDTH | PGUI_FLAG_FIX_HEIGHT
    );
    pgui_set_size(gui, pax_buf_get_dims(gfx));
    pgui_set_row_growable(gui, 0, false);
    pgui_set_row_growable(gui, 2, false);
    {
        // Create top bar.
        top_bar = pgui_new_grid2(2, 1);
        pgui_set_variant(top_bar, PGUI_VARIANT_PANEL);
        pgui_enable_flags(top_bar, PGUI_FLAG_NOBORDER | PGUI_FLAG_NOSEPARATOR | PGUI_FLAG_NOROUNDING);
        pgui_child_replace(gui, 0, top_bar);
        {
            // Misc decor.
            pgui_elem_t *top_left = pgui_new_text("13:37");
            pgui_set_variant(top_left, PGUI_VARIANT_PANEL);
            pgui_set_halign(top_left, PAX_ALIGN_BEGIN);
            pgui_child_replace(top_bar, 0, top_left);

            pgui_elem_t *top_right = pgui_new_text("42%");
            pgui_set_variant(top_right, PGUI_VARIANT_PANEL);
            pgui_set_halign(top_right, PAX_ALIGN_END);
            pgui_child_replace(top_bar, 1, top_right);
        }

        // Create bottom bar.
        bottom_bar = pgui_new_text(NULL);
        pgui_enable_flags(bottom_bar, PGUI_FLAG_NOROUNDING);
        pgui_disable_flags(bottom_bar, PGUI_FLAG_NOBACKGROUND);
        pgui_set_variant(bottom_bar, PGUI_VARIANT_PANEL);
        pgui_set_halign(bottom_bar, PAX_ALIGN_BEGIN);
        pgui_child_replace(gui, 2, bottom_bar);
    }

    // Set up the menu screens.
    menu_root_init();
    menu_enable(root_menu);
    bsp_disp_backlight(1, 0, 65535);
    bsp_input_backlight(1, 0, 65535);

    bool needs_draw   = true;
    bool needs_redraw = false;
    while (true) {
        bsp_event_t event;
        if (menu_change) {
            while (menu_stack_prev > menu_stack_len) {
                menu_stack_prev--;
                if (menu_stack[menu_stack_prev].on_close) {
                    menu_stack[menu_stack_prev].on_close(menu_stack[menu_stack_prev].on_close_cookie);
                }
            }
            menu_stack_prev = menu_stack_len;
            menu_enable(menu_stack_len ? menu_stack[menu_stack_len - 1] : root_menu);
            needs_draw = true;
            pgui_calc_layout(pax_buf_get_dims(gfx), gui, NULL);
        }

        if (needs_draw) {
            // Full re-draw required.
            pax_background(gfx, pgui_get_default_theme()->palette[PGUI_VARIANT_DEFAULT].bg_col);
            pgui_draw(gfx, gui, NULL);
            bsp_disp_update(1, 0, pax_buf_get_pixels(gfx));
            needs_draw   = false;
            needs_redraw = false;

        } else if (needs_redraw) {
            // Partial re-draw required.
            pgui_redraw(gfx, gui, NULL);
            bsp_disp_update(1, 0, pax_buf_get_pixels(gfx));
            needs_redraw = false;
        }

        // Run all pending events.
        uint64_t timeout = UINT64_MAX;
        while (bsp_event_wait(&event, timeout)) {
            if (event.type == BSP_EVENT_INPUT) {

                if (event.input.type == BSP_INPUT_EVENT_PRESS) {
                    if (event.input.input == 0) {
                        ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_c6_control(false, true));
                    }
                    if (event.input.input == 262) {
                        ESP_ERROR_CHECK_WITHOUT_ABORT(bsp_c6_control(true, true));
                    }
                }

                // Convert BSP event to PGUI event.
                pgui_event_t p_event = {
                    .type    = event.input.type,
                    .input   = event.input.nav_input,
                    .value   = event.input.text_input,
                    .modkeys = event.input.modkeys,
                };
                // Run event through GUI.
                pgui_resp_t resp = pgui_event(pax_buf_get_dims(gfx), gui, NULL, p_event);
                if (resp) {
                    // Mark as dirty.
                    if (resp == PGUI_RESP_CAPTURED_DIRTY) {
                        needs_draw = true;
                    }
                    needs_redraw = true;
                } else if (p_event.input == PGUI_INPUT_BACK && p_event.type == PGUI_EVENT_TYPE_PRESS) {
                    // Exit current screen and go back one level.
                    menu_pop();
                }
            }
            timeout = 0;
        }
    }
}

// Camera stuff
/*
static bool s_camera_get_new_vb(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data);
static bool s_camera_get_finished_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data);

void camera_main(void* frame_buffer, size_t frame_buffer_size)
{
    esp_err_t ret = ESP_FAIL;

    gpio_config_t dp_config = {
        .pin_bit_mask = BIT64(1),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = true,
        .pull_down_en = false,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&dp_config);
    if (ret != ESP_OK) {
        return;
    }

    gpio_set_level(1, true);

    esp_cam_ctlr_trans_t new_trans = {
        .buffer = frame_buffer,
        .buflen = frame_buffer_size,
    };

    //--------Camera Sensor and SCCB Init-----------//
    i2c_master_bus_handle_t i2c_bus_handle = NULL;
    example_sensor_init(I2C_NUM_0, &i2c_bus_handle);

    //---------------CSI Init------------------//
    esp_cam_ctlr_csi_config_t csi_config = {
        .ctlr_id = 0,
        .h_res = 800,
        .v_res = 480,
        .lane_bit_rate_mbps = EXAMPLE_MIPI_CSI_LANE_BITRATE_MBPS,
        .input_data_color_type = CAM_CTLR_COLOR_RAW8,
        .output_data_color_type = CAM_CTLR_COLOR_RGB565,
        .data_lane_num = 2,
        .byte_swap_en = false,
        .queue_items = 1,
    };
    esp_cam_ctlr_handle_t cam_handle = NULL;
    ret = esp_cam_new_csi_ctlr(&csi_config, &cam_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "csi init fail[%d]", ret);
        return;
    }

    esp_cam_ctlr_evt_cbs_t cbs = {
        .on_get_new_trans = s_camera_get_new_vb,
        .on_trans_finished = s_camera_get_finished_trans,
    };
    if (esp_cam_ctlr_register_event_callbacks(cam_handle, &cbs, &new_trans) != ESP_OK) {
        ESP_LOGE(TAG, "ops register fail");
        return;
    }

    ESP_ERROR_CHECK(esp_cam_ctlr_enable(cam_handle));

    //---------------ISP Init------------------//
    isp_proc_handle_t isp_proc = NULL;
    esp_isp_processor_cfg_t isp_config = {
        .clk_hz = 80 * 1000 * 1000,
        .input_data_source = ISP_INPUT_DATA_SOURCE_CSI,
        .input_data_color_type = ISP_COLOR_RAW8,
        .output_data_color_type = ISP_COLOR_RGB565,
        .has_line_start_packet = false,
        .has_line_end_packet = false,
        .h_res = 800,
        .v_res = 480,
    };
    ESP_ERROR_CHECK(esp_isp_new_processor(&isp_config, &isp_proc));
    ESP_ERROR_CHECK(esp_isp_enable(isp_proc));

    //---------------DPI Reset------------------//

    //init to all white
    memset(frame_buffer, 0xFF, frame_buffer_size);
    esp_cache_msync((void *)frame_buffer, frame_buffer_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M);

    if (esp_cam_ctlr_start(cam_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Driver start fail");
        return;
    }

    while (1) {
        ESP_ERROR_CHECK(esp_cam_ctlr_receive(cam_handle, &new_trans, ESP_CAM_CTLR_MAX_DELAY));
        bsp_disp_update(1, 0, frame_buffer);
    }
}

static bool s_camera_get_new_vb(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    esp_cam_ctlr_trans_t new_trans = *(esp_cam_ctlr_trans_t *)user_data;
    trans->buffer = new_trans.buffer;
    trans->buflen = new_trans.buflen;

    return false;
}

static bool s_camera_get_finished_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    return false;
}*/

void camera_test_open(void) {
    return;
    //void* frame_buffer = malloc(800*480*2);
    //camera_main(frame_buffer, sizeof(frame_buffer));
}

