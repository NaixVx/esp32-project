#include <stdio.h>

#include "DS18B20.hpp"
#include "WiFiManager.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

extern "C" void app_main() {
    static DS18B20 ds18b20_sensor(GPIO_NUM_4);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    static WiFiManager wifi("ESP32-Project", "TestPassword");
    wifi.startAP();

    xTaskCreate(
        [](void*) {
            while (true) {
                if (ds18b20_sensor.init()) {
                    float temp = ds18b20_sensor.readTemperature();
                    printf("Temperature: %.2f°C\n", temp);
                } else {
                    printf("DS18B20 not found!\n");
                }
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        },
        "ds18b20_task", 4096, nullptr, 1, nullptr);
}