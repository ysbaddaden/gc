#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include "collector.h"
#include "line_header.h"
#include "memory.h"
#include "utils.h"

void GC_Collector_init(Collector *self, GlobalAllocator *allocator) {
    self->global_allocator = allocator;
    self->collect_callback = NULL;
    Stack_init(&self->roots, GC_getMemoryLimit());
}

static inline void Collector_unmarkSmallObjects(Collector *self) {
    Block *block = self->global_allocator->small_heap_start;
    Block *stop = self->global_allocator->small_heap_stop;

    while (block < stop) {
        Block_unmark(block);

        char *line_headers = Block_lineHeaders(block);

        for (int line_index = 0; line_index < LINE_COUNT; line_index++) {
            char *line_header = line_headers + line_index;
            LineHeader_unmark(line_header);

            if (LineHeader_containsObject(line_header)) {
                char *line = Block_line(block, line_index);
                int offset = LineHeader_getOffset(line_header);

                while (offset < LINE_SIZE) {
                    Object *object = (Object *)(line + offset);
                    if (object->size == 0) break;

                    Object_unmark(object);

                    offset = offset + object->size;
                }
            }
        }

        block = (Block *)((char *)block + BLOCK_SIZE);
    }
}

static inline void Collector_unmarkLargeObjects(Collector *self) {
    Chunk *chunk = self->global_allocator->large_chunk_list.first;
    while (chunk != NULL) {
        Chunk_unmark(chunk);
        chunk = chunk->next;
    }
}

static inline void Collector_scanObject(Collector *self, Object *object) {
    DEBUG("GC: mark ptr=%p size=%zu atomic=%d\n",
            Object_mutatorAddress(object), object->size, object->atomic);

    if (!object->atomic) {
        void *sp = Object_mutatorAddress(object);
        void *bottom = (char*)object + object->size;
        Stack_push(&self->roots, sp, bottom);
    }
}

static inline void Collector_markChunk(Collector *self, Chunk *chunk) {
    if (chunk != NULL && Chunk_isAllocated(chunk)) {
        Object *object = &chunk->object;

        if (!Object_isMarked(object)) {
            Object_mark(object);
            Collector_scanObject(self, object);
        }
    }
}

static inline void Collector_findAndMarkSmallObject(Collector *self, void *pointer) {
    Block *block = Block_from(pointer);

    int line_index = Block_lineIndex(block, pointer);

    // invalid: pointer to block metadata
    if (line_index < 0) return;

    char *line_header = Block_lineHeader(block, line_index);

    // first object in line is *after* the pointer? then object is probably on
    // the previous line!
    if (LineHeader_containsObject(line_header)) {
        char *line = Block_line(block, line_index);
        int offset = LineHeader_getOffset(line_header);

        if ((char *)pointer < (line + offset)) {
            line_index--;
        }

        // invalid: pointer to block metadata
        if (line_index < 0) return;
    }

    // an object may span multiple lines, search previous lines until we find
    // one with an object:
    for (; line_index >= 0; line_index--) {
        line_header = Block_lineHeader(block, line_index);

        if (LineHeader_containsObject(line_header)) {
            char *line = Block_line(block, line_index);
            int offset = LineHeader_getOffset(line_header);

            // done: crossed line (warning: didn't find object)
            while (offset < LINE_SIZE) {
                Object *object = (Object*)(line + offset);

                // done: no more object in line (warning: didn't find object)
                if (object->size == 0) return;

#ifndef NDEBUG
                if (object->size > LARGE_OBJECT_SIZE) {
                    fprintf(stderr, "GC: invalid small object size %zu (maximum is %zu) line=%p offset=%d\n",
                            object->size, LARGE_OBJECT_SIZE, (void *)line, offset);
                    abort();
                }
#endif

                if (Object_contains(object, pointer)) {
                    if (!Object_isMarked(object)) {
                        Object_mark(object);
                        Block_mark(block);

                        // conservative marking (immix page 5): small objects
                        // (smaller than LINE_SIZE) are more common than medium
                        // objects (larger than LINE_SIZE) and we can speed up
                        // marking by only marking the starting line.
                        if (object->size <= LINE_SIZE) {
                            // small object: only mark the starting line
                            // DEBUG("GC: mark line=%p\n", (void *)line);
                            LineHeader_mark(line_header);
                        } else {
                            // medium object: mark all lines exactly
                            char *limit = (char *)object + object->size;
                            do {
                                // DEBUG("GC: mark line=%p\n", (void *)line);
                                LineHeader_mark(line_header);
                                line += LINE_SIZE;
                                line_header++;
                            } while (line < limit);
                        }

                        Collector_scanObject(self, object);
                    }
                    return;
                }

                offset += object->size;
            }

            // should be unreachable (warning: didn't find object)
            return;
        }
    }
}

