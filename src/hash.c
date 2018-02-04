// Adapted from musl-libc hsearch implementation:
// https://git.musl-libc.org/cgit/musl/tree/src/search/hsearch.c
//
// - open addressing hash table with 2^n table size
// - quadratic probing is used in case of hash collision

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "murmur.h"
#include "hash.h"

#define SEED 0x33da085c
#define HASH(x) MurmurHash2(&x, sizeof(void *), SEED)
typedef uint32_t hash_t;

#define MINSIZE 8
#define MAXSIZE ((size_t)-1/2 + 1)

typedef enum {
    FREE = 0,
    ALLOCATED = 1,
    DELETED = 2
} STATUS;

typedef struct {
    int status;
    void *key;
    void *value;
} ENTRY;

static inline void allocate(ENTRY *self, void *key, void *value) {
    self->status = ALLOCATED;
    self->key = key;
    self->value = value;
}

static ENTRY *lookupForInsert(Hash *self, void *key, hash_t hash) {
    size_t i, j;
    ENTRY *entry;

    for (i = (size_t)hash, j = 1; ; i += j++) {
        entry = (ENTRY *)self->entries + (i & self->mask);
        if (entry->status != ALLOCATED || entry->key == key) {
            break;
        }
    }
    return entry;
}

static void resize(Hash *self, size_t capacity) {
    size_t new_capacity = MINSIZE;
    if (capacity > MAXSIZE) {
        capacity = MAXSIZE;
    }
    while (new_capacity < capacity) {
        new_capacity *= 2;
    }
    //printf("GC: Hash_resize(%p, %zu) capacity=%zu\n", (void *)self, capacity, new_capacity);

    ENTRY *old_entries = self->entries;
    ENTRY *old_limit = (ENTRY *)self->entries + self->mask + 1;

    self->entries = calloc(new_capacity, sizeof(ENTRY));
    if (self->entries == NULL) {
        fprintf(stderr, "GC: calloc failed\n");
        abort();
    }
    self->mask = new_capacity - 1;

    if (old_entries == NULL) {
        return;
    }
    ENTRY *e = old_entries;

    size_t used = 0;
    while (e < old_limit) {
        if (e->status == ALLOCATED) {
            ENTRY *entry = lookupForInsert(self, e->key, HASH(e->key));
            allocate(entry, e->key, e->value);
            used++;
        }
        e++;
    }
    self->used = used;
    self->deleted = 0;

    free(old_entries);
}

Hash *GC_Hash_create(size_t capacity) {
    Hash *self = calloc(1, sizeof(Hash));
    resize(self, capacity);
    return self;
}

void *GC_Hash_search(Hash *self, void *key) {
    hash_t hash = HASH(key);
    ENTRY *entry;

    size_t i, j;
    for (i = (size_t)hash, j = 1; ; i += j++) {
        entry = (ENTRY *)self->entries + (i & self->mask);
        if (entry->status == FREE || entry->key == key) {
            break;
        }
    }

    if (entry->status == ALLOCATED) {
        return entry->value;
    }
    return NULL;
}

void GC_Hash_insert(Hash *self, void *key, void *value) {
    hash_t hash = HASH(key);
    ENTRY *entry = lookupForInsert(self, key, hash);

    switch (entry->status) {
        case ALLOCATED:
            // replace value:
            entry->value = value;
            return;
        case FREE:
            // double capacity when 75% load reached:
            if (self->used >= self->mask - self->mask / 4) {
                resize(self, self->used * 2);
                entry = lookupForInsert(self, key, hash);
            }
            self->used++;
            break;
        case DELETED:
            // recycle tombstone:
            self->deleted--;
            break;
    }

    allocate(entry, key, value);
}

// clear tombstones when they reach 25% load
static inline void clearTombstones(Hash *self) {
    if (self->deleted >= (self->mask + 1) / 4) {
        resize(self, self->used);
    }
}

void *GC_Hash_delete(Hash *self, void *key) {
    size_t i, j;
    ENTRY *entry;
    hash_t hash = HASH(key);

    for (i = (size_t)hash, j = 1; ; i += j++) {
        entry = (ENTRY *)self->entries + (i & self->mask);
        if (entry->status == FREE || entry->key == key) {
            break;
        }
    }

    if (entry->status == ALLOCATED) {
        entry->status = DELETED;
        self->deleted++;

        void *value = entry->value;
        clearTombstones(self);
        return value;
    }

    return NULL;
}

void GC_Hash_deleteIf(Hash *self, hash_iterator_t callback) {
    ENTRY *entry = self->entries;
    ENTRY *limit = (ENTRY *)self->entries + (self->mask + 1);

    while (entry < limit) {
        if (entry->status == ALLOCATED) {
            if (callback(entry->key, entry->value)) {
                entry->status = DELETED;
                self->deleted++;
            }
        }
        entry++;
    }
    clearTombstones(self);
}

void GC_Hash_free(Hash *self) {
    free(self->entries);
    free(self);
    self = NULL;
}
