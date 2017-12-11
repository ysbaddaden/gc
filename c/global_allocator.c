#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "global_allocator.h"
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
    Chunk_init(chunk, initial_size);

    ChunkList_clear(&self->chunk_list);
    ChunkList_push(&self->chunk_list, chunk);

    self->large_cursor = chunk;
}

inline void GlobalAllocator_GrowLarge(GlobalAllocator *self, size_t increment) {
    size_t size = (size_t)1 << (size_t)ceil(log2((double)increment));
    size = ROUND_TO_NEXT_MULTIPLE(size, BLOCK_SIZE);

    assert(size >= increment);

    if (self->large_heap_size + size > GC_getMemoryLimit()) {
        fprintf(stderr, "GC: out of memory\n");
        abort();
    }

#ifdef GC_DEBUG
    fprintf(stderr, "GC: grow large HEAP by %zu bytes to %zu bytes\n", size, self->large_heap_size + size);
#endif

    void *cursor = self->large_heap_stop;
    self->large_heap_stop = (char *)(self->large_heap_stop) + size;
    self->large_heap_size = self->large_heap_size + size;

    Chunk *chunk = (Chunk *)cursor;
    Chunk_init(chunk, size);

    ChunkList_push(&self->chunk_list, chunk);
}

inline void* GlobalAllocator_tryAllocateLarge(GlobalAllocator *self, size_t size, int atomic) {
    size_t chunk_size = size + sizeof(Chunk);
    size_t object_size = size + sizeof(Object);

    Chunk *chunk = self->chunk_list.first;
    while (chunk != NULL) {
        if (!chunk->allocated) {
            size_t available = chunk->object.size;

            // object fits right into the chunk:
            if (object_size == available) {
                // allocate
                chunk->allocated = 1;
                chunk->object.atomic = atomic;
                return Chunk_mutatorAddress(chunk);
            }

            // object fits into the chunk:
            if (object_size < available) {
                size_t remaining = available - object_size;

                if (remaining >= CHUNK_HEADER_SIZE + sizeof(uintptr_t)) {
                    // insert new chunk for remaining free space:
                    Chunk *free = (Chunk *)((char*)chunk + chunk_size);
                    Chunk_init(free, remaining - CHUNK_HEADER_SIZE);
                    ChunkList_insert(&self->chunk_list, free, chunk);

                    // update current chunk's size
                    chunk->object.size = object_size;
                }

                // allocate
                chunk->allocated = 1;
                chunk->object.atomic = atomic;
                return Chunk_mutatorAddress(chunk);
            }
        }

        // try next chunk
        chunk = chunk->next;
    }

    // unreachable
    return NULL;
}

void* GC_GlobalAllocator_allocateLarge(GlobalAllocator *self, size_t size, int atomic) {
    size = ROUND_TO_NEXT_MULTIPLE(size, sizeof(void *));

//#ifdef GC_DEBUG
//    fprintf(stderr, "GC: malloc size=%zu atomic=%d\n", size, atomic);
//#endif

    void *mutator;

    // 1. try to allocate
    mutator = GlobalAllocator_tryAllocateLarge(self, size, atomic);
    if (mutator != NULL) return mutator;

    // 2. collect memory
    GC_collect();

    // 3. try to allocate (again)
    mutator = GlobalAllocator_tryAllocateLarge(self, size, atomic);
    if (mutator != NULL) return mutator;

    // 4. grow memory
    GlobalAllocator_GrowLarge(self, size + sizeof(Chunk));

    // 5. try to allocate (again * 2)
    mutator = GlobalAllocator_tryAllocateLarge(self, size, atomic);
    if (mutator != NULL) return mutator;

    // 6. allocate!
    mutator = GlobalAllocator_tryAllocateLarge(self, size, atomic);
    if (mutator != NULL) return mutator;

    // 7. seriously? no luck.
    fprintf(stderr, "GC: failed to allocate object size=%zu\n", size);
    abort();
}

void GC_GlobalAllocator_deallocateLarge(GlobalAllocator *self, void *pointer) {
    assert(GlobalAllocator_inLargeHeap(self, pointer));

    Chunk *chunk = (Chunk *)pointer - 1;
    chunk->allocated = (uint8_t)0;

    // TODO: merge with next free chunks
}
