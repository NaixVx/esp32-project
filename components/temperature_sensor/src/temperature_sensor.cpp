#include "temperature_sensor.hpp"

#include "driver/ds18b20.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include "utils/lock_guard.hpp"

static const char* TAG = "temperature_sensor";

namespace temperature_sensor
{

static DS18B20 sensor(gpio_num_t::GPIO_NUM_NC);
static bool initialized = false;

static float last_temperature_C = 0.0f;
static status_t sensor_status = status_t::UNINITIALIZED;

static SemaphoreHandle_t data_mutex = nullptr;
static TaskHandle_t sensor_task_handle = nullptr;

static void sensor_task(void*);

void init(gpio_num_t data_pin)
{
    if (initialized) {
        return;
    }

    data_mutex = xSemaphoreCreateMutex();
    if (data_mutex == nullptr) {
        ESP_LOGE(TAG, "Mutex creation failed");
        return;
    }

    sensor = DS18B20(data_pin);

    if (!sensor.init()) {
        ESP_LOGE(TAG, "DS18B20 init failed");
        sensor_status = status_t::ERROR;
    }

    if (xTaskCreate(sensor_task,
            "ds18b20_task",
            4096,
            nullptr,
            tskIDLE_PRIORITY + 1,
            &sensor_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Task creation failed");
        sensor_status = status_t::ERROR;
        return;
    }

    ESP_LOGI(TAG, "Initialized");
    initialized = true;
}

float get_last_temperature_C(void)
{
    LockGuard lock(data_mutex);
    return last_temperature_C;
}

status_t get_status(void)
{
    LockGuard lock(data_mutex);
    return sensor_status;
}

static void sensor_task(void*)
{
    vTaskDelay(pdMS_TO_TICKS(1000));

    for (;;) {
        const float temp_C = sensor.readTemperature();
        const bool valid = (temp_C > -55.0f) && (temp_C < 125.0f);

        {
            LockGuard lock(data_mutex);
            last_temperature_C = temp_C;
            sensor_status = valid ? status_t::OK : status_t::ERROR;
        }

        if (valid) {
            ESP_LOGI(TAG, "Temperature: %.2f C", temp_C);
        } else {
            ESP_LOGW(TAG, "Invalid temperature reading: %.2f", temp_C);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

} // namespace temperature_sensor
