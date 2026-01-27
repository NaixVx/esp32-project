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
    static WiFiManager& getInstance();

    WiFiManager();
    ~WiFiManager();

    void init();
    void syncWithConfig();

    const char* getMacAddress() const;
    const char* getApIp() const;
    const char* getStaIp() const;

  private:
    esp_netif_t* ap_netif_{nullptr};

    SemaphoreHandle_t mutex_{nullptr};

    char mac_address_[MAC_ADDR_LEN]{};

    bool ap_running_{false};
    char ap_ip_[IP_ADDR_LEN]{"0.0.0.0"};

    esp_netif_t* sta_netif_{nullptr};
    bool sta_running_{false};
    char sta_ip_[IP_ADDR_LEN]{"0.0.0.0"};

    static void
    onWiFiEvent(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    void startAP(const NetworkConfigAP& ap);
    void stopAP();
    void applyConfigAP(const NetworkConfigAP& ap);
    void scheduleReconfigureAP();

    void startSTA(const NetworkConfigSTA& sta);
    void stopSTA();
    void applyConfigSTA(const NetworkConfigSTA& sta);
};
