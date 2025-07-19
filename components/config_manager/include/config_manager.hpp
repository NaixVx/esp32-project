#pragma once

#include <cstdint>

#include "esp_err.h"

struct DeviceInfo {
    char device_name[32];
    char firmware_version[16];
    uint8_t max_connections;
};

struct NetworkConfig {
    char ap_ssid[32];
    char ap_password[64];
    bool ap_enabled;
    bool sta_enabled;
};

struct DeviceConfig {
    DeviceInfo info;
    NetworkConfig network;
};

class ConfigManager {
   public:
    static esp_err_t load(DeviceConfig& config);
    static esp_err_t save(const DeviceConfig& config);
    static void setDefaults(DeviceConfig& config);
};
