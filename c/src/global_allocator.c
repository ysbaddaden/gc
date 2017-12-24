#include "config.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "global_allocator.h"
#include "immix.h"
#include "memory.h"
#include "utils.h"

void GC_GlobalAllocator_init(GlobalAllocator *self, size_t initial_size) {
    assert(initial_size >= BLOCK_SIZE * 2);
    assert(initial_size % BLOCK_SIZE == 0);

    size_t memory_limit = GC_getMemoryLimit();

    void *small_start = GC_mapAndAlign(memory_limit, initial_size);
    self->small_heap_size = initial_size;
    self->small_heap_start = small_start;
    self->small_heap_stop = (char *)small_start + initial_size;

    Chunk *small_chunk = (Chunk *)small_start;
    Chunk_init(small_chunk, initial_size - CHUNK_HEADER_SIZE);

    ChunkList_clear(&self->small_chunk_list);
    ChunkList_push(&self->small_chunk_list, small_chunk);

    void *large_start = GC_mapAndAlign(memory_limit, initial_size);
    self->large_heap_size = initial_size;
    self->large_heap_start = large_start;
    self->large_heap_stop = (char *)large_start + initial_size;

    Chunk *large_chunk = (Chunk *)large_start;
    Chunk_init(large_chunk, initial_size - CHUNK_HEADER_SIZE);

    ChunkList_clear(&self->large_chunk_list);
    ChunkList_push(&self->large_chunk_list, large_chunk);

    DEBUG("GC: heap size=%zu start=%p stop=%p large_start=%p large_stop=%p\n",
            initial_size, self->small_heap_start, self->small_heap_stop, self->large_heap_start, self->large_heap_stop);
}

static inline void GlobalAllocator_growSmall(GlobalAllocator *self, size_t increment) {
    size_t size = (size_t)1 << (size_t)ceil(log2((double)increment));
    size = ROUND_TO_NEXT_MULTIPLE(size, BLOCK_SIZE);

    if (self->small_heap_size + size > GC_getMemoryLimit()) {
        fprintf(stderr, "GC: out of memory\n");
        abort();
    }

    DEBUG("GC: grow small heap by %zu bytes to %zu bytes\n", size, self->small_heap_size + size);

    void *cursor = self->small_heap_stop;
    self->small_heap_stop = (char *)(self->small_heap_stop) + size;
    self->small_heap_size = self->small_heap_size + size;

    Chunk *chunk = (Chunk *)cursor;
    Chunk_init(chunk, size - CHUNK_HEADER_SIZE);

    ChunkList_push(&self->small_chunk_list, chunk);
#ifdef GC_DEBUG
    ChunkList_validate(&self->small_chunk_list, self->small_heap_stop);
#endif
}

static inline void GlobalAllocator_growLarge(GlobalAllocator *self, size_t increment) {
    size_t size = (size_t)1 << (size_t)ceil(log2((double)increment));
    size = ROUND_TO_NEXT_MULTIPLE(size, BLOCK_SIZE);

    if (self->large_heap_size + size > GC_getMemoryLimit()) {
        fprintf(stderr, "GC: out of memory\n");
        abort();
    }

    DEBUG("GC: grow large heap by %zu bytes to %zu bytes\n", size, self->large_heap_size + size);

    void *cursor = self->large_heap_stop;
    self->large_heap_stop = (char *)(self->large_heap_stop) + size;
    self->large_heap_size = self->large_heap_size + size;

    Chunk *chunk = (Chunk *)cursor;
    Chunk_init(chunk, size - CHUNK_HEADER_SIZE);

    ChunkList_push(&self->large_chunk_list, chunk);
#ifdef GC_DEBUG
    ChunkList_validate(&self->large_chunk_list, self->large_heap_stop);
#endif
}

static inline void GlobalAllocator_allocateChunk(GlobalAllocator *self, Chunk *chunk, int atomic) {
    chunk->allocated = 1;
    chunk->object.atomic = atomic;
}

