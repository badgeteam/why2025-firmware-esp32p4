
// SPDX-License-Identifier: MIT

#include "app.h"

#include <esp_log.h>
#include <esp_partition.h>
#include <esp_private/cache_utils.h>
#include <esp_system.h>
#include <hal/cache_hal.h>
#include <hal/cache_ll.h>
#include <hal/mmu_ll.h>
#include <soc/soc.h>

static char const TAG[] = "apploader";



// Get info about an ESP image in an AppFS file.
esp_err_t app_info(appfs_handle_t fd, app_info_t *info) {
    // Read the header.
    esp_err_t res = appfsRead(fd, 0, &info->header, sizeof(esp_image_header_t));
    if (res) {
        return res;
    }
    info->is_firmware  = info->header.chip_id == APP_CHIP_ID;
    info->segments_len = info->header.segment_count;

    // Read the segments.
    size_t offset = sizeof(esp_image_header_t);
    for (uint8_t i = 0; i < info->segments_len; i++) {
        esp_image_segment_header_t seghdr;
        res = appfsRead(fd, offset, &seghdr, sizeof(esp_image_segment_header_t));
        if (res) {
            return res;
        }
        info->segments[i].len     = seghdr.data_len;
        info->segments[i].vaddr   = seghdr.load_addr;
        info->segments[i].offset  = offset + sizeof(esp_image_segment_header_t);
        offset                   += sizeof(esp_image_segment_header_t) + seghdr.data_len;
    }

    return ESP_OK;
}

static bool is_ram_segment(app_segment_t seg) {
    if (seg.vaddr >= SOC_DRAM_LOW && seg.vaddr + seg.len <= SOC_DRAM_HIGH) {
        return true;
    } else if (seg.vaddr >= SOC_EXTRAM_LOW && seg.vaddr + seg.len <= SOC_EXTRAM_HIGH) {
        return true;
    } else {
        return false;
    }
}

static bool is_rom_segment(app_segment_t seg) {
    if (seg.vaddr >= SOC_DROM_LOW && seg.vaddr + seg.len <= SOC_DROM_HIGH) {
        return true;
    } else {
        return false;
    }
}

// Reboot into a firmware or load and start an application.
IRAM_ATTR esp_err_t app_start(appfs_handle_t fd, app_info_t *info) {
    esp_err_t res;

    // Firmwares are rebooted into instead of loaded.
    if (info->is_firmware) {
        appfsBootSelect(fd, NULL);
        esp_restart();
    }

    // Print segment info.
    for (uint8_t i = 0; i < info->segments_len; i++) {
        // TODO: Address checks.
        ESP_LOGI(
            TAG,
            "Segment %d: off=0x%08zx vaddr=0x%08zx len=0x%08zx",
            i,
            info->segments[i].offset,
            info->segments[i].vaddr,
            info->segments[i].len
        );
    }

    // Map jump table into memory.
    esp_partition_t const *part = esp_partition_find_first(0xca, 0xfe, NULL);
    if (!part || part->size > APP_JUMPTAB_SIZE) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    spi_flash_disable_interrupts_caches_and_other_cpu();
    mmu_ll_write_entry(
        MMU_LL_FLASH_MMU_ID,
        mmu_ll_get_entry_id(MMU_LL_FLASH_MMU_ID, APP_JUMPTAB_VADDR),
        mmu_ll_format_paddr(MMU_LL_FLASH_MMU_ID, part->address, MMU_TARGET_FLASH0),
        MMU_TARGET_FLASH0
    );
    spi_flash_enable_interrupts_caches_and_other_cpu();

    // Map segments into memory.
    for (uint8_t i = 0; i < info->segments_len; i++) {
        app_segment_t seg = info->segments[i];
        if (is_ram_segment(seg)) {
            // RAM regment; read into vaddr.
            res = appfsRead(fd, seg.offset, (void *)seg.vaddr, seg.len);
        } else if (is_rom_segment(seg)) {
            // ROM segment; map FLASH.
            res = appfsMmapAt(fd, seg.offset, seg.len, seg.vaddr, SPI_FLASH_MMAP_INST);
        } else {
            // Invalid segment.
            res = ESP_ERR_IMAGE_INVALID;
        }
        if (res) {
            return res;
        }
    }

    // Make sure data writes appear in instruction cache.
    asm("fence rw,rw");
    cache_ll_writeback_all(CACHE_LL_LEVEL_ALL, CACHE_TYPE_DATA, CACHE_LL_ID_ALL);
    asm("fence.i");

    // Call that start function.
    ((void (*)())info->header.entry_addr)();

    return ESP_OK;
}
