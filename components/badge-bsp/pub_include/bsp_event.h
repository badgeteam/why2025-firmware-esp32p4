
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp_input.h"

#include <list.h>
#include <refcount.h>



// Maximum possible wait time for a BSP event.
#define BSP_EVENT_MAX_WAIT INT64_MAX

// Possible types of BSP event.
typedef enum {
    // Placeholder for enabling all events in a callback.
    BSP_EVENT_FILTER_ANY,
    // Input changed event.
    BSP_EVENT_INPUT,
    // Display event.
    BSP_EVENT_DISP_REFRESH,
} bsp_event_type_t;

// Types of input event.
typedef enum {
    // Button pressed.
    BSP_INPUT_EVENT_PRESS,
    // Button held down.
    BSP_INPUT_EVENT_HOLD,
    // Button released.
    BSP_INPUT_EVENT_RELEASE,
} bsp_input_event_type_t;

// Input event.
typedef struct {
    // Input event type.
    bsp_input_event_type_t type;
    // Input device ID.
    uint32_t               dev_id;
    // Input device endpoint.
    uint8_t                endpoint;
    // Input interpreted as navigation.
    bsp_input_t            nav_input;
    // Input value.
    bsp_input_t            input;
    // Input interpreted as text including DEL(0x7f), BS(\b) and ENTER(\n).
    char                   text_input;
    // Raw input value or keyboard scan code.
    int                    raw_input;
    // Active modifier keys, if any.
    uint32_t               modkeys;
} bsp_input_event_t;

// Events sent by the BSP to the application.
typedef struct {
    // Event type.
    bsp_event_type_t type;
    union {
        // Input event data.
        bsp_input_event_t input;
    };
} bsp_event_t;

// Event callback function.
typedef void (*bsp_event_cb_fn_t)(bsp_event_t event, void *userdata);

// Event callback entry.
typedef struct {
    // Event type filer.
    bsp_event_type_t  filter;
    // Event callback function.
    bsp_event_cb_fn_t callback;
    // Callback user data.
    void             *userdata;
} bsp_event_cb_t;

// Handle to installed callback.
typedef rc_t bsp_event_cb_handle_t;
