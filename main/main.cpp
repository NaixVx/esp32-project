#include <stdio.h>

#include "config_manager.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http_server.hpp"
#include "nvs_flash.h"
#include "sensor_manager.hpp"
#include "wifi_manager.hpp"

extern "C" void app_main() {
    // INIT DEVICE, CONFIG AND SENSORS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    static DeviceConfig config;
    if (ConfigManager::load(config) != ESP_OK || !ConfigManager::isValid(config)) {
        ConfigManager::setDefaults(config);
        ConfigManager::save(config);
    }

    static WiFiManager wifi(config.network);
    wifi.startAP();

    static HttpServer http_server(config.info);
    http_server.start();

    DS18B20SensorManager::init(GPIO_NUM_4);
}