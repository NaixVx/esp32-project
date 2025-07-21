#pragma once

#include <mutex>

#include "esp_err.h"

#define DEVICE_NAME_MAX_LEN 32
#define FW_VERSION_MAX_LEN 16
#define SSID_MAX_LEN 32
#define PASSWORD_MAX_LEN 64
#define MAC_ADDR_LEN 18
#define IP_ADDR_LEN 16

struct DeviceInfo {
    char device_name[DEVICE_NAME_MAX_LEN];
    char firmware_version[FW_VERSION_MAX_LEN];
};

struct NetworkConfig {
    char ap_ssid[SSID_MAX_LEN];
    char ap_password[PASSWORD_MAX_LEN];
    bool ap_enabled;
    bool sta_enabled;
    char ssid[SSID_MAX_LEN];
    char bssid[MAC_ADDR_LEN];
    char ip_address[IP_ADDR_LEN];
    char mac_address[MAC_ADDR_LEN];
};

struct DeviceConfig {
    DeviceInfo info;
    NetworkConfig network;
};

class ConfigManager {
   public:
    static ConfigManager& getInstance();

    // Full config
    DeviceConfig getConfig();
    void updateConfig(const DeviceConfig& config);

    // Device config
    DeviceInfo getDeviceInfo();
    void updateDeviceInfo(const DeviceInfo& info);

    // Network config
    NetworkConfig getNetworkConfig();
    void updateNetworkConfig(const NetworkConfig& netConfig);

    // Internal operations
    esp_err_t loadFromNVS();
    esp_err_t saveToNVS();
    void setDefaults();
    bool isValid();

   private:
    ConfigManager();
    DeviceConfig config_;
    std::mutex mutex_;
};
