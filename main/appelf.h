
// SPDX-License-Identifier: MIT

#pragma once

#include <appfs.h>

// Returns ESP_OK if it is an ELF app, error code otherwise.
esp_err_t appelf_vfs_detect(char const *path);
// Try to load and run an ELF from filesystem.
esp_err_t appelf_vfs_run(char const *path);

// Returns ESP_OK if it is an ELF app, error code otherwise.
esp_err_t appelf_appfs_detect(appfs_handle_t fd);
// Try to load and run an ELF from AppFS.
esp_err_t appelf_appfs_run(appfs_handle_t fd);
