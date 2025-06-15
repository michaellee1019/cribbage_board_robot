#include <unity.h>

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_basic_pass(void) {
    TEST_ASSERT_EQUAL(1, 1);
}

void test_basic_fail(void) {
    TEST_ASSERT_EQUAL(1, 1); // This should pass
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_basic_pass);
    RUN_TEST(test_basic_fail);
    return UNITY_END();
}
