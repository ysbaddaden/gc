#include <stdlib.h>
#include <string.h>
#include "immix.h"

static GlobalAllocator GC_global_allocator;
#define global_allocator GC_global_allocator

void GC_init(size_t initial_size) {
    GlobalAllocator_init(&global_allocator, initial_size);
}

int GC_in_heap(void *pointer) {
  return GlobalAllocator_inHeap(&global_allocator, pointer);
}

void* GC_malloc(size_t size) {
    return GlobalAllocator_allocateLarge(&global_allocator, size, 0);
}

void* GC_malloc_atomic(size_t size) {
    return GlobalAllocator_allocateLarge(&global_allocator, size, 1);
}

void* GC_realloc(void *pointer, size_t size) {
    // realloc(3) compatibility
    if (pointer == NULL) {
        return GC_malloc(size);
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
    void *new_pointer = GlobalAllocator_allocateLarge(&global_allocator, size, object->atomic);
    memcpy(new_pointer, pointer, object->size);
    GC_free(pointer);

    return new_pointer;
}

void GC_collect() {
    // TODO: GC_collect()
}

void GC_free(void *pointer) {
    if (GlobalAllocator_inLargeHeap(&global_allocator, pointer)) {
        GlobalAllocator_deallocateLarge(&global_allocator, pointer);
    }
}
