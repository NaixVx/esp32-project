
#include "config_manager.hpp"
#include "unity.h"

extern "C" void test_config_manager_defaults() {
    ConfigManager& cm = ConfigManager::getInstance();
    DeviceInfo info = cm.getDeviceInfo();
    NetworkConfig net = cm.getNetworkConfig();

    // Check default device name
    TEST_ASSERT_EQUAL_STRING("esp32-project", info.device_name);

    // Check default firmware version
    TEST_ASSERT_EQUAL_STRING("0.001", info.firmware_version);

    // Check default AP SSID
    TEST_ASSERT_EQUAL_STRING("ESP32_default_AP", net.ap_ssid);

    // Check AP is enabled, STA is disabled
    TEST_ASSERT_TRUE(net.ap_enabled);
    TEST_ASSERT_FALSE(net.sta_enabled);
}