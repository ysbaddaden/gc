#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "local_allocator.h"
#include "line_header.h"

static inline void LocalAllocator_initCursor(LocalAllocator *self) {
    self->block = GlobalAllocator_nextBlock(self->global_allocator);

    if (Block_isFree(self->block)) {
        self->cursor = Block_start(self->block);
        self->limit = Block_stop(self->block);
        return;
    }

    if (Block_isRecyclable(self->block)) {
         self->cursor = Block_firstFreeLine(self->block);
         Hole *hole = (Hole *)self->cursor;
         self->limit = hole->limit;
         self->next = hole->next;
         return;
    }

    fprintf(stderr, "GC: can't init cursors into unavailable block\n");
    abort();
}

static inline int LocalAllocator_findNextHole(LocalAllocator *self) {
    if (self->next) {
        self->cursor = (char *)self->next;
        self->limit = self->next->limit;
        self->next = self->next->next;
        return 1;
    }
    return 0;
}

static inline void LocalAllocator_initOverflowCursor(LocalAllocator *self) {
    self->overflow_block = GlobalAllocator_nextFreeBlock(self->global_allocator);
    self->overflow_cursor = Block_start(self->overflow_block);
    self->overflow_limit = Block_stop(self->overflow_block);
}

// TODO: Get 2 blocks from the global allocator at once, one that may be a
//       recycled or free block, and another one that must be a free block for
//       overflow allocations; then initialize cursors. That may allow the
//       global allocator to decide better when and how much to grow the HEAP.
void GC_LocalAllocator_reset(LocalAllocator *self) {
    LocalAllocator_initCursor(self);
    LocalAllocator_initOverflowCursor(self);
}

static inline Object *LocalAllocator_overflowAllocateSmall(LocalAllocator *self, size_t size) {
    while (1) {
        char *cursor = self->overflow_cursor;
        char *stop = cursor + size;

        if (stop <= self->overflow_limit) {
            Object *object = (Object *)cursor;
            Line_update(self->overflow_block, object);

            // make sure to clear the size of next object in line, in order to
            // know when to stop iterating objects in the line; obviously we
            // don't clear if we'd cross the limit:
            if (stop < self->overflow_limit) {
                ((Object *)stop)->size = 0;
            }

            // update cursor
            self->overflow_cursor = stop;

            return object;
        }

        // failed to allocate: get another block
        LocalAllocator_initOverflowCursor(self);
    }
}

static inline Object *LocalAllocator_tryAllocateSmall(LocalAllocator *self, size_t size) {
    while (1) {
        char *cursor = self->cursor;
        char *stop = cursor + size;

        // object fits current hole
        if (stop <= self->limit) {
            Object *object = (Object *)cursor;
            Line_update(self->block, object);

            // make sure to clear the size of next object in line, in order to
            // know when to stop iterating objects in the line; obviously we
            // don't clear if we'd cross the limit:
            if (stop < self->limit) {
                ((Object *)stop)->size = 0;
            }

            // update cursor
            self->cursor = stop;

            return object;
        }

        // overflow allocation (immix, page 4): we failed to allocate a
        // medium object into the current hole, but there are one or more
        // free lines available in the current hole, we allocate into an
        // overflow block, to avoid wasting holes for occasional medium sized
        // objects:
        if (size > LINE_SIZE && (self->limit - cursor) > LINE_SIZE) {
            return LocalAllocator_overflowAllocateSmall(self, size);
        }

        // reached end of block
        if (!LocalAllocator_findNextHole(self)) {
            return NULL;
        }
    }
}

void *GC_LocalAllocator_allocateSmall(LocalAllocator *self, size_t size, int atomic) {
    size_t rsize = ROUND_TO_NEXT_MULTIPLE(size + sizeof(Object), WORD_SIZE);
    assert(rsize < LARGE_OBJECT_SIZE);

    while (1) {
        Object *object = LocalAllocator_tryAllocateSmall(self, rsize);

        if (object != NULL) {
            Object_allocate(object, rsize, atomic);
            GlobalAllocator_incrementCounters(self->global_allocator, size);
            return Object_mutatorAddress(object);
        }

        // failed to allocate: get another block
        LocalAllocator_initCursor(self);
    }
}
