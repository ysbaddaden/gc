#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "collector.h"
#include "global_allocator.h"
#include "local_allocator.h"
#include "immix.h"
#include "utils.h"
#include "options.h"

static GlobalAllocator *GC_global_allocator;
#define global_allocator GC_global_allocator

// TODO: associate local allocators to threads.
static LocalAllocator *GC_local_allocator;
#define local_allocator GC_local_allocator

static Collector *GC_collector;
#define collector GC_collector

void GC_init() {
    // We could allocate static values instead of using `malloc`, but then the
    // structs would be inlined in the BSS section, along with pointers to the
    // HEAP...
    //
    // To keep things simple, we allocate into the libc HEAP, so the BSS section
    // will only have pointers to the program HEAP and won't find pointers to
    // the GC HEAP anymore.

    global_allocator = malloc(sizeof(GlobalAllocator));
    if (global_allocator == NULL) {
        fprintf(stderr, "malloc failed %s\n", strerror(errno));
        abort();
    }
    GlobalAllocator_init(global_allocator, GC_initialHeapSize());

    local_allocator = malloc(sizeof(LocalAllocator));
    if (local_allocator == NULL) {
        fprintf(stderr, "malloc failed %s\n", strerror(errno));
        abort();
    }
    LocalAllocator_init(local_allocator, global_allocator);

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

static inline void *GC_malloc_with_atomic(size_t size, int atomic) {
    void *pointer;

    if (size <= LARGE_OBJECT_SIZE - sizeof(Object)) {
        pointer = LocalAllocator_allocateSmall(local_allocator, size, atomic);

        DEBUG("GC: malloc object=%p size=%zu actual=%zu atomic=%d ptr=%p\n",
                (void *)((Object *)pointer - 1),
                size,
                ((Object *)pointer - 1)->size,
                atomic, pointer);
    } else {
        pointer = GlobalAllocator_allocateLarge(global_allocator, size, atomic);

        DEBUG("GC: malloc chunk=%p size=%zu actual=%zu atomic=%d ptr=%p\n",
                (void *)((Chunk *)pointer - 1),
                size,
                ((Chunk *)pointer - 1)->object.size + CHUNK_HEADER_SIZE,
                atomic, pointer);
    }

    return pointer;
}

void* GC_malloc(size_t size) {
    return GC_malloc_with_atomic(size, 0);
}

void* GC_malloc_atomic(size_t size) {
    return GC_malloc_with_atomic(size, 1);
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
    size_t available = Object_mutatorSize(object);

    // keep current allocation
    if (size <= available) {
        return pointer;
    }

    // reallocate
    void *new_pointer = GC_malloc_with_atomic(size, object->atomic);
    memcpy(new_pointer, pointer, available);
    DEBUG("GC: realloc old=%p new=%p size=%zu atomic=%d\n", pointer, new_pointer, size, object->atomic);

    GC_free(pointer);

    return new_pointer;
}

void GC_free(void *pointer) {
    DEBUG("GC: free ptr=%p\n", pointer);

    if (GlobalAllocator_inLargeHeap(global_allocator, pointer)) {
        GlobalAllocator_deallocateLarge(global_allocator, pointer);
    }
}

void GC_collect_once() {
    Collector_collect(collector);
    LocalAllocator_reset(GC_local_allocator);
}

void GC_register_collect_callback(collect_callback_t collect_callback) {
    Collector_registerCollectCallback(collector, collect_callback);
}

void GC_add_roots(void *stack_pointer, void *stack_bottom, const char *source) {
    Collector_addRoots(collector, stack_pointer, stack_bottom, source);
}

void GC_small_heap_stats(size_t *count, size_t *bytes) {
    *count = 0;
    *bytes = 0;

    Block *block = global_allocator->small_heap_start;
    Block *stop = global_allocator->small_heap_stop;

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

                    *count += 1;
                    *bytes += object->size - sizeof(Object);

                    offset = offset + object->size;
                }
            }
        }

        block = (Block *)((char *)block + BLOCK_SIZE);
    }
}

void GC_large_heap_stats(size_t *count, size_t *bytes) {
    *count = 0;
    *bytes = 0;

    Chunk *chunk = global_allocator->large_chunk_list.first;
    while (chunk != NULL) {
        if (chunk->allocated) {
            *count += 1;
            *bytes += chunk->object.size - sizeof(Object);
        }
        chunk = chunk->next;
    }
}

size_t GC_get_memory_use() {
    return GlobalAllocator_heapSize(global_allocator);
}

size_t GC_get_heap_usage() {
    size_t small_count, small_bytes;
    size_t large_count, large_bytes;

    GC_small_heap_stats(&small_count, &small_bytes);
    GC_large_heap_stats(&large_count, &large_bytes);

    return small_bytes + large_bytes;
}

//void GC_print_stats() {
//    size_t small_count, small_bytes;
//    size_t large_count, large_bytes;
//
//    GC_small_heap_stats(&small_count, &small_bytes);
//    GC_large_heap_stats(&large_count, &large_bytes);
//
//    fprintf(stderr, "\nGC: small: count=%zu bytes=%zu; large: count=%zu bytes=%zu\n; total count=%zu bytes=%zu\n",
//            small_count, small_bytes, large_count, large_bytes, small_count + large_count, small_bytes + large_bytes);
//}
