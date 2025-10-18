#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "utils/lock_guard.hpp"

// --- Limits ---
#define DEVICE_NAME_MAX_LEN 32
#define FW_VERSION_MAX_LEN 16
#define SSID_MAX_LEN 32
#define PASSWORD_MAX_LEN 64
#define MAC_ADDR_LEN 18  // "AA:BB:CC:DD:EE:FF" + '\0'
#define IP_ADDR_LEN 16   // "255.255.255.255" + '\0'

// --- Persisted layout helpers ---
struct ConfigHeader {
    uint16_t version = 1;      // bump when the on-flash schema changes
    uint16_t struct_size = 0;  // sizeof(DeviceConfig) at time of save
};

struct DeviceInfo {
    char device_name[DEVICE_NAME_MAX_LEN];
    char firmware_version[FW_VERSION_MAX_LEN];
};

struct NetworkConfig {
    char ap_ssid[SSID_MAX_LEN];
    char ap_password[PASSWORD_MAX_LEN];

    uint8_t ap_enabled;
    uint8_t sta_enabled;
    uint8_t _pad0[2] = {0};

    char ssid[SSID_MAX_LEN];         // STA SSID (if sta_enabled)
    char bssid[MAC_ADDR_LEN];        // optional, as "AA:BB:CC:DD:EE:FF"
    char ip_address[IP_ADDR_LEN];    // optional static IP, dotted quad
    char mac_address[MAC_ADDR_LEN];  // device MAC, as string
};

struct DeviceConfig {
    ConfigHeader header{};
    DeviceInfo info{};
    NetworkConfig network{};
};

static_assert(sizeof(ConfigHeader) == 4, "ConfigHeader unexpected padding");

// --- Manager ---
class ConfigManager {
   public:
    static ConfigManager& getInstance();

    // Accessors (return copies to keep internal state encapsulated)
    DeviceConfig getConfig();
    DeviceInfo getDeviceInfo();
    NetworkConfig getNetworkConfig();

    // Mutators report status. Observers fire only on ESP_OK save.
    esp_err_t updateConfig(const DeviceConfig& config);
    esp_err_t updateDeviceInfo(const DeviceInfo& info);
    esp_err_t updateNetworkConfig(const NetworkConfig& netConfig);

    // Persistence
    esp_err_t loadFromNVS();
    esp_err_t saveToNVS();

    // Defaults / validation
    void setDefaults();           // set in-memory defaults (no save)
    esp_err_t resetToDefaults();  // set defaults and persist
    bool isValid();               // validates current in-memory config

    // Observability
    using NetworkObserver = std::function<void(const NetworkConfig&)>;
    using DeviceInfoObserver = std::function<void(const DeviceInfo&)>;

    void registerNetworkObserver(NetworkObserver obs);
    void registerDeviceInfoObserver(DeviceInfoObserver obs);

   private:
    ConfigManager();
    ~ConfigManager();

    // internal state
    DeviceConfig config_{};
    SemaphoreHandle_t mutex_{nullptr};

    std::vector<NetworkObserver> network_observers_;
    std::vector<DeviceInfoObserver> info_observers_;
};
