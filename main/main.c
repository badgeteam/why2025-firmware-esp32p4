
// SPDX-License-Identifier: MIT

#include "bsp.h"

#include <stdio.h>

#include <esp_app_desc.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

#include "bsp/why2025_coproc.h"

extern void display_test();

void display_version() {
    esp_app_desc_t const *app_description = esp_app_get_description();
    printf("BADGE.TEAM %s launcher firmware v%s\n", app_description->project_name, app_description->version);
}


void app_main(void) {
    display_version();
    bsp_init();
    ch32_set_display_backlight(255);
    display_test();
}
