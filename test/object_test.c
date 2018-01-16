#include "greatest.h"
#include "object.h"

TEST test_Object_init() {
    Object object;
    Object_init(&object);

    ASSERT_EQ(0, object.size);
    ASSERT_EQ(0, object.marked);
    ASSERT_EQ(0, object.atomic);

    PASS();
}

SUITE(ObjectSuite) {
    RUN_TEST(test_Object_init);
}
