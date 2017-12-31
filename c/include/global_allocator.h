#ifndef GC_GLOBAL_ALLOCATOR_H
#define GC_GLOBAL_ALLOCATOR_H

#include "constants.h"
#include "block_list.h"
#include "chunk_list.h"

typedef struct GC_GlobalAllocator {
    size_t small_heap_size;
    void *small_heap_start;
    void *small_heap_stop;

    BlockList free_list;
    BlockList recyclable_list;

    size_t large_heap_size;
    void *large_heap_start;
    void *large_heap_stop;

    ChunkList large_chunk_list;
} GlobalAllocator;

void GC_GlobalAllocator_init(GlobalAllocator *self, size_t initial_size);
void *GC_GlobalAllocator_allocateLarge(GlobalAllocator *self, size_t size, int atomic);
void GC_GlobalAllocator_deallocateLarge(GlobalAllocator *self, void *pointer);
Block *GC_GlobalAllocator_nextBlock(GlobalAllocator *self);
void GC_GlobalAllocator_recycleBlocks(GlobalAllocator *self);

static inline int GlobalAllocator_inSmallHeap(GlobalAllocator *self, void *pointer) {
    return (pointer >= self->small_heap_start) && (pointer < self->small_heap_stop);
}

static inline int GlobalAllocator_inLargeHeap(GlobalAllocator *self, void *pointer) {
    return (pointer >= self->large_heap_start) && (pointer < self->large_heap_stop);
}

static inline int GlobalAllocator_inHeap(GlobalAllocator *self, void *pointer) {
    return GlobalAllocator_inSmallHeap(self, pointer) || GlobalAllocator_inLargeHeap(self, pointer);
}

#define GlobalAllocator_init GC_GlobalAllocator_init
#define GlobalAllocator_allocateLarge GC_GlobalAllocator_allocateLarge
#define GlobalAllocator_deallocateLarge GC_GlobalAllocator_deallocateLarge
#define GlobalAllocator_nextBlock GC_GlobalAllocator_nextBlock
#define GlobalAllocator_recycleBlocks GC_GlobalAllocator_recycleBlocks

#endif
