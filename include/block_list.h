#ifndef GC_BLOCK_LIST_H
#define GC_BLOCK_LIST_H

#include "config.h"
#include "block.h"

typedef struct GC_BlockList {
    Block *first;
    Block *last;
    size_t size;
} BlockList;

static inline void BlockList_clear(BlockList *self) {
    self->first = NULL;
    self->last = NULL;
    self->size = 0;
}

static inline int BlockList_isEmpty(BlockList *self) {
    return self->first == NULL;
}

static inline void BlockList_push(BlockList *self, Block *block) {
    block->next = NULL;

    if (BlockList_isEmpty(self)) {
        self->first = block;
    } else {
        self->last->next = block;
    }
    self->last = block;
    self->size++;
}

static inline Block *BlockList_shift(BlockList *self) {
    Block *block = self->first;

    if (block != NULL) {
        self->first = block->next;
        self->size--;
    }

    return block;
}

#endif
