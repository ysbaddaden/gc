#include "config.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "global_allocator.h"
#include "immix.h"
#include "memory.h"
#include "utils.h"
#include "options.h"

void GC_GlobalAllocator_init(GlobalAllocator *self, size_t initial_size) {
    assert(initial_size >= BLOCK_SIZE * 2);
    assert(initial_size % BLOCK_SIZE == 0);

    self->memory_limit = GC_maximumHeapSize();
    self->free_space_divisor = GC_freeSpaceDivisor();
    self->allocated_bytes_since_collect = 0;
    self->total_allocated_bytes = 0;

    // small object space (immix)
#if defined(GC_REMAP)
    void *heap_start = GC_mapAndAlign(&self->small_heap_addr, initial_size, initial_size);
#else
    void *heap_start = GC_mapAndAlign(&self->small_heap_addr, self->memory_limit, initial_size);
#endif
    self->small_heap_size = initial_size;
    self->small_heap_start = heap_start;
    self->small_heap_stop = (char *)heap_start + initial_size;

    BlockList_clear(&self->free_list);
    BlockList_clear(&self->recyclable_list);

    Block *block = (Block *)self->small_heap_start;
    Block *stop = (Block *)self->small_heap_stop;
    while (block < stop) {
        Block_init(block);
        BlockList_push(&self->free_list, block);
        block = (Block *)((char *)block + BLOCK_SIZE);
    }

    // large objects space (linked list)
#if defined(GC_REMAP)
    void *large_start = GC_mapAndAlign(&self->large_heap_addr, initial_size, initial_size);
#else
    void *large_start = GC_mapAndAlign(&self->large_heap_addr, self->memory_limit, initial_size);
#endif
    self->large_heap_size = initial_size;
    self->large_heap_start = large_start;
    self->large_heap_stop = (char *)large_start + initial_size;

    Chunk *large_chunk = (Chunk *)large_start;
    Chunk_init(large_chunk, initial_size - CHUNK_HEADER_SIZE);

    ChunkList_clear(&self->large_chunk_list);
    ChunkList_push(&self->large_chunk_list, large_chunk);

    self->finalizers = Hash_create(8);

    DEBUG("GC: heap size=%zu start=%p stop=%p large_start=%p large_stop=%p\n",
            initial_size, self->small_heap_start, self->small_heap_stop, self->large_heap_start, self->large_heap_stop);
}

static inline void GlobalAllocator_growSmall(GlobalAllocator *self) {
    size_t increment = self->small_heap_size * GROWTH_RATE / 100;
    increment = ROUND_TO_NEXT_MULTIPLE(increment, BLOCK_SIZE);
    size_t new_size = self->small_heap_size + increment;

    if (self->small_heap_size + self->large_heap_size + increment > self->memory_limit) {
        fprintf(stderr, "GC: out of memory\n");
        abort();
    }

    DEBUG("GC: grow small heap by %zu bytes to %zu bytes\n", increment, new_size);
#if defined(GC_REMAP)
    GC_remapAligned(self->small_heap_addr, self->small_heap_size, new_size);
#endif

    char *cursor = self->small_heap_stop;
    self->small_heap_stop = (char *)(self->small_heap_stop) + increment;
    self->small_heap_size = new_size;

    int count = increment / BLOCK_SIZE;
    for (int i = 0; i < count; i++) {
        Block *block = (Block*)(cursor + i * BLOCK_SIZE);
        Block_init(block);
        BlockList_push(&self->free_list, block);
    }
}

static inline void GlobalAllocator_growLarge(GlobalAllocator *self, size_t increment) {
    size_t size = (size_t)1 << (size_t)ceil(log2((double)increment));
    size = ROUND_TO_NEXT_MULTIPLE(size, BLOCK_SIZE);
    size_t new_size = self->large_heap_size + size;

    if (self->small_heap_size + self->large_heap_size + size > self->memory_limit) {
        fprintf(stderr, "GC: out of memory\n");
        abort();
    }

    DEBUG("GC: grow large heap by %zu bytes to %zu bytes\n", size, new_size);
#if defined(GC_REMAP)
    GC_remapAligned(self->large_heap_addr, self->large_heap_size, new_size);
#endif

    void *cursor = self->large_heap_stop;
    self->large_heap_stop = (char *)(self->large_heap_stop) + size;
    self->large_heap_size = new_size;

    Chunk *chunk = (Chunk *)cursor;
    Chunk_init(chunk, size - CHUNK_HEADER_SIZE);

    ChunkList_push(&self->large_chunk_list, chunk);
//#ifndef NDEBUG
//    ChunkList_validate(&self->large_chunk_list, self->large_heap_stop);
//#endif
}

