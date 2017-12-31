#ifndef GC_IMMIX_H
#define GC_IMMIX_H

#include <stddef.h>

// GC must have been initialized before calling any other GC_ function.
// Otherwise these functions will segfault the program.
void GC_init(size_t initial_size);

// GC may be deallocated. This will free a few malloc so valgrind doesn't
// complain. Of course calling any GC_ function thereafter will segfault the
// program.
void GC_deinit();

int GC_in_heap(void *pointer);

void *GC_malloc(size_t size);
void *GC_malloc_atomic(size_t size);
void *GC_realloc(void *pointer, size_t size);
void GC_free(void *pointer);

// typedef void (*GC_finalizer_t)(void *);
// void GC_register_finalizer(void *pointer, GC_finalizer_t);

// The library doesn't define GC_collect. The program is responsible for
// defining the symbol and calling GC_collect_once. For example in crystal we
// switch to a dedicated fiber that will run the collector. Switching fibers
// will save the registers on the stack and save the current stack pointer (so
// GC doesn't have to).
void GC_collect();
void GC_collect_once();

// We don't detect or collect stacks to iterate to find objects to mark. The
// program is responsible for registering a callback that will call
// GC_mark_region for all required stack roots; except for the DATA and BSS
// sections that are automatically handled.
typedef void (*GC_collect_callback_t)(void);
void GC_register_collect_callback(GC_collect_callback_t);
void GC_mark_region(void *stack_pointer, void *stack_bottom, const char *source);

#endif
