#pragma once

#include <mutex>

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

    /**
     * @brief Retrieve the current full device configuration.
     * @return DeviceConfig structure
     */
    DeviceConfig getConfig();

    /**
     * @brief Update the entire device configuration.
     * @param config New configuration to store
     */
    void updateConfig(const DeviceConfig& config);

    // === Device Info ===

    /**
     * @brief Get stored device information.
     * @return DeviceInfo structure
     */
    DeviceInfo getDeviceInfo();

    /**
     * @brief Update and save device information.
     * @param info New device info to store
     */
    void updateDeviceInfo(const DeviceInfo& info);

    // === Network Config ===

    /**
     * @brief Get stored network configuration.
     * @return NetworkConfig structure
     */
    NetworkConfig getNetworkConfig();

    /**
     * @brief Update and save network configuration.
     * @param netConfig New network configuration
     */
    void updateNetworkConfig(const NetworkConfig& netConfig);

    // === Internal Operations ===

    /**
     * @brief Load configuration from NVS.
     * @return ESP_OK on success, or error code
     */
    esp_err_t loadFromNVS();

    /**
     * @brief Save current configuration to NVS.
     * @return ESP_OK on success, or error code
     */
    esp_err_t saveToNVS();

    /**
     * @brief Reset configuration to default values.
     */
    void setDefaults();

    /**
     * @brief Validate current configuration contents.
     * @return true if config is valid, false otherwise
     */
    bool isValid();

   private:
    /**
     * @brief Private constructor (singleton pattern).
     */
    ConfigManager();

    DeviceConfig config_;  ///< Internal storage for current config
    std::mutex mutex_;     ///< Mutex to protect concurrent access
};
