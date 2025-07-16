#include <stdio.h>

#include "DS18B20.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main() {
    static DS18B20 sensor(GPIO_NUM_4);

    xTaskCreate(
        [](void*) {
            while (true) {
                if (sensor.init()) {
                    float temp = sensor.readTemperature();
                    printf("Temperature: %.2f°C\n", temp);
                } else {
                    printf("DS18B20 not found!\n");
                }
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        },
        "ds18b20_task", 4096, nullptr, 1, nullptr);
}