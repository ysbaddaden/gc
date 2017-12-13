#ifndef GC_CHUNK_LIST_H
#define GC_CHUNK_LIST_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include "object.h"
#include "utils.h"

typedef struct Chunk {
    struct Chunk *next;
    uint8_t allocated;
    Object object;
} Chunk;

#define CHUNK_HEADER_SIZE (sizeof(Chunk) - sizeof(Object))
#define CHUNK_MIN_SIZE (sizeof(Chunk) * 2)

static inline void Chunk_init(Chunk *chunk, size_t size) {
    chunk->next = NULL;
    chunk->allocated = 0;
    chunk->object.size = size;

    // not required:
    chunk->object.marked = 0;
    chunk->object.atomic = 0;
}

static inline void Chunk_unmark(Chunk *chunk) {
    chunk->object.marked = 0;
}

static inline int Chunk_isMarked(Chunk *chunk) {
    return chunk->object.marked == 1;
}

static inline void* Chunk_mutatorAddress(Chunk *chunk) {
    return (char*)chunk + sizeof(Chunk);
}

static inline int Chunk_contains(Chunk *chunk, void *pointer) {
    void *start = Chunk_mutatorAddress(chunk);
    void *stop = (char *)chunk + CHUNK_HEADER_SIZE + chunk->object.size;
    return (pointer >= start) && (pointer < stop);
}


typedef struct {
    Chunk *first;
    Chunk *last;
    size_t size;
} ChunkList;

static inline void ChunkList_clear(ChunkList *self) {
    self->first = NULL;
    self->last = NULL;
    self->size = 0;
}

static inline int ChunkList_isEmpty(ChunkList *self) {
    return self->first == NULL;
}

static inline void ChunkList_push(ChunkList *self, Chunk *chunk) {
    chunk->next = NULL;

    if (ChunkList_isEmpty(self)) {
        self->first = chunk;
    } else {
        self->last->next = chunk;
    }
    self->last = chunk;
    self->size++;
}

//static inline Chunk *ChunkList_shift(ChunkList *self) {
//    Chunk *chunk = self->first;
//
//    if (chunk != NULL) {
//        self->first = chunk->next;
//        self->size--;
//    }
//
//    return chunk;
//}

static inline void ChunkList_insert(ChunkList *self, Chunk *chunk, Chunk* after) {
    Chunk *previous = after;

    if (after == self->last) {
        chunk->next = NULL;
        self->last->next = chunk;
        self->last = chunk;
    } else {
        chunk->next = previous->next;
        previous->next = chunk;
    }

    self->size++;
}

static inline Chunk *ChunkList_split(ChunkList *self, Chunk *chunk, size_t size) {
    size_t remaining = chunk->object.size - size;

    // enough space to accomodate new chunk?
    if (remaining < CHUNK_MIN_SIZE) {
        return NULL;
    }

    // resize current chunk
    chunk->object.size = size;

    // insert new chunk (free)
    Chunk *free = (Chunk *)((char *)chunk + CHUNK_HEADER_SIZE + size);
    Chunk_init(free, remaining - CHUNK_HEADER_SIZE);
    ChunkList_insert(self, free, chunk);

    DEBUG("GC: insert chunk=%p previous=%p next=%p size=%zu\n",
            (void *)free, (void *)chunk, (void *)free->next, free->object.size);

    return free;
}

static inline void ChunkList_validate(ChunkList *self, void *heap_stop) {
    Chunk *chunk = self->first;

    size_t count = 0;

    while (chunk != NULL) {
        count++;

        if (chunk->next == NULL) {
            void *actual = (char *)chunk + CHUNK_HEADER_SIZE + chunk->object.size;

            if (actual != heap_stop) {
                fprintf(stderr, "ASSERTION FAILED: heap_stop %p == chunk+header+size %p\n", heap_stop, actual);
                abort();
            }

            if (chunk != self->last) {
                fprintf(stderr, "ASSERTION FAILED: chunk %p == self->last %p\n", (void *)chunk, (void *)self->last);
                abort();
            }
            if (count != self->size) {
                fprintf(stderr, "ASSERTION FAILED: count %zu == self->size %zu\n", count, self->size);
                abort();
            }
            return;
        }

        char *expected = (char *)chunk->next;
        char *actual = (char *)chunk + CHUNK_HEADER_SIZE + chunk->object.size;

        if (actual != expected) {
            fprintf(stderr, "ASSERTION FAILED: chunk->next %p == chunk+header+size %p\n", expected, actual);
            abort();
        }

        chunk = chunk->next;
    }
}

static inline void ChunkList_debug(ChunkList *self) {
    Chunk *chunk = self->first;
    while (chunk != NULL) {
        fprintf(stderr, "CHUNK_LIST: chunk=%p allocated=%d size=%zu marked=%d atomic=%d\n",
                (void *)chunk, chunk->allocated, chunk->object.size, chunk->object.marked, chunk->object.atomic);
        chunk = chunk->next;
    }
}

#endif
