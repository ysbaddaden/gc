#include "greatest.h"
#include "array.h"

TEST test_Array_init() {
    Array ary;
    Array_init(&ary, 64);

    ASSERT_EQ(0, Array_size(&ary));
    ASSERT(Array_isEmpty(&ary));

    ASSERT_EQ(ary.buffer, ary.cursor);
    ASSERT_EQ(ary.buffer + 64, ary.limit);

    PASS();
}

TEST test_Array_lifetime() {
    Array ary;
    Array_init(&ary, 2);

    int v[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    for (long i = 0; i < 10; i++) {
        Array_push(&ary, &v[i]);
        ASSERT_EQ(i + 1, Array_size(&ary));
    }

    ASSERT_EQ(ary.buffer + 10, ary.cursor);
    ASSERT_EQ(ary.buffer + 16, ary.limit);

    for (long i = 9; i >= 0; i--) {
        void *w = Array_pop(&ary);
        ASSERT_EQ_FMT((void *)&v[i], w, "%p");
    }
    ASSERT_EQ(NULL, Array_pop(&ary));

    ASSERT_EQ(ary.buffer, ary.cursor);
    ASSERT_EQ(ary.buffer + 16, ary.limit);

    PASS();
}

TEST test_Array_each() {
  SKIP();
}

TEST test_Array_delete() {
  SKIP();
}

TEST test_Array_clear() {
  SKIP();
}

SUITE(ArraySuite) {
    RUN_TEST(test_Array_init);
    RUN_TEST(test_Array_lifetime);
    RUN_TEST(test_Array_each);
    RUN_TEST(test_Array_delete);
    RUN_TEST(test_Array_clear);
}
