
// SPDX-License-Identifier: MIT

#pragma once

#include <appfs.h>
#include <esp_image_format.h>

// Chip ID used to distinguish firmwares from apps.
#define APPFS2_CHIP_ID       0xcafe
// Virtual address of the jump table.
#define APPFS2_JUMPTAB_VADDR 0x43F80000
// Maximum size of the jump table.
#define APPFS2_JUMPTAB_SIZE  65536

// Loadable application segment.
typedef struct {
    // File offset.
    size_t offset;
    // Virtual address.
    size_t vaddr;
    // Segment length.
    size_t len;
} appfs2_seg_t;

// Information about an application stored in AppFS.
typedef struct {
    esp_image_header_t header;
    bool               is_firmware;
    appfs2_seg_t       segments[ESP_IMAGE_MAX_SEGMENTS];
    uint8_t            segments_len;
} appfs2_info_t;



// Get info about an ESP image in an AppFS file.
esp_err_t appfs2_info(appfs_handle_t fd, appfs2_info_t *info);
// Quickly tell whether an AppFS app is an AppFS2 app.
esp_err_t appfs2_detect(appfs_handle_t fd, bool *is_appfs2);
// Reboot into a firmware or load and start an application.
esp_err_t appfs2_start(appfs_handle_t fd, appfs2_info_t *info);
