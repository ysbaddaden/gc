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

TEST test_Chunk_contains() {
    Chunk *chunk = malloc(sizeof(Chunk) + 32);
    Chunk_init(chunk, 32 + sizeof(Object));

    void *pointer = Chunk_mutatorAddress(chunk);

    ASSERT(Chunk_contains(chunk, pointer));
    ASSERT(Chunk_contains(chunk, (char *)pointer + 31));
    ASSERT_FALSE(Chunk_contains(chunk, (char *)pointer - 1));
    ASSERT_FALSE(Chunk_contains(chunk, (char *)pointer + 32));

    free(chunk);
    PASS();
}

SUITE(ChunkSuite) {
    RUN_TEST(test_Chunk_init);
    RUN_TEST(test_Chunk_mutatorAddress);
    RUN_TEST(test_Chunk_contains);
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

//TEST test_ChunkList_shift() {
//    ChunkList list;
//    ChunkList_clear(&list);
//
//    Chunk chunk1; Chunk_init(&chunk1, 8); ChunkList_push(&list, &chunk1);
//    Chunk chunk2; Chunk_init(&chunk2, 8); ChunkList_push(&list, &chunk2);
//    Chunk chunk3; Chunk_init(&chunk3, 8); ChunkList_push(&list, &chunk3);
//
//    ASSERT_EQ(&chunk1, ChunkList_shift(&list));
//    ASSERT_EQ(2, list.size);
//
//    ASSERT_EQ(&chunk2, ChunkList_shift(&list));
//    ASSERT_EQ(1, list.size);
//
//    ASSERT_EQ(&chunk3, ChunkList_shift(&list));
//    ASSERT_EQ(0, list.size);
//    ASSERT(ChunkList_isEmpty(&list));
//
//    ASSERT_EQ(NULL, ChunkList_shift(&list));
//
//    PASS();
//}

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

TEST test_ChunkList_split() {
    // [
    //     [CHUNK, OBJECT, DATA],  1. [size=1024-CHUNK_HEADER_SIZE]
    // ]
    Chunk *chunk1 = malloc(1024);
    Chunk_init(chunk1, 1024 - CHUNK_HEADER_SIZE);

    ChunkList list;
    ChunkList_clear(&list);
    ChunkList_push(&list, chunk1);

    // splits chunk1 into 2 parts:
    // [
    //   [size=64]
    //   [size=960 - CHUNK_HEADER_SIZE * 2]    =928
    // ]
    Chunk *chunk2 = ChunkList_split(&list, chunk1, 64);
    ASSERT_EQ_FMT((char *)chunk1 + CHUNK_HEADER_SIZE + 64, (char *)chunk2, "%p");

    ASSERT_EQ_FMT((size_t)64, chunk1->object.size, "%zu");
    ASSERT_EQ_FMT((size_t)960 - CHUNK_HEADER_SIZE * 2, chunk2->object.size, "%zu");

    // splits chunk2 into 2 parts:
    // [
    //   1. [size=64]
    //   2. [size=512]
    //   3. [size=448 - CHUNK_HEADER_SIZE * 3] =400
    // ]
    Chunk *chunk3 = ChunkList_split(&list, chunk2, 512);
    ASSERT_EQ_FMT((char *)chunk2 + CHUNK_HEADER_SIZE + 512, (char *)chunk3, "%p");

    ASSERT_EQ_FMT((size_t)64, chunk1->object.size, "%zu");
    ASSERT_EQ_FMT((size_t)512, chunk2->object.size, "%zu");
    ASSERT_EQ_FMT((size_t)448 - CHUNK_HEADER_SIZE * 3, chunk3->object.size, "%zu");

    // splits chunk2 into 2 parts again:
    // [
    //   1. [size=64]
    //   2. [size=64]
    //   4. [size=448 - CHUNK_HEADER_SIZE]     =432
    //   3. [size=448 - CHUNK_HEADER_SIZE * 3] =400
    // ]
    Chunk *chunk4 = ChunkList_split(&list, chunk2, 64);
    ASSERT_EQ_FMT((char *)chunk2 + CHUNK_HEADER_SIZE + 64, (char *)chunk4, "%p");

    // can't split chunk4 (remaining space below minimum):
    Chunk *chunk5 = ChunkList_split(&list, chunk4, chunk4->object.size - CHUNK_MIN_SIZE + sizeof(uintptr_t));
    ASSERT(chunk5 == NULL);

    // initialize mutator segments (makes sure sizes were fine, and we won't overwrite chunk metadata).
    memset(Chunk_mutatorAddress(chunk1), 0x7f, chunk1->object.size - sizeof(Object));
    memset(Chunk_mutatorAddress(chunk2), 0x7f, chunk2->object.size - sizeof(Object));
    memset(Chunk_mutatorAddress(chunk4), 0x7f, chunk4->object.size - sizeof(Object));
    memset(Chunk_mutatorAddress(chunk3), 0x7f, chunk3->object.size - sizeof(Object));

    // it sized chunks correctly:
    ASSERT_EQ_FMT((size_t)64, chunk1->object.size, "%zu");
    ASSERT_EQ_FMT((size_t)64, chunk2->object.size, "%zu");
    ASSERT_EQ_FMT((size_t)448 - CHUNK_HEADER_SIZE, chunk4->object.size, "%zu");
    ASSERT_EQ_FMT((size_t)448 - CHUNK_HEADER_SIZE * 3, chunk3->object.size, "%zu");

    // it inserted chunks correctly:
    ASSERT_EQ_FMT((char *)chunk2, (char *)chunk1->next, "%p");
    ASSERT_EQ_FMT((char *)chunk4, (char *)chunk2->next, "%p");
    ASSERT_EQ_FMT((char *)chunk3, (char *)chunk4->next, "%p");
    ASSERT(chunk3->next == NULL);
    ASSERT_EQ(chunk3, list.last);

    ASSERT_EQ(0, chunk1->allocated);
    ASSERT_EQ(0, chunk2->allocated);
    ASSERT_EQ(0, chunk4->allocated);
    ASSERT_EQ(0, chunk3->allocated);

    ASSERT_EQ(0, chunk1->object.marked);
    ASSERT_EQ(0, chunk2->object.marked);
    ASSERT_EQ(0, chunk4->object.marked);
    ASSERT_EQ(0, chunk3->object.marked);

    ASSERT_EQ(0, chunk1->object.atomic);
    ASSERT_EQ(0, chunk2->object.atomic);
    ASSERT_EQ(0, chunk4->object.atomic);
    ASSERT_EQ(0, chunk3->object.atomic);

    // split chunk3:
    chunk5 = ChunkList_split(&list, chunk3, chunk3->object.size - CHUNK_MIN_SIZE);

    ChunkList_debug(&list);

    free(chunk1);
    PASS();
}

SUITE(ChunkListSuite) {
    RUN_TEST(test_ChunkList_clear);
    RUN_TEST(test_ChunkList_push);
    //RUN_TEST(test_ChunkList_shift);
    RUN_TEST(test_ChunkList_insert);
    RUN_TEST(test_ChunkList_split);
}
