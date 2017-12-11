#ifndef GC_IMMIX_H
#define GC_IMMIX_H

#include "global_allocator.h"
#include "object.h"

void GC_init(size_t initial_size);
int GC_in_heap(void *pointer);
void *GC_malloc(size_t size);
void *GC_malloc_atomic(size_t size);
void *GC_realloc(void *pointer, size_t size);
void GC_collect();
void GC_free(void *pointer);

typedef void (*finalizer_t)(void *);
void GC_register_finalizer(void *pointer, finalizer_t);

#endif
