#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "utils/lock_guard.hpp"

// ---- Limits ----
#define DEVICE_NAME_MAX_LEN 32
#define DEVICE_TYPE_MAX_LEN 32
#define FW_VERSION_MAX_LEN 16
#define SSID_MAX_LEN 32
#define PASSWORD_MAX_LEN 32
#define MAC_ADDR_LEN 18 // "AA:BB:CC:DD:EE:FF" + '\0'
#define IP_ADDR_LEN 16  // "255.255.255.255" + '\0'
#define ID_MAX_LEN 16

struct ConfigHeader {
    uint16_t version = 1;     // bump when the on-flash schema changes
    uint16_t struct_size = 0; // sizeof(PersistedConfig) at time of save
};

struct DeviceInfo {
    char id[ID_MAX_LEN];
    char device_name[DEVICE_NAME_MAX_LEN];
    char device_type[DEVICE_TYPE_MAX_LEN];
    char firmware_version[FW_VERSION_MAX_LEN];
};

struct NetworkConfigAP {
    uint8_t enabled;
    uint8_t _pad[3];
    char ssid[SSID_MAX_LEN];
    char password[PASSWORD_MAX_LEN];
};

struct NetworkConfigSTA {
    char ssid[SSID_MAX_LEN];
    char password[PASSWORD_MAX_LEN];
};

struct NetworkConfig {
    char mac_address[MAC_ADDR_LEN];
    uint8_t _pad[2];
    NetworkConfigAP ap{};
    NetworkConfigSTA sta{};
};

struct PersistedConfig {
    ConfigHeader header{};
    DeviceInfo info{};
    NetworkConfig network{};
};

static_assert(sizeof(ConfigHeader) % 4 == 0, "ConfigHeader unexpected padding");
static_assert(sizeof(DeviceInfo) % 4 == 0, "DeviceInfo not 4-byte aligned");
static_assert(sizeof(NetworkConfig) % 4 == 0, "NetworkConfig not 4-byte aligned");
static_assert(sizeof(PersistedConfig) % 4 == 0, "PersistedConfig not 4-byte aligned");

class ConfigManager
{
  public:
    static ConfigManager& getInstance();

    DeviceInfo getDeviceInfo();
    esp_err_t setDeviceInfo(const DeviceInfo& info);

    NetworkConfig getNetworkConfig();
    esp_err_t setNetworkConfig(const NetworkConfig& net);
    esp_err_t setNetworkConfigAP(const NetworkConfigAP& net);
    esp_err_t setNetworkConfigSTA(const NetworkConfigSTA& net);

    esp_err_t loadFromNVS();
    esp_err_t saveToNVS();

    void setDefaults();
    esp_err_t resetToDefaults();
    bool isValid();

    using NetworkObserver = std::function<void(const NetworkConfig&)>;
    using DeviceInfoObserver = std::function<void(const DeviceInfo&)>;

    void registerNetworkObserver(NetworkObserver obs);
    void registerDeviceInfoObserver(DeviceInfoObserver obs);

  private:
    ConfigManager();
    ~ConfigManager();

    PersistedConfig persisted_{};

    SemaphoreHandle_t mutex_{nullptr};

    std::vector<NetworkObserver> network_observers_;
    std::vector<DeviceInfoObserver> info_observers_;
};
