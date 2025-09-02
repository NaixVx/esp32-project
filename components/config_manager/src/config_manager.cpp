#include "config_manager.hpp"

#include <cstring>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "utils/lock_guard.hpp"

static const char* TAG = "config_manager";
constexpr const char* NVS_NAMESPACE = "storage";
constexpr const char* NVS_KEY = "dev_config";

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    ESP_LOGI(TAG, "Initializing ConfigManager");

    // Create mutex
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ESP_LOGE(TAG, "Failed to create mutex!");
        abort();
    }

    if (loadFromNVS() != ESP_OK || !isValid()) {
        ESP_LOGW(TAG, "Invalid or missing config, using defaults");
        setDefaults();
        saveToNVS();
    } else {
        ESP_LOGI(TAG, "Loaded valid config from NVS");
    }
}

ConfigManager::~ConfigManager() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

// --- Getters and setters ---
DeviceInfo ConfigManager::getDeviceInfo() {
    LockGuard guard(mutex_);
    return config_.info;
}

void ConfigManager::updateDeviceInfo(const DeviceInfo& info) {
    DeviceInfo snapshot;
    std::vector<DeviceInfoObserver> observers_copy;

    {
        LockGuard guard(mutex_);
        ESP_LOGI(TAG, "Updating device info: name=%s, fw=%s", info.device_name,
                 info.firmware_version);
        config_.info = info;
        snapshot = config_.info;
        observers_copy = info_observers_;
    }

    if (saveToNVS() == ESP_OK) {
        for (auto& obs : observers_copy)
            if (obs) obs(snapshot);
    }
}

NetworkConfig ConfigManager::getNetworkConfig() {
    LockGuard guard(mutex_);
    return config_.network;
}

void ConfigManager::updateNetworkConfig(const NetworkConfig& netConfig) {
    NetworkConfig snapshot;
    std::vector<NetworkObserver> observers_copy;

    {
        LockGuard guard(mutex_);
        ESP_LOGI(TAG, "Updating network config: AP=%s", netConfig.ap_ssid);
        config_.network = netConfig;
        snapshot = config_.network;
        observers_copy = network_observers_;
    }

    if (saveToNVS() == ESP_OK) {
        for (auto& obs : observers_copy)
            if (obs) obs(snapshot);
    }
}

DeviceConfig ConfigManager::getConfig() {
    LockGuard guard(mutex_);
    return config_;
}

void ConfigManager::updateConfig(const DeviceConfig& newConfig) {
    DeviceConfig snapshot;
    std::vector<NetworkObserver> net_copy;
    std::vector<DeviceInfoObserver> info_copy;

    {
        LockGuard guard(mutex_);
        config_ = newConfig;
        snapshot = config_;
        net_copy = network_observers_;
        info_copy = info_observers_;
    }

    if (saveToNVS() == ESP_OK) {
        for (auto& obs : info_copy)
            if (obs) obs(snapshot.info);
        for (auto& obs : net_copy)
            if (obs) obs(snapshot.network);
    }
}

// --- NVS operations ---
esp_err_t ConfigManager::saveToNVS() {
    LockGuard guard(mutex_);
    ESP_LOGI(TAG, "Saving device config to NVS");

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(nvs, NVS_KEY, &config_, sizeof(config_));
    if (err == ESP_OK) err = nvs_commit(nvs);

    nvs_close(nvs);
    return err;
}

esp_err_t ConfigManager::loadFromNVS() {
    LockGuard guard(mutex_);
    ESP_LOGI(TAG, "Loading device config from NVS");

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    size_t required_size = 0;
    err = nvs_get_blob(nvs, NVS_KEY, NULL, &required_size);
    if (err == ESP_OK && required_size == sizeof(config_)) {
        err = nvs_get_blob(nvs, NVS_KEY, &config_, &required_size);
    } else {
        err = ESP_ERR_NVS_INVALID_LENGTH;
    }

    nvs_close(nvs);
    return err;
}

// --- Defaults & validation ---
void ConfigManager::setDefaults() {
    LockGuard guard(mutex_);
    std::memset(&config_, 0, sizeof(config_));
    strncpy(config_.info.device_name, "esp32-project", DEVICE_NAME_MAX_LEN - 1);
    strncpy(config_.info.firmware_version, "0.001", FW_VERSION_MAX_LEN - 1);
    strncpy(config_.network.ap_ssid, "ESP32_default_AP", SSID_MAX_LEN - 1);
    config_.network.ap_password[0] = '\0';
    config_.network.ap_enabled = true;
    config_.network.sta_enabled = false;
}

bool ConfigManager::isValid() {
    LockGuard guard(mutex_);
    return strlen(config_.info.device_name) > 0 && strlen(config_.info.firmware_version) > 0 &&
           strlen(config_.network.ap_ssid) > 0;
}

// --- Observer registration ---
void ConfigManager::registerNetworkObserver(NetworkObserver obs) {
    LockGuard guard(mutex_);
    network_observers_.push_back(std::move(obs));
}

void ConfigManager::registerDeviceInfoObserver(DeviceInfoObserver obs) {
    LockGuard guard(mutex_);
    info_observers_.push_back(std::move(obs));
}
