
// SPDX-License-Identifier: MIT

#include "bsp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void bsp_platform_preinit();
void bsp_platform_init();

// Devide table mutex.
extern SemaphoreHandle_t bsp_dev_mtx;


// Initialize the event queues.
void bsp_event_queue_init();

// Pre-init function; initialize BSP but not external devices.
void bsp_preinit() {
    // Make sure this is only called once.
    static bool called = false;
    if (called) {
        return;
    }
    called = true;

    bsp_dev_mtx = xSemaphoreCreateBinary();
    xSemaphoreGive(bsp_dev_mtx);
    bsp_event_queue_init();
    bsp_platform_preinit();
}

// Initialise the BSP, should be called early on in `app_main`.
void bsp_init() {
    // Make sure this is only called once.
    static bool called = false;
    if (called) {
        return;
    }
    called = true;

    bsp_preinit();
    bsp_platform_init();
}
