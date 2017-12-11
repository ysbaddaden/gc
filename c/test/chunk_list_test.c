#include <stddef.h>
#include <stdlib.h>
#include "greatest.h"
#include "../chunk_list.h"

TEST test_Chunk_init() {
    Chunk chunk;
    memset(&chunk, 0xff, sizeof(Chunk));
    Chunk_init(&chunk, 123);

    ASSERT_EQ(NULL, chunk.next);
    ASSERT_EQ(0, chunk.allocated);
    ASSERT_EQ(123, chunk.object.size);

    PASS();
}

TEST test_Chunk_mutatorAddress() {
    Chunk chunk;
    void *pointer = Chunk_mutatorAddress(&chunk);

    ASSERT_EQ((char*)&chunk + sizeof(Chunk), pointer);

    PASS();
}

SUITE(ChunkSuite) {
    RUN_TEST(test_Chunk_init);
    RUN_TEST(test_Chunk_mutatorAddress);
}


TEST test_ChunkList_clear() {
    ChunkList list;
    memset(&list, 0xff, sizeof(ChunkList));
    ChunkList_clear(&list);

    ASSERT_EQ(NULL, list.first);
    ASSERT_EQ(NULL, list.last);
    ASSERT_EQ(0, list.size);

    PASS();
}

TEST test_ChunkList_push() {
    ChunkList list;
    ChunkList_clear(&list);
    ASSERT(ChunkList_isEmpty(&list));
    ASSERT_EQ(0, list.size);

    Chunk chunk1;
    chunk1.next = (void *)0x1234;
    Chunk_init(&chunk1, 8);

    Chunk chunk2;
    chunk2.next = (void *)0x1234;
    Chunk_init(&chunk2, 8);

    Chunk chunk3;
    chunk2.next = (void *)0x1234;
    Chunk_init(&chunk2, 8);

    // push first element
    ChunkList_push(&list, &chunk1);
    ASSERT_EQ(NULL, chunk1.next);
    ASSERT_EQ(&chunk1, list.first);
    ASSERT_EQ(&chunk1, list.last);
    ASSERT_FALSE(ChunkList_isEmpty(&list));
    ASSERT_EQ(1, list.size);

    // push second + third element
    ChunkList_push(&list, &chunk2);
    ChunkList_push(&list, &chunk3);

    ASSERT_EQ(&chunk2, chunk1.next);
    ASSERT_EQ(&chunk3, chunk2.next);
    ASSERT_EQ(NULL, chunk3.next);
    ASSERT_EQ(&chunk1, list.first);
    ASSERT_EQ(&chunk3, list.last);
    ASSERT_EQ(3, list.size);

    PASS();
}

TEST test_ChunkList_shift() {
    ChunkList list;
    ChunkList_clear(&list);

    Chunk chunk1; Chunk_init(&chunk1, 8); ChunkList_push(&list, &chunk1);
    Chunk chunk2; Chunk_init(&chunk2, 8); ChunkList_push(&list, &chunk2);
    Chunk chunk3; Chunk_init(&chunk3, 8); ChunkList_push(&list, &chunk3);

    ASSERT_EQ(&chunk1, ChunkList_shift(&list));
    ASSERT_EQ(2, list.size);

    ASSERT_EQ(&chunk2, ChunkList_shift(&list));
    ASSERT_EQ(1, list.size);

    ASSERT_EQ(&chunk3, ChunkList_shift(&list));
    ASSERT_EQ(0, list.size);
    ASSERT(ChunkList_isEmpty(&list));

    ASSERT_EQ(NULL, ChunkList_shift(&list));

    PASS();
}

TEST test_ChunkList_insert() {
    ChunkList list;
    ChunkList_clear(&list);

    Chunk chunk1; Chunk_init(&chunk1, 8); ChunkList_push(&list, &chunk1);
    Chunk chunk2; Chunk_init(&chunk2, 8); ChunkList_push(&list, &chunk2);
    Chunk chunk4; Chunk_init(&chunk4, 8); ChunkList_push(&list, &chunk4);

    Chunk chunk3; Chunk_init(&chunk3, 8);
    ChunkList_insert(&list, &chunk3, &chunk2);

    ASSERT_EQ(&chunk3, chunk2.next);
    ASSERT_EQ(&chunk4, chunk3.next);
    ASSERT_EQ(4, list.size);

    // we can iterate the list:
    Chunk *chunk = list.first;
    int count = 0;
    while (chunk != NULL) {
        count++;
        switch (count) {
        case 1: ASSERT_EQ(&chunk1, chunk); break;
        case 2: ASSERT_EQ(&chunk2, chunk); break;
        case 3: ASSERT_EQ(&chunk3, chunk); break;
        case 4: ASSERT_EQ(&chunk4, chunk); break;
        }
        chunk = chunk->next;
    }
    ASSERT_EQ(4, count);

    PASS();
}

SUITE(ChunkListSuite) {
    RUN_TEST(test_ChunkList_clear);
    RUN_TEST(test_ChunkList_push);
    RUN_TEST(test_ChunkList_shift);
    RUN_TEST(test_ChunkList_insert);
}
