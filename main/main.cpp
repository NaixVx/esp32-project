#include <stdio.h>

#include "config_manager.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "http_server.hpp"
#include "nvs_flash.h"
// #include "sensor_manager.hpp"
#include "wifi_manager.hpp"
#include "esp_log.h"

static const char* TAG = "main";

extern "C" void app_main()
{
    // -----------------------------------------------------------------
    // 1. INIT NVS
    // -----------------------------------------------------------------
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "NVS initialized");

    // --- INIT CONFIG MANAGER ---
    ConfigManager& configManager = ConfigManager::getInstance();

    // --- INIT WIFI MANAGER ---
    WiFiManager wifi;

    wifi.init();
    wifi.applyConfig();

    // --- INIT HTTP SERVER ---
    // static HttpServer http_server(configManager.getDeviceInfo());
    // http_server.start();

    // --- OTHER COMPONENTS ---
    // DS18B20SensorManager::init(GPIO_NUM_4);
}