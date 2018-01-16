#ifndef GC_LOCAL_ALLOCATOR_H
#define GC_LOCAL_ALLOCATOR_H

#include "global_allocator.h"

typedef struct GC_LocalAllocator {
    GlobalAllocator *global_allocator;

    Block *block;
    char *cursor;
    char *limit;
    Hole *next;

    Block *overflow_block;
    char *overflow_cursor;
    char *overflow_limit;
} LocalAllocator;

void *GC_LocalAllocator_allocateSmall(LocalAllocator *self, size_t size, int atomic);
void GC_LocalAllocator_reset(LocalAllocator *self);

#define LocalAllocator_allocateSmall GC_LocalAllocator_allocateSmall
#define LocalAllocator_reset GC_LocalAllocator_reset

static inline void LocalAllocator_init(LocalAllocator *self, GlobalAllocator *global_allocator) {
    self->global_allocator = global_allocator;
    LocalAllocator_reset(self);
}

#endif
