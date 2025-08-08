#include "config_manager.hpp"

#include <cstring>
#include <mutex>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "config_manager";
constexpr const char* NVS_NAMESPACE = "storage";
constexpr const char* NVS_KEY = "dev_config";

/**
 * @brief Get singleton instance of ConfigManager
 *
 * @return Reference to the ConfigManager instance
 */
ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

/**
 * @brief Private constructor that loads config from NVS or sets defaults
 */
ConfigManager::ConfigManager() {
    ESP_LOGI(TAG, "Initializing ConfigManager");

    if (loadFromNVS() != ESP_OK || !isValid()) {
        ESP_LOGW(TAG, "Invalid or missing config, using defaults");
        setDefaults();
        saveToNVS();
    } else {
        ESP_LOGI(TAG, "Loaded valid config from NVS");
    }
}

/**
 * @brief Get current device info
 *
 * @return DeviceInfo structure
 */
DeviceInfo ConfigManager::getDeviceInfo() {
    std::lock_guard<std::mutex> lock(mutex_);
    ESP_LOGD(TAG, "Returning device info");
    return config_.info;
}

/**
 * @brief Update device info and save to NVS
 *
 * @param info New device info
 */
void ConfigManager::updateDeviceInfo(const DeviceInfo& info) {
    std::lock_guard<std::mutex> lock(mutex_);
    ESP_LOGI(TAG, "Updating device info: name=%s, fw=%s", info.device_name, info.firmware_version);
    config_.info = info;
    saveToNVS();
}

/**
 * @brief Get current network configuration
 *
 * @return NetworkConfig structure
 */
NetworkConfig ConfigManager::getNetworkConfig() {
    std::lock_guard<std::mutex> lock(mutex_);
    ESP_LOGD(TAG, "Returning network config");
    return config_.network;
}

/**
 * @brief Update network configuration and save to NVS
 *
 * @param netConfig New network configuration
 */
void ConfigManager::updateNetworkConfig(const NetworkConfig& netConfig) {
    std::lock_guard<std::mutex> lock(mutex_);
    ESP_LOGI(TAG, "Updating network config: AP=%s", netConfig.ap_ssid);
    config_.network = netConfig;
    saveToNVS();
}

/**
 * @brief Get full device configuration
 *
 * @return DeviceConfig structure
 */
DeviceConfig ConfigManager::getConfig() {
    std::lock_guard<std::mutex> lock(mutex_);
    ESP_LOGD(TAG, "Returning full config");
    return config_;
}

/**
 * @brief Update full configuration and save to NVS
 *
 * @param newConfig New configuration
 */
void ConfigManager::updateConfig(const DeviceConfig& newConfig) {
    std::lock_guard<std::mutex> lock(mutex_);
    ESP_LOGI(TAG, "Updating full config");
    config_ = newConfig;
    saveToNVS();
}

/**
 * @brief Save current config to NVS
 *
 * @return esp_err_t ESP_OK on success or error code
 */
esp_err_t ConfigManager::saveToNVS() {
    std::lock_guard<std::mutex> lock(mutex_);
    ESP_LOGI(TAG, "Saving device config to NVS");

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs, NVS_KEY, &config_, sizeof(config_));
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Config saved successfully");
        } else {
            ESP_LOGE(TAG, "Failed to commit config: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to set config blob: %s", esp_err_to_name(err));
    }

    nvs_close(nvs);
    return err;
}

/**
 * @brief Load config from NVS
 *
 * @return esp_err_t ESP_OK on success or error code
 */
esp_err_t ConfigManager::loadFromNVS() {
    std::lock_guard<std::mutex> lock(mutex_);
    ESP_LOGI(TAG, "Loading device config from NVS");

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    size_t required_size = sizeof(config_);
    err = nvs_get_blob(nvs, NVS_KEY, &config_, &required_size);
    nvs_close(nvs);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Config loaded successfully");
    } else {
        ESP_LOGW(TAG, "No valid config found: %s", esp_err_to_name(err));
    }

    return err;
}

/**
 * @brief Set config to default values
 */
void ConfigManager::setDefaults() {
    std::lock_guard<std::mutex> lock(mutex_);
    ESP_LOGW(TAG, "Setting default config");

    std::memset(&config_, 0, sizeof(config_));

    // Device Info
    strcpy(config_.info.device_name, "esp32-project");
    strcpy(config_.info.firmware_version, "0.001");

    // Network defaults
    strcpy(config_.network.ap_ssid, "ESP32_default_AP");
    config_.network.ap_password[0] = '\0';
    config_.network.ap_enabled = true;
    config_.network.sta_enabled = false;
    config_.network.ssid[0] = '\0';
    config_.network.bssid[0] = '\0';
    config_.network.ip_address[0] = '\0';
    config_.network.mac_address[0] = '\0';

    ESP_LOGI(TAG, "Default config set");
}

/**
 * @brief Check if current config is valid
 *
 * @return true if valid, false otherwise
 */
bool ConfigManager::isValid() {
    std::lock_guard<std::mutex> lock(mutex_);
    bool valid = true;

    if (strlen(config_.info.device_name) == 0 || strlen(config_.info.firmware_version) == 0 ||
        strlen(config_.network.ap_ssid) == 0) {
        valid = false;
    }

    if (!valid) {
        ESP_LOGW(TAG, "Config validation failed");
    } else {
        ESP_LOGI(TAG, "Config validation passed");
    }

    return valid;
}
