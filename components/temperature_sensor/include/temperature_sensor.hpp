#pragma once

#include "driver/gpio.h"

namespace temperature_sensor {

enum class status_t { UNINITIALIZED, OK, ERROR };

void init(gpio_num_t data_pin);

float get_last_temperature_C(void);
int8_t get_status(void);

} // namespace temperature_sensor
