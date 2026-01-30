#include <stdio.h>

#include "nvs_flash.h"
#include "esp_littlefs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "config_manager.hpp"
#include "http_server.hpp"
#include "wifi_manager.hpp"
#include "factory_reset.hpp"
#include "temperature_sensor.hpp"

static const char* TAG = "main";

extern "C" void app_main()
{
    // --- INIT NVS ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "NVS initialized");

    // --- INIT LITTLEFS ---
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/www",
        .partition_label = "storage",
        .partition = nullptr,
        .format_if_mount_failed = true,
        .read_only = false,
        .dont_mount = false,
        .grow_on_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS mount failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "LittleFS mounted at /www");
    }

    // --- INIT FACOTRY RESET ---
    static FactoryReset factory_reset;
    factory_reset.init(GPIO_NUM_13);

    // --- INIT CONFIG MANAGER ---
    ConfigManager::getInstance();

    // --- INIT WIFI MANAGER ---
    WiFiManager::getInstance().init();

    // --- INIT HTTP SERVER ---
    static HttpServer http_server;
    http_server.start();

    // --- OTHER COMPONENTS ---
    temperature_sensor::init(GPIO_NUM_4);
    if (temperature_sensor::getStatus()) {
        ESP_LOGI(TAG, "Temperature: %d", temperature_sensor::getLastTemperature());
    } else {
        ESP_LOGE(TAG, "Sensor error");
    }
}