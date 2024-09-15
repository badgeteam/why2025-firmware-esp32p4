
// SPDX-License-Identifier: MIT

#pragma once

#include "pax_gfx.h"



// Application types.
typedef enum {
    // AppFS ESP-IDF firmware.
    APP_TYPE_APPFS_ESP,
    // AppFS ELF app.
    APP_TYPE_APPFS_ELF,
    // RAM-loaded ELF app.
    APP_TYPE_RAM_ELF,
    // Python app.
    // APP_TYPE_PYTHON,
    // BadgerOS app.
    // APP_TYPE_BADGEROS,
} app_type_t;

// Application metadata.
typedef struct {
    // Application type.
    app_type_t type;
    // Application slug / ID.
    char      *id;
    // Display name.
    char      *name;
    // Description.
    char      *desc;
    // Main binary / executable path.
    char      *main_path;
    // AppFS handle.
    int        appfs_fd;
    // App icon, if any.
    pax_buf_t *icon_img;
} app_meta_t;



// List of detected apps.
extern app_meta_t *app_list;
// Number of detected apps.
extern size_t      app_list_len;

// Detect apps, updating `app_list` and `app_list_len`.
void app_detect();
// Clear the apps list.
void app_list_clear();
// Start an app.
void app_start(app_meta_t *app);
