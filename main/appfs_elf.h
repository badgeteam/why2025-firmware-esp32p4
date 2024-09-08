
// SPDX-License-Identifier: MIT

#pragma once

#include <appfs.h>

// Returns ESP_OK if it is an ELF app, error code otherwise.
esp_err_t appelf_detect(appfs_handle_t fd);
// Try to load and run an ELF from AppFS.
esp_err_t appelf_run(appfs_handle_t fd);
