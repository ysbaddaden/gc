#include "config.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "array.h"
#include "collector.h"
#include "global_allocator.h"
#include "local_allocator.h"
#include "immix.h"
#include "utils.h"
#include "options.h"

static GlobalAllocator *GC_global_allocator;
#define global_allocator GC_global_allocator

static pthread_key_t GC_local_allocator_key;
static Array *GC_local_allocators;

static Collector *GC_collector;
#define collector GC_collector

static pthread_mutex_t *GC_mutex;

static inline void setLocalAllocator(LocalAllocator *local_allocator) {
  int err = pthread_setspecific(GC_local_allocator_key, local_allocator);
  if (err) {
      fprintf(stderr, "pthread_setspecific failed: %s", strerror(err));
      abort();
  }
}

static inline LocalAllocator *getLocalAllocator() {
  return (LocalAllocator *)pthread_getspecific(GC_local_allocator_key);
}

void GC_init() {
    // We could allocate static values instead of using `malloc`, but then the
    // structs would be inlined in the BSS section, along with pointers to the
    // HEAP...
    //
    // To keep things simple, we allocate into the libc HEAP, so the BSS section
    // will only have pointers to the program HEAP and won't find pointers to
    // the GC HEAP anymore.

    GC_mutex = malloc(sizeof(pthread_mutex_t));
    if (GC_mutex == NULL) {
        fprintf(stderr, "malloc failed %s\n", strerror(errno));
        abort();
    }
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(GC_mutex, &attr);

    global_allocator = malloc(sizeof(GlobalAllocator));
    if (global_allocator == NULL) {
        fprintf(stderr, "malloc failed %s\n", strerror(errno));
        abort();
    }
    GlobalAllocator_init(global_allocator, GC_initialHeapSize());

    int err = pthread_key_create(&GC_local_allocator_key, GC_deinit_thread);
    if (err) {
        fprintf(stderr, "malloc failed %s\n", strerror(err));
        abort();
    }
    GC_local_allocators = malloc(sizeof(Array));
    if (GC_local_allocators == NULL) {
        fprintf(stderr, "malloc failed %s\n", strerror(errno));
        abort();
    }
    Array_init(GC_local_allocators, 16l);

    collector = malloc(sizeof(Collector));
    if (collector == NULL) {
        fprintf(stderr, "malloc failed: %s\n", strerror(errno));
        abort();
    }
    Collector_init(collector, global_allocator);

    // Last but not least: initialize the current thread!
    GC_init_thread();
}

void GC_deinit() {
    free(collector);
    collector = NULL;

    pthread_key_delete(GC_local_allocator_key);
    Array_each(GC_local_allocators, free);
    free(GC_local_allocators);

    Hash_free(global_allocator->finalizers);
    global_allocator->finalizers = NULL;

    free(global_allocator);
    global_allocator = NULL;

    free(GC_mutex);
    GC_mutex = NULL;
}

void GC_init_thread() {
    void *local_allocator = malloc(sizeof(LocalAllocator));
    if (local_allocator == NULL) {
        fprintf(stderr, "malloc failed %s\n", strerror(errno));
        abort();
    }
    LocalAllocator_init((LocalAllocator *)local_allocator, global_allocator);
    setLocalAllocator((LocalAllocator *)local_allocator);

    GC_lock();
    Array_push(GC_local_allocators, local_allocator);
    GC_unlock();
}

void GC_deinit_thread(void *local_allocator) {
    GC_lock();
    Array_delete(GC_local_allocators, local_allocator);
    GC_unlock();

    free(local_allocator);
}

void GC_lock() {
    int err = pthread_mutex_lock(GC_mutex);
    if (err) {
      fprintf(stderr, "pthread_mutex_lock failed: %s", strerror(err));
      abort();
    }
}

void GC_unlock() {
    int err = pthread_mutex_unlock(GC_mutex);
    if (err) {
      fprintf(stderr, "pthread_mutex_unlock failed: %s", strerror(err));
      abort();
    }
}

int GC_in_heap(void *pointer) {
    return GlobalAllocator_inHeap(global_allocator, pointer);
}

static inline void *GC_malloc_with_atomic(size_t size, int atomic) {
    void *pointer;

    if (size <= LARGE_OBJECT_SIZE - sizeof(Object)) {
        pointer = LocalAllocator_allocateSmall(getLocalAllocator(), size, atomic);

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

    finalizer_t finalizer = GlobalAllocator_deleteFinalizer(global_allocator, object);
    if (finalizer != NULL) {
        Object *new_object = (Object *)((char *)new_pointer - sizeof(Object));
        GlobalAllocator_registerFinalizer(global_allocator, new_object, finalizer);
    }
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

void GC_register_finalizer(void *pointer, finalizer_t callback) {
    Object *object = (Object *)pointer - 1;
    GlobalAllocator_registerFinalizer(global_allocator, object, callback);
}

void GC_collect_once() {
    Collector_setCollecting(collector, 1);
    Collector_collect(collector);
    Array_each(GC_local_allocators, (Array_iterator_t)LocalAllocator_reset);
    Collector_setCollecting(collector, 0);
}

void GC_register_collect_callback(collect_callback_t collect_callback) {
    Collector_registerCollectCallback(collector, collect_callback);
}

int GC_isCollecting() {
    return Collector_isCollecting(collector);
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
