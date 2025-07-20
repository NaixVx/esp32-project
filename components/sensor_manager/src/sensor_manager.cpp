#include "sensor_manager.hpp"

#include "ds18b20.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static DS18B20* ds18b20_sensor = nullptr;

float DS18B20SensorManager::last_temperature_ = 0.0f;
bool DS18B20SensorManager::sensor_ok_ = false;
std::mutex DS18B20SensorManager::mutex_;

void DS18B20SensorManager::init(gpio_num_t pin) {
    ds18b20_sensor = new DS18B20(pin);
    xTaskCreate(sensorTask, "ds18b20_sensor_task", 4096, nullptr, 1, nullptr);
}

float DS18B20SensorManager::getLastTemperature() {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_temperature_;
}

bool DS18B20SensorManager::getSensorStatus() {
    std::lock_guard<std::mutex> lock(mutex_);
    return sensor_ok_;
}

void DS18B20SensorManager::sensorTask(void* arg) {
    while (true) {
        float temp_val = 0.0f;
        bool ok = false;

        if (ds18b20_sensor && ds18b20_sensor->init()) {
            temp_val = ds18b20_sensor->readTemperature();
            ok = true;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            last_temperature_ = temp_val;
            sensor_ok_ = ok;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
