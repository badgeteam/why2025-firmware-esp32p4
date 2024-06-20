
// SPDX-License-Identifier: MIT

#include "bsp_device.h"

#include "bsp/disp_mipi_dsi.h"
#include "bsp/disp_st7701.h"
#include "bsp_color.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

static char const TAG[] = "bsp-device";



// Input driver table.
static bsp_input_driver_t const *input_tab[] = {
    // TODO.
};
static size_t const input_tab_len = sizeof(input_tab) / sizeof(bsp_input_driver_t const *);

// LED driver table.
static bsp_led_driver_t const *led_tab[] = {
    // TODO.
};
static size_t const led_tab_len = sizeof(led_tab) / sizeof(bsp_led_driver_t const *);

// Display driver table.
static bsp_disp_driver_t const *const disp_tab[] = {
    [BSP_DISP_ST7701] = &(bsp_disp_driver_t const){
        .common = {
            .init    = bsp_disp_st7701_init,
            .deinit  = bsp_disp_dsi_deinit,
        },
        .update      = bsp_disp_dsi_update,
        .update_part = bsp_disp_dsi_update_part,
    },
};
static size_t const disp_tab_len = sizeof(disp_tab) / sizeof(bsp_disp_driver_t const *);

// Driver tables per endpoint type.
static bsp_driver_common_t const *const *const driver_tab[] = {
    (void *)input_tab,
    (void *)led_tab,
    (void *)disp_tab,
};

// Driver table lengths per endpoint type.
static size_t const driver_len[] = {
    input_tab_len,
    led_tab_len,
    disp_tab_len,
};



// Device table mutex.
SemaphoreHandle_t     bsp_dev_mtx;
// Number of shares currently held.
static int            bsp_dev_shares;
// Next device ID to be handed out.
static uint32_t       next_dev_id = 1;
// Number of registered devices.
static size_t         devices_len = 0;
// Registered devices.
static bsp_device_t **devices     = NULL;

// Get the device mutex shared.
static bool acq_shared() {
    if (xSemaphoreTake(bsp_dev_mtx, pdMS_TO_TICKS(50)) != pdTRUE) {
        return false;
    }
    bsp_dev_shares++;
    xSemaphoreGive(bsp_dev_mtx);
    return true;
}

// Release the device mutex shared.
static void rel_shared() {
    xSemaphoreTake(bsp_dev_mtx, portMAX_DELAY);
    bsp_dev_shares--;
    xSemaphoreGive(bsp_dev_mtx);
}

// Get the device mutex exclusively.
static bool acq_excl() {
    TickType_t now = xTaskGetTickCount();
    TickType_t lim = now + pdMS_TO_TICKS(50);
    if (xSemaphoreTake(bsp_dev_mtx, lim - now) != pdTRUE) {
        return false;
    }
    while (bsp_dev_shares) {
        xSemaphoreGive(bsp_dev_mtx);
        vTaskDelay(1);
        now = xTaskGetTickCount();
        if (now >= lim || xSemaphoreTake(bsp_dev_mtx, lim - now) != pdTRUE) {
            return false;
        }
    }
    return true;
}

// Release the device mutex exclusively.
void rel_excl() {
    xSemaphoreGive(bsp_dev_mtx);
}



// Names for endpoint types.
char const *const ep_type_str[] = {"input", "LED", "display"};

