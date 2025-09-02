#pragma once

#include <cstring>

#include "config_manager.hpp"
#include "esp_event.h"
#include "esp_wifi.h"
#include "freertos/semphr.h"
#include "utils/lock_guard.hpp"

/**
 * @brief WiFiManager handles ESP32 Wi-Fi AP mode reactively.
 *        Subscribes to ConfigManager for network updates.
 */
class WiFiManager {
   public:
    WiFiManager();
    ~WiFiManager();

    void init();
    void startAP();
    void stopAP();
    void logApIp();
    void updateNetworkConfig(const NetworkConfig& netConfig);

   private:
    bool _ap_running{false};
    char _current_ap_ssid[SSID_MAX_LEN];
    char _current_ap_pass[PASSWORD_MAX_LEN];
    SemaphoreHandle_t _mutex{nullptr};

    static void onWiFiEvent(void* arg, esp_event_base_t event_base, int32_t event_id,
                            void* event_data);
};
