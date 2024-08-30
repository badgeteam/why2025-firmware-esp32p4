
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp_devtree.h"

#include <refcount.h>

#define bsp_dev_get_tree_raw(dev) ((bsp_devtree_t const *)(dev->tree->data))



// Register a new device and assign an ID to it.
// If `is_rom` is true, the BSP will not attempt to free the tree.
uint32_t bsp_dev_register(bsp_devtree_t const *tree, bool is_rom);
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
bsp_devtree_t *bsp_dev_clone_devtree(uint32_t dev_id);
// Obtain a share of the device tree shared pointer that can be cleaned up with `rc_delete()`.
rc_t           bsp_dev_get_devtree(uint32_t dev_id);
