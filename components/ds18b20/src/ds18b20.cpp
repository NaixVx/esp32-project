#include "ds18b20.hpp"

#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

DS18B20::DS18B20(gpio_num_t pin) : _pin(pin) {
    gpio_set_direction(_pin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_level(_pin, 1);
}

bool DS18B20::resetPulse() {
    gpio_set_level(_pin, 0);
    esp_rom_delay_us(480);
    gpio_set_level(_pin, 1);
    esp_rom_delay_us(70);
    bool presence = !gpio_get_level(_pin);
    esp_rom_delay_us(410);
    return presence;
}

void DS18B20::writeBit(bool bit) {
    gpio_set_level(_pin, 0);
    esp_rom_delay_us(bit ? 6 : 60);
    gpio_set_level(_pin, 1);
    esp_rom_delay_us(bit ? 64 : 10);
}

bool DS18B20::readBit() {
    gpio_set_level(_pin, 0);
    esp_rom_delay_us(6);
    gpio_set_level(_pin, 1);
    esp_rom_delay_us(9);
    bool bit = gpio_get_level(_pin);
    esp_rom_delay_us(55);
    return bit;
}

void DS18B20::writeByte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        writeBit(byte & 0x01);
        byte >>= 1;
    }
}

uint8_t DS18B20::readByte() {
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        if (readBit()) byte |= (1 << i);
    }
    return byte;
}

bool DS18B20::init() {
    return resetPulse();
}

float DS18B20::readTemperature() {
    if (!resetPulse()) return -1000.0f;

    writeByte(0xCC);  // Skip ROM
    writeByte(0x44);  // Convert T

    vTaskDelay(pdMS_TO_TICKS(750));  // Wait for conversion

    if (!resetPulse()) return -1000.0f;

    writeByte(0xCC);  // Skip ROM
    writeByte(0xBE);  // Read Scratchpad

    uint8_t lsb = readByte();
    uint8_t msb = readByte();

    int16_t raw = (msb << 8) | lsb;
    return raw / 16.0f;
}
