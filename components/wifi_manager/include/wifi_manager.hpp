#pragma once

#include <cstring>

#include "config_manager.hpp"
#include "esp_event.h"
#include "esp_wifi.h"

class WiFiManager {
   public:
    WiFiManager(const NetworkConfig& config);
    void startAP();
    void logApIp();

   private:
    NetworkConfig _config;

    static void onWiFiEvent(void* arg, esp_event_base_t event_base, int32_t event_id,
                            void* event_data);
};
