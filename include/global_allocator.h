#ifndef GC_GLOBAL_ALLOCATOR_H
#define GC_GLOBAL_ALLOCATOR_H

#include "constants.h"
#include "block_list.h"
#include "chunk_list.h"
#include "hash.h"

typedef void (*finalizer_t)(void *);

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

    Hash *finalizers;

    size_t memory_limit;
    size_t free_space_divisor;
    size_t allocated_bytes_since_collect;
    size_t total_allocated_bytes;
} GlobalAllocator;

void GC_GlobalAllocator_init(GlobalAllocator *self, size_t initial_size);
void *GC_GlobalAllocator_allocateLarge(GlobalAllocator *self, size_t size, int atomic);
void GC_GlobalAllocator_deallocateLarge(GlobalAllocator *self, void *pointer);
Block *GC_GlobalAllocator_nextBlock(GlobalAllocator *self);
Block *GC_GlobalAllocator_nextFreeBlock(GlobalAllocator *self);
void GC_GlobalAllocator_recycleBlocks(GlobalAllocator *self);

static inline void GlobalAllocator_registerFinalizer(GlobalAllocator *self, Object *object, finalizer_t callback) {
    void *ptr = *(void **)(&callback);
    Hash_insert(self->finalizers, object, ptr);
}

static inline finalizer_t GlobalAllocator_deleteFinalizer(GlobalAllocator *self, Object *object) {
    finalizer_t callback;
    *(void **)(&callback) = Hash_delete(self->finalizers, object);
    return callback;
}

static inline void GlobalAllocator_finalize(GlobalAllocator *self, Object *object) {
    finalizer_t callback = GlobalAllocator_deleteFinalizer(self, object);
    if (callback != NULL) {
        callback(Object_mutatorAddress(object));
    }
}

static inline int GlobalAllocator_finalizeObjectCallback(Object *object, finalizer_t callback) {
    if (!Object_isMarked(object)) {
        callback(Object_mutatorAddress(object));
        return 1;
    }
    return 0;
}
static inline void GlobalAllocator_finalizeObjects(GlobalAllocator *self) {
    Hash_deleteIf(self->finalizers, (hash_iterator_t)GlobalAllocator_finalizeObjectCallback);
}

static inline int GlobalAllocator_inSmallHeap(GlobalAllocator *self, void *pointer) {
    return (pointer >= self->small_heap_start) && (pointer < self->small_heap_stop);
}

static inline int GlobalAllocator_inLargeHeap(GlobalAllocator *self, void *pointer) {
    return (pointer >= self->large_heap_start) && (pointer < self->large_heap_stop);
}

static inline int GlobalAllocator_inHeap(GlobalAllocator *self, void *pointer) {
    return GlobalAllocator_inSmallHeap(self, pointer) || GlobalAllocator_inLargeHeap(self, pointer);
}

static inline void GlobalAllocator_incrementCounters(GlobalAllocator *self, size_t increment) {
    self->allocated_bytes_since_collect += increment;
    self->total_allocated_bytes += increment;
}

static inline void GlobalAllocator_resetCounters(GlobalAllocator *self) {
    self->allocated_bytes_since_collect = 0;
}

static inline size_t GlobalAllocator_allocatedBytesSinceCollect(GlobalAllocator *self) {
    return self->allocated_bytes_since_collect;
}

static inline size_t GlobalAllocator_totalAllocatedBytes(GlobalAllocator *self) {
    return self->total_allocated_bytes;
}

static inline size_t GlobalAllocator_heapSize(GlobalAllocator *self) {
    return self->small_heap_size + self->large_heap_size;
}

#define GlobalAllocator_init GC_GlobalAllocator_init
#define GlobalAllocator_allocateLarge GC_GlobalAllocator_allocateLarge
#define GlobalAllocator_deallocateLarge GC_GlobalAllocator_deallocateLarge
#define GlobalAllocator_nextBlock GC_GlobalAllocator_nextBlock
#define GlobalAllocator_nextFreeBlock GC_GlobalAllocator_nextFreeBlock
#define GlobalAllocator_recycleBlocks GC_GlobalAllocator_recycleBlocks

#endif
