#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "collector.h"
#include "global_allocator.h"
#include "immix.h"
#include "utils.h"

static GlobalAllocator *GC_global_allocator;
#define global_allocator GC_global_allocator

static Collector *GC_collector;
#define collector GC_collector

void GC_init(size_t initial_size) {
    // We could allocate static values instead of using `malloc`, but then the
    // structs would be inlined in the BSS section, along with pointers to the
    // HEAP, that would be invalid and complicated to skip.
    //
    // To keep things simple, we allocate into the libc HEAP, so the BSS section
    // will only have pointers to the program HEAP and won't find pointers to
    // the GC HEAP anymore.

    global_allocator = malloc(sizeof(GlobalAllocator));
    if (global_allocator == NULL) {
        fprintf(stderr, "malloc failed %s\n", strerror(errno));
        abort();
    }
    GlobalAllocator_init(global_allocator, initial_size);

    collector = malloc(sizeof(Collector));
    if (collector == NULL) {
        fprintf(stderr, "malloc failed: %s\n", strerror(errno));
        abort();
    }
    Collector_init(collector, global_allocator);
}

void GC_deinit() {
    free(collector);
    collector = NULL;

    free(global_allocator);
    global_allocator = NULL;
}

int GC_in_heap(void *pointer) {
  return GlobalAllocator_inHeap(global_allocator, pointer);
}

void* GC_malloc(size_t size) {
    void *pointer = GlobalAllocator_allocateLarge(global_allocator, size, 0);
    DEBUG("GC: malloc chunk=%p size=%zu atomic=0 ptr=%p\n", (void*)((Chunk *)pointer - 1), size, pointer);
    return pointer;
}

void* GC_malloc_atomic(size_t size) {
    void *pointer = GlobalAllocator_allocateLarge(global_allocator, size, 1);
    DEBUG("GC: malloc chunk=%p size=%zu atomic=1 ptr=%p\n", (void*)((Chunk *)pointer - 1), size, pointer);
    return pointer;
}

void* GC_realloc(void *pointer, size_t size) {
    void *new_pointer;

    // realloc(3) compatibility
    if (pointer == NULL) {
        new_pointer = GC_malloc(size);
        return new_pointer;
    }

    if (size == 0) {
        GC_free(pointer);
        return NULL;
    }

    // find object size
    Object *object = (Object *)((char *)pointer - sizeof(Object));

    // keep current allocation
    if (size <= object->size) {
        return pointer;
    }

    // reallocate
    new_pointer = GlobalAllocator_allocateLarge(global_allocator, size, object->atomic);
    memcpy(new_pointer, pointer, object->size);
    //GlobalAllocator_deallocateLarge(global_allocator, pointer);

    DEBUG("GC: realloc old=%p new=%p size=%zu atomic=%d\n", pointer, new_pointer, size, object->atomic);

    return new_pointer;
}

void GC_free(void *pointer) {
    DEBUG("GC: free ptr=%p\n", pointer);

    //if (GlobalAllocator_inLargeHeap(global_allocator, pointer)) {
    //    GlobalAllocator_deallocateLarge(global_allocator, pointer);
    //}
}

void GC_collect_once() {
    Collector_collect(collector);
}

void GC_register_collect_callback(collect_callback_t collect_callback) {
    Collector_registerCollectCallback(collector, collect_callback);
}

void GC_mark_region(void *stack_pointer, void *stack_bottom, const char *source) {
    Collector_markRegion(collector, stack_pointer, stack_bottom, source);
}