// Run device init functions.
static void run_init_funcs(bsp_device_t *dev, bool is_deinit) {
    // Count number of endpoints.
    uint16_t ep_count = 0;
    for (int i = 0; i < BSP_EP_TYPE_COUNT; i++) {
        ep_count += dev->tree->ep_counts[i];
    }

    int cur_prio = is_deinit ? INT_MIN : INT_MAX;
    while (ep_count) {
        int thr_prio = is_deinit ? INT_MAX : INT_MIN;

        // Get minimum priority.
        for (int i = 0; i < BSP_EP_TYPE_COUNT; i++) {
            for (int j = 0; j < dev->tree->ep_counts[i]; j++) {
                int prio = dev->tree->ep_trees[i][j]->init_prio;
                if (is_deinit && prio <= thr_prio && prio > cur_prio) {
                    cur_prio = prio;
                } else if (!is_deinit && prio >= thr_prio && prio < cur_prio) {
                    cur_prio = prio;
                }
            }
        }

        // Run all matching init functions.
        for (int i = 0; i < BSP_EP_TYPE_COUNT; i++) {
            for (int j = 0; j < dev->tree->ep_counts[i]; j++) {
                int prio = dev->tree->ep_trees[i][j]->init_prio;
                if (prio != cur_prio) {
                    continue;
                }
                ep_count--;
                if (!dev->ep_drivers[i][j]) {
                    continue;
                }
                if (is_deinit && dev->ep_drivers[i][j]->deinit) {
                    if (!dev->ep_drivers[i][j]->deinit(dev, j)) {
                        ESP_LOGE(
                            TAG,
                            "Device %s endpoint %" PRId8 " %" PRId32 " deinit failed",
                            ep_type_str[i],
                            j,
                            dev->id
                        );
                    }
                } else if (!is_deinit && dev->ep_drivers[i][j]->init) {
                    if (!dev->ep_drivers[i][j]->init(dev, j)) {
                        ESP_LOGE(
                            TAG,
                            "Device %s endpoint %" PRId8 " %" PRId32 " init failed",
                            ep_type_str[i],
                            j,
                            dev->id
                        );
                    }
                }
            }
        }

        // Increment priority threshold.
        thr_prio = cur_prio + (is_deinit ? -1 : 1);
    }
}



// Get a device from the list.
static ptrdiff_t bsp_find_device(uint32_t dev_id) {
    for (size_t i = 0; i < devices_len; i++) {
        if (devices[i]->id == dev_id) {
            return i;
        }
    }
    return -1;
}

// Free all the BSP-managed memory from a device.
static void bsp_dev_free(bsp_device_t *dev) {
    for (int i = 0; i < BSP_EP_TYPE_COUNT; i++) {
        free(dev->ep_aux[i]);
        free(dev->ep_drivers[i]);
    }
    free(dev);
}

// Unregister an existing device.
static bool bsp_dev_unregister_int(uint32_t dev_id, bool show_msg) {
    if (!acq_excl()) {
        return false;
    }

    ptrdiff_t idx = bsp_find_device(dev_id);
    if (idx <= 0) {
        return false;
    }
    bsp_device_t *dev = devices[idx];

    // Run deinit functions.
    run_init_funcs(dev, true);

    // Remove device from the list.
    for (size_t i = 0; i < devices_len; i++) {
        if (devices[i]->id == dev_id) {
            memcpy(devices + i, devices + i + 1, (devices_len - i - 1) * sizeof(bsp_device_t *));
            break;
        }
    }

    // Clean up.
    if (show_msg) {
        ESP_LOGI(TAG, "Device %" PRId32 " unregistered", dev->id);
    }
    bsp_dev_free(dev);
    rel_excl();

    return true;
}

// Register a new device and assign an ID to it.
uint32_t bsp_dev_register(bsp_devtree_t const *tree) {
    if (!acq_excl()) {
        return 0;
    }

    // Allocate device structure.
    bsp_device_t *dev = calloc(1, sizeof(bsp_device_t));
    if (!dev) {
        return 0;
    }
    for (int i = 0; i < BSP_EP_TYPE_COUNT; i++) {
        if (!tree->ep_counts[i]) {
            continue;
        }
        dev->ep_aux[i]     = calloc(tree->ep_counts[i], sizeof(void *));
        dev->ep_drivers[i] = calloc(tree->ep_counts[i], sizeof(bsp_driver_common_t const *));
        if (!dev->ep_aux[i] || !dev->ep_drivers[i]) {
            bsp_dev_free(dev);
            return 0;
        }
    }

    // Add it to the device list.
    void *mem = realloc(devices, (devices_len + 1) * sizeof(bsp_device_t *));
    if (!mem) {
        bsp_dev_free(dev);
        rel_excl();
        return 0;
    }
    devices              = mem;
    devices[devices_len] = dev;
    devices_len++;
    dev->id   = next_dev_id;
    dev->tree = tree;
    next_dev_id++;

    // Install drivers.
    for (int i = 0; i < BSP_EP_TYPE_COUNT; i++) {
        for (uint8_t j = 0; j < tree->ep_counts[i]; j++) {
            int type = tree->ep_trees[i][j]->type;
            if (type >= driver_len[i] || !driver_tab[i][type]) {
                ESP_LOGE(TAG, "Unsupported %s endpoint type %d", ep_type_str[i], type);
                continue;
            }
            dev->ep_drivers[i][j] = driver_tab[i][type];
        }
    }

    // Run init functions.
    run_init_funcs(dev, false);
    ESP_LOGI(TAG, "Device %" PRId32 " registered", dev->id);
    rel_excl();
    return dev->id;
}

