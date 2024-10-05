
// SPDX-License-Identifier: MIT

#include "appfs.h"
#include "kbelf.h"

#include <stdio.h>
#include <stdlib.h>

#include <esp_log.h>
#include <hal/cache_ll.h>
#include <soc/soc.h>
#include <string.h>

#define KBELFX_APPFS_PREFIX "/appfs/"

static char const TAG[] = "kbelfx";

__attribute__((section(".ext_ram.bss"))) static char const psram_resvmem[128 * 1024];

#define RESVMEM_LOW  ((size_t)psram_resvmem)
#define RESVMEM_HIGH (RESVMEM_LOW + sizeof(psram_resvmem))

// File descriptor passed to KBElf.
typedef struct {
    bool is_appfs;
    long pos;
    union {
        FILE          *stdio;
        appfs_handle_t appfs;
    };
} kbelfx_fd_t;



// Measure the length of `str`.
size_t kbelfq_strlen(char const *str) {
    return strlen(str);
}

// Copy string from `src` to `dst`.
void kbelfq_strcpy(char *dst, char const *src) {
    strcpy(dst, src);
}

// Find last occurrance of `c` in `str`.
char const *kbelfq_strrchr(char const *str, char c) {
    return strrchr(str, c);
}

// Compare string `a` to `b`.
bool kbelfq_streq(char const *a, char const *b) {
    return !strcmp(a, b);
}


// Copy memory from `src` to `dst`.
void kbelfq_memcpy(void *dst, void const *src, size_t nmemb) {
    memcpy(dst, src, nmemb);
}

// Fill memory `dst` with `c`.
void kbelfq_memset(void *dst, uint8_t c, size_t nmemb) {
    memset(dst, c, nmemb);
}

// Compare memory `a` to `b`.
bool kbelfq_memeq(void const *a, void const *b, size_t nmemb) {
    return !memcmp(a, b, nmemb);
}



// Memory allocator function to use for allocating metadata.
// User-defined.
void *kbelfx_malloc(size_t len) {
    return malloc(len);
}

// Memory allocator function to use for allocating metadata.
// User-defined.
void *kbelfx_realloc(void *mem, size_t len) {
    return realloc(mem, len);
}

// Memory allocator function to use for allocating metadata.
// User-defined.
void kbelfx_free(void *mem) {
    free(mem);
}



// Memory allocator function to use for loading program segments.
// Takes a segment with requested address and permissions and returns a segment with physical and virtual address
// information. Returns success status. User-defined.
bool kbelfx_seg_alloc(kbelf_inst inst, size_t segs_len, kbelf_segment *segs) {
    // Check for FLASH-mapped code.
    size_t flash_count = 0;
    size_t psram_count = 0;
    for (size_t i = 0; i < segs_len; i++) {
        if (segs[i].vaddr_req >= SOC_DROM_LOW && segs[i].vaddr_req + segs[i].size <= SOC_DROM_HIGH) {
            flash_count++;
        } else if (segs[i].vaddr_req >= RESVMEM_LOW || segs[i].vaddr_req + segs[i].size <= RESVMEM_HIGH) {
            psram_count++;
        }
    }

    if (flash_count == 0 && psram_count == 0) {
        // Nothing in FLASH/PSRAM; dynamically allocate memory for segments.
        // Measure address ranges and alignment.
        size_t max_align = 1;
        size_t start     = SIZE_MAX;
        size_t end       = 0;
        for (size_t i = 0; i < segs_len; i++) {
            if (end < segs[i].vaddr_req + segs[i].size) {
                end = segs[i].vaddr_req + segs[i].size;
            }
            if (start > segs[i].vaddr_req) {
                start = segs[i].vaddr_req;
            }
            if (max_align < segs[i].alignment) {
                max_align = segs[i].alignment;
            }
        }

        // Allocate some memory to load into.
        void *mem = aligned_alloc(max_align, end - start);
        if (!mem)
            return false;

        // Set virtual addresses.
        segs[0].alloc_cookie = mem;
        for (size_t i = 0; i < segs_len; i++) {
            segs[i].vaddr_real = segs[i].vaddr_req - start + (kbelf_addr)mem;
            segs[i].laddr      = segs[i].vaddr_real;
        }

        return true;

    } else if (flash_count + psram_count == segs_len) {
        // All in FLASH/PSRAM; set virtual addresses.
        for (size_t i = 0; i < segs_len; i++) {
            segs[i].vaddr_real = segs[i].vaddr_req;
            segs[i].laddr      = segs[i].vaddr_real;
        }
        return true;

    } else {
        // Code must either be all FLASH/PSRAM or not at all FLASH/PSRAM to succeed.
        ESP_LOGE(TAG, "Malformed app: Segments are partially FLASH/PSRAM, partially not.");
        return false;
    }
}

// Memory allocator function to use for loading program segments.
// Takes a previously allocated segment and unloads it.
// User-defined.
void kbelfx_seg_free(kbelf_inst inst, size_t segs_len, kbelf_segment *segs) {
    free(segs[0].alloc_cookie);
}



