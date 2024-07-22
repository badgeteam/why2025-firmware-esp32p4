
// SPDX-License-Identifier: MIT

#include "bsp.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static QueueHandle_t queue;

void bsp_platform_preinit();
void bsp_platform_init();

// Devide table mutex.
extern SemaphoreHandle_t bsp_dev_mtx;



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
    queue = xQueueCreate(16, sizeof(bsp_event_t));
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

// Add an event to the BSP's event queue.
bool bsp_event_queue(bsp_event_t *event) {
    return xQueueSend(queue, event, 0) == pdTRUE;
}

// Add an event to the BSP's event queue from interrupt handler.
bool bsp_event_queue_from_isr(bsp_event_t *event) {
    return xQueueSendFromISR(queue, event, 0) == pdTRUE;
}

// Wait for a limited time for a BSP event to happen.
// If time is 0, only returns a valid event when there is one in the queue.
bool bsp_event_wait(bsp_event_t *event_out, uint64_t wait_ms) {
    // Clamp max wait time.
    TickType_t ticks;
    if (wait_ms > pdTICKS_TO_MS(portMAX_DELAY)) {
        ticks = portMAX_DELAY;
    } else {
        ticks = pdMS_TO_TICKS(wait_ms);
    }
    return xQueueReceive(queue, event_out, ticks) == pdTRUE;
}
