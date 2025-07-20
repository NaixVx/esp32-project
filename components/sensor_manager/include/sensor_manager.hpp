#pragma once

#include <mutex>

#include "driver/gpio.h"

class DS18B20SensorManager {
   public:
    static void init(gpio_num_t pin);

    static float getLastTemperature();

    static bool getSensorStatus();

   private:
    static void sensorTask(void* arg);

    static float last_temperature_;
    static bool sensor_ok_;
    static std::mutex mutex_;
};
