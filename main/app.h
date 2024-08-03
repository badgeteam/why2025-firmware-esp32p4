
// SPDX-License-Identifier: MIT

#pragma once

#include <appfs.h>
#include <esp_image_format.h>

// Chip ID used to distinguish firmwares from apps.
#define APP_CHIP_ID       0xcafe
// Virtual address of the jump table.
#define APP_JUMPTAB_VADDR 0x43F80000
// Maximum size of the jump table.
#define APP_JUMPTAB_SIZE  65536

// Loadable application segment.
typedef struct {
    // File offset.
    size_t offset;
    // Virtual address.
    size_t vaddr;
    // Segment length.
    size_t len;
} app_segment_t;

// Information about an application stored in AppFS.
typedef struct {
    esp_image_header_t header;
    bool               is_firmware;
    app_segment_t      segments[ESP_IMAGE_MAX_SEGMENTS];
    uint8_t            segments_len;
} app_info_t;



// Get info about an ESP image in an AppFS file.
esp_err_t app_info(appfs_handle_t fd, app_info_t *info);
// Reboot into a firmware or load and start an application.
esp_err_t app_start(appfs_handle_t fd, app_info_t *info);
