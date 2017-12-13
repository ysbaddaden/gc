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
    void *large_start = GC_mapAndAlign(memory_limit, initial_size);

    self->large_heap_size = initial_size;
    self->large_heap_start = large_start;
    self->large_heap_stop = (char *)large_start + initial_size;

    Chunk *chunk = (Chunk *)large_start;
    Chunk_init(chunk, initial_size - CHUNK_HEADER_SIZE);

    ChunkList_clear(&self->chunk_list);
    ChunkList_push(&self->chunk_list, chunk);

#ifdef GC_LARGE_CURSOR
    self->large_cursor = chunk;
#endif

    DEBUG("GC: heap size=%zu large_start=%p large_stop=%p\n",
            initial_size, self->large_heap_start, self->large_heap_stop);
}

static inline void GlobalAllocator_growLarge(GlobalAllocator *self, size_t increment) {
    size_t size = (size_t)1 << (size_t)ceil(log2((double)increment));
    size = ROUND_TO_NEXT_MULTIPLE(size, BLOCK_SIZE);

    if (self->large_heap_size + size > GC_getMemoryLimit()) {
        fprintf(stderr, "GC: out of memory\n");
        abort();
    }

    DEBUG("GC: grow large HEAP by %zu bytes to %zu bytes\n", size, self->large_heap_size + size);

    void *cursor = self->large_heap_stop;
    self->large_heap_stop = (char *)(self->large_heap_stop) + size;
    self->large_heap_size = self->large_heap_size + size;

    Chunk *chunk = (Chunk *)cursor;
    Chunk_init(chunk, size - CHUNK_HEADER_SIZE);

    ChunkList_push(&self->chunk_list, chunk);
#ifdef GC_DEBUG
    ChunkList_validate(&self->chunk_list, self->large_heap_stop);
#endif
}

#ifdef GC_LARGE_CURSOR
static inline void GlobalAllocator_updateLargeCursor(GlobalAllocator *self, Chunk *chunk) {
    Chunk *next = chunk->next;
    if (next == NULL) {
        self->large_cursor = self->chunk_list.first;
    } else {
        self->large_cursor = next;
    }
}
#endif

static inline void GlobalAllocator_allocateChunk(GlobalAllocator *self, Chunk *chunk, int atomic) {
    // allocate
    chunk->allocated = 1;
    chunk->object.atomic = atomic;

    // update cursor
#ifdef GC_LARGE_CURSOR
    GlobalAllocator_updateLargeCursor(self, chunk);
#endif
}

static inline void* GlobalAllocator_tryAllocateLarge(GlobalAllocator *self, size_t size, int atomic) {
    size_t object_size = size + sizeof(Object);

#ifdef GC_LARGE_CURSOR
    // keeps a cursor to next chunk (happens to be slower?!)
    Chunk *start = self->large_cursor;
    Chunk *chunk = start;

    while (1) {
#else
    // simply iterate again from the start (happens to be faster?!):
    Chunk *chunk = self->chunk_list.first;

    while (chunk != NULL) {
#endif
        assert((void *)chunk >= self->large_heap_start);
        assert((void *)chunk < self->large_heap_stop);

        if (!chunk->allocated) {
            size_t available = chunk->object.size;

            if (object_size <= available) {
                ChunkList_split(&self->chunk_list, chunk, object_size);
#ifdef GC_DEBUG
                ChunkList_validate(&self->chunk_list, self->large_heap_stop);
#endif
                GlobalAllocator_allocateChunk(self, chunk, atomic);
                return Chunk_mutatorAddress(chunk);
            }
        }

        // try next chunk
        chunk = chunk->next;

#ifdef GC_LARGE_CURSOR
        if (chunk == NULL) {
            chunk = self->chunk_list.first;
        }
        if (chunk == start) {
            return NULL;
        }
#endif
    }

    // unreachable
    return NULL;
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
    fprintf(stderr, "GC: failed to allocate object size=%zu object_size=%zu + %zu\n",
            size, rsize, sizeof(Object));

    ChunkList_debug(&self->chunk_list);
    abort();
}

void GC_GlobalAllocator_deallocateLarge(GlobalAllocator *self, void *pointer) {
     assert(GlobalAllocator_inLargeHeap(self, pointer));

     // Chunk *chunk = (Chunk *)pointer - 1;
     // chunk->allocated = (uint8_t)0;

    // TODO: merge with next free chunks
}
