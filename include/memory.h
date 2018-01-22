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

static inline void *GC_map(size_t memory_limit) {
    return mmap(NULL, memory_limit, MEM_PROT, MEM_FLAGS, MEM_FD, MEM_OFFSET);
}

static inline void *GC_mapAndAlign(size_t memory_limit, size_t alignment_size) {
    void *start = GC_map(memory_limit);

    if (start == (void *)-1) {
        printf("GC: mmap error: %s\n", strerror(errno));
        abort();
    }

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

#endif
