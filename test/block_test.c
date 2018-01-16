#include "greatest.h"
#include "block.h"

TEST test_Block_init() {
    Block *block = malloc(BLOCK_SIZE);
    memset(block, 0xff, BLOCK_SIZE);

    Block_init(block);

    ASSERT_EQ(0, block->marked);
    ASSERT_EQ(BLOCK_FLAG_FREE, block->flag);
    ASSERT_EQ(0, block->first_free_line_index);
    ASSERT_EQ(NULL, block->next);

    for (int i = 0; i < LINE_COUNT; i++) {
        ASSERT_EQ_FMT(0, *Block_lineHeader(block, i), "%d");
    }

    ASSERT_EQ((char *)block + LINE_SIZE, Block_start(block));
    ASSERT_EQ((char *)block + BLOCK_SIZE, Block_stop(block));

    PASS();
}

TEST test_Block_flags() {
    Block *block = malloc(BLOCK_SIZE);

    Block_setFlag(block, BLOCK_FLAG_FREE);
    ASSERT(Block_isFree(block));
    ASSERT_FALSE(Block_isRecyclable(block));
    ASSERT_FALSE(Block_isUnavailable(block));

    Block_setFlag(block, BLOCK_FLAG_RECYCLABLE);
    ASSERT_FALSE(Block_isFree(block));
    ASSERT(Block_isRecyclable(block));
    ASSERT_FALSE(Block_isUnavailable(block));

    Block_setFlag(block, BLOCK_FLAG_UNAVAILABLE);
    ASSERT_FALSE(Block_isFree(block));
    ASSERT_FALSE(Block_isRecyclable(block));
    ASSERT(Block_isUnavailable(block));

    PASS();
}

TEST test_Block_mark() {
    Block *block = malloc(BLOCK_SIZE);

    Block_mark(block);
    ASSERT(Block_isMarked(block));

    Block_unmark(block);
    ASSERT_FALSE(Block_isMarked(block));

    PASS();
}

TEST test_Block_firstFreeLine() {
    Block *block = malloc(BLOCK_SIZE);

    Block_init(block);
    ASSERT_EQ((char *)block + LINE_SIZE, Block_firstFreeLine(block));

    block->first_free_line_index = 37;
    ASSERT_EQ((char *)block + LINE_SIZE * 38, Block_firstFreeLine(block));

    PASS();
}

TEST test_Block_lineHeaders() {
    Block *block = malloc(BLOCK_SIZE);
    ASSERT_EQ((char *)block + sizeof(Object), Block_lineHeaders(block));
    PASS();
}

TEST test_Block_lineIndex() {
    Block *block = malloc(BLOCK_SIZE);

    ASSERT_EQ(-1, Block_lineIndex(block, (char *)block));
    ASSERT_EQ(-1, Block_lineIndex(block, Block_start(block) - WORD_SIZE));
    ASSERT_EQ(0, Block_lineIndex(block, Block_start(block) + 53));
    ASSERT_EQ(125, Block_lineIndex(block, Block_start(block) + LINE_SIZE * 125 + 100));
    ASSERT_EQ(126, Block_lineIndex(block, Block_start(block) + LINE_SIZE * 127 - WORD_SIZE));
    PASS();
}

TEST test_Block_line() {
    Block *block = malloc(BLOCK_SIZE);
    ASSERT_EQ(Block_start(block), Block_line(block, 0));
    ASSERT_EQ(Block_start(block) + LINE_SIZE * 125, Block_line(block, 125));
    PASS();
}

TEST test_Block_contains() {
    Block *block = malloc(BLOCK_SIZE);
    Block_init(block);

    ASSERT_FALSE(Block_contains(block, Block_start(block) - 1));
    ASSERT(Block_contains(block, Block_start(block)));
    ASSERT(Block_contains(block, Block_stop(block) - 1));
    ASSERT_FALSE(Block_contains(block, Block_stop(block)));

    PASS();
}

TEST test_Line_update() {
    Block *block = malloc(BLOCK_SIZE);
    Block_init(block);

    Line_update(block, (void *)(Block_start(block)));
    ASSERT(LineHeader_containsObject(Block_lineHeader(block, 0)));
    ASSERT_EQ(0, LineHeader_getOffset(Block_lineHeader(block, 0)));

    // don't update twice
    Line_update(block, (void *)(Block_start(block) + 56));
    ASSERT_EQ(0, LineHeader_getOffset(Block_lineHeader(block, 0)));

    Line_update(block, (void *)(Block_start(block) + LINE_SIZE * 4));
    ASSERT(LineHeader_containsObject(Block_lineHeader(block, 4)));
    ASSERT_EQ(0, LineHeader_getOffset(Block_lineHeader(block, 4)));

    Line_update(block, (void *)(Block_start(block) + LINE_SIZE * LINE_COUNT - WORD_SIZE));
    ASSERT(LineHeader_containsObject(Block_lineHeader(block, LINE_COUNT - 1)));
    ASSERT_EQ(LINE_SIZE - WORD_SIZE, LineHeader_getOffset(Block_lineHeader(block, LINE_COUNT - 1)));

    PASS();
}


SUITE(BlockSuite) {
    RUN_TEST(test_Block_init);
    RUN_TEST(test_Block_flags);
    RUN_TEST(test_Block_mark);
    RUN_TEST(test_Block_firstFreeLine);
    RUN_TEST(test_Block_lineHeaders);
    RUN_TEST(test_Block_lineIndex);
    RUN_TEST(test_Block_line);
    RUN_TEST(test_Block_contains);
    RUN_TEST(test_Line_update);
}
