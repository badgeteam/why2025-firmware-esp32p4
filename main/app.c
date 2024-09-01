
// SPDX-License-Identifier: MIT

#include "app.h"

#include "appfs2.h"
#include "arrays.h"
#include "esp_log.h"

static char const TAG[] = "app";



// List of detected apps.
app_meta_t *app_list     = NULL;
// Number of detected apps.
size_t      app_list_len = 0;



// Clean up an app meta struct.
static void app_meta_del(app_meta_t app) {
    free(app.id);
    free(app.name);
    free(app.main_path);
    if (app.icon_img) {
        pax_buf_destroy(app.icon_img);
    }
}

// Add an app to the list.
static void app_add(app_meta_t app) {
    if (array_len_insert(&app_list, sizeof(app_meta_t), &app_list_len, &app, app_list_len)) {
        ESP_LOGI(TAG, "Registerred app '%s'", app.name);
    } else {
        app_meta_del(app);
    }
}

// Read metadata.
static app_meta_t app_read_metadata(char const *id) {
    return (app_meta_t){
        .id        = strdup(id),
        .name      = NULL,
        .desc      = NULL,
        .main_path = NULL,
        .icon_img  = NULL,
    };
}

// Detect apps, updating `app_list` and `app_list_len`.
void app_detect() {
    app_list_clear();
    ESP_LOGI(TAG, "Scanning for installed apps");
    // Read AppFS apps.
    appfs_handle_t fd = appfsNextEntry(APPFS_INVALID_FD);
    while (fd != APPFS_INVALID_FD) {
        char const *id;
        char const *name;
        appfsEntryInfoExt(fd, &id, &name, NULL, NULL);
        app_meta_t meta = app_read_metadata(id);
        bool       is_appfs2;
        appfs2_detect(fd, &is_appfs2);
        meta.type     = is_appfs2 ? APP_TYPE_APPFS2 : APP_TYPE_APPFS1;
        meta.appfs_fd = fd;
        if (!meta.name) {
            meta.name = strdup(name);
        }
        app_add(meta);
        fd = appfsNextEntry(fd);
    }
}

// Clear the apps list.
void app_list_clear() {
    for (size_t i = 0; i < app_list_len; i++) {
        app_meta_del(app_list[i]);
    }
    free(app_list);
    app_list     = NULL;
    app_list_len = 0;
}

// Start an app.
void app_start(app_meta_t *app) {
    if (app->type == APP_TYPE_APPFS1 || app->type == APP_TYPE_APPFS2) {
        appfs2_info_t info;
        appfs2_info(app->appfs_fd, &info);
        appfs2_start(app->appfs_fd, &info);
    }
}
