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

TEST test_Array_delete_first_item() {
    Array ary;
    Array_init(&ary, 8);

    int v[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (long i = 0; i < 8; i++) {
        Array_push(&ary, &v[i]);
    }
    ASSERT_EQ(8, Array_size(&ary));

    Array_delete(&ary, &v[0]);
    ASSERT_EQ(7, Array_size(&ary));
    ASSERT_EQ(ary.buffer + 7, ary.cursor);

    ASSERT_EQ(&v[1], Array_get(&ary, 0));
    ASSERT_EQ(&v[2], Array_get(&ary, 1));
    ASSERT_EQ(&v[3], Array_get(&ary, 2));
    ASSERT_EQ(&v[4], Array_get(&ary, 3));
    ASSERT_EQ(&v[5], Array_get(&ary, 4));
    ASSERT_EQ(&v[6], Array_get(&ary, 5));
    ASSERT_EQ(&v[7], Array_get(&ary, 6));
    ASSERT_EQ(NULL, Array_get(&ary, 7));

    PASS();
}

TEST test_Array_delete_inner_item() {
    Array ary;
    Array_init(&ary, 8);

    int v[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (long i = 0; i < 8; i++) {
        Array_push(&ary, &v[i]);
    }
    ASSERT_EQ(8, Array_size(&ary));

    Array_delete(&ary, &v[4]);
    ASSERT_EQ(7, Array_size(&ary));
    ASSERT_EQ(ary.buffer + 7, ary.cursor);

    ASSERT_EQ(&v[0], Array_get(&ary, 0));
    ASSERT_EQ(&v[1], Array_get(&ary, 1));
    ASSERT_EQ(&v[2], Array_get(&ary, 2));
    ASSERT_EQ(&v[3], Array_get(&ary, 3));
    ASSERT_EQ(&v[5], Array_get(&ary, 4));
    ASSERT_EQ(&v[6], Array_get(&ary, 5));
    ASSERT_EQ(&v[7], Array_get(&ary, 6));
    ASSERT_EQ(NULL, Array_get(&ary, 7));

    PASS();
}

TEST test_Array_delete_last_item() {
    Array ary;
    Array_init(&ary, 8);

    int v[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (long i = 0; i < 8; i++) {
        Array_push(&ary, &v[i]);
    }
    ASSERT_EQ(8, Array_size(&ary));

    Array_delete(&ary, &v[7]);
    ASSERT_EQ(7, Array_size(&ary));
    ASSERT_EQ(ary.buffer + 7, ary.cursor);

    ASSERT_EQ(&v[0], Array_get(&ary, 0));
    ASSERT_EQ(&v[1], Array_get(&ary, 1));
    ASSERT_EQ(&v[2], Array_get(&ary, 2));
    ASSERT_EQ(&v[3], Array_get(&ary, 3));
    ASSERT_EQ(&v[4], Array_get(&ary, 4));
    ASSERT_EQ(&v[5], Array_get(&ary, 5));
    ASSERT_EQ(&v[6], Array_get(&ary, 6));
    ASSERT_EQ(NULL, Array_get(&ary, 7));

    PASS();
}

TEST test_Array_delete() {
    Array ary;
    Array_init(&ary, 8);

    int v[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (long i = 0; i < 8; i++) {
        Array_push(&ary, &v[i]);
    }
    Array_delete(&ary, &v[7]);
    Array_delete(&ary, &v[0]);
    Array_delete(&ary, &v[1]);
    Array_push(&ary, &v[0]);

    ASSERT_EQ(6, Array_size(&ary));

    ASSERT_EQ(&v[2], Array_get(&ary, 0));
    ASSERT_EQ(&v[3], Array_get(&ary, 1));
    ASSERT_EQ(&v[4], Array_get(&ary, 2));
    ASSERT_EQ(&v[5], Array_get(&ary, 3));
    ASSERT_EQ(&v[6], Array_get(&ary, 4));
    ASSERT_EQ(&v[0], Array_get(&ary, 5));
    ASSERT_EQ(NULL, Array_get(&ary, 6));
    ASSERT_EQ(NULL, Array_get(&ary, 7));

    PASS();
}

TEST test_Array_clear() {
    Array ary;
    Array_init(&ary, 4);

    int v[4] = {1, 2, 3, 4};

    for (long i = 0; i < 4; i++) {
        Array_push(&ary, &v[i]);
    }
    ASSERT_EQ(4, Array_size(&ary));

    Array_clear(&ary);
    ASSERT_EQ(0, Array_size(&ary));
    ASSERT_EQ(ary.buffer, ary.cursor);

    PASS();
}

SUITE(ArraySuite) {
    RUN_TEST(test_Array_init);
    RUN_TEST(test_Array_lifetime);
    RUN_TEST(test_Array_each);
    RUN_TEST(test_Array_delete_first_item);
    RUN_TEST(test_Array_delete_inner_item);
    RUN_TEST(test_Array_delete_last_item);
    RUN_TEST(test_Array_delete);
    RUN_TEST(test_Array_clear);
}
