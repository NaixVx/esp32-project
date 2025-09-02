#include "wifi_manager.hpp"

#include "esp_log.h"
#include "esp_mac.h"

static const char* TAG = "wifi_manager";

WiFiManager::WiFiManager() {
    std::memset(_current_ap_ssid, 0, sizeof(_current_ap_ssid));
    std::memset(_current_ap_pass, 0, sizeof(_current_ap_pass));

    _mutex = xSemaphoreCreateMutex();
    if (!_mutex) {
        ESP_LOGE(TAG, "Failed to create WiFiManager mutex!");
        abort();
    }
}

WiFiManager::~WiFiManager() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
    }
}

// ---------------- Wi-Fi Init ----------------
void WiFiManager::init() {
    static bool wifi_initialized = false;

    {
        LockGuard guard(_mutex);
        if (wifi_initialized) return;

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_ap();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   &WiFiManager::onWiFiEvent, nullptr));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

        ConfigManager::getInstance().registerNetworkObserver(
            [this](const NetworkConfig& netConfig) { this->updateNetworkConfig(netConfig); });

        wifi_initialized = true;
    }

    // Apply current config at startup (outside lock)
    updateNetworkConfig(ConfigManager::getInstance().getNetworkConfig());
    ESP_LOGI(TAG, "Wi-Fi stack initialized and observer registered");
}

// ---------------- Start AP ----------------
void WiFiManager::startAP() {
    char ap_ssid[SSID_MAX_LEN];
    char ap_pass[PASSWORD_MAX_LEN];

    {
        LockGuard guard(_mutex);
        if (_ap_running) return;

        std::strncpy(ap_ssid, _current_ap_ssid, sizeof(ap_ssid));
        std::strncpy(ap_pass, _current_ap_pass, sizeof(ap_pass));
        _ap_running = true;
    }

    ESP_LOGI(TAG, "Starting Wi-Fi AP...");
    wifi_config_t ap_config = {};
    strncpy((char*)ap_config.ap.ssid, ap_ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = strlen(ap_ssid);
    strncpy((char*)ap_config.ap.password, ap_pass, sizeof(ap_config.ap.password));
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = strlen(ap_pass) > 0 ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started: SSID=%s", ap_ssid);
    logApIp();
}

// ---------------- Stop AP ----------------
void WiFiManager::stopAP() {
    bool was_running = false;

    {
        LockGuard guard(_mutex);
        was_running = _ap_running;
        _ap_running = false;
    }

    if (was_running) {
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_LOGI(TAG, "AP stopped");
    }
}

// ---------------- Apply network config ----------------
void WiFiManager::updateNetworkConfig(const NetworkConfig& netConfig) {
    bool ssid_changed = false;
    bool pass_changed = false;

    {
        LockGuard guard(_mutex);
        ssid_changed = strcmp(_current_ap_ssid, netConfig.ap_ssid) != 0;
        pass_changed = strcmp(_current_ap_pass, netConfig.ap_password) != 0;

        if (ssid_changed || pass_changed) {
            std::strncpy(_current_ap_ssid, netConfig.ap_ssid, sizeof(_current_ap_ssid));
            std::strncpy(_current_ap_pass, netConfig.ap_password, sizeof(_current_ap_pass));
        }
    }

    // Restart AP outside lock
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