void GC_Collector_addRoots(Collector *self, void *top, void *bottom, const char *source) {
    DEBUG("GC: mark region top=%p bottom=%p source=%s\n", top, bottom, source);
    assert(top <= bottom);
    Stack_push(&self->roots, top, bottom);
}

void GC_Collector_mark(Collector *self) {
    Chunk *chunk;
    ChunkList *large_chunk_list = &self->global_allocator->large_chunk_list;

    void *sp;
    void *bottom;

    while (Stack_pop(&self->roots, &sp, &bottom)) {
        while (sp < bottom) {
            // dereference stack pointer's value as heap pointer:
            void *pointer = (void *)(*(uintptr_t *)(sp));

            // search chunk for pointer (may be inner pointer):
            if (GlobalAllocator_inSmallHeap(self->global_allocator, pointer)) {
                Collector_findAndMarkSmallObject(self, pointer);
            } else if (GlobalAllocator_inLargeHeap(self->global_allocator, pointer)) {
                chunk = ChunkList_find(large_chunk_list, pointer);
                Collector_markChunk(self, chunk);
            }

            // try next stack pointer
            sp = (char*)sp + sizeof(void *);
        }
    }
}

static inline void Collector_sweep(Collector *self) {
    // small objects
    GlobalAllocator_recycleBlocks(self->global_allocator);

    // large objects
    ChunkList_sweep(&self->global_allocator->large_chunk_list);
//#ifndef NDEBUG
//    ChunkList_validate(&self->global_allocator->large_chunk_list, self->global_allocator->large_heap_stop);
//#endif
}

static inline void Collector_finalizeSmallObjects(Collector *self) {
    Block *block = self->global_allocator->small_heap_start;
    Block *stop = self->global_allocator->small_heap_stop;

    while (block < stop) {
        char *line_headers = Block_lineHeaders(block);

        for (int line_index = 0; line_index < LINE_COUNT; line_index++) {
            char *line_header = line_headers + line_index;

            if (LineHeader_containsObject(line_header)) {
                char *line = Block_line(block, line_index);
                int offset = LineHeader_getOffset(line_header);

                while (offset < LINE_SIZE) {
                    Object *object = (Object *)(line + offset);
                    if (object->size == 0) break;

                    if (!Object_isMarked(object) && Object_hasFinalizer(object)) {
                        Object_runThenClearFinalizer(object);
                    }
                    offset += object->size;
                }
            }
        }

        block = (Block *)((char *)block + BLOCK_SIZE);
    }
}

static inline void Collector_finalizeLargeObjects(Collector *self) {
    Chunk *chunk = self->global_allocator->large_chunk_list.first;
    while (chunk != NULL) {
        if (Chunk_isAllocated(chunk) && !Chunk_isMarked(chunk) && Object_hasFinalizer(&chunk->object)) {
            Object_runThenClearFinalizer(&chunk->object);
        }
        chunk = chunk->next;
    }
}

void GC_Collector_collect(Collector *self) {
    DEBUG("GC: collect start\n");

    // 1. unmark all objects
    Collector_unmarkSmallObjects(self);
    Collector_unmarkLargeObjects(self);

    // 2. collect stack roots
    Collector_addRoots(self, GC_DATA_START, GC_DATA_END, ".data");
    Collector_addRoots(self, GC_BSS_START, GC_BSS_END, ".bss");
    Collector_callCollectCallback(self);

    // 3. search reachable objects to mark (recursively)
    Collector_mark(self);
    GlobalAllocator_resetCounters(self->global_allocator);

    // 4. finalize unreachable objects
    Collector_finalizeSmallObjects(self);
    Collector_finalizeLargeObjects(self);

    // 5. cleanup
    Collector_sweep(self);

    // TODO: reset local allocators (block = cursor = limit = NULL)
    //       this is done in GC_collect_once for the time being

    DEBUG("GC: collect end\n");
}
