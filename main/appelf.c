
// SPDX-License-Identifier: MIT

#include "appelf.h"

#include "appfs.h"
#include "bsp.h"

#include <esp_log.h>
#include <kbelf.h>
#include <string.h>

static char const TAG[] = "appelf";



// Returns ESP_OK if it is an ELF app, error code otherwise.
esp_err_t appelf_vfs_detect(char const *path) {
    FILE *fd = fopen(path, "rb");
    if (!fd)
        return ESP_ERR_NOT_FOUND;
    char tmp[sizeof(kbelf_magic)];
    fread(tmp, 1, sizeof(tmp), fd);
    fclose(fd);
    if (memcmp(tmp, kbelf_magic, sizeof(tmp)))
        return ESP_ERR_NOT_SUPPORTED;
    return ESP_OK;
}

// Try to load and run an ELF from filesystem.
esp_err_t appelf_vfs_run(char const *path) {
    ESP_LOGI(TAG, "Loading app %s", path);
    kbelf_dyn dyn = kbelf_dyn_create(0);
    if (!kbelf_dyn_set_exec(dyn, path, NULL))
        return ESP_ERR_NO_MEM;
    if (!kbelf_dyn_load(dyn))
        return ESP_FAIL;

    ESP_LOGI(TAG, "Starting app");
    void (*entry)() = (void *)kbelf_dyn_entrypoint(dyn);
    entry();

    kbelf_dyn_unload(dyn);
    kbelf_dyn_destroy(dyn);

    return ESP_OK;
}



// Returns ESP_OK if it is an ELF app, error code otherwise.
esp_err_t appelf_appfs_detect(appfs_handle_t fd) {
    char      tmp[sizeof(kbelf_magic)];
    esp_err_t res = appfsRead(fd, 0, tmp, sizeof(tmp));
    if (res)
        return res;
    if (memcmp(tmp, kbelf_magic, sizeof(tmp)))
        return ESP_ERR_NOT_SUPPORTED;
    return ESP_OK;
}

// Try to load and run an ELF from AppFS.
esp_err_t appelf_appfs_run(appfs_handle_t fd) {
    char const *app_id;
    appfsEntryInfo(fd, &app_id, NULL);

    char const *prefix = "/appfs/";
    char       *path   = malloc(strlen(app_id) + strlen(prefix) + 1);
    if (!path) {
        return ESP_ERR_NO_MEM;
    }
    strcpy(path, prefix);
    strcat(path, app_id);

    esp_err_t res = appelf_vfs_run(path);
    free(path);

    return res;
}
