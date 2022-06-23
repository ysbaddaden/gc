#ifndef GC_ARRAY_H
#define GC_ARRAY_H

#include <stdlib.h>

typedef struct {
    long capacity;
    void **buffer;
    void **cursor;
    void **limit;
} Array;

typedef void (*Array_iterator_t)(void *);

static inline void Array_init(Array *, long) __attribute__ ((__unused__));
static inline int Array_isEmpty(Array *) __attribute__ ((__unused__));
static inline long Array_size(Array *) __attribute__ ((__unused__));
static inline void Array_each(Array *, Array_iterator_t) __attribute__ ((__unused__));
static inline void Array_push(Array *, void *) __attribute__ ((__unused__));
static inline void *Array_pop(Array *) __attribute__ ((__unused__));
static inline void Array_delete(Array *, void *) __attribute__ ((__unused__));
static inline void Array_clear(Array *) __attribute__ ((__unused__));

static inline void Array_resize(Array *self, long new_capacity) {
    long current_index = Array_size(self);
    self->capacity = new_capacity;

    self->buffer = realloc(self->buffer, new_capacity * sizeof(void *));
    if (self->buffer == NULL) {
        perror("GC: realloc");
        abort();
    }
    self->cursor = self->buffer + current_index;
    self->limit = self->buffer + new_capacity;
}

static inline void Array_init(Array *self, long capacity) {
    self->buffer = NULL;
    Array_resize(self, capacity);
    self->cursor = self->buffer;
}

static inline int Array_isEmpty(Array *self) {
    return self->cursor == self->buffer;
}

static inline long Array_size(Array *self) {
    return (self->cursor - self->buffer);
}

static inline void *Array_get(Array *self, long index) {
    if (index < 0 || index >= Array_size(self)) {
      return NULL;
    }
    return *(self->buffer + index);
}

static inline void Array_each(Array *self, Array_iterator_t callback) {
    void **cursor = self->buffer;

    while (cursor < self->cursor) {
        callback(*cursor);
        cursor += 1;
    }
}

static inline void Array_push(Array *self, void *item) {
    if (self->cursor == self->limit) {
        Array_resize(self, self->capacity * 2);
    }
    *self->cursor = item;
    self->cursor += 1;
}

static inline void *Array_pop(Array *self) {
    if (Array_isEmpty(self)) {
        return NULL;
    }
    self->cursor -= 1;
    void *item = *self->cursor;
    return item;
}

static inline void Array_delete(Array *self, void *item) {
    void **cursor = self->buffer;

    while (cursor < self->cursor) {
        if (*cursor == item) {
          while (cursor < self->limit) {
              *cursor = *(cursor + 1);
              cursor += 1;
          }
          self->cursor -= 1;
          return;
        }
        cursor += 1;
    }
}

static inline void Array_clear(Array *self) {
    if (Array_isEmpty(self)) {
        return;
    }
    self->cursor = self->buffer;
}

#endif