static inline void* GlobalAllocator_tryAllocateSmall(GlobalAllocator *self, size_t size, int atomic) {
    size_t object_size = size + sizeof(Object);

    // simply iterate again from the start (happens to be faster?!):
    Chunk *chunk = self->small_chunk_list.first;

    while (chunk != NULL) {

#ifndef NDEBUG
        if ((void *)chunk < self->small_heap_start) {
            ChunkList_validate(&self->small_chunk_list, self->small_heap_stop);
            abort();
        }
        if ((void *)chunk >= self->small_heap_stop) {
            ChunkList_validate(&self->small_chunk_list, self->small_heap_stop);
            abort();
        }
#endif

        if (!chunk->allocated) {
            size_t available = chunk->object.size;

            if (object_size <= available) {
                ChunkList_split(&self->small_chunk_list, chunk, object_size);
#ifdef GC_DEBUG
                ChunkList_validate(&self->small_chunk_list, self->small_heap_stop);
#endif
                GlobalAllocator_allocateChunk(self, chunk, atomic);
                return Chunk_mutatorAddress(chunk);
            }
        }

        // try next chunk
        chunk = chunk->next;
    }

    // unreachable
    return NULL;
}

static inline void* GlobalAllocator_tryAllocateLarge(GlobalAllocator *self, size_t size, int atomic) {
    size_t object_size = size + sizeof(Object);

    // simply iterate again from the start (happens to be faster?!):
    Chunk *chunk = self->large_chunk_list.first;

    while (chunk != NULL) {
        assert((void *)chunk >= self->large_heap_start);
        assert((void *)chunk < self->large_heap_stop);

        if (!chunk->allocated) {
            size_t available = chunk->object.size;

            if (object_size <= available) {
                ChunkList_split(&self->large_chunk_list, chunk, object_size);
#ifdef GC_DEBUG
                ChunkList_validate(&self->large_chunk_list, self->large_heap_stop);
#endif
                GlobalAllocator_allocateChunk(self, chunk, atomic);
                return Chunk_mutatorAddress(chunk);
            }
        }

        // try next chunk
        chunk = chunk->next;
    }

    // unreachable
    return NULL;
}

void* GC_GlobalAllocator_allocateSmall(GlobalAllocator *self, size_t size, int atomic) {
    size_t rsize = ROUND_TO_NEXT_MULTIPLE(size, sizeof(void *));

    void *mutator;

    // 1. try to allocate
    mutator = GlobalAllocator_tryAllocateSmall(self, rsize, atomic);
    if (mutator != NULL) return mutator;

    // 2. collect memory
    GC_collect();

    // 3. try to allocate (again)
    mutator = GlobalAllocator_tryAllocateSmall(self, rsize, atomic);
    if (mutator != NULL) return mutator;

    // 4. grow memory
    GlobalAllocator_growSmall(self, rsize + sizeof(Chunk));

    // 5. allocate!
    mutator = GlobalAllocator_tryAllocateSmall(self, rsize, atomic);
    if (mutator != NULL) return mutator;

    // 6. seriously? no luck.
    fprintf(stderr, "GC: failed to allocate small object size=%zu object_size=%zu + %zu\n",
            size, rsize, sizeof(Object));

    ChunkList_debug(&self->small_chunk_list);
    abort();
}

void* GC_GlobalAllocator_allocateLarge(GlobalAllocator *self, size_t size, int atomic) {
    size_t rsize = ROUND_TO_NEXT_MULTIPLE(size, sizeof(void *));

    void *mutator;

    // 1. try to allocate
    mutator = GlobalAllocator_tryAllocateLarge(self, rsize, atomic);
    if (mutator != NULL) return mutator;

    // 2. collect memory
    GC_collect();

    // 3. try to allocate (again)
    mutator = GlobalAllocator_tryAllocateLarge(self, rsize, atomic);
    if (mutator != NULL) return mutator;

    // 4. grow memory
    GlobalAllocator_growLarge(self, rsize + sizeof(Chunk));

    // 5. allocate!
    mutator = GlobalAllocator_tryAllocateLarge(self, rsize, atomic);
    if (mutator != NULL) return mutator;

    // 6. seriously? no luck.
    fprintf(stderr, "GC: failed to allocate large object size=%zu object_size=%zu + %zu\n",
            size, rsize, sizeof(Object));

    ChunkList_debug(&self->large_chunk_list);
    abort();
}

void GC_GlobalAllocator_deallocateSmall(GlobalAllocator *self, void *pointer) {
    Chunk *chunk = (Chunk *)pointer - 1;
    chunk->allocated = (uint8_t)0;

    // TODO: merge with next free chunks
}

void GC_GlobalAllocator_deallocateLarge(GlobalAllocator *self, void *pointer) {
    Chunk *chunk = (Chunk *)pointer - 1;
    chunk->allocated = (uint8_t)0;

    // TODO: merge with next free chunks
}
