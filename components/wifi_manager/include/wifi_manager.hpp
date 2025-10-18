#pragma once

#include <cstring>

#include "config_manager.hpp"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/semphr.h"
#include "utils/lock_guard.hpp"

/**
 * @brief WiFiManager handles ESP32 Wi-Fi AP & STA modes reactively.
 *        Subscribes to ConfigManager for network updates.
 */
class WiFiManager {
   public:
    WiFiManager();
    ~WiFiManager();

    void init();

    // AP controls
    void startAP();
    void stopAP();
    void logApIp();

    // STA controls
    void startSTA();
    void stopSTA();
    void connectSTA();
    void logStaIp();

    // Apply new config reactively
    void updateNetworkConfig(const NetworkConfig& netConfig);

   private:
    // current applied settings (copies for change detection)
    bool _ap_running{false};
    bool _sta_running{false};

    char _current_ap_ssid[SSID_MAX_LEN];
    char _current_ap_pass[PASSWORD_MAX_LEN];
    char _current_sta_ssid[SSID_MAX_LEN];

    SemaphoreHandle_t _mutex{nullptr};

    // optional: keep netif handles for logging
    esp_netif_t* _ap_netif{nullptr};
    esp_netif_t* _sta_netif{nullptr};

    // decide and set wifi mode based on desired AP/STA enable flags
    void setModeFromFlags(bool want_ap, bool want_sta);

    static void onWiFiEvent(void* arg, esp_event_base_t event_base, int32_t event_id,
                            void* event_data);
};
