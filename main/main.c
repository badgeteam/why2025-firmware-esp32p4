
// SPDX-License-Identifier: MIT

#include "main.h"

#include "appfs.h"
// #include "bsp.h"
// #include "bsp/why2025_coproc.h"
// #include "bsp_device.h"
#include "bsp/device.h"
#include "pax_gfx.h"
#include "pax_gui.h"

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

void app_main(void) {
    esp_err_t res;
    // Initialize the hardware.
    bsp_init();

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
}
