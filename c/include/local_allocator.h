#ifndef GC_LOCAL_ALLOCATOR_H
#define GC_LOCAL_ALLOCATOR_H

#include "global_allocator.h"

typedef struct GC_LocalAllocator {
    GlobalAllocator *global_allocator;
    Block *block;
    char *cursor;
    char *limit;
    Hole *next;
} LocalAllocator;

static inline void LocalAllocator_reset(LocalAllocator *self) {
    self->block = NULL;
    self->cursor = NULL;
    self->limit = NULL;
    self->next = NULL;
}

static inline void LocalAllocator_init(LocalAllocator *self, GlobalAllocator *global_allocator) {
    self->global_allocator = global_allocator;
    LocalAllocator_reset(self);
}

void *GC_LocalAllocator_allocateSmall(LocalAllocator *self, size_t size, int atomic);

#define LocalAllocator_allocateSmall GC_LocalAllocator_allocateSmall

#endif
