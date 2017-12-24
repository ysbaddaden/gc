#include "config.h"

#include "chunk_list_test.c"
#include "memory_test.c"
#include "object_test.c"
#include "immix_test.c"

void GC_collect() {
    GC_collect_once();
}

GREATEST_MAIN_DEFS();

int main(int argc, char *argv[]) {
    GREATEST_MAIN_BEGIN();

    GC_init(BLOCK_SIZE * 2); // 64 KB

    // data structures
    RUN_SUITE(ChunkSuite);
    RUN_SUITE(ChunkListSuite);
    RUN_SUITE(MemorySuite);
    RUN_SUITE(ObjectSuite);

    // public api
    RUN_SUITE(ImmixSuite);

    GC_deinit();

    GREATEST_MAIN_END();
}
