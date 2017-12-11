#ifndef GC_H
#define GC_H

void GC_init(size_t initial_size);
void GC_collect();

void *GC_malloc(size_t size);
void *GC_malloc_atomic(size_t size);
void *GC_realloc(void *pointer, size_t size);
void GC_free(void *pointer);

#endif
