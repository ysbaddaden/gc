#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include "collector.h"
#include "utils.h"

extern char __data_start[];
extern char __bss_start[];
extern char _end[];

// TODO: support various platforms (only linux-gnu for now)
void GC_Collector_init(Collector *self, GlobalAllocator *allocator) {
    self->global_allocator = allocator;
    self->collect_callback = NULL;

    // DATA section (initialized constants):
    self->data_start = &__data_start;
    self->data_end = &__bss_start;

    // BSS section (uninitialized constants):
    self->bss_start = &__bss_start;
    self->bss_end = &_end;
}

static inline void Collector_unmarkAll(Collector *self) {
    Chunk *chunk;

    chunk = self->global_allocator->small_chunk_list.first;
    while (chunk != NULL) {
        Chunk_unmark(chunk);
        chunk = chunk->next;
    }

    chunk = self->global_allocator->large_chunk_list.first;
    while (chunk != NULL) {
        Chunk_unmark(chunk);
        chunk = chunk->next;
    }
}

static inline void Collector_markObject(Collector *self, Object *object) {
    if (!object->marked) {
        object->marked = 1;

        DEBUG("GC: mark ptr=%p size=%zu atomic=%d\n",
                Object_mutatorAddress(object), object->size, object->atomic);

        if (!object->atomic) {
            void *sp = Object_mutatorAddress(object);
            void *bottom = (char*)object + object->size;
            Collector_markRegion(self, sp, bottom, "object");
        }
    }
}

static inline void Collector_markChunk(Collector *self, Chunk *chunk) {
    if (chunk != NULL && chunk->allocated) {
        Collector_markObject(self, &chunk->object);
    }
}

void GC_Collector_markRegion(Collector *self, void *top, void *bottom, const char *source) {
    DEBUG("GC: mark region top=%p bottom=%p source=%s\n", top, bottom, source);
    assert(top <= bottom);

    ChunkList *small_chunk_list = &self->global_allocator->small_chunk_list;
    ChunkList *large_chunk_list = &self->global_allocator->large_chunk_list;
    Chunk *chunk;

    void *sp = top;
    while (sp < bottom) {
        // dereference stack pointer's value as heap pointer:
        void *pointer = (void *)(*(uintptr_t *)(sp));

        // search chunk for pointer (may be inner pointer):
        if (GlobalAllocator_inSmallHeap(self->global_allocator, pointer)) {
            chunk = ChunkList_find(small_chunk_list, pointer);
            Collector_markChunk(self, chunk);
        } else if (GlobalAllocator_inLargeHeap(self->global_allocator, pointer)) {
            chunk = ChunkList_find(large_chunk_list, pointer);
            Collector_markChunk(self, chunk);
        }

        // try next stack pointer
        sp = (char*)sp + sizeof(void *);
    }
}

static inline void Collector_sweep(Collector *self) {
    ChunkList_sweep(&self->global_allocator->small_chunk_list);
#ifndef NDEBUG
    ChunkList_validate(&self->global_allocator->small_chunk_list, self->global_allocator->small_heap_stop);
#endif

    ChunkList_sweep(&self->global_allocator->large_chunk_list);
#ifndef NDEBUG
    ChunkList_validate(&self->global_allocator->large_chunk_list, self->global_allocator->large_heap_stop);
#endif
}

void GC_Collector_collect(Collector *self) {
    DEBUG("GC: collect start\n");

    Collector_unmarkAll(self);

    Collector_markRegion(self, self->data_start, self->data_end, ".data");
    Collector_markRegion(self, self->bss_start, self->bss_end, ".bss");
    Collector_callCollectCallback(self);

    Collector_sweep(self);
    DEBUG("GC: collect end\n");
}
