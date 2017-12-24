#ifndef IMMIX_COLLECTOR_H
#define IMMIX_COLLECTOR_H

#include "global_allocator.h"

typedef void (*collect_callback_t)(void);

typedef struct GC_Collector {
    GlobalAllocator *global_allocator;
    collect_callback_t collect_callback;
    void *data_start;
    void *data_end;
    void *bss_start;
    void *bss_end;
} Collector;

void GC_Collector_init(Collector *self, GlobalAllocator *allocator);
void GC_Collector_collect(Collector *self);
void GC_Collector_markRegion(Collector *self, void *stack_top, void *stack_bottom, const char *source);

static inline void Collector_registerCollectCallback(Collector *self, collect_callback_t callback) {
    self->collect_callback = callback;
}

static inline void Collector_callCollectCallback(Collector *self) {
    collect_callback_t callback = self->collect_callback;
    if (callback != NULL) {
        callback();
    }
}

#define Collector_init GC_Collector_init
#define Collector_collect GC_Collector_collect
#define Collector_markRegion GC_Collector_markRegion

#endif
