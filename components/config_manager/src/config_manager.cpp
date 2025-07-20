#include "config_manager.hpp"

#include <cstring>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "config_manager";
constexpr const char* NVS_NAMESPACE = "storage";
constexpr const char* NVS_KEY = "dev_config";

esp_err_t ConfigManager::save(const DeviceConfig& config) {
    ESP_LOGI(TAG, "Saving device config to NVS");

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs, NVS_KEY, &config, sizeof(config));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set config blob: %s", esp_err_to_name(err));
    } else {
        err = nvs_commit(nvs);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit config: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Config saved successfully");
        }
    }

    nvs_close(nvs);
    return err;
}

esp_err_t ConfigManager::load(DeviceConfig& config) {
    ESP_LOGI(TAG, "Loading device config from NVS");

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    size_t required_size = sizeof(config);
    err = nvs_get_blob(nvs, NVS_KEY, &config, &required_size);
    nvs_close(nvs);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Config loaded successfully");
    } else {
        ESP_LOGW(TAG, "No valid config found: %s", esp_err_to_name(err));
    }

    return err;
}

void ConfigManager::setDefaults(DeviceConfig& config) {
    ESP_LOGW(TAG, "Setting default config");
    std::memset(&config, 0, sizeof(config));

    // Device Info
    strcpy(config.info.device_name, "esp32-project");
    strcpy(config.info.firmware_version, "0.001");
    config.info.last_boot_ts = 0;

    // Network
    strcpy(config.network.ap_ssid, "ESP32_default_AP");
    config.network.ap_password[0] = '\0';
    config.network.ap_enabled = true;
    config.network.sta_enabled = false;
    config.network.max_connections = 5;

    config.networkStatus.ssid[0] = '\0';
    config.networkStatus.bssid[0] = '\0';
    config.networkStatus.ip_address[0] = '\0';
    config.networkStatus.mac_address[0] = '\0';
}

bool ConfigManager::isValid(const DeviceConfig& config) {
    bool valid = true;

    if (strlen(config.info.device_name) == 0 || strlen(config.info.firmware_version) == 0 ||
        strlen(config.network.ap_ssid) == 0) {
        valid = false;
    }

    if (!valid) {
        ESP_LOGW(TAG, "Config validation failed");
    } else {
        ESP_LOGI(TAG, "Config validation passed");
    }

    return valid;
}
