#ifndef GC_CHUNK_LIST_H
#define GC_CHUNK_LIST_H

#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include "object.h"

typedef struct Chunk {
    struct Chunk *next;
    uint8_t allocated;
    Object object;
} Chunk;

#define CHUNK_HEADER_SIZE (sizeof(Chunk) - sizeof(Object))

inline void Chunk_init(Chunk *chunk, size_t size) {
    chunk->next = NULL;
    chunk->allocated = 0;
    chunk->object.size = size;
}

inline void* Chunk_mutatorAddress(Chunk *chunk) {
    return (char*)chunk + sizeof(Chunk);
}

typedef struct {
    Chunk *first;
    Chunk *last;
    size_t size;
} ChunkList;

inline void ChunkList_clear(ChunkList *chunk_list) {
    chunk_list->first = NULL;
    chunk_list->last = NULL;
    chunk_list->size = 0;
}

inline int ChunkList_isEmpty(ChunkList *chunk_list) {
    return chunk_list->first == NULL;
}

inline void ChunkList_push(ChunkList *chunk_list, Chunk *chunk) {
    if (chunk_list->first == NULL) {
        chunk_list->first = chunk;
    } else {
        chunk_list->last->next = chunk;
    }
    chunk->next = NULL;

    chunk_list->last = chunk;
    chunk_list->size++;
}

inline Chunk *ChunkList_shift(ChunkList *chunk_list) {
    Chunk *chunk = chunk_list->first;

    if (chunk != NULL) {
        chunk_list->first = chunk->next;
        chunk_list->size--;
    }

    return chunk;
}

inline void ChunkList_insert(ChunkList *chunk_list, Chunk *chunk, Chunk* after) {
    chunk->next = after->next;
    after->next = chunk;

    if (after == chunk_list->last) {
        chunk_list->last = chunk;
    }
    chunk_list->size++;
}

#endif
