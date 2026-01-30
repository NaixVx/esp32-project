#include "temperature_sensor.hpp"
#include "driver/ds18b20.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace temperature_sensor
{

static DS18B20* sensor = nullptr;
static gpio_num_t sensor_pin;
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

    sensor_pin = pin;
    static DS18B20 static_sensor(pin);
    sensor = &static_sensor;

    xTaskCreate(sensorTask, "ds18b20_task", 4096, nullptr, 1, nullptr);
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
    while (true) {
        float temp = 0.0f;
        bool ok = false;

        if (sensor && sensor->init()) {
            temp = sensor->readTemperature();
            ok = (temp > -100.0f);
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
