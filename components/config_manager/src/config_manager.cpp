#include "config_manager.hpp"

#include <cstring>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "config_manager";
constexpr const char* NVS_NAMESPACE = "storage";
constexpr const char* NVS_KEY = "dev_config";

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

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

DeviceInfo ConfigManager::getDeviceInfo() {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.info;
}

void ConfigManager::updateDeviceInfo(const DeviceInfo& info) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ESP_LOGI(TAG, "Updating device info: name=%s, fw=%s", info.device_name,
                 info.firmware_version);
        config_.info = info;
    }

    if (saveToNVS() == ESP_OK) {
        // Notify observers
        DeviceInfo snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = config_.info;
        }
        std::vector<DeviceInfoObserver> observers_copy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            observers_copy = info_observers_;
        }
        for (auto& obs : observers_copy) {
            if (obs) obs(snapshot);
        }
    }
}

NetworkConfig ConfigManager::getNetworkConfig() {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.network;
}

void ConfigManager::updateNetworkConfig(const NetworkConfig& netConfig) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ESP_LOGI(TAG, "Updating network config: AP=%s", netConfig.ap_ssid);
        config_.network = netConfig;
    }

    if (saveToNVS() == ESP_OK) {
        // Notify observers
        NetworkConfig snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = config_.network;
        }
        std::vector<NetworkObserver> observers_copy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            observers_copy = network_observers_;
        }
        for (auto& obs : observers_copy) {
            if (obs) obs(snapshot);
        }
    }
}

DeviceConfig ConfigManager::getConfig() {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void ConfigManager::updateConfig(const DeviceConfig& newConfig) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = newConfig;
    }
    saveToNVS();
    // NOTE: you could also notify both observers here if needed
}

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

void ConfigManager::setDefaults() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::memset(&config_, 0, sizeof(config_));

    strcpy(config_.info.device_name, "esp32-project");
    strcpy(config_.info.firmware_version, "0.001");

    strcpy(config_.network.ap_ssid, "ESP32_default_AP");
    config_.network.ap_password[0] = '\0';
    config_.network.ap_enabled = true;
    config_.network.sta_enabled = false;
    config_.network.ssid[0] = '\0';
    config_.network.bssid[0] = '\0';
    config_.network.ip_address[0] = '\0';
    config_.network.mac_address[0] = '\0';
}

bool ConfigManager::isValid() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (strlen(config_.info.device_name) == 0 || strlen(config_.info.firmware_version) == 0 ||
        strlen(config_.network.ap_ssid) == 0) {
        return false;
    }
    return true;
}

// ---- New observer registration functions ----
void ConfigManager::registerNetworkObserver(NetworkObserver obs) {
    std::lock_guard<std::mutex> lock(mutex_);
    network_observers_.push_back(std::move(obs));
}

void ConfigManager::registerDeviceInfoObserver(DeviceInfoObserver obs) {
    std::lock_guard<std::mutex> lock(mutex_);
    info_observers_.push_back(std::move(obs));
}
