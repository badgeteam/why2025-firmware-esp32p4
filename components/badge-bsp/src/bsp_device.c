
// SPDX-License-Identifier: MIT

#include "bsp_device.h"

#include "bsp/disp_mipi_dsi.h"
#include "bsp/disp_st7701.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

static char const TAG[] = "bsp-device";



// Display driver table.
static bsp_disp_driver_t const *disp_tab[] = {
    [BSP_DISP_ST7701] = &(bsp_disp_driver_t const){
        .init        = bsp_disp_st7701_init,
        .deinit      = bsp_disp_dsi_deinit,
        .backlight   = NULL,
        .update      = bsp_disp_dsi_update,
        .update_part = bsp_disp_dsi_update_part,
    },
};
size_t disp_tab_len = sizeof(disp_tab) / sizeof(bsp_disp_driver_t const *);

// Device register/unregister mutex.
SemaphoreHandle_t bsp_dev_register_mtx;
// Device table mutex.
SemaphoreHandle_t bsp_dev_mtx;
// Next device ID to be handed out.
uint32_t          next_dev_id = 1;
// Number of registered devices.
size_t            devices_len = 0;
// Registered devices.
bsp_device_t    **devices     = NULL;



// Get a device from the list.
static ptrdiff_t bsp_find_device(uint32_t dev_id) {
    for (size_t i = 0; i < devices_len; i++) {
        if (devices[i]->id == dev_id) {
            return i;
        }
    }
    return -1;
}

// Unregister an existing device.
static bool bsp_dev_unregister_int(uint32_t dev_id, bool show_msg) {
    if (xSemaphoreTake(bsp_dev_register_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }

    if (xSemaphoreTake(bsp_dev_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    ptrdiff_t idx = bsp_find_device(dev_id);
    if (idx <= 0) {
        return false;
    }
    bsp_device_t *dev = devices[idx];

    // Run deinit functions.
    if (dev->disp_driver) {
        if (!dev->disp_driver->deinit(dev)) {
            ESP_LOGE(TAG, "Display device %" PRId32 " deinit failed", dev->id);
        }
    }

    // Remove device from the list.
    for (size_t i = 0; i < devices_len; i++) {
        if (devices[i]->id == dev_id) {
            memcpy(devices + i, devices + i + 1, (devices_len - i - 1) * sizeof(bsp_device_t *));
            break;
        }
    }
    xSemaphoreGive(bsp_dev_mtx);

    // Clean up.
    if (show_msg) {
        ESP_LOGI(TAG, "Device %" PRId32 " unregistered", dev->id);
    }
    free(dev);
    xSemaphoreGive(bsp_dev_register_mtx);
    return true;
}

// Register a new device and assign an ID to it.
uint32_t bsp_dev_register(bsp_devtree_t const *tree) {
    if (xSemaphoreTake(bsp_dev_register_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return 0;
    }

    // Allocate device structure.
    bsp_device_t *dev = calloc(1, sizeof(bsp_device_t));
    if (!dev) {
        return 0;
    }

    // Add it to the device list.
    if (xSemaphoreTake(bsp_dev_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        free(dev);
        return 0;
    }
    void *mem = realloc(devices, (devices_len + 1) * sizeof(bsp_device_t *));
    if (!mem) {
        free(dev);
        xSemaphoreGive(bsp_dev_mtx);
        return 0;
    }
    devices              = mem;
    devices[devices_len] = dev;
    devices_len++;
    dev->id = next_dev_id;
    next_dev_id++;
    xSemaphoreGive(bsp_dev_mtx);

    // Install drivers.
    dev->tree = tree;
    if (tree->disp_dev) {
        // Check for display drivers.
        if (tree->disp_dev->type < disp_tab_len && disp_tab[tree->disp_dev->type]) {
            dev->disp_driver = disp_tab[tree->disp_dev->type];
        } else {
            ESP_LOGE(TAG, "Unsupported display type %d", tree->disp_dev->type);
        }
    }

    // Run init functions.
    if (dev->disp_driver) {
        if (!dev->disp_driver->init(dev)) {
            dev->disp_driver = NULL;
            goto error;
        }
        ESP_LOGI(TAG, "Display device %" PRId32 " init", dev->id);
    }

    ESP_LOGI(TAG, "Device %" PRId32 " registered", dev->id);
    xSemaphoreGive(bsp_dev_register_mtx);
    return dev->id;

    // If an init function failed, remove the device again.
error:
    xSemaphoreGive(bsp_dev_register_mtx);
    bsp_dev_unregister_int(dev->id, false);
    return 0;
}

// Unregister an existing device.
bool bsp_dev_unregister(uint32_t dev_id) {
    return bsp_dev_unregister_int(dev_id, true);
}



// Get current input value.
bool bsp_input_get(uint32_t dev_id, bsp_input_t input) {
    if (xSemaphoreTake(bsp_dev_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    ptrdiff_t idx = bsp_find_device(dev_id);
    bool      ret = false;
    if (idx >= 0 && devices[idx]->input_driver) {
        ret = devices[idx]->input_driver->get(devices[idx], input);
    }
    xSemaphoreGive(bsp_dev_mtx);
    return ret;
}

// Get current input value by raw input number.
bool bsp_input_get_raw(uint32_t dev_id, uint16_t raw_input) {
    if (xSemaphoreTake(bsp_dev_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    ptrdiff_t idx = bsp_find_device(dev_id);
    bool      ret = false;
    if (idx >= 0 && devices[idx]->input_driver) {
        ret = devices[idx]->input_driver->get_raw(devices[idx], raw_input);
    }
    xSemaphoreGive(bsp_dev_mtx);
    return ret;
}

// Set a device's input backlight.
void bsp_input_backlight(uint32_t dev_id, uint16_t pwm) {
    // TODO.
}

// Send new image data to a device's display.
void bsp_disp_update(uint32_t dev_id, void const *framebuffer) {
    if (xSemaphoreTake(bsp_dev_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }
    ptrdiff_t idx = bsp_find_device(dev_id);
    if (idx >= 0 && devices[idx]->disp_driver) {
        devices[idx]->disp_driver->update(devices[idx], framebuffer);
    }
    xSemaphoreGive(bsp_dev_mtx);
}

// Send new image data to part of a device's display.
void bsp_disp_update_part(uint32_t dev_id, void const *framebuffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (xSemaphoreTake(bsp_dev_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }
    ptrdiff_t idx = bsp_find_device(dev_id);
    if (idx >= 0 && devices[idx]->disp_driver) {
        devices[idx]->disp_driver->update_part(devices[idx], framebuffer, x, y, w, h);
    }
    xSemaphoreGive(bsp_dev_mtx);
}

// Set a device's display backlight.
void bsp_disp_backlight(uint32_t dev_id, uint16_t pwm) {
    // TODO.
}