// Unregister an existing device.
bool bsp_dev_unregister(uint32_t dev_id) {
    return bsp_dev_unregister_int(dev_id, true);
}



// Get current input value.
bool bsp_input_get(uint32_t dev_id, uint8_t endpoint, bsp_input_t input) {
    if (!acq_shared()) {
        return false;
    }
    ptrdiff_t     idx = bsp_find_device(dev_id);
    bsp_device_t *dev = devices[idx];
    bool          ret = false;
    if (idx >= 0 && endpoint < dev->tree->input_count && dev->input_drivers[endpoint]) {
        ret = dev->input_drivers[endpoint]->get(dev, endpoint, input);
    }
    rel_shared();
    return ret;
}

// Get current input value by raw input number.
bool bsp_input_get_raw(uint32_t dev_id, uint8_t endpoint, uint16_t raw_input) {
    if (!acq_shared()) {
        return false;
    }
    ptrdiff_t     idx = bsp_find_device(dev_id);
    bsp_device_t *dev = devices[idx];
    bool          ret = false;
    if (idx >= 0 && endpoint < dev->tree->input_count && dev->input_drivers[endpoint]) {
        ret = dev->input_drivers[endpoint]->get_raw(dev, endpoint, raw_input);
    }
    rel_shared();
    return ret;
}

// Set a device's input backlight.
void bsp_input_backlight(uint32_t dev_id, uint8_t endpoint, uint16_t pwm) {
    if (!acq_shared()) {
        return;
    }
    ptrdiff_t     idx = bsp_find_device(dev_id);
    bsp_device_t *dev = devices[idx];
    if (idx < 0 || endpoint >= dev->tree->disp_count) {
        rel_shared();
        return;
    }
    uint8_t bl_ep  = dev->tree->disp_dev[endpoint]->backlight_endpoint;
    uint8_t bl_idx = dev->tree->disp_dev[endpoint]->backlight_index;
    if (bl_ep >= dev->tree->led_count || bl_idx >= dev->tree->led_count) {
        rel_shared();
        return;
    }
    if (dev->led_drivers[endpoint]) {
        uint64_t value = bsp_16grey_to_col(dev->tree->disp_dev[endpoint]->pixfmt.color, pwm);
        dev->led_drivers[endpoint]->set_raw(dev, endpoint, idx, value);
        dev->led_drivers[endpoint]->update(dev, endpoint);
    }
    rel_shared();
}

// Send new image data to a device's display.
void bsp_disp_update(uint32_t dev_id, uint8_t endpoint, void const *framebuffer) {
    if (!acq_shared()) {
        return;
    }
    ptrdiff_t     idx = bsp_find_device(dev_id);
    bsp_device_t *dev = devices[idx];
    if (idx >= 0 && endpoint < dev->tree->disp_count && dev->disp_drivers[endpoint]) {
        dev->disp_drivers[endpoint]->update(dev, endpoint, framebuffer);
    }
    rel_shared();
}

// Send new image data to part of a device's display.
void bsp_disp_update_part(
    uint32_t dev_id, uint8_t endpoint, void const *framebuffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h
) {
    if (!acq_shared()) {
        return;
    }
    ptrdiff_t     idx = bsp_find_device(dev_id);
    bsp_device_t *dev = devices[idx];
    if (idx >= 0 && endpoint < dev->tree->disp_count && dev->disp_drivers[endpoint]) {
        dev->disp_drivers[endpoint]->update_part(dev, endpoint, framebuffer, x, y, w, h);
    }
    rel_shared();
}

// Set a device's display backlight.
void bsp_disp_backlight(uint32_t dev_id, uint8_t endpoint, uint16_t pwm) {
    // TODO.
}
