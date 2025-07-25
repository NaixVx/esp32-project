#pragma once

#include <cstdint>

#include "esp_err.h"

struct DeviceInfo {
    char device_name[32];
    char firmware_version[16];
    uint64_t last_boot_ts;
};

struct NetworkConfig {
    char ap_ssid[32];
    char ap_password[64];
    bool ap_enabled;
    bool sta_enabled;
    uint8_t max_connections;
};

struct RuntimeNetworkStatus {
    char ssid[32];
    char bssid[18];
    char ip_address[16];
    char mac_address[18];
};

struct DeviceConfig {
    DeviceInfo info;
    NetworkConfig network;
    RuntimeNetworkStatus networkStatus;
};

class ConfigManager {
   public:
    static esp_err_t load(DeviceConfig& config);
    static esp_err_t save(const DeviceConfig& config);
    static void setDefaults(DeviceConfig& config);
    static bool isValid(const DeviceConfig& config);
};
