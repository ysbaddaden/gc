#include "chunk_list_test.c"
#include "memory_test.c"
#include "object_test.c"
#include "immix_test.c"

GREATEST_MAIN_DEFS();

int main(int argc, char** argv) {
    GREATEST_MAIN_BEGIN();

    // data structures
    RUN_SUITE(ChunkSuite);
    RUN_SUITE(ChunkListSuite);
    RUN_SUITE(MemorySuite);
    RUN_SUITE(ObjectSuite);

    // public api
    RUN_SUITE(ImmixSuite);

    GREATEST_MAIN_END();
}
