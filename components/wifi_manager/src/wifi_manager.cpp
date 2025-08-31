#include "wifi_manager.hpp"

#include "esp_log.h"
#include "esp_mac.h"

static const char* TAG = "wifi_manager";

WiFiManager::WiFiManager() {
    std::memset(_current_ap_ssid, 0, sizeof(_current_ap_ssid));
    std::memset(_current_ap_pass, 0, sizeof(_current_ap_pass));
}

void WiFiManager::init() {
    std::lock_guard<std::mutex> lock(_mutex);
    static bool wifi_initialized = false;
    if (wifi_initialized) return;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &WiFiManager::onWiFiEvent, nullptr));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_initialized = true;
    ESP_LOGI(TAG, "Wi-Fi stack initialized");
}

// ---------------- Start AP ----------------
void WiFiManager::startAP() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_ap_running) return;

    ESP_LOGI(TAG, "Starting Wi-Fi AP...");

    wifi_config_t ap_config = {};
    strncpy((char*)ap_config.ap.ssid, _current_ap_ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid[sizeof(ap_config.ap.ssid) - 1] = '\0';

    strncpy((char*)ap_config.ap.password, _current_ap_pass, sizeof(ap_config.ap.password));
    ap_config.ap.password[sizeof(ap_config.ap.password) - 1] = '\0';

    ap_config.ap.ssid_len = strlen(_current_ap_ssid);
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = strlen(_current_ap_pass) > 0 ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    _ap_running = true;
    ESP_LOGI(TAG, "AP started: SSID=%s", _current_ap_ssid);
    logApIp();
}

// ---------------- Stop AP ----------------
void WiFiManager::stopAP() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_ap_running) return;

    ESP_ERROR_CHECK(esp_wifi_stop());
    _ap_running = false;
    ESP_LOGI(TAG, "AP stopped");
}

// ---------------- Apply network config ----------------
void WiFiManager::applyNetworkConfig(const NetworkConfig& netConfig) {
    bool ssid_changed = false;
    bool pass_changed = false;

    // Copy new config under lock
    {
        std::lock_guard<std::mutex> lock(_mutex);

        ssid_changed = strcmp(_current_ap_ssid, netConfig.ap_ssid) != 0;
        pass_changed = strcmp(_current_ap_pass, netConfig.ap_password) != 0;

        if (ssid_changed || pass_changed) {
            ESP_LOGI(TAG, "Network config changed, updating AP...");

            strncpy(_current_ap_ssid, netConfig.ap_ssid, sizeof(_current_ap_ssid));
            _current_ap_ssid[sizeof(_current_ap_ssid) - 1] = '\0';

            strncpy(_current_ap_pass, netConfig.ap_password, sizeof(_current_ap_pass));
            _current_ap_pass[sizeof(_current_ap_pass) - 1] = '\0';
        }
    }

    // Restart AP outside lock to avoid deadlock
    if (ssid_changed || pass_changed) {
        if (_ap_running) stopAP();
        startAP();
    }
}

// ---------------- Log AP IP ----------------
void WiFiManager::logApIp() {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            ESP_LOGI(TAG, "AP IP address: " IPSTR, IP2STR(&ip_info.ip));
        }
    }
}

// ---------------- Wi-Fi Event Handler ----------------
void WiFiManager::onWiFiEvent(void* arg, esp_event_base_t event_base, int32_t event_id,
                              void* event_data) {
    switch (event_id) {
        case WIFI_EVENT_AP_STACONNECTED: {
            auto* e = static_cast<wifi_event_ap_staconnected_t*>(event_data);
            ESP_LOGI(TAG, "Device connected: MAC=" MACSTR ", AID=%d", MAC2STR(e->mac), e->aid);
            break;
        }
        case WIFI_EVENT_AP_STADISCONNECTED: {
            auto* e = static_cast<wifi_event_ap_stadisconnected_t*>(event_data);
            ESP_LOGI(TAG, "Device disconnected: MAC=" MACSTR ", AID=%d", MAC2STR(e->mac), e->aid);
            break;
        }
        default:
            break;
    }
}
