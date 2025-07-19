#include "wifi_manager.hpp"

#include <cstring>

#include "esp_log.h"
#include "esp_mac.h"

static const char* TAG = "wifi_manager";

WiFiManager::WiFiManager(const NetworkConfig& config) : _config(config) {}

void WiFiManager::startAP() {
    ESP_LOGI(TAG, "Starting Wi-Fi in AP mode...");

    static bool wifi_initialized = false;
    if (!wifi_initialized) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_ap();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        // Register event handler
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   &WiFiManager::onWiFiEvent, nullptr));

        wifi_initialized = true;
    }

    // Configure access point
    wifi_config_t ap_config = {};
    strncpy((char*)ap_config.ap.ssid, _config.ap_ssid, sizeof(ap_config.ap.ssid));
    strncpy((char*)ap_config.ap.password, _config.ap_password, sizeof(ap_config.ap.password));
    ap_config.ap.ssid_len = strlen(_config.ap_ssid);
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    if (strlen(_config.ap_password) == 0) {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Access Point started. SSID: %s", _config.ap_ssid);
}

void WiFiManager::onWiFiEvent(void* arg, esp_event_base_t event_base, int32_t event_id,
                              void* event_data) {
    switch (event_id) {
        case WIFI_EVENT_AP_STACONNECTED: {
            auto* event = static_cast<wifi_event_ap_staconnected_t*>(event_data);
            ESP_LOGI(TAG, "Device connected: MAC=" MACSTR ", AID=%d", MAC2STR(event->mac),
                     event->aid);
            break;
        }
        case WIFI_EVENT_AP_STADISCONNECTED: {
            auto* event = static_cast<wifi_event_ap_stadisconnected_t*>(event_data);
            ESP_LOGI(TAG, "Device disconnected: MAC=" MACSTR ", AID=%d", MAC2STR(event->mac),
                     event->aid);
            break;
        }
        default:
            break;
    }
}
