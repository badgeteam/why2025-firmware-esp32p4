
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp_devtree.h"



// Register a new device and assign an ID to it.
uint32_t bsp_dev_register(bsp_devtree_t const *tree);
// Unregister an existing device.
bool     bsp_dev_unregister(uint32_t dev_id);

// Call to notify the BSP of a button press.
void bsp_raw_button_pressed(uint32_t dev_id, uint8_t endpoint, int input);
// Call to notify the BSP of a button release.
void bsp_raw_button_released(uint32_t dev_id, uint8_t endpoint, int input);

// Call to notify the BSP of a button press.
void bsp_raw_button_pressed_from_isr(uint32_t dev_id, uint8_t endpoint, int input);
// Call to notify the BSP of a button release.
void bsp_raw_button_released_from_isr(uint32_t dev_id, uint8_t endpoint, int input);

// Obtain a copy of a device's devtree that can be cleaned up with `free()`.
// static inline bsp_devtree_t *bsp_dev_clone_devtree(uint32_t dev_id) {}
// Obtain a share of the device tree shared pointer.
// TODO.
