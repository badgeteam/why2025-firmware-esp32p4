
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp_input.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Maximum possible wait time for a BSP event.
#define BSP_EVENT_MAX_WAIT INT64_MAX

// Possible types of BSP event.
typedef enum {
    // Input changed event.
    BSP_EVENT_INPUT,
} bsp_event_type_t;

// Events sent by the BSP to the application.
typedef struct {
    // Event type.
    bsp_event_type_t  type;
    // Input event data.
    bsp_input_event_t input;
} bsp_event_t;



// Initialise the BSP, should be called early on in `app_main`.
void bsp_init();

// Add an event to the BSP's event queue.
bool bsp_event_queue(bsp_event_t *event);
// Add an event to the BSP's event queue from interrupt handler.
bool bsp_event_queue_from_isr(bsp_event_t *event);
// Wait for a limited time for a BSP event to happen.
// If time is 0, only returns a valid event when there is one in the queue.
bool bsp_event_wait(bsp_event_t *event_out, uint64_t wait_ms);

// Call to notify the BSP of a button press.
void bsp_raw_button_pressed(uint32_t dev_id, bsp_input_t input);
// Call to notify the BSP of a button release.
void bsp_raw_button_released(uint32_t dev_id, bsp_input_t input);
