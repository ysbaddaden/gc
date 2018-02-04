#ifndef GC_HASH_H
#define GC_HASH_H

typedef struct {
    void *entries;
    size_t mask;
    size_t used;
    size_t deleted;
} Hash;

typedef void (*hash_iterator_t)(void *, void *);

Hash *GC_Hash_create(size_t capacity);
void *GC_Hash_search(Hash *self, void *key);
void GC_Hash_each(Hash *self, hash_iterator_t);
void GC_Hash_insert(Hash *self, void *key, void *value);
void *GC_Hash_delete(Hash *self, void *key);
void GC_Hash_free(Hash *self);

#define Hash_create GC_Hash_create
#define Hash_search GC_Hash_search
#define Hash_each GC_Hash_each
#define Hash_insert GC_Hash_insert
#define Hash_delete GC_Hash_delete
#define Hash_free GC_Hash_free

#endif
