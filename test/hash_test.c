#include "greatest.h"
#include "hash.h"
#include "random.h"

TEST test_Hash_insert() {
    Hash *hash = Hash_create(8);

    int key = 1;
    int a = 2;
    int b = 2;

    // inserts key/value:
    Hash_insert(hash, &key, &a);
    ASSERT_EQ_FMT((size_t)1, hash->used, "%zu");
    ASSERT_EQ(&a, Hash_search(hash, &key));

    // inserts key once:
    Hash_insert(hash, &key, &a);
    ASSERT_EQ_FMT((size_t)1, hash->used, "%zu");
    ASSERT_EQ(&a, Hash_search(hash, &key));

    // replaces previously set value:
    Hash_insert(hash, &key, &b);
    ASSERT_EQ_FMT((size_t)1, hash->used, "%zu");
    ASSERT_EQ(&b, Hash_search(hash, &key));

    PASS();
}

TEST test_Hash_delete() {
    Hash *hash = Hash_create(8);

    int keys[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    int values[8] = { 9, 10, 11, 12, 13, 14, 15, 16 };

    // fill up to resize limit:
    for (int i = 0; i < 6; i++) {
        Hash_insert(hash, keys+i, values+i);
    }
    ASSERT_EQ(6, hash->used);

    // delete returns existing value:
    ASSERT_EQ(values+3, Hash_delete(hash, keys+3));
    ASSERT_EQ(values+4, Hash_delete(hash, keys+4));

    // cleared tombstones (they reached 25% load):
    ASSERT_EQ(4, hash->used);
    ASSERT_EQ(0, hash->deleted);

    // can't remove unknown key:
    ASSERT_EQ(NULL, Hash_delete(hash, keys+6));

    // can't delete twice:
    ASSERT_EQ(NULL, Hash_delete(hash, keys+3));
    ASSERT_EQ(NULL, Hash_delete(hash, keys+4));

    // can still search:
    ASSERT_EQ(values+0, Hash_search(hash, keys+0));
    ASSERT_EQ(values+1, Hash_search(hash, keys+1));
    ASSERT_EQ(values+2, Hash_search(hash, keys+2));
    ASSERT_EQ(NULL, Hash_search(hash, keys+3));
    ASSERT_EQ(NULL, Hash_search(hash, keys+4));
    ASSERT_EQ(values+5, Hash_search(hash, keys+5));

    PASS();
}

TEST test_Hash_tombstones() {
    Hash *hash = Hash_create(8);

    int n = 192;
    int *keys = malloc(n * sizeof(int));
    int *values = malloc(n * sizeof(int));

    // insert all keys:
    for (int i = 0; i < n; i++) {
        Hash_insert(hash, keys+i, values+i);
    }

    // remove 1/3 keys (creating tombstones):
    for (int i = 0; i < n; i += 3) {
        Hash_delete(hash, keys+i);
    }
    for (int i = 0; i < n; i++) {
        if (i % 3 == 0) {
            ASSERT_EQ(NULL, Hash_search(hash, keys+i));
        } else {
            ASSERT_EQ(values+i, Hash_search(hash, keys+i));
        }
    }

    // reinsert removed keys (recyling tombstones, or not):
    for (int i = 0; i < n; i += 3) {
        Hash_insert(hash, keys+i, values+i);
    }
    for (int i = 0; i < n; i++) {
        ASSERT_EQ(values+i, Hash_search(hash, keys+i));
    }

    PASS();
}

static int test_Hash_sum = 0;

static int sum_hash_test(__attribute__((__unused__)) void *key, int *value) {
    test_Hash_sum += *value;
    return (*value % 2) == 0;
}

TEST test_Hash_deleteIf() {
    Hash *hash = Hash_create(8);

    int keys[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    int values[8] = { 9, 10, 11, 12, 13, 14, 15, 16 };

    for (int i = 0; i < 8; i++) {
        Hash_insert(hash, keys+i, values+i);
    }

    Hash_deleteIf(hash, (hash_iterator_t)sum_hash_test);
    ASSERT_EQ_FMT(100, test_Hash_sum, "%d");

    for (int i = 0; i < 8; i++) {
        if (*(values + i) % 2 == 0) {
            ASSERT_EQ(NULL, Hash_search(hash, keys+i));
        } else {
            ASSERT_EQ(values+i, Hash_search(hash, keys+i));
        }
    }

    PASS();
}

TEST test_Hash() {
    GC_random_init();

    int max = 4096;
    int *keys = calloc((size_t)max, sizeof(int));
    int *values = calloc((size_t)max, sizeof(int));

    GC_random((char *)keys, sizeof(int) * max);
    GC_random((char *)values, sizeof(int) * max);

    unsigned int n;
    unsigned int i;
    for (int j = 0; j < 50000; j++) {
        if (j % 1000 == 0) {
            printf("RUN  test_Hash:  (iteration %d)\r", j);
            fflush(stdout);
        }

        Hash *hash = Hash_create(8);

        GC_random((char *)&n, sizeof(unsigned int));
        n = n % max;

        // empty: can't find anything
        for (i = 0; i < n; i++) {
            ASSERT_EQ(NULL, Hash_search(hash, keys+i));
        }

        // fill the hash (just so it won't resize)
        for (i = 0; i < n; i++) {
            Hash_insert(hash, keys+i, values+i);
        }
        for (i = 0; i < n; i++) {
            ASSERT_EQ(values+i, Hash_search(hash, keys+i));
        }

        // delete 1/4 keys
        int deleted = 0;
        for (i = 0; i < n; i += 4) {
            deleted += 1;
            ASSERT_EQ(values+i, Hash_delete(hash, keys+i));
        }
        for (i = 0; i < n; i++) {
            if (i % 4 == 0) {
                ASSERT_EQ(NULL, Hash_search(hash, keys+i));
            } else {
                ASSERT_EQ(values+i, Hash_search(hash, keys+i));
            }
        }

        // re-insert deleted keys
        for (i = 0; i < n; i += 4) {
            Hash_insert(hash, keys+i, values+i);
            ASSERT_EQ(values+i, Hash_search(hash, keys+i));
        }
        for (i = 0; i < n; i++) {
            ASSERT_EQ(values+i, Hash_search(hash, keys+i));
        }

        // remove all keys
        for (i = 0; i < n; i++) {
            Hash_delete(hash, keys+i);
        }
        for (i = 0; i < n; i++) {
            ASSERT_EQ(NULL, Hash_search(hash, keys+i));
        }

        Hash_free(hash);
    }

    free(keys);
    free(values);
    PASS();
}

SUITE(HashSuite) {
    RUN_TEST(test_Hash_insert);
    RUN_TEST(test_Hash_delete);
    RUN_TEST(test_Hash_tombstones);
    RUN_TEST(test_Hash_deleteIf);
    RUN_TEST(test_Hash);
}
