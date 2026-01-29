#include "factory_reset.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "config_manager.hpp"

static const char* TAG = "factory_reset";

/* --- factory reset timing --- */
static constexpr uint32_t STARTUP_WINDOW_S = 30;
static constexpr uint32_t FIRST_PRESS_S = 5;
static constexpr uint32_t SECOND_PRESS_S = 10;
static constexpr uint32_t THIRD_PRESS_S = 5;

esp_err_t FactoryReset::init(gpio_num_t button_pin)
{
    ESP_LOGI(TAG, "Initializing Factory Reset");
    if (button_pin == GPIO_NUM_NC) {
        ESP_LOGE(TAG, "Invalid button pin");
        return ESP_ERR_INVALID_ARG;
    }

    m_button_pin = button_pin;

    gpio_config_t cfg{};
    cfg.pin_bit_mask = 1ULL << m_button_pin;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;

    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Factory reset armed (startup window %u s)", STARTUP_WINDOW_S);

    xTaskCreate(taskEntry, "factory_reset", 4096, this, 5, nullptr);

    return ESP_OK;
}

void FactoryReset::taskEntry(void* arg)
{
    auto* self = static_cast<FactoryReset*>(arg);
    const int64_t start_us = esp_timer_get_time();

    ESP_LOGI(TAG, "Factory reset task started");

    while ((esp_timer_get_time() - start_us) < (STARTUP_WINDOW_S * 1000000LL)) {

        ESP_LOGD(TAG, "Waiting for first press (%u s)", FIRST_PRESS_S);
        if (!self->waitForPress_s(FIRST_PRESS_S)) {
            continue;
        }

        ESP_LOGD(TAG, "Waiting for second press (%u s)", SECOND_PRESS_S);
        if (!self->waitForPress_s(SECOND_PRESS_S)) {
            continue;
        }

        ESP_LOGD(TAG, "Waiting for third press (%u s)", THIRD_PRESS_S);
        if (!self->waitForPress_s(THIRD_PRESS_S)) {
            continue;
        }

        ESP_LOGI(TAG, "Factory reset sequence confirmed");
        ConfigManager::getInstance().resetToDefaults();
        break;
    }

    ESP_LOGD(TAG, "Factory reset task exiting");
    vTaskDelete(nullptr);
}

bool FactoryReset::waitForPress_s(uint32_t duration_s) const
{
    int64_t pressed_since_us = 0;

    while (true) {
        if (gpio_get_level(m_button_pin) == 0) {
            if (pressed_since_us == 0) {
                pressed_since_us = esp_timer_get_time();
                ESP_LOGI(TAG, "Button pressed");
            }

            if ((esp_timer_get_time() - pressed_since_us) >= duration_s * 1000000LL) {
                ESP_LOGI(TAG, "Button held for %u s", duration_s);
                return true;
            }
        } else {
            if (pressed_since_us != 0) {
                ESP_LOGI(TAG, "Button released early");
            }
            pressed_since_us = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
