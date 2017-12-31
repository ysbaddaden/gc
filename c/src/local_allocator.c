#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "local_allocator.h"
#include "line_header.h"

static inline void LocalAllocator_initCursors(LocalAllocator *self) {
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

static inline Object *LocalAllocator_tryAllocateSmall(LocalAllocator *self, size_t size) {
    while (1) {
        char *cursor = self->cursor;
        char *stop = cursor + size;

        if (stop <= self->limit) {
            // allocate
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

        // try with a new block (requires global allocator sync)
        if (!LocalAllocator_findNextHole(self)) {
            // reached end of block
            return NULL;
        }
    }
}

void *GC_LocalAllocator_allocateSmall(LocalAllocator *self, size_t size, int atomic) {
    assert(size < LARGE_OBJECT_SIZE);

    size_t rsize = ROUND_TO_NEXT_MULTIPLE(size + sizeof(Object), WORD_SIZE);

    while (1) {
        if (self->block != NULL) {
            Object *object = LocalAllocator_tryAllocateSmall(self, rsize);

            if (object != NULL) {
                Object_allocate(object, rsize, atomic);
                return Object_mutatorAddress(object);
            }
        }

        // failed to allocate: try next block
        self->block = GlobalAllocator_nextBlock(self->global_allocator);
        LocalAllocator_initCursors(self);
    }

    abort();
}
