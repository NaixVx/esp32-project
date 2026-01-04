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
#define DEVICE_NAME_MAX_LEN 16
#define DEVICE_TYPE_MAX_LEN 16
#define FW_VERSION_MAX_LEN 16
#define SSID_MAX_LEN 16
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

struct NetworkPrivateConfig {
    char sta_ssid[SSID_MAX_LEN];
    char sta_password[PASSWORD_MAX_LEN];

    uint8_t ap_enabled;
    uint8_t _pad[3];

    char ap_ssid[SSID_MAX_LEN];
    char ap_password[PASSWORD_MAX_LEN];
};

struct NetworkPublicConfig {
    char mac_address[MAC_ADDR_LEN];

    uint8_t sta_connected;
    uint8_t ap_active;

    char sta_ssid[SSID_MAX_LEN];
    char sta_ip[IP_ADDR_LEN];

    char ap_ssid[SSID_MAX_LEN];
    char ap_ip[IP_ADDR_LEN];
};

struct PersistedConfig {
    ConfigHeader header{};
    DeviceInfo info{};
    NetworkPrivateConfig network_private{};
};

static_assert(sizeof(ConfigHeader) % 4 == 0, "ConfigHeader unexpected padding");
static_assert(sizeof(DeviceInfo) % 4 == 0, "DeviceInfo not 4-byte aligned");
static_assert(sizeof(NetworkPrivateConfig) % 4 == 0, "NetworkPrivateConfig not 4-byte aligned");
static_assert(sizeof(PersistedConfig) % 4 == 0, "PersistedConfig not 4-byte aligned");

class ConfigManager
{
  public:
    static ConfigManager& getInstance();

    DeviceInfo getDeviceInfo();
    NetworkPrivateConfig getNetworkPrivateConfig();

    esp_err_t updateDeviceInfo(const DeviceInfo& info);
    esp_err_t updateNetworkPrivateConfig(const NetworkPrivateConfig& net);

    NetworkPublicConfig getNetworkPublicConfig();
    void updateNetworkPublicConfig(const NetworkPublicConfig& net);

    esp_err_t loadFromNVS();
    esp_err_t saveToNVS();

    void setDefaults();
    esp_err_t resetToDefaults();
    bool isValid();

    using NetworkPrivateObserver = std::function<void(const NetworkPrivateConfig&)>;
    using DeviceInfoObserver = std::function<void(const DeviceInfo&)>;

    void registerNetworkPrivateObserver(NetworkPrivateObserver obs);
    void registerDeviceInfoObserver(DeviceInfoObserver obs);

  private:
    ConfigManager();
    ~ConfigManager();

    PersistedConfig persisted_{};
    NetworkPublicConfig network_public_{};

    SemaphoreHandle_t mutex_{nullptr};

    std::vector<NetworkPrivateObserver> network_private_observers_;
    std::vector<DeviceInfoObserver> info_observers_;
};
