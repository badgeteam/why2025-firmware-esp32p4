
// SPDX-License-Identifier: MIT

#pragma once

#include "bsp_device.h"

#include <pax_gfx.h>



// Create an appropriate PAX buffer given display endpoint.
pax_buf_t *bsp_pax_buf_from_ep(uint32_t dev_id, uint8_t endpoint);
// Create an appropriate PAX buffer given display devtree.
pax_buf_t *bsp_pax_buf_from_tree(bsp_display_devtree_t const *tree);
