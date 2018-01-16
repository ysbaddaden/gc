#include "greatest.h"
#include "line_header.h"

TEST test_LineHeader_isMarked() {
    char flag = 0;
    ASSERT_FALSE(LineHeader_isMarked(&flag));

    LineHeader_mark(&flag);
    ASSERT(LineHeader_isMarked(&flag));

    LineHeader_unmark(&flag);
    ASSERT_FALSE(LineHeader_isMarked(&flag));

    PASS();
}

TEST test_LineHeader_containsObject() {
    char flag = 0;
    ASSERT_FALSE(LineHeader_containsObject(&flag));

    LineHeader_setOffset(&flag, 64);
    ASSERT(LineHeader_containsObject(&flag));
    ASSERT_EQ(64, LineHeader_getOffset(&flag));

    LineHeader_setOffset(&flag, 0);
    ASSERT(LineHeader_containsObject(&flag));
    ASSERT_EQ(0, LineHeader_getOffset(&flag));

    LineHeader_setOffset(&flag, LINE_SIZE - WORD_SIZE);
    ASSERT(LineHeader_containsObject(&flag));
    ASSERT_EQ(LINE_SIZE - WORD_SIZE, LineHeader_getOffset(&flag));

    PASS();
}

TEST test_LineHeader_mark() {
    char flag = 0;
    LineHeader_setOffset(&flag, 248);
    LineHeader_mark(&flag);

    ASSERT(LineHeader_containsObject(&flag));
    ASSERT_EQ(248, LineHeader_getOffset(&flag));
    ASSERT(LineHeader_isMarked(&flag));

    LineHeader_unmark(&flag);

    ASSERT(LineHeader_containsObject(&flag));
    ASSERT_EQ(248, LineHeader_getOffset(&flag));
    ASSERT_FALSE(LineHeader_isMarked(&flag));

    PASS();
}

TEST test_LineHeader_clear() {
    char flag[] = {0, 0};

    LineHeader_setOffset(&flag[0], 32);
    LineHeader_setOffset(&flag[1], 64);

    ASSERT(LineHeader_containsObject(&flag[0]));
    ASSERT_EQ(32, LineHeader_getOffset(&flag[0]));

    ASSERT(LineHeader_containsObject(&flag[1]));
    ASSERT_EQ(64, LineHeader_getOffset(&flag[1]));

    LineHeader_clear(&flag[0]);

    ASSERT_FALSE(LineHeader_containsObject(&flag[0]));
    ASSERT(LineHeader_containsObject(&flag[1]));
    ASSERT_EQ(64, LineHeader_getOffset(&flag[1]));

    PASS();
}

SUITE(LineHeaderSuite) {
    RUN_TEST(test_LineHeader_isMarked);
    RUN_TEST(test_LineHeader_containsObject);
    RUN_TEST(test_LineHeader_clear);
    RUN_TEST(test_LineHeader_mark);
}
