#ifndef GC_BLOCK_H
#define GC_BLOCK_H

#include "config.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "constants.h"
#include "object.h"
#include "line_header.h"
#include "utils.h"

#define INVALID_LINE_INDEX -1

enum BlockFlag {
    BLOCK_FLAG_FREE = 0x0,
    BLOCK_FLAG_RECYCLABLE = 0x1,
    BLOCK_FLAG_UNAVAILABLE = 0x2
};

typedef struct GC_Block {
    uint8_t marked;
    uint8_t flag;
    int16_t first_free_line_index;
    struct GC_Block *next;
    char line_headers[LINE_COUNT];
} Block;

static inline void Block_init(Block *self) {
    memset((char *)self, 0, sizeof(Block));
}

static inline Block *Block_from(void *pointer) {
    return (Block *)((uintptr_t)pointer & BLOCK_SIZE_IN_BYTES_INVERSE_MASK);
}

static inline void Block_setFlag(Block *self, enum BlockFlag flag) {
    self->flag = flag;
}

static inline int Block_isFree(Block *self) {
    return self->flag == BLOCK_FLAG_FREE;
}

static inline int Block_isRecyclable(Block *self) {
    return self->flag == BLOCK_FLAG_RECYCLABLE;
}

static inline int Block_isUnavailable(Block *self) {
    return self->flag == BLOCK_FLAG_UNAVAILABLE;
}

static inline void Block_setFree(Block *self) {
    Block_init(self);
}

static inline void Block_setRecyclable(Block *self, int first_free_line_index) {
    self->flag = BLOCK_FLAG_RECYCLABLE;
    self->first_free_line_index = first_free_line_index;
}

static inline void Block_setUnavailable(Block *self) {
    self->flag = BLOCK_FLAG_UNAVAILABLE;
}


static inline void Block_mark(Block *self) {
    self->marked = 1;
}

static inline void Block_unmark(Block *self) {
    self->marked = 0;
}

static inline int Block_isMarked(Block *self) {
    return self->marked == 1;
}


// Returns a pointer to the first line in the block (after metadata).
static inline char *Block_start(Block *self) {
    return (char *)self + LINE_SIZE;
}

// Returns a pointer to the block limit.
static inline char *Block_stop(Block *self) {
    return (char *)self + BLOCK_SIZE;
}

// Returns whether a pointer points into the allocatable lines of a block.
static inline int Block_contains(Block *self, char *pointer) {
    return pointer >= Block_start(self) && pointer < Block_stop(self);
}

// Returns a pointer to the first free line in the block.
static inline char *Block_firstFreeLine(Block *self) {
    assert(self->first_free_line_index >= 0);
    assert(self->first_free_line_index < LINE_COUNT);
    return Block_start(self) + (LINE_SIZE * self->first_free_line_index);
}

// Returns a pointer to the block's line metadata.
static inline char *Block_lineHeaders(Block *self) {
    return self->line_headers;
}

// Returns the line header for the line that a pointer into the block points to.
static inline char *Block_lineHeader(Block *self, int line_index) {
    assert(line_index >= 0);
    assert(line_index < LINE_COUNT);
    return self->line_headers + line_index;
}

// Returns the line index that a pointer into the block points to.
static inline int Block_lineIndex(Block *self, void *pointer) {
    //assert((char *)pointer >= Block_start(self));
    assert((char *)pointer <= Block_stop(self));

    intptr_t diff = (char *)pointer - Block_start(self);
    if (diff < 0) return INVALID_LINE_INDEX;
    return (int)(diff / LINE_SIZE);
}

// Returns a pointer to the line at a given index.
static inline char *Block_line(Block *self, int line_index) {
    assert(line_index >= 0);
    assert(line_index < LINE_COUNT);
    return Block_start(self) + (line_index * LINE_SIZE);
}

static inline void Line_update(Block *block, Object *object) {
    assert(Block_contains(block, (char *)object));

    int line_index = Block_lineIndex(block, object);
    assert(line_index != INVALID_LINE_INDEX);

    char *line_header = Block_lineHeader(block, line_index);

    if (!LineHeader_containsObject(line_header)) {
        intptr_t offset = (char *)object - Block_line(block, line_index);
        assert(offset >= 0);
        assert(offset < LINE_SIZE);

        //DEBUG("GC: set first object in line block=%p object=%p line_index=%d offset=%ld\n",
        //        (void *)block, (void *)object, line_index, offset);

        LineHeader_setOffset(line_header, offset);
    //} else {
    //    DEBUG("GC: not first object in line block=%p object=%p line_index=%d\n",
    //            (void *)block, (void *)object, line_index);
    }
}

#endif
