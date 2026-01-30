#pragma once

#include <mutex>
#include "driver/gpio.h"

namespace temperature_sensor
{

void init(gpio_num_t pin);
float getLastTemperature();
bool getStatus();

} // namespace temperature_sensor
