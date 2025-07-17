#pragma once

#include <cstring>

#include "esp_event.h"
#include "esp_wifi.h"

class WiFiManager {
   public:
    WiFiManager(const char* ssid, const char* password, int maxConnections = 4);
    void startAP();

   private:
    const char* _ssid;
    const char* _password;
    int _maxConnections;

    static void onWiFiEvent(void* arg, esp_event_base_t event_base, int32_t event_id,
                            void* event_data);
};
