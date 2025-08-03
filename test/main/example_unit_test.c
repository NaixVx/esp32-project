#include <stdio.h>

#include "unity.h"

#ifdef __cplusplus
extern "C" {
#endif

void setUp(void) {
    // Set up before every test
}

void tearDown(void) {
    // Clean up after every test
}

void test_config_manager_defaults();

#ifdef __cplusplus
}
#endif

TEST_CASE("Config manager basic test", "[config_manager]") {
    test_config_manager_defaults();
}

void app_main(void) {
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}