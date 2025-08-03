#include <stdio.h>

#include "nvs_flash.h"
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
    // init nvs
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // tests
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}