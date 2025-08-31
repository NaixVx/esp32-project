#pragma once

#include <functional>
#include <mutex>
#include <vector>

#include "esp_err.h"

#define DEVICE_NAME_MAX_LEN 32
#define FW_VERSION_MAX_LEN 16
#define SSID_MAX_LEN 32
#define PASSWORD_MAX_LEN 64
#define MAC_ADDR_LEN 18
#define IP_ADDR_LEN 16

/**
 * @brief Structure holding basic device information.
 */
struct DeviceInfo {
    char device_name[DEVICE_NAME_MAX_LEN];      ///< Device name string
    char firmware_version[FW_VERSION_MAX_LEN];  ///< Firmware version string
};

/**
 * @brief Structure holding network-related configuration.
 */
struct NetworkConfig {
    char ap_ssid[SSID_MAX_LEN];          ///< SSID for the access point
    char ap_password[PASSWORD_MAX_LEN];  ///< Password for the access point
    bool ap_enabled;                     ///< Whether AP mode is enabled
    bool sta_enabled;                    ///< Whether STA mode is enabled
    char ssid[SSID_MAX_LEN];             ///< SSID for STA connection
    char bssid[MAC_ADDR_LEN];            ///< BSSID (MAC) of connected AP
    char ip_address[IP_ADDR_LEN];        ///< Static IP address (if used)
    char mac_address[MAC_ADDR_LEN];      ///< Device MAC address
};

/**
 * @brief Structure holding the full device configuration.
 */
struct DeviceConfig {
    DeviceInfo info;        ///< Device-specific info
    NetworkConfig network;  ///< Network-specific config
};

/**
 * @brief Singleton class for managing persistent device configuration using NVS.
 */
class ConfigManager {
   public:
    /**
     * @brief Get singleton instance of ConfigManager.
     * @return Reference to ConfigManager
     */
    static ConfigManager& getInstance();

    // === Full config ===
    DeviceConfig getConfig();
    void updateConfig(const DeviceConfig& config);

    // === Device Info ===
    DeviceInfo getDeviceInfo();
    void updateDeviceInfo(const DeviceInfo& info);

    // === Network Config ===
    NetworkConfig getNetworkConfig();
    void updateNetworkConfig(const NetworkConfig& netConfig);

    // === Internal Operations ===
    esp_err_t loadFromNVS();
    esp_err_t saveToNVS();
    void setDefaults();
    bool isValid();

    // === Observers ===
    using NetworkObserver = std::function<void(const NetworkConfig&)>;
    using DeviceInfoObserver = std::function<void(const DeviceInfo&)>;

    /**
     * @brief Register a callback to be notified when network config changes.
     */
    void registerNetworkObserver(NetworkObserver obs);

    /**
     * @brief Register a callback to be notified when device info changes.
     */
    void registerDeviceInfoObserver(DeviceInfoObserver obs);

   private:
    /**
     * @brief Private constructor (singleton pattern).
     */
    ConfigManager();

    DeviceConfig config_;  ///< Internal storage for current config
    std::mutex mutex_;     ///< Mutex to protect concurrent access

    std::vector<NetworkObserver> network_observers_;  ///< List of network observers
    std::vector<DeviceInfoObserver> info_observers_;  ///< List of device info observers
};
