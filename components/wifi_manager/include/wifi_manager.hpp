#pragma once

#include <cstring>
#include <mutex>

#include "config_manager.hpp"
#include "esp_event.h"
#include "esp_wifi.h"

/**
 * @brief WiFiManager handles ESP32 Wi-Fi AP mode reactively.
 *        Subscribes to ConfigManager for network updates.
 */
class WiFiManager {
   public:
    WiFiManager();

    /** Initialize ESP Wi-Fi stack (must call before starting AP) */
    void init();

    /** Start the access point with current config */
    void startAP();

    /** Stop the access point */
    void stopAP();

    /** Log the current AP IP address */
    void logApIp();

    /** Apply new network configuration from ConfigManager */
    void applyNetworkConfig(const NetworkConfig& netConfig);

   private:
    bool _ap_running{false};                  ///< Is AP currently running
    char _current_ap_ssid[SSID_MAX_LEN];      ///< Current AP SSID
    char _current_ap_pass[PASSWORD_MAX_LEN];  ///< Current AP password
    std::mutex _mutex;                        ///< Mutex for thread safety

    static void onWiFiEvent(void* arg, esp_event_base_t event_base, int32_t event_id,
                            void* event_data);
};
