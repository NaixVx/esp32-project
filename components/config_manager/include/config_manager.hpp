#pragma once

#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "utils/lock_guard.hpp"

/**
 * @file config_manager.hpp
 * @brief Persistent device and network configuration manager.
 *
 * Stores configuration in NVS and provides thread-safe access
 * with observer notifications on changes.
 */

// ---- Limits ----

/// Maximum device name length (including null terminator)
#define DEVICE_NAME_MAX_LEN 32
/// Maximum device type length
#define DEVICE_TYPE_MAX_LEN 32
/// Maximum firmware version string length
#define FW_VERSION_MAX_LEN 16
/// Maximum Wi-Fi SSID length
#define SSID_MAX_LEN 32
/// Maximum Wi-Fi password length
#define PASSWORD_MAX_LEN 32
/// MAC address string length ("AA:BB:CC:DD:EE:FF")
#define MAC_ADDR_LEN 18
/// IPv4 address string length ("255.255.255.255")
#define IP_ADDR_LEN 16
/// Device ID length
#define ID_MAX_LEN 16

/**
 * @brief Header stored alongside persisted configuration.
 *
 * Used for versioning and backward compatibility.
 */
struct ConfigHeader {
  uint16_t version = 1;     ///< Configuration schema version
  uint16_t struct_size = 0; ///< Size of PersistedConfig when saved
};

/**
 * @brief Device identification information.
 */
struct DeviceInfo {
  char id[ID_MAX_LEN];                       ///< Device unique ID
  char device_name[DEVICE_NAME_MAX_LEN];     ///< Human-readable name
  char device_type[DEVICE_TYPE_MAX_LEN];     ///< Device type/model
  char firmware_version[FW_VERSION_MAX_LEN]; ///< Firmware version string
};

/**
 * @brief Access Point (AP) network configuration.
 */
struct NetworkConfigAP {
  uint8_t enabled;                 ///< AP enabled flag (0 or 1)
  uint8_t _pad[3];                 ///< Padding for alignment
  char ssid[SSID_MAX_LEN];         ///< AP SSID
  char password[PASSWORD_MAX_LEN]; ///< AP password
};

/**
 * @brief Station (STA) network configuration.
 */
struct NetworkConfigSTA {
  char ssid[SSID_MAX_LEN];         ///< STA SSID
  char password[PASSWORD_MAX_LEN]; ///< STA password
};

/**
 * @brief Combined network configuration.
 */
struct NetworkConfig {
  NetworkConfigAP ap;   ///< Access Point configuration
  NetworkConfigSTA sta; ///< Station configuration
};

/**
 * @brief Full configuration persisted to NVS.
 */
struct PersistedConfig {
  ConfigHeader header;   ///< Metadata header
  DeviceInfo info;       ///< Device information
  NetworkConfig network; ///< Network configuration
};

// Alignment guarantees for NVS blob storage
static_assert(sizeof(ConfigHeader) % 4 == 0, "ConfigHeader unexpected padding");
static_assert(sizeof(DeviceInfo) % 4 == 0, "DeviceInfo not 4-byte aligned");
static_assert(sizeof(NetworkConfig) % 4 == 0,
              "NetworkConfig not 4-byte aligned");
static_assert(sizeof(PersistedConfig) % 4 == 0,
              "PersistedConfig not 4-byte aligned");

/**
 * @brief Thread-safe singleton managing persistent configuration.
 *
 * Loads configuration from NVS on startup and falls back to defaults
 * if data is missing or invalid.
 */
class ConfigManager {
public:
  /**
   * @brief Get the singleton instance.
   */
  static ConfigManager &getInstance();

  /**
   * @brief Get current device information.
   */
  DeviceInfo getDeviceInfo();

  /**
   * @brief Update device information and persist it.
   */
  esp_err_t setDeviceInfo(const DeviceInfo &info);

  /**
   * @brief Get full network configuration.
   */
  NetworkConfig getNetworkConfig();

  /**
   * @brief Update full network configuration.
   */
  esp_err_t setNetworkConfig(const NetworkConfig &net);

  /**
   * @brief Update only Access Point configuration.
   */
  esp_err_t setNetworkConfigAP(const NetworkConfigAP &net);

  /**
   * @brief Update only Station configuration.
   */
  esp_err_t setNetworkConfigSTA(const NetworkConfigSTA &net);

  /**
   * @brief Load configuration from NVS.
   */
  esp_err_t loadFromNVS();

  /**
   * @brief Save configuration to NVS.
   */
  esp_err_t saveToNVS();

  /**
   * @brief Apply default configuration values.
   */
  void setDefaults();

  /**
   * @brief Reset configuration to defaults and persist it.
   */
  void resetToDefaults();

  /**
   * @brief Validate current configuration contents.
   */
  bool isValid();

  /// Observer callback for network configuration changes
  using NetworkObserver = std::function<void(const NetworkConfig &)>;
  /// Observer callback for device info changes
  using DeviceInfoObserver = std::function<void(const DeviceInfo &)>;

  /**
   * @brief Register a callback for network configuration updates.
   */
  void registerNetworkObserver(NetworkObserver obs);

  /**
   * @brief Register a callback for device info updates.
   */
  void registerDeviceInfoObserver(DeviceInfoObserver obs);

private:
  ConfigManager();
  ~ConfigManager();

  PersistedConfig persisted_; ///< In-memory configuration cache
  SemaphoreHandle_t mutex_;   ///< FreeRTOS mutex

  std::vector<NetworkObserver> network_observers_;
  std::vector<DeviceInfoObserver> info_observers_;
};
