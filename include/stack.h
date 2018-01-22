#ifndef GC_STACK_H
#define GC_STACK_H

#include "memory.h"

typedef struct {
    void **buffer;
    void **cursor;
} Stack;

static void Stack_init(Stack *, size_t) __attribute__((__unused__));
static void Stack_dispose(Stack *) __attribute__((__unused__));
static void Stack_push(Stack *, void *, void *) __attribute__((__unused__));
static int Stack_pop(Stack *, void **, void **) __attribute__((__unused__));
static size_t Stack_size(Stack *) __attribute__((__unused__));

static void Stack_init(Stack *self, size_t capacity) {
    self->buffer = GC_map(sizeof(void *) * capacity * 2);
    self->cursor = self->buffer;
}

static void Stack_dispose(Stack *self) {
    munmap(self->buffer, self->cursor - self->buffer);
}

static size_t Stack_size(Stack *self) {
    return (self->cursor - self->buffer) / 2;
}

static int Stack_isEmpty(Stack *self) {
    return self->cursor == self->buffer;
}

static void Stack_push(Stack *self, void *sp, void *bottom) {
    self->cursor[0] = sp;
    self->cursor[1] = bottom;
    self->cursor += 2;
}

static int Stack_pop(Stack *self, void **sp, void **bottom) {
    if (Stack_isEmpty(self)) {
        *sp = NULL;
        *bottom = NULL;
        return 0;
    }
    self->cursor -= 2;
    *sp = self->cursor[0];
    *bottom = self->cursor[1];
    return 1;
}

#endif