static inline void *GlobalAllocator_tryAllocateLarge(GlobalAllocator *self, size_t size, int atomic) {
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
//#ifndef NDEBUG
//                ChunkList_validate(&self->large_chunk_list, self->large_heap_stop);
//#endif
                Chunk_allocate(chunk, atomic);
                GlobalAllocator_incrementCounters(self, size);
                return Chunk_mutatorAddress(chunk);
            }
        }

        // try next chunk
        chunk = chunk->next;
    }

    // unreachable
    return NULL;
}

// Collects memory if we allocated at least 1/Nth of the HEAP memory since the
// last collection. Returns immediately if we're already collecting.
static int GlobalAllocator_tryCollect(GlobalAllocator *self) {
    if (GC_is_collecting()) {
        return 0;
    }

    size_t allocated = GlobalAllocator_allocatedBytesSinceCollect(self);
    size_t total = GlobalAllocator_heapSize(self);

    if (allocated < total / self->free_space_divisor) {
        DEBUG("skip collect memory=%zu allocated=%zu\n", total, allocated);
        return 0;
    }

    GC_collect();
    return 1;
}

Block *GC_GlobalAllocator_nextBlock(GlobalAllocator *self) {
    Block *block;

    GC_lock();

    // 1. exhaust recyclable list:
    block = BlockList_shift(&self->recyclable_list);
    if (block != NULL) {
        GC_unlock();
        return block;
    }

    // 2. exhaust free list:
    block = BlockList_shift(&self->free_list);
    if (block != NULL) {
        GC_unlock();
        return block;
    }

    // 3. no block? allocated enough since last collect? collect!
    if (GlobalAllocator_tryCollect(self)) {
        // 4. exhaust freshly recycled list:
        block = BlockList_shift(&self->recyclable_list);
        if (block != NULL) {
            GC_unlock();
            return block;
        }
    }

    // 5. no free blocks? grow!
    if (BlockList_isEmpty(&self->free_list)) {
        GlobalAllocator_growSmall(self);
    }

    // 6. get free block!
    block = BlockList_shift(&self->free_list);
    if (block != NULL) {
        GC_unlock();
        return block;
    }

    // 7. seriously, no luck
    fprintf(stderr, "GC: failed to allocate small object (can't shift block from free list)\n");
    abort();
}

Block *GC_GlobalAllocator_nextFreeBlock(GlobalAllocator *self) {
    Block *block;

    GC_lock();

    // 1. exhaust free list:
    block = BlockList_shift(&self->free_list);
    if (block != NULL) {
        GC_unlock();
        return block;
    }

    // 2. no block? collect!
    if (GlobalAllocator_tryCollect(self)) {
        // 2a. still no free blocks? grow!
        if (BlockList_isEmpty(&self->free_list)) {
            GlobalAllocator_growSmall(self);
        }
    } else {
        // 2b. grow
        GlobalAllocator_growSmall(self);
    }

    // 3. get free block!
    block = BlockList_shift(&self->free_list);
    if (block != NULL) {
        GC_unlock();
        return block;
    }

    // 4. seriously, no luck
    fprintf(stderr, "GC: failed to allocate small object (can't shift block from free list)\n");
    abort();
}

void *GC_GlobalAllocator_allocateLarge(GlobalAllocator *self, size_t size, int atomic) {
    size_t rsize = ROUND_TO_NEXT_MULTIPLE(size, WORD_SIZE);
    void *mutator;

    GC_lock();

    // 1. try to allocate
    mutator = GlobalAllocator_tryAllocateLarge(self, rsize, atomic);
    if (mutator != NULL) {
        GC_unlock();
        return mutator;
    }

    // 2. collect memory
    if (GlobalAllocator_tryCollect(self)) {
        // 2a. try to allocate (again)
        mutator = GlobalAllocator_tryAllocateLarge(self, rsize, atomic);
        if (mutator != NULL) {
            GC_unlock();
            return mutator;
        }
    }

    // 3. grow memory
    GlobalAllocator_growLarge(self, rsize + sizeof(Chunk));

    // 4. allocate!
    mutator = GlobalAllocator_tryAllocateLarge(self, rsize, atomic);
    if (mutator != NULL) {
        GC_unlock();
        return mutator;
    }

    // 5. seriously? no luck.
    fprintf(stderr, "GC: failed to allocate large object size=%zu actual=%zu +metadata=%zu\n",
            size, rsize, sizeof(Chunk));

    ChunkList_debug(&self->large_chunk_list);
    abort();
}

