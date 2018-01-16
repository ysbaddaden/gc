#ifndef GC_LINE_HEADER_H
#define GC_LINE_HEADER_H

#include <stdint.h>
#include <assert.h>
#include "constants.h"

// 0x 111111             1        1
//    ^                  ^        ^
//    offset/word size   marked   object

enum LineHeaderFlags {
    LINE_EMPTY = 0x0,
    LINE_MARKED = 0x1,
    LINE_CONTAINS_OBJECT = 0x2,
};

#define LINE_OBJECT_OFFSET_MARK (uint8_t)0xFC

static inline void LineHeader_clear(char *flag) {
    *flag = (uint8_t)LINE_EMPTY;
}

static inline int LineHeader_isMarked(char *flag) {
    return (*flag & LINE_MARKED) == LINE_MARKED;
}

static inline int LineHeader_mark(char *flag) {
    return *flag |= LINE_MARKED;
}

static inline int LineHeader_unmark(char *flag) {
    return *flag &= ~LINE_MARKED;
}

static inline int LineHeader_containsObject(char *flag) {
    return (*flag & LINE_CONTAINS_OBJECT) == LINE_CONTAINS_OBJECT;
}

static inline void LineHeader_setOffset(char *flag, int offset) {
    assert(offset % WORD_SIZE == 0);
    assert(offset >= 0);
    assert(offset < LINE_SIZE);
    *flag = (offset & LINE_OBJECT_OFFSET_MARK) | LINE_CONTAINS_OBJECT;
}

static inline int LineHeader_getOffset(char *flag) {
    assert(LineHeader_containsObject(flag));
    return (int)(*flag & LINE_OBJECT_OFFSET_MARK);
}


typedef struct GC_Hole {
    char *limit;
    struct GC_Hole *next;
} Hole;

static inline void Hole_init(Hole *self) {
    self->limit = NULL;
    self->next = NULL;
}

#endif
