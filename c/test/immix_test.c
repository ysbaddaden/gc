#include "greatest.h"
#include "../immix.h"

TEST test_GC_malloc() {
    void *pointer = GC_malloc(128);

    // allocated memory
    ASSERT(pointer != NULL);

    // initialized chunk
    Chunk *chunk = (Chunk *)((char *)pointer - sizeof(Chunk));
    ASSERT(chunk->next != NULL);
    ASSERT_EQ_FMT(1, chunk->allocated, "%d");

    // initialized object
    Object *object = (Object *)((char *)pointer - sizeof(Object));
    ASSERT_EQ_FMT(sizeof(Object) + 128, object->size, "%zu");
    ASSERT_EQ_FMT(0, object->marked, "%d");
    ASSERT_EQ_FMT(0, object->atomic, "%d");

    PASS();
}

TEST test_GC_malloc_atomic() {
    void *pointer = GC_malloc_atomic(256);

    // allocated memory
    ASSERT(pointer != NULL);

    // initialized chunk
    Chunk *chunk = (Chunk *)((char *)pointer - sizeof(Chunk));
    ASSERT(chunk->next != NULL);
    ASSERT_EQ_FMT(1, chunk->allocated, "%d");

    // initialized object
    Object *object = (Object *)((char *)pointer - sizeof(Object));
    ASSERT_EQ_FMT(sizeof(Object) + 256, object->size, "%zu");
    ASSERT_EQ_FMT(0, object->marked, "%d");
    ASSERT_EQ_FMT(1, object->atomic, "%d");

    PASS();
}

TEST test_GC_realloc() {
    // realloc NULL pointer: allocate memory
    void *ptr1 = GC_realloc(NULL, 64);
    ASSERT(ptr1 != NULL);

    Object *obj = (Object *)((char *)ptr1 - sizeof(Object));
    ASSERT_EQ_FMT(sizeof(Object) + 64, obj->size, "%zu");
    ASSERT_EQ(0, obj->atomic);

    // no resize: keep allocation
    void *ptr3 = GC_realloc(ptr1, 64);
    ASSERT_EQ(ptr1, ptr3);
    ASSERT_MEM_EQ(ptr1, ptr3, 64);

    // shrink: keep allocation
    void *ptr4 = GC_realloc(ptr3, 48);
    ASSERT_EQ(ptr3, ptr4);
    ASSERT_MEM_EQ(ptr3, ptr4, 48);

    // initialize memory
    memset(ptr4, 0xff, 48);

    // grow: reallocate
    void *ptr5 = GC_realloc(ptr4, 128);
    ASSERT(ptr5 != ptr4);
    ASSERT_MEM_EQ(ptr4, ptr5, 48);

    // resize to zero: free allocation
    void *ptr6 = GC_realloc(ptr5, 0);
    ASSERT(ptr6 == NULL);

    obj = (Object *)((char *)ptr5 - sizeof(Object));
    ASSERT_EQ_FMT(sizeof(Object) + 128, obj->size, "%zu");

    PASS();
}

TEST test_GC_collect() {
    SKIP();
}

TEST test_GC_free() {
    void *pointer = GC_malloc_atomic(8192);
    ASSERT(pointer != NULL);

    Chunk *chunk = (Chunk *)((char *)pointer - sizeof(Chunk));
    Object *object = (Object *)((char *)pointer - sizeof(Object));

    GC_free(pointer);

    // deallocated chunk
    ASSERT_EQ_FMT(0, chunk->allocated, "%d");

    // didn't touch object
    ASSERT_EQ_FMT(sizeof(Object) + 8192, object->size, "%zu");
    ASSERT_EQ_FMT(0, object->marked, "%d");
    ASSERT_EQ_FMT(1, object->atomic, "%d");

    PASS();
}

TEST test_grows_memory() {
    void *pointers[3];
    int i = 0;

    // fills & requires memory to grow
    pointers[0] = GC_malloc(BLOCK_SIZE);
    pointers[1] = GC_malloc(BLOCK_SIZE);
    pointers[2] = GC_malloc(BLOCK_SIZE);

    for (; i < 3; i++) {
        ASSERT(pointers[i] != NULL);
    }
    PASS();
}

SUITE(ImmixSuite) {
    RUN_TEST(test_GC_malloc);
    RUN_TEST(test_GC_malloc_atomic);
    RUN_TEST(test_GC_realloc);
    RUN_TEST(test_GC_collect);
    RUN_TEST(test_GC_free);
    RUN_TEST(test_grows_memory);
}
