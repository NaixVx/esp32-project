#pragma once

#include <cstdint>

#include "driver/gpio.h"

class DS18B20 {
   public:
    explicit DS18B20(gpio_num_t pin);

    bool init();
    float readTemperature();

   private:
    gpio_num_t _pin;

    void writeBit(bool bit);
    bool readBit();
    void writeByte(uint8_t byte);
    uint8_t readByte();
    bool resetPulse();
};
