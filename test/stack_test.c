#include "greatest.h"
#include "stack.h"

TEST test_Stack_init() {
    Stack stack;
    Stack_init(&stack, 123);

    ASSERT_EQ(0, Stack_size(&stack));
    ASSERT(Stack_isEmpty(&stack));

    PASS();
}

TEST test_Stack_push_pop() {
    Stack stack;
    Stack_init(&stack, 2);

    int a = 0;
    int b = 1;
    int c = 2;
    int d = 3;

    Stack_push(&stack, &a, &b);
    ASSERT_EQ(1, Stack_size(&stack));
    ASSERT_EQ_FMT((void *)&a, stack.buffer[0], "%p");
    ASSERT_EQ_FMT((void *)&b, stack.buffer[1], "%p");

    Stack_push(&stack, &c, &d);
    ASSERT_EQ(2, Stack_size(&stack));
    ASSERT_EQ_FMT((void *)&c, stack.buffer[2], "%p");
    ASSERT_EQ_FMT((void *)&d, stack.buffer[3], "%p");

    void *sp;
    void *bottom;

    ASSERT(Stack_pop(&stack, &sp, &bottom));
    ASSERT_EQ(1, Stack_size(&stack));
    ASSERT_EQ_FMT((void *)&c, sp, "%p");
    ASSERT_EQ_FMT((void *)&d, bottom, "%p");

    ASSERT(Stack_pop(&stack, &sp, &bottom));
    ASSERT_EQ(0, Stack_size(&stack));
    ASSERT_EQ_FMT((void *)&a, sp, "%p");
    ASSERT_EQ_FMT((void *)&b, bottom, "%p");

    ASSERT_FALSE(Stack_pop(&stack, &sp, &bottom));
    ASSERT_EQ(0, Stack_size(&stack));
    ASSERT_EQ_FMT(NULL, sp, "%p");
    ASSERT_EQ_FMT(NULL, bottom, "%p");

    PASS();
}

TEST test_Stack_grows_limitless() {
    Stack stack;
    Stack_init(&stack, GC_getMemoryLimit());

    int a = 0;
    int b = 1;

    for (int i = 1; i < 1000000; ++i) {
        Stack_push(&stack, &a, &b);
        ASSERT_EQ(i, Stack_size(&stack));
    }
    ASSERT_EQ(999999, Stack_size(&stack));

    void *sp;
    void *bottom;
    for (int i = 1; i < 1000000; ++i) {
        ASSERT(Stack_pop(&stack, &sp, &bottom));
    }

    ASSERT_EQ(0, Stack_size(&stack));
    ASSERT_FALSE(Stack_pop(&stack, &sp, &bottom));

    PASS();
}

SUITE(StackSuite) {
    RUN_TEST(test_Stack_init);
    RUN_TEST(test_Stack_push_pop);
    RUN_TEST(test_Stack_grows_limitless);
}
