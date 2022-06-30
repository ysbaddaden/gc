#include "greatest.h"
#include "memory.h"

TEST test_mapAndAlign() {
    // mapped memory
    void *addr;
    void *start = GC_mapAndAlign(&addr, 32768 * 4, 32768);
    ASSERT(start != NULL);

    // aligned memory start
    size_t mask = ~(32768 - 1);
    ASSERT_EQ((uintptr_t)start, (uintptr_t)start & mask);

    PASS();
}

TEST test_getMemoryLimit() {
    size_t limit = GC_getMemoryLimit();
    ASSERT(limit > 0);
    PASS();
}

SUITE(MemorySuite) {
    RUN_TEST(test_mapAndAlign);
    RUN_TEST(test_getMemoryLimit);
}