// Open a binary file for reading.
// User-defined.
void *kbelfx_open(char const *path) {
    kbelfx_fd_t *fd = malloc(sizeof(kbelfx_fd_t));
    if (strncmp(path, KBELFX_APPFS_PREFIX, sizeof(KBELFX_APPFS_PREFIX) - 1)) {
        // Try STDIO file.
        fd->is_appfs = false;
        fd->stdio    = fopen(path, "rb");
        if (!fd->stdio) {
            ESP_LOGE(TAG, "File not found: %s", path);
            free(fd);
            return NULL;
        }
    } else {
        // Try AppFS file.
        fd->is_appfs = true;
        fd->appfs    = appfsOpen(path + sizeof(KBELFX_APPFS_PREFIX) - 1);
        fd->pos      = 0;
        if (fd->appfs == APPFS_INVALID_FD) {
            ESP_LOGE(TAG, "AppFS app not found: %s", path + sizeof(KBELFX_APPFS_PREFIX) - 1);
            free(fd);
            return NULL;
        }
    }
    return fd;
}

// Close a file.
// User-defined.
void kbelfx_close(void *_fd) {
    kbelfx_fd_t *fd = _fd;
    if (!fd->is_appfs) {
        fclose(fd->stdio);
    }
}

// Reads a number of bytes from a file.
// Returns the number of bytes read, or less than that on error.
// User-defined.
long kbelfx_read(void *_fd, void *buf, long buf_len) {
    if (buf_len <= 0)
        return 0;
    kbelfx_fd_t *fd = _fd;
    if (fd->is_appfs) {
        esp_err_t res = appfsRead(fd->appfs, fd->pos, buf, buf_len);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "AppFS read error: %s", esp_err_to_name(res));
            return 0;
        }
        fd->pos += buf_len;
        return buf_len;
    } else {
        return fread(buf, 1, buf_len, fd->stdio);
    }
}

// Reads a number of bytes from a file to a load address in the program.
// Returns the number of bytes read, or less than that on error.
// User-defined.
long kbelfx_load(kbelf_inst inst, void *_fd, kbelf_laddr laddr, kbelf_laddr file_size, kbelf_laddr mem_size) {
    kbelfx_fd_t *fd = _fd;
    if (laddr < SOC_DROM_LOW || laddr >= SOC_DROM_HIGH) {
        long res = kbelfx_read(_fd, (void *)laddr, file_size);
        if (res == file_size && mem_size > file_size) {
            memset((void *)(laddr + file_size), 0, mem_size - file_size);
        }
        return res;
    }

    // TODO: SOCs with separate data and code MMUs?
    esp_err_t res = appfsMmapAt(fd->appfs, fd->pos, file_size, laddr, SPI_FLASH_MMAP_DATA);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Unable to mmap: %s", esp_err_to_name(res));
        return 0;
    }

    // Verify the MMAP succeeded by reading it.
    void *tmp = malloc(file_size);
    appfsRead(fd->appfs, fd->pos, tmp, file_size);
    if (memcmp(tmp, (void const *)laddr, file_size)) {
        ESP_LOGE(TAG, "Memory map failed! Read data does not match mmap'ed data!");
        return 0;
    }

    fd->pos += file_size;

    return file_size;
}

// Sets the absolute offset in the file.
// Returns 0 on success, -1 on error.
// User-defined.
int kbelfx_seek(void *_fd, long pos) {
    kbelfx_fd_t *fd = _fd;
    if (fd->is_appfs) {
        int size = -1;
        appfsEntryInfo(fd->appfs, NULL, &size);
        if (pos < 0) {
            pos = 0;
        } else if (pos > size) {
            pos = size;
        }
        fd->pos = pos;
        return pos;
    } else {
        return fseek(fd->stdio, pos, SEEK_SET);
    }
}


// Read bytes from a load address in the program.
bool kbelfx_copy_from_user(kbelf_inst inst, void *buf, kbelf_laddr laddr, size_t len) {
    memcpy(buf, (void const *)laddr, len);
    return true;
}

// Write bytes to a load address in the program.
bool kbelfx_copy_to_user(kbelf_inst inst, kbelf_laddr laddr, void *buf, size_t len) {
    memcpy((void *)laddr, buf, len);
    return true;
}

// Get string length from a load address in the program.
ptrdiff_t kbelfx_strlen_from_user(kbelf_inst inst, kbelf_laddr laddr) {
    return (ptrdiff_t)strlen((char const *)laddr);
}


// Find and open a dynamic library file.
// Returns non-null on success, NULL on error.
// User-defined.
kbelf_file kbelfx_find_lib(char const *needed) {
    return NULL;
}



// Symbols to export to applications.
extern kbelf_builtin_lib const exported_syms;
// Number of built-in libraries.
size_t                         kbelfx_builtin_libs_len = 1;
// Array of built-in libraries.
kbelf_builtin_lib const       *kbelfx_builtin_libs[]   = {
    &exported_syms,
};
