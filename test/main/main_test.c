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

// config manager tests
void test_config_defaults_are_correct();
void test_config_loads_defaults_on_invalid_nvs();
void test_validation_passes_on_default_config();
void test_singleton_returns_same_instance();
void test_device_info_can_be_set_and_read();
void test_network_config_can_be_set_and_read();
void test_full_device_config_can_be_set_and_read();
void test_config_is_saved_and_restored_from_nvs();
void test_validation_fails_with_empty_device_name();
void test_validation_fails_with_empty_firmware_version();
void test_validation_fails_with_empty_ap_ssid();
void test_config_fails_to_load_with_wrong_blob_size();

#ifdef __cplusplus
}
#endif

TEST_CASE("Config: Defaults are set correctly", "[config]") {
    test_config_defaults_are_correct();
}

TEST_CASE("Config: Loads defaults on invalid NVS", "[config]") {
    test_config_loads_defaults_on_invalid_nvs();
}

TEST_CASE("Config: Validation passes on defaults", "[config]") {
    test_validation_passes_on_default_config();
}

TEST_CASE("Config: Singleton instance is consistent", "[config]") {
    test_singleton_returns_same_instance();
}

TEST_CASE("Config: Can set and retrieve DeviceInfo", "[config]") {
    test_device_info_can_be_set_and_read();
}

TEST_CASE("Config: Can set and retrieve NetworkConfig", "[config]") {
    test_network_config_can_be_set_and_read();
}

TEST_CASE("Config: Can update and retrieve full DeviceConfig", "[config]") {
    test_full_device_config_can_be_set_and_read();
}

TEST_CASE("Config: Saves and restores config from NVS", "[config]") {
    test_config_is_saved_and_restored_from_nvs();
}

TEST_CASE("Validation: Fails with empty device name", "[validation]") {
    test_validation_fails_with_empty_device_name();
}

TEST_CASE("Validation: Fails with empty firmware version", "[validation]") {
    test_validation_fails_with_empty_firmware_version();
}

TEST_CASE("Validation: Fails with empty AP SSID", "[validation]") {
    test_validation_fails_with_empty_ap_ssid();
}

TEST_CASE("NVS: Load fails with wrong blob size, loads defaults", "[nvs]") {
    test_config_fails_to_load_with_wrong_blob_size();
}

void app_main(void) {
    // Global test setup before UNITY_BEGIN
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();

    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
}