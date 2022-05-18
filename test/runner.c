#include "config.h"

#ifdef __linux__
#include <stdio.h>
#endif

#include "chunk_list_test.c"
#include "memory_test.c"
#include "object_test.c"
#include "line_header_test.c"
#include "block_test.c"
#include "block_list_test.c"
#include "stack_test.c"
#include "immix_test.c"
#include "hash_test.c"

void GC_collect() {
    GC_collect_once();
}

unsigned long getRandomSeed() {
    unsigned long value;

    int io = open("/dev/urandom", O_RDONLY);
    if (io < 0) {
        fprintf(stderr, "open(/dev/urandom) failed: %s\n", strerror(io));
        abort();
    }

    ssize_t ret = read(io, (char *)&value, sizeof(unsigned long));
    if (ret < 0) {
        fprintf(stderr, "read(/dev/urandom) failed: %s\n", strerror(io));
        abort();
    }

    return value;
}

unsigned long getSeed() {
    char *str = getenv("SEED");

    if (str != NULL) {
        return strtoul(str, NULL, 10);
    }

    return getRandomSeed();
}

GREATEST_MAIN_DEFS();

int main(int argc, char *argv[]) {
    unsigned long int seed = getSeed();
    fprintf(stderr, "SEED=%lu\n\n", seed);

    GREATEST_MAIN_BEGIN();

#ifdef __linux__
    FILE *coredump_filter = fopen("/proc/self/coredump_filter", "w");
    fprintf(coredump_filter, "0x16");
    fclose(coredump_filter);
#endif

    GC_init(BLOCK_SIZE * 2); // 64 KB

    SHUFFLE_SUITES(seed, {
        // data structures
        RUN_SUITE(ChunkSuite);
        RUN_SUITE(ChunkListSuite);
        RUN_SUITE(MemorySuite);
        RUN_SUITE(ObjectSuite);
        RUN_SUITE(LineHeaderSuite);
        RUN_SUITE(BlockSuite);
        RUN_SUITE(BlockListSuite);
        RUN_SUITE(StackSuite);
        RUN_SUITE(HashSuite);

        // public api
        RUN_SUITE(ImmixSuite);
    });

    GC_deinit();

    GREATEST_MAIN_END();
}