void GC_GlobalAllocator_deallocateLarge(__attribute__((__unused__)) GlobalAllocator *self, void *pointer) {
    Chunk *chunk = (Chunk *)pointer - 1;

    GC_lock();

    finalizer_t finalizer = GlobalAllocator_deleteFinalizer(self, &chunk->object);
    if (finalizer != NULL) {
        finalizer(Chunk_mutatorAddress(chunk));
    }

    chunk->allocated = (uint8_t)0;

    // TODO: merge with next free chunks (?)

    GC_unlock();
}

void GC_GlobalAllocator_recycleBlocks(GlobalAllocator *self) {
    BlockList_clear(&self->free_list);
    BlockList_clear(&self->recyclable_list);

    Block *block = self->small_heap_start;
    Block *stop = self->small_heap_stop;

    while (block < stop) {
        if (!Block_isMarked(block)) {
            // free block
            Block_setFree(block);
            DEBUG("GC: free block=%p\n", (void *)block);
            BlockList_push(&self->free_list, block);
        } else {
            // try to recycle block (find unmarked lines)
            char *line_headers = block->line_headers;

            int first_free_line_index = INVALID_LINE_INDEX;

            // we determine, link and record holes (free lines) in a recycled
            // block while sweeping (so allocating doesn't have to do it):
            Hole *hole = NULL;
            Hole *previous_hole = NULL;

            // conservative marking (immix page 5): requires that we skip a free
            // line (see below), and iterate free lines by 2, so we iterate up
            // to N-1 only:
            for (int line_index = 0; line_index < LINE_COUNT; line_index++) {
                char *line_header = line_headers + line_index;

                if (LineHeader_isMarked(line_header)) {
                    if (hole != NULL) {
                        //DEBUG("GC: line=%d marked=1 stop=%p\n", line_index, (void *)Block_line(block, line_index));
                        hole->limit = Block_line(block, line_index);
                        previous_hole = hole;
                        hole = NULL;
                    } else {
                        //DEBUG("GC: line=%d marked=1\n", line_index);
                    }
                } else {
                    //if (hole == NULL) {
                    //    DEBUG("GC: line=%d marked=0 skip\n", line_index);
                    //} else {
                    //    DEBUG("GC: line=%d marked=0\n", line_index);
                    //}
                    LineHeader_clear(line_header);

                    if (hole == NULL && line_index != LINE_COUNT - 1) {
                        // conservative marking (immix page 5): the collector only
                        // marked the starting line for small objects (smaller than
                        // LINE_SIZE), but small objects may span a line, so we skip
                        // a free line when determining holes:
                        if (!LineHeader_isMarked(line_header + 1)) {
                            line_index++;
                            line_header++;

                            LineHeader_clear(line_header);
//#ifndef NDEBUG
//                            // clear free lines: if marking was wrong it will
//                            // corrupt allocations, causing a rapid segfault!
//                            memset(Block_line(block, line_index), 0, LINE_SIZE);
//#endif
                            if (first_free_line_index == INVALID_LINE_INDEX) {
                                first_free_line_index = line_index;
                            }
                            if (hole == NULL) {
                                //DEBUG("GC: line=%d marked=0 start=%p\n", line_index, (void *)Block_line(block, line_index));
                                hole = (Hole *)Block_line(block, line_index);
                                Hole_init(hole);
                            //} else {
                            //    DEBUG("GC: line=%d marked=0\n", line_index);
                            }
                            if (previous_hole != NULL) {
                                previous_hole->next = hole;
                            }
                        }
                    }
                }
            }

            if (hole != NULL && hole->limit == NULL) {
                //DEBUG("GC: line=126 marked=0 stop=%p\n", (void *)Block_stop(block));
                hole->limit = Block_stop(block);
            }

            // at least 1 free line? recycle block; otherwise block is unavailable
            if (first_free_line_index == INVALID_LINE_INDEX) {
                DEBUG("GC: unavailable block=%p\n", (void *)block);
                Block_setUnavailable(block);
            } else {
                DEBUG("GC: recyclable block=%p first_free_line_index=%d\n",
                        (void *)block, first_free_line_index);
                Block_setRecyclable(block, first_free_line_index);
                BlockList_push(&self->recyclable_list, block);

//#ifdef GC_DEBUG
//                hole = (Hole *)Block_firstFreeLine(block);
//                while (hole != NULL) {
//                    intptr_t size = hole->limit - (char *)hole;
//                    fprintf(stderr, "GC: hole start=%p limit=%p size=%ld next=%p\n",
//                            (void *)hole, (void *)hole->limit, size, (void *)hole->next);
//                    hole = hole->next;
//                }
//#endif
            }
        }

        block = (Block *)((char *)block + BLOCK_SIZE);
    }
}
