#include "config_manager.hpp"

#include <cstring>

#include "nvs.h"
#include "nvs_flash.h"

esp_err_t ConfigManager::save(const DeviceConfig& config) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(nvs, "dev_config", &config, sizeof(config));
    if (err == ESP_OK) err = nvs_commit(nvs);

    nvs_close(nvs);
    return err;
}

esp_err_t ConfigManager::load(DeviceConfig& config) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;

    size_t required_size = sizeof(config);
    err = nvs_get_blob(nvs, "dev_config", &config, &required_size);
    nvs_close(nvs);

    return err;
}

void ConfigManager::setDefaults(DeviceConfig& config) {
    std::memset(&config, 0, sizeof(config));

    // General device info
    strcpy(config.info.device_name, "esp32-project");
    strcpy(config.info.firmware_version, "0.001");

    // Network settings
    strcpy(config.network.ap_ssid, "ESP32_default_AP");
    strcpy(config.network.ap_password, "defaultPassword");
    config.network.ap_enabled = true;
    config.network.sta_enabled = false;
}
