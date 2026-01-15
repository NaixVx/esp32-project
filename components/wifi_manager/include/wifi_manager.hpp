#pragma once

#include "config_manager.hpp"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/semphr.h"
#include "utils/lock_guard.hpp"

class WiFiManager
{
  public:
    WiFiManager();
    ~WiFiManager();

    void init();
    void syncWithConfig();

  private:
    esp_netif_t* ap_netif_{nullptr};

    SemaphoreHandle_t mutex_{nullptr};

    bool ap_running_{false};

    static void
    onWiFiEvent(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    void startAP(const NetworkPrivateConfig& cfg);
    void stopAP();

    void updatePublicState(bool ap_active, const char* ap_ssid);
};
