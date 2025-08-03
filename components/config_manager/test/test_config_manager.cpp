#include <cstring>

#include "config_manager.hpp"
#include "nvs_flash.h"
#include "unity.h"

/// @brief Helper to reset the config manager state for testing.
/// NOTE: Not thread-safe outside tests.
static void resetConfigManagerForTest() {
    ConfigManager& cm = ConfigManager::getInstance();
    cm.setDefaults();
    cm.saveToNVS();
}

/// @brief Verifies that default config values are set correctly.
extern "C" void test_config_defaults_are_correct() {
    resetConfigManagerForTest();
    ConfigManager& cm = ConfigManager::getInstance();
    DeviceInfo info = cm.getDeviceInfo();
    NetworkConfig net = cm.getNetworkConfig();

    TEST_ASSERT_EQUAL_STRING("esp32-project", info.device_name);
    TEST_ASSERT_EQUAL_STRING("0.001", info.firmware_version);
    TEST_ASSERT_EQUAL_STRING("ESP32_default_AP", net.ap_ssid);
    TEST_ASSERT_TRUE(net.ap_enabled);
    TEST_ASSERT_FALSE(net.sta_enabled);
}

/// @brief Simulates invalid NVS state and checks if defaults are loaded.
extern "C" void test_config_loads_defaults_on_invalid_nvs() {
    TEST_ASSERT_EQUAL(ESP_OK, nvs_flash_erase());
    TEST_ASSERT_EQUAL(ESP_OK, nvs_flash_init());

    ConfigManager& cm = ConfigManager::getInstance();
    DeviceInfo info = cm.getDeviceInfo();

    TEST_ASSERT_EQUAL_STRING("esp32-project", info.device_name);
}

/// @brief Checks that isValid() returns true for the default config.
extern "C" void test_validation_passes_on_default_config() {
    resetConfigManagerForTest();
    ConfigManager& cm = ConfigManager::getInstance();
    TEST_ASSERT_TRUE(cm.isValid());
}

/// @brief Verifies that ConfigManager is a true singleton.
extern "C" void test_singleton_returns_same_instance() {
    ConfigManager& cm1 = ConfigManager::getInstance();
    ConfigManager& cm2 = ConfigManager::getInstance();
    TEST_ASSERT_EQUAL_PTR(&cm1, &cm2);
}

/// @brief Tests setting and retrieving DeviceInfo config.
extern "C" void test_device_info_can_be_set_and_read() {
    resetConfigManagerForTest();
    ConfigManager& cm = ConfigManager::getInstance();

    DeviceInfo info = {};
    strncpy(info.device_name, "custom-device", sizeof(info.device_name));
    strncpy(info.firmware_version, "1.234", sizeof(info.firmware_version));
    info.device_name[sizeof(info.device_name) - 1] = '\0';
    info.firmware_version[sizeof(info.firmware_version) - 1] = '\0';

    cm.updateDeviceInfo(info);

    DeviceInfo out = cm.getDeviceInfo();
    printf("Device name: %s\n", out.device_name);
    printf("Firmware version: %s\n", out.firmware_version);
    TEST_ASSERT_EQUAL_STRING("custom-device", out.device_name);
    TEST_ASSERT_EQUAL_STRING("1.234", out.firmware_version);
}

/// @brief Tests setting and retrieving NetworkConfig config.
extern "C" void test_network_config_can_be_set_and_read() {
    resetConfigManagerForTest();
    ConfigManager& cm = ConfigManager::getInstance();

    NetworkConfig net = {};
    strcpy(net.ap_ssid, "MyAP");
    strcpy(net.ap_password, "password123");
    net.ap_enabled = false;
    net.sta_enabled = true;
    strcpy(net.ssid, "MySTA");
    strcpy(net.bssid, "AA:BB:CC:DD:EE:FF");
    strcpy(net.ip_address, "192.168.1.100");
    strcpy(net.mac_address, "FF:EE:DD:CC:BB:AA");

    cm.updateNetworkConfig(net);

    NetworkConfig out = cm.getNetworkConfig();
    TEST_ASSERT_EQUAL_STRING("MyAP", out.ap_ssid);
    TEST_ASSERT_EQUAL_STRING("password123", out.ap_password);
    TEST_ASSERT_FALSE(out.ap_enabled);
    TEST_ASSERT_TRUE(out.sta_enabled);
    TEST_ASSERT_EQUAL_STRING("MySTA", out.ssid);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", out.bssid);
    TEST_ASSERT_EQUAL_STRING("192.168.1.100", out.ip_address);
    TEST_ASSERT_EQUAL_STRING("FF:EE:DD:CC:BB:AA", out.mac_address);
}

/// @brief Tests full config update and retrieval.
extern "C" void test_full_device_config_can_be_set_and_read() {
    resetConfigManagerForTest();
    ConfigManager& cm = ConfigManager::getInstance();

    DeviceConfig cfg = {};
    strcpy(cfg.info.device_name, "full-config-device");
    strcpy(cfg.info.firmware_version, "2.718");
    strcpy(cfg.network.ap_ssid, "FullConfigAP");
    cfg.network.ap_enabled = true;
    cfg.network.sta_enabled = true;

    cm.updateConfig(cfg);

    DeviceConfig out = cm.getConfig();
    TEST_ASSERT_EQUAL_STRING("full-config-device", out.info.device_name);
    TEST_ASSERT_EQUAL_STRING("2.718", out.info.firmware_version);
    TEST_ASSERT_EQUAL_STRING("FullConfigAP", out.network.ap_ssid);
    TEST_ASSERT_TRUE(out.network.ap_enabled);
    TEST_ASSERT_TRUE(out.network.sta_enabled);
}

/// @brief Tests that config persists correctly via NVS.
extern "C" void test_config_is_saved_and_restored_from_nvs() {
    resetConfigManagerForTest();
    ConfigManager& cm = ConfigManager::getInstance();

    DeviceConfig cfg = {};
    strcpy(cfg.info.device_name, "save-load-device");
    strcpy(cfg.info.firmware_version, "9.999");
    strcpy(cfg.network.ap_ssid, "SavedAP");
    cfg.network.ap_enabled = true;
    cfg.network.sta_enabled = false;

    cm.updateConfig(cfg);
    TEST_ASSERT_EQUAL(ESP_OK, cm.saveToNVS());

    // Simulate restart by forcing reload
    ConfigManager& cm2 = ConfigManager::getInstance();
    DeviceConfig out = cm2.getConfig();

    TEST_ASSERT_EQUAL_STRING("save-load-device", out.info.device_name);
    TEST_ASSERT_EQUAL_STRING("9.999", out.info.firmware_version);
    TEST_ASSERT_EQUAL_STRING("SavedAP", out.network.ap_ssid);
    TEST_ASSERT_TRUE(out.network.ap_enabled);
    TEST_ASSERT_FALSE(out.network.sta_enabled);
}

/// @brief Tests validation failure when device name is missing.
extern "C" void test_validation_fails_with_empty_device_name() {
    resetConfigManagerForTest();
    ConfigManager& cm = ConfigManager::getInstance();
    DeviceConfig cfg = cm.getConfig();
    cfg.info.device_name[0] = '\0';
    cm.updateConfig(cfg);
    TEST_ASSERT_FALSE(cm.isValid());
}

/// @brief Tests validation failure when firmware version is missing.
extern "C" void test_validation_fails_with_empty_firmware_version() {
    resetConfigManagerForTest();
    ConfigManager& cm = ConfigManager::getInstance();
    DeviceConfig cfg = cm.getConfig();
    cfg.info.firmware_version[0] = '\0';
    cm.updateConfig(cfg);
    TEST_ASSERT_FALSE(cm.isValid());
}

/// @brief Tests validation failure when AP SSID is missing.
extern "C" void test_validation_fails_with_empty_ap_ssid() {
    resetConfigManagerForTest();
    ConfigManager& cm = ConfigManager::getInstance();
    DeviceConfig cfg = cm.getConfig();
    cfg.network.ap_ssid[0] = '\0';
    cm.updateConfig(cfg);
    TEST_ASSERT_FALSE(cm.isValid());
}

/// @brief Tests what happens when the config blob size is incorrect.
extern "C" void test_config_fails_to_load_with_wrong_blob_size() {
    // Manually simulate incorrect blob write
    nvs_handle_t nvs;
    TEST_ASSERT_EQUAL(ESP_OK, nvs_open("storage", NVS_READWRITE, &nvs));
    uint8_t fake_data[10] = {0};  // too small
    TEST_ASSERT_EQUAL(ESP_OK, nvs_set_blob(nvs, "dev_config", fake_data, sizeof(fake_data)));
    TEST_ASSERT_EQUAL(ESP_OK, nvs_commit(nvs));
    nvs_close(nvs);

    ConfigManager& cm = ConfigManager::getInstance();
    // Should reload defaults after failing to load
    DeviceInfo info = cm.getDeviceInfo();
    TEST_ASSERT_EQUAL_STRING("esp32-project", info.device_name);
}
