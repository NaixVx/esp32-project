#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

// TODO: add power sequence reset mode
enum class FactoryResetMode {
    BUTTON_SEQUENCE
};

class FactoryReset
{
  public:
    esp_err_t init(gpio_num_t button_pin);

  private:
    static void taskEntry(void* arg);
    bool waitForPress_s(uint32_t duration_s) const;

    gpio_num_t m_button_pin{GPIO_NUM_NC};
};
