#include <stddef.h>
#include <stdlib.h>
#include "greatest.h"
#include "chunk_list.h"

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

    // ChunkList_debug(&list);

    free(chunk1);
    PASS();
}

// TODO: drop merge tests (test sweep instead)
TEST test_ChunkList_merge() {
    char *heap = malloc(1024);

    ChunkList list;
    ChunkList_clear(&list);

    size_t size = 128 - CHUNK_HEADER_SIZE;
    Chunk *chunk1 = (Chunk *)(heap +   0); Chunk_init(chunk1, size); ChunkList_push(&list, chunk1);
    Chunk *chunk2 = (Chunk *)(heap + 128); Chunk_init(chunk2, size); ChunkList_push(&list, chunk2);
    Chunk *chunk3 = (Chunk *)(heap + 256); Chunk_init(chunk3, size); ChunkList_push(&list, chunk3);
    Chunk *chunk4 = (Chunk *)(heap + 384); Chunk_init(chunk4, size); ChunkList_push(&list, chunk4);
    Chunk *chunk5 = (Chunk *)(heap + 512); Chunk_init(chunk5, size); ChunkList_push(&list, chunk5);
    Chunk *chunk6 = (Chunk *)(heap + 640); Chunk_init(chunk6, size); ChunkList_push(&list, chunk6);
    Chunk *chunk7 = (Chunk *)(heap + 768); Chunk_init(chunk7, size); ChunkList_push(&list, chunk7);
    Chunk *chunk8 = (Chunk *)(heap + 896); Chunk_init(chunk8, size); ChunkList_push(&list, chunk8);

    ASSERT_EQ_FMT(heap + 1024, ChunkList_limit(&list), "%p");

    ChunkList_merge(&list, chunk1, chunk3, 1);
    ASSERT_EQ(chunk3, chunk1->next);
    ASSERT_EQ_FMT(256 - CHUNK_HEADER_SIZE, chunk1->object.size, "%zu");
    ASSERT_EQ_FMT((size_t)7, list.size, "%zu");

    ChunkList_merge(&list, chunk3, chunk6, 2);
    ASSERT_EQ(chunk6, chunk3->next);
    ASSERT_EQ_FMT(384 - CHUNK_HEADER_SIZE, chunk3->object.size, "%zu");
    ASSERT_EQ_FMT((size_t)5, list.size, "%zu");

    ChunkList_merge(&list, chunk6, NULL, 2);
    ASSERT_EQ(NULL, chunk6->next);
    ASSERT_EQ_FMT(384 - CHUNK_HEADER_SIZE, chunk6->object.size, "%zu");
    ASSERT_EQ_FMT((void *)chunk6, (void *)list.last, "%p");
    ASSERT_EQ_FMT((size_t)3, list.size, "%zu");

    PASS();
}

TEST test_ChunkList_find() {
    SKIP();
}

TEST test_ChunkList_sweep() {
    char *heap = malloc(1024);

    ChunkList list;
    ChunkList_clear(&list);

    ASSERT_EQ_FMT(NULL, ChunkList_limit(&list), "%p");

    size_t size = 128 - CHUNK_HEADER_SIZE;
    Chunk *chunk1 = (Chunk *)(heap +   0); Chunk_init(chunk1, size); ChunkList_push(&list, chunk1);
    Chunk *chunk2 = (Chunk *)(heap + 128); Chunk_init(chunk2, size); ChunkList_push(&list, chunk2);
    Chunk *chunk3 = (Chunk *)(heap + 256); Chunk_init(chunk3, size); ChunkList_push(&list, chunk3);
    Chunk *chunk4 = (Chunk *)(heap + 384); Chunk_init(chunk4, size); ChunkList_push(&list, chunk4);
    Chunk *chunk5 = (Chunk *)(heap + 512); Chunk_init(chunk5, size); ChunkList_push(&list, chunk5);
    Chunk *chunk6 = (Chunk *)(heap + 640); Chunk_init(chunk6, size); ChunkList_push(&list, chunk6);
    Chunk *chunk7 = (Chunk *)(heap + 768); Chunk_init(chunk7, size); ChunkList_push(&list, chunk7);
    Chunk *chunk8 = (Chunk *)(heap + 896); Chunk_init(chunk8, size); ChunkList_push(&list, chunk8);

    ASSERT_EQ_FMT(heap + 1024, ChunkList_limit(&list), "%p");

    chunk1->allocated = 1;
    chunk2->allocated = 1;
    chunk3->allocated = 1;
    chunk4->allocated = 1;
    chunk5->allocated = 1;
    chunk6->allocated = 1;
    chunk7->allocated = 1;
    chunk8->allocated = 1;

    Chunk_unmark(chunk1);
    Chunk_unmark(chunk2);
    Chunk_mark(chunk3);
    Chunk_unmark(chunk4);
    Chunk_mark(chunk5);
    Chunk_unmark(chunk6);
    Chunk_unmark(chunk7);
    Chunk_unmark(chunk8);

    ChunkList_sweep(&list);

    // merged chunks 1 and 2:
    ASSERT_FALSE(chunk1->allocated);
    ASSERT_EQ_FMT((void *)chunk3, (void *)chunk1->next, "%p");
    ASSERT_EQ_FMT(256 - CHUNK_HEADER_SIZE, chunk1->object.size, "%zu");

    // kept chunks 3, 4 and 5:
    ASSERT(chunk3->allocated);
    ASSERT_EQ_FMT((void *)chunk4, (void *)chunk3->next, "%p");

    ASSERT_FALSE(chunk4->allocated);
    ASSERT_EQ_FMT((void *)chunk5, (void *)chunk4->next, "%p");

    ASSERT(chunk5->allocated);
    ASSERT_EQ_FMT((void *)chunk6, (void *)chunk5->next, "%p");

    // merged chunks 6, 7 and 8:
    ASSERT_FALSE(chunk6->allocated);
    ASSERT_EQ_FMT(NULL, (void *)chunk6->next, "%p");
    ASSERT_EQ_FMT(384 - CHUNK_HEADER_SIZE, chunk6->object.size, "%zu");

    // updated list:
    ASSERT_EQ_FMT((void *)chunk6, (void *)list.last, "%p");
    ASSERT_EQ_FMT((size_t)5, list.size, "%zu");

    PASS();
}

SUITE(ChunkListSuite) {
    RUN_TEST(test_ChunkList_clear);
    RUN_TEST(test_ChunkList_push);
    //RUN_TEST(test_ChunkList_shift);
    RUN_TEST(test_ChunkList_insert);
    RUN_TEST(test_ChunkList_split);
    RUN_TEST(test_ChunkList_merge);
    RUN_TEST(test_ChunkList_find);
    RUN_TEST(test_ChunkList_sweep);
}
