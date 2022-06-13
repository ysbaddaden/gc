#include "greatest.h"
#include "block_list.h"
#include "memory.h"

TEST test_BlockList_clear() {
    BlockList list;
    BlockList_clear(&list);

    ASSERT_EQ(NULL, list.first);
    ASSERT_EQ(NULL, list.last);
    ASSERT_EQ(0, list.size);

    PASS();
}

TEST test_BlockList_push() {
    void *addr;
    void *heap = GC_mapAndAlign(&addr, BLOCK_SIZE * 4, BLOCK_SIZE * 4);

    BlockList list;
    BlockList_clear(&list);
    ASSERT(BlockList_isEmpty(&list));
    ASSERT_EQ(0, list.size);

    Block *block1 = (Block *)heap;
    block1->next = (void *)0x1234;
    Block_init(block1);

    Block *block2 = (Block *)((char *)heap + BLOCK_SIZE);
    block2->next = (void *)0x1234;
    Block_init(block2);

    Block *block3 = (Block *)((char *)heap + BLOCK_SIZE * 2);
    block3->next = (void *)0x1234;
    Block_init(block3);

    // push first element
    BlockList_push(&list, block1);
    ASSERT_EQ(NULL, block1->next);
    ASSERT_EQ(block1, list.first);
    ASSERT_EQ(block1, list.last);
    ASSERT_FALSE(BlockList_isEmpty(&list));
    ASSERT_EQ(1, list.size);

    // push second + third element
    BlockList_push(&list, block2);
    BlockList_push(&list, block3);

    ASSERT_EQ(block2, block1->next);
    ASSERT_EQ(block3, block2->next);
    ASSERT_EQ(NULL, block3->next);
    ASSERT_EQ(block1, list.first);
    ASSERT_EQ(block3, list.last);
    ASSERT_EQ(3, list.size);

    PASS();
}

TEST test_BlockList_shift() {
    void *addr;
    void *heap = GC_mapAndAlign(&addr, BLOCK_SIZE * 4, BLOCK_SIZE * 4);

    BlockList list;
    BlockList_clear(&list);

    Block *block1 = (Block *)heap;
    Block_init(block1);
    BlockList_push(&list, block1);

    Block *block2 = (Block *)((char *)heap + BLOCK_SIZE);
    Block_init(block2);
    BlockList_push(&list, block2);

    Block *block3 = (Block *)((char *)heap + BLOCK_SIZE * 2);
    Block_init(block3);
    BlockList_push(&list, block3);

    ASSERT_EQ(block1, BlockList_shift(&list));
    ASSERT_EQ(2, list.size);

    ASSERT_EQ(block2, BlockList_shift(&list));
    ASSERT_EQ(1, list.size);

    BlockList_push(&list, block1);
    ASSERT_EQ(2, list.size);

    ASSERT_EQ(block3, BlockList_shift(&list));
    ASSERT_EQ(1, list.size);

    ASSERT_EQ(block1, BlockList_shift(&list));
    ASSERT_EQ(0, list.size);

    ASSERT(BlockList_isEmpty(&list));
    ASSERT_EQ(NULL, BlockList_shift(&list));

    BlockList_push(&list, block2);
    ASSERT_EQ(1, list.size);
    ASSERT_EQ(block2, BlockList_shift(&list));

    PASS();
}

SUITE(BlockListSuite) {
    RUN_TEST(test_BlockList_clear);
    RUN_TEST(test_BlockList_push);
    RUN_TEST(test_BlockList_shift);
}
