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
    Chunk *chunk = self->global_allocator->chunk_list.first;

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

static inline Chunk *Collector_findChunk(Collector *self, void *pointer) {
    Chunk* chunk = self->global_allocator->chunk_list.first;

    while (chunk != NULL) {
        if (Chunk_contains(chunk, pointer)) {
            return chunk;
        }
        chunk = chunk->next;
    }

    return NULL;
}

void GC_Collector_markRegion(Collector *self, void *top, void *bottom, const char *source) {
    DEBUG("GC: mark region top=%p bottom=%p source=%s\n", top, bottom, source);
    assert(top <= bottom);

    void *sp = top;
    while (sp < bottom) {
        // dereference stack pointer's value as heap pointer:
        void *pointer = (void *)(*(uintptr_t *)(sp));

        if (GlobalAllocator_inLargeHeap(self->global_allocator, pointer)) {
            // search chunk (may be an inner pointer):
            Chunk *chunk = Collector_findChunk(self, pointer);

            if (chunk == NULL) {
                DEBUG("GC: failed to find chunk for ptr=%p\n", pointer);
            } else if (chunk->allocated) {
                Collector_markObject(self, &chunk->object);
            }
        }

        sp = (char*)sp + sizeof(void *);
    }
}

static inline void Collector_sweep(Collector *self) {
    Chunk* chunk = self->global_allocator->chunk_list.first;
    while (chunk != NULL) {
        if (!Chunk_isMarked(chunk)) {
            DEBUG("GC: free chunk=%p ptr=%p size=%zu\n",
                    (void *)chunk, Chunk_mutatorAddress(chunk), chunk->object.size);
            chunk->allocated = 0;
        }
        chunk = chunk->next;
    }
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
