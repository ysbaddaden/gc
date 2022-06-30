#ifndef GC_MEMORY_H
#define GC_MEMORY_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "constants.h"

#define MEM_PROT (PROT_READ | PROT_WRITE)
#define MEM_FLAGS (MAP_NORESERVE | MAP_PRIVATE | MAP_ANONYMOUS)
#define MEM_FD (-1)
#define MEM_OFFSET (0)

static inline size_t GC_alignToPageSize(size_t size) {
    int page_size = getpagesize();
    size_t aligned_size = size & ~(page_size - 1);
    if (aligned_size < size) aligned_size += page_size;
    return aligned_size;
}

static inline void *GC_map(size_t size) {
    void *addr = mmap(NULL, size, MEM_PROT, MEM_FLAGS, MEM_FD, MEM_OFFSET);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "GC: mmap error: %s\n", strerror(errno));
        abort();
    }
    return addr;
}

static inline void *GC_mapAndAlign(void **addr, size_t size, size_t alignment_size) {
    void *start = GC_map(GC_alignToPageSize(size + BLOCK_SIZE));
    *addr = start;

    size_t alignment_mask = ~(alignment_size - 1);
    if (((uintptr_t)start & alignment_mask) != (uintptr_t)start) {
        void* previous_block = (void*)((uintptr_t)start & BLOCK_SIZE_IN_BYTES_INVERSE_MASK);
        start = (char*)previous_block + BLOCK_SIZE;
    }

    return start;
}

static inline size_t GC_getMemoryLimit() {
    return (size_t)sysconf(_SC_PHYS_PAGES) * (size_t)sysconf(_SC_PAGESIZE);
}

#if defined(GC_REMAP)
static inline void GC_remap(void *addr, size_t old_size, size_t new_size) {
    void *ret = mremap(addr, old_size, new_size, 0);
    if (ret == MAP_FAILED) {
        fprintf(stderr, "GC: mremap error: %s\n", strerror(errno));
        abort();
    }
}

static inline void GC_remapAligned(void *addr, size_t old_size, size_t new_size) {
    old_size = GC_alignToPageSize(old_size + BLOCK_SIZE);
    new_size = GC_alignToPageSize(new_size + BLOCK_SIZE);
    GC_remap(addr, old_size, new_size);
}
#endif // GC_REMAP

#endif
