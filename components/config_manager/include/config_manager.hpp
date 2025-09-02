#pragma once

#include <cstring>
#include <functional>
#include <vector>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "utils/lock_guard.hpp"

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

    DeviceConfig getConfig();
    void updateConfig(const DeviceConfig& config);

    DeviceInfo getDeviceInfo();
    void updateDeviceInfo(const DeviceInfo& info);

    NetworkConfig getNetworkConfig();
    void updateNetworkConfig(const NetworkConfig& netConfig);

    esp_err_t loadFromNVS();
    esp_err_t saveToNVS();
    void setDefaults();
    bool isValid();

    using NetworkObserver = std::function<void(const NetworkConfig&)>;
    using DeviceInfoObserver = std::function<void(const DeviceInfo&)>;

    void registerNetworkObserver(NetworkObserver obs);
    void registerDeviceInfoObserver(DeviceInfoObserver obs);

   private:
    ConfigManager();
    ~ConfigManager();

    DeviceConfig config_;
    SemaphoreHandle_t mutex_;

    std::vector<NetworkObserver> network_observers_;
    std::vector<DeviceInfoObserver> info_observers_;
};
