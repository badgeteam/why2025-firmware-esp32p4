
// SPDX-License-Identifier: MIT

#include "kbelf.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"



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
    return false;
}

// Memory allocator function to use for loading program segments.
// Takes a previously allocated segment and unloads it.
// User-defined.
void kbelfx_seg_free(kbelf_inst inst, size_t segs_len, kbelf_segment *segs) {
}


// Open a binary file for reading.
// User-defined.
void *kbelfx_open(char const *path) {
    return fopen(path, "rb");
}

// Close a file.
// User-defined.
void kbelfx_close(void *fd) {
    fclose(fd);
}

// Reads a single byte from a file.
// Returns byte on success, -1 on error.
// User-defined.
int kbelfx_getc(void *fd) {
    return fgetc(fd);
}

// Reads a number of bytes from a file.
// Returns the number of bytes read, or less than that on error.
// User-defined.
long kbelfx_read(void *fd, void *buf, long buf_len) {
    return fread(buf, 1, buf_len, fd);
}

// Reads a number of bytes from a file to a load address in the program.
// Returns the number of bytes read, or less than that on error.
// User-defined.
long kbelfx_load(kbelf_inst inst, void *fd, kbelf_laddr laddr, long len) {
    return -1;
}

// Sets the absolute offset in the file.
// Returns 0 on success, -1 on error.
// User-defined.
int kbelfx_seek(void *fd, long pos) {
    return fseek(fd, pos, SEEK_SET);
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
