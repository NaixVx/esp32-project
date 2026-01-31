// temperature_sensor.cpp
#include "temperature_sensor.hpp"
#include "driver/ds18b20.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <mutex>

static const char* TAG = "temperature_sensor";

namespace temperature_sensor
{

static DS18B20* sensor = nullptr;
static bool initialized = false;

static float last_temperature = 0.0f;
static bool sensor_ok = false;

static std::mutex mutex;

static void sensorTask(void*);

void init(gpio_num_t pin)
{
    if (initialized) {
        return;
    }

    static DS18B20 static_sensor(pin);
    sensor = &static_sensor;

    if (!sensor->init()) {
        ESP_LOGE(TAG, "DS18B20 init failed");
    }

    xTaskCreate(sensorTask, "ds18b20_task", 4096, nullptr, 2, nullptr);
    initialized = true;
}

float getLastTemperature()
{
    std::lock_guard<std::mutex> lock(mutex);
    return last_temperature;
}

bool getStatus()
{
    std::lock_guard<std::mutex> lock(mutex);
    return sensor_ok;
}

static void sensorTask(void*)
{
    // Allow sensor power-up and first conversion
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (true) {
        float temp = sensor->readTemperature();

        bool ok = (temp > -55.0f && temp < 125.0f);

        if (ok) {
            ESP_LOGI(TAG, "Temperature: %.2f C", temp);
        } else {
            ESP_LOGW(TAG, "Invalid temperature reading: %.2f", temp);
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            last_temperature = temp;
            sensor_ok = ok;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

} // namespace temperature_sensor
