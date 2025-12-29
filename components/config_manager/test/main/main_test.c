#include <stdio.h>

#include "nvs_flash.h"
#include "unity.h"

#ifdef __cplusplus
extern "C" {
#endif

void setUp(void)
{
    // Set up before every test
}

void tearDown(void)
{
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

TEST_CASE("Config: Defaults are set correctly", "[config]")
{
    test_config_defaults_are_correct();
}

TEST_CASE("Config: Loads defaults on invalid NVS", "[config]")
{
    test_config_loads_defaults_on_invalid_nvs();
}

TEST_CASE("Config: Validation passes on defaults", "[config]")
{
    test_validation_passes_on_default_config();
}

TEST_CASE("Config: Singleton instance is consistent", "[config]")
{
    test_singleton_returns_same_instance();
}

TEST_CASE("Config: Can set and retrieve DeviceInfo", "[config]")
{
    test_device_info_can_be_set_and_read();
}

TEST_CASE("Config: Can set and retrieve NetworkConfig", "[config]")
{
    test_network_config_can_be_set_and_read();
}

TEST_CASE("Config: Can update and retrieve full DeviceConfig", "[config]")
{
    test_full_device_config_can_be_set_and_read();
}

TEST_CASE("Config: Saves and restores config from NVS", "[config]")
{
    test_config_is_saved_and_restored_from_nvs();
}

TEST_CASE("Validation: Fails with empty device name", "[validation]")
{
    test_validation_fails_with_empty_device_name();
}

TEST_CASE("Validation: Fails with empty firmware version", "[validation]")
{
    test_validation_fails_with_empty_firmware_version();
}

TEST_CASE("Validation: Fails with empty AP SSID", "[validation]")
{
    test_validation_fails_with_empty_ap_ssid();
}

TEST_CASE("NVS: Load fails with wrong blob size, loads defaults", "[nvs]")
{
    test_config_fails_to_load_with_wrong_blob_size();
}

static void logDeviceInfo(const DeviceInfo& info)
{
    ESP_LOGI("TEST", "DeviceInfo:");
    ESP_LOGI("TEST", "  id: %s", info.id);
    ESP_LOGI("TEST", "  name: %s", info.device_name);
    ESP_LOGI("TEST", "  type: %s", info.device_type);
    ESP_LOGI("TEST", "  fw: %s", info.firmware_version);
}

static void logNetworkPrivate(const NetworkPrivateConfig& net)
{
    ESP_LOGI("TEST", "NetworkPrivateConfig:");
    ESP_LOGI("TEST", "  sta_ssid: %s", net.sta_ssid);
    ESP_LOGI("TEST", "  sta_password: %s", net.sta_password);
    ESP_LOGI("TEST", "  ap_enabled: %u", net.ap_enabled);
    ESP_LOGI("TEST", "  ap_ssid: %s", net.ap_ssid);
    ESP_LOGI("TEST", "  ap_password: %s", net.ap_password);
}

static void logNetworkPublic(const NetworkPublicConfig& net)
{
    ESP_LOGI("TEST", "NetworkPublicConfig:");
    ESP_LOGI("TEST", "  mac: %s", net.mac_address);
    ESP_LOGI("TEST", "  sta_connected: %u", net.sta_connected);
    ESP_LOGI("TEST", "  sta_ssid: %s", net.sta_ssid);
    ESP_LOGI("TEST", "  sta_ip: %s", net.sta_ip);
    ESP_LOGI("TEST", "  ap_active: %u", net.ap_active);
    ESP_LOGI("TEST", "  ap_ssid: %s", net.ap_ssid);
    ESP_LOGI("TEST", "  ap_ip: %s", net.ap_ip);
}

void app_main(void)
{
    // Global test setup before UNITY_BEGIN
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    //  // -----------------------------------------------------------------
    // // 2. INIT CONFIG MANAGER
    // // -----------------------------------------------------------------
    // ConfigManager& config = ConfigManager::getInstance();
    // ESP_LOGI("TEST", "ConfigManager initialized");

    // // -----------------------------------------------------------------
    // // 3. READ INITIAL STATE (loaded or defaults)
    // // -----------------------------------------------------------------
    // DeviceInfo info = config.getDeviceInfo();
    // NetworkPrivateConfig net_priv = config.getNetworkPrivateConfig();
    // NetworkPublicConfig net_pub = config.getNetworkPublicConfig();

    // logDeviceInfo(info);
    // logNetworkPrivate(net_priv);
    // logNetworkPublic(net_pub);

    // // -----------------------------------------------------------------
    // // 4. REGISTER OBSERVERS (public state only)
    // // -----------------------------------------------------------------
    // config.registerNetworkObserver([](const NetworkPublicConfig& net) {
    //     ESP_LOGI("OBSERVER", "Network public state changed:");
    //     ESP_LOGI("OBSERVER", "  STA connected: %u", net.sta_connected);
    //     ESP_LOGI("OBSERVER", "  STA IP: %s", net.sta_ip);
    // });

    // config.registerDeviceInfoObserver([](const DeviceInfo& info) {
    //     ESP_LOGI("OBSERVER", "DeviceInfo updated:");
    //     ESP_LOGI("OBSERVER", "  name: %s", info.device_name);
    // });

    // // -----------------------------------------------------------------
    // // 5. UPDATE DEVICE INFO (persisted + observer)
    // // -----------------------------------------------------------------
    // DeviceInfo new_info = info;
    // std::strncpy(new_info.device_name, "esp32-test-device", DEVICE_NAME_MAX_LEN - 1);

    // ESP_ERROR_CHECK(config.updateDeviceInfo(new_info));

    // // -----------------------------------------------------------------
    // // 6. UPDATE PRIVATE NETWORK CONFIG (persisted, NO observer)
    // // -----------------------------------------------------------------
    // NetworkPrivateConfig new_priv = net_priv;
    // std::strncpy(new_priv.sta_ssid, "MyWiFi", SSID_MAX_LEN - 1);
    // std::strncpy(new_priv.sta_password, "password123", PASSWORD_MAX_LEN - 1);

    // ESP_ERROR_CHECK(config.updateNetworkPrivateConfig(new_priv));

    // ESP_LOGI("TEST", "Updated private network config (Wi-Fi manager would reconnect now)");

    // // -----------------------------------------------------------------
    // // 7. UPDATE PUBLIC NETWORK STATE (runtime + observer)
    // // -----------------------------------------------------------------
    // NetworkPublicConfig new_pub = net_pub;
    // std::strncpy(new_pub.mac_address, "AA:BB:CC:DD:EE:FF", MAC_ADDR_LEN - 1);
    // new_pub.sta_connected = 1;
    // std::strncpy(new_pub.sta_ssid, "MyWiFi", SSID_MAX_LEN - 1);
    // std::strncpy(new_pub.sta_ip, "192.168.1.42", IP_ADDR_LEN - 1);

    // config.updateNetworkPublicConfig(new_pub);

    // // -----------------------------------------------------------------
    // // 8. READ BACK EVERYTHING AGAIN
    // // -----------------------------------------------------------------
    // ESP_LOGI("TEST", "Reading back config after updates");

    // logDeviceInfo(config.getDeviceInfo());
    // logNetworkPrivate(config.getNetworkPrivateConfig());
    // logNetworkPublic(config.getNetworkPublicConfig());

    // ESP_LOGI("TEST", "ConfigManager basic test finished");

    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();

    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());
}