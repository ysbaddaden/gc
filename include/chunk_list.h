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

static inline void Chunk_allocate(Chunk *self, int atomic) {
    self->allocated = 1;
    self->object.atomic = atomic;
    self->object.finalizer = NULL;
}

// Returns the chunk size, counting the chunk and object metadata and the
// mutator size.
static inline int Chunk_size(Chunk *chunk) {
    return chunk->object.size + CHUNK_HEADER_SIZE;
}

// Returns the mutator size, counting neither the chunk nor object metadata.
//static inline int Chunk_mutatorSize(Chunk *chunk) {
//    return Object_mutatorSize(chunk->object);
//}

static inline void Chunk_mark(Chunk *chunk) {
    chunk->object.marked = 1;
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

    if (remaining < CHUNK_MIN_SIZE) {
        // not enough space to accomodate a new chunk, the use the whole chunk.
        // chunk (no resize, no free chunk insertion).
        return NULL;
    }

    // resize current chunk
    chunk->object.size = size;

    // insert new chunk (free)
    Chunk *free_chunk = (Chunk *)((char *)chunk + CHUNK_HEADER_SIZE + size);
    Chunk_init(free_chunk, remaining - CHUNK_HEADER_SIZE);
    ChunkList_insert(self, free_chunk, chunk);

    DEBUG("GC: split chunk=%p [size=%zu] free=%p [size=%zu] next=%p\n",
            (void *)chunk, chunk->object.size,
            (void *)free_chunk, free_chunk->object.size,
            (void *)free_chunk->next);

    assert(((char *)chunk + CHUNK_HEADER_SIZE + size) == ((char *)free_chunk));

    if (free_chunk->next != NULL) {
        assert(((char *)free_chunk + remaining) == ((char *)(free_chunk->next)));
    }

    return free_chunk;
}

// Iterates the list in search of a chunk containing the pointer.
static inline Chunk *ChunkList_find(ChunkList *self, void *pointer) {
    Chunk* chunk = self->first;

    while (chunk != NULL) {
        if (Chunk_contains(chunk, pointer)) {
            return chunk;
        }
        chunk = chunk->next;
    }

    DEBUG("GC: failed to find large chunk for ptr=%p\n", pointer);

    return NULL;
}

static inline char *ChunkList_limit(ChunkList *self) {
    if (self->last == NULL) {
        return NULL;
    }
    return (char *)self->last + Chunk_size(self->last);
}

static inline void ChunkList_merge(ChunkList *self, Chunk *chunk, Chunk *limit, size_t count) {
    size_t size;
    char *stop;

    if (limit == NULL) {
        stop = ChunkList_limit(self);
    } else {
        stop = (char *)limit;
    }
    size = (size_t)(stop - (char *)chunk) - CHUNK_HEADER_SIZE;

    DEBUG("GC: merge chunk=%p size=%zu next=%p new_size=%zu\n",
            (void *)chunk, chunk->object.size,
            (void *)limit, size);
    assert(size > chunk->object.size);

    chunk->next = limit;
    chunk->object.size = size;

    if (limit == NULL) {
        self->last = chunk;
    }
    self->size = self->size - count;
}

// Iterates the list and deallocates any chunk whose chunk hasn't been marked.
static inline void ChunkList_sweep(ChunkList *self) {
    Chunk *chunk = self->first;

    while (chunk != NULL) {
        if (Chunk_isMarked(chunk)) {
            // chunk is marked: keep allocation
            DEBUG("GC: keep chunk=%p ptr=%p size=%zu\n",
                    (void *)chunk, Chunk_mutatorAddress(chunk), Object_size(&chunk->object));
            chunk = chunk->next;
        } else {
            DEBUG("GC: free chunk=%p ptr=%p size=%zu\n",
                    (void *)chunk, Chunk_mutatorAddress(chunk), Object_size(&chunk->object));

            // 'free' chunk
            chunk->allocated = 0;

            // iterate the following chunks until we find a marked chunk
            Chunk *limit = chunk->next;
            size_t count = 0;

            while ((limit != NULL) && !Chunk_isMarked(limit)) {
                limit = limit->next;
                count++;
            }

            // merge unmarked chunks
            if (limit != chunk->next) {
                ChunkList_merge(self, chunk, limit, count);
            }

            chunk = limit;
        }
    }
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
            fprintf(stderr, "ASSERTION FAILED: chunk->next %p == chunk+header+size %p\n", (void *)expected, (void *)actual);
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
