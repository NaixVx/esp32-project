#include "wifi_manager.hpp"

#include <cstring>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"

static const char* TAG = "wifi_manager";

WiFiManager::WiFiManager() {
    std::memset(_current_ap_ssid, 0, sizeof(_current_ap_ssid));
    std::memset(_current_ap_pass, 0, sizeof(_current_ap_pass));
    std::memset(_current_sta_ssid, 0, sizeof(_current_sta_ssid));

    _mutex = xSemaphoreCreateMutex();
    if (!_mutex) {
        ESP_LOGE(TAG, "Failed to create WiFiManager mutex!");
        abort();
    }
}

WiFiManager::~WiFiManager() {
    if (_mutex) vSemaphoreDelete(_mutex);
}

void WiFiManager::init() {
    static bool wifi_initialized = false;

    {
        LockGuard guard(_mutex);
        if (wifi_initialized) return;

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        // Create default netifs for both roles so we can switch to AP/STA/APSTA freely
        _ap_netif = esp_netif_create_default_wifi_ap();
        _sta_netif = esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   &WiFiManager::onWiFiEvent, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   &WiFiManager::onWiFiEvent, nullptr));

        wifi_initialized = true;
    }

    // Apply current config at startup (outside lock)
    updateNetworkConfig(ConfigManager::getInstance().getNetworkConfig());
    ESP_LOGI(TAG, "Wi-Fi stack initialized and observer registered");

    // subscribe after init so we don’t miss future changes
    ConfigManager::getInstance().registerNetworkObserver(
        [this](const NetworkConfig& netConfig) { this->updateNetworkConfig(netConfig); });
}

// ---------------- Mode selection ----------------
void WiFiManager::setModeFromFlags(bool want_ap, bool want_sta) {
    wifi_mode_t new_mode = WIFI_MODE_NULL;
    if (want_ap && want_sta)
        new_mode = WIFI_MODE_APSTA;
    else if (want_ap)
        new_mode = WIFI_MODE_AP;
    else if (want_sta)
        new_mode = WIFI_MODE_STA;
    else
        new_mode = WIFI_MODE_NULL;

    wifi_mode_t cur_mode;
    esp_err_t err = esp_wifi_get_mode(&cur_mode);
    if (err != ESP_OK || cur_mode != new_mode) {
        if (cur_mode != WIFI_MODE_NULL) {
            ESP_ERROR_CHECK(esp_wifi_stop());
        }
        ESP_ERROR_CHECK(esp_wifi_set_mode(new_mode));
        if (new_mode != WIFI_MODE_NULL) {
            ESP_ERROR_CHECK(esp_wifi_start());
        }
        ESP_LOGI(TAG, "Wi-Fi mode -> %s",
                 new_mode == WIFI_MODE_APSTA ? "AP+STA"
                 : new_mode == WIFI_MODE_AP  ? "AP"
                 : new_mode == WIFI_MODE_STA ? "STA"
                                             : "OFF");
    }
}

// ---------------- AP controls ----------------
void WiFiManager::startAP() {
    char ap_ssid[SSID_MAX_LEN];
    char ap_pass[PASSWORD_MAX_LEN];

    {
        LockGuard guard(_mutex);
        if (_ap_running) return;

        std::strncpy(ap_ssid, _current_ap_ssid, sizeof(ap_ssid));
        std::strncpy(ap_pass, _current_ap_pass, sizeof(ap_pass));
        ap_ssid[sizeof(ap_ssid) - 1] = '\0';
        ap_pass[sizeof(ap_pass) - 1] = '\0';
    }

    wifi_config_t ap_config = {};
    std::strncpy(reinterpret_cast<char*>(ap_config.ap.ssid), ap_ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid[sizeof(ap_config.ap.ssid) - 1] = '\0';
    ap_config.ap.ssid_len = std::strlen(reinterpret_cast<const char*>(ap_config.ap.ssid));

    std::strncpy(reinterpret_cast<char*>(ap_config.ap.password), ap_pass,
                 sizeof(ap_config.ap.password));
    ap_config.ap.password[sizeof(ap_config.ap.password) - 1] = '\0';

    ap_config.ap.max_connection = 4;
    ap_config.ap.channel = 1;  // or leave 0 for default
    ap_config.ap.authmode = std::strlen(ap_pass) > 0 ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN;
#if ESP_IDF_VERSION_MAJOR >= 5
    ap_config.ap.sae_pwe_h2e = WPA3_SAE_PWE_UNSPECIFIED;  // default; tweak if enabling WPA3
#endif

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    {
        LockGuard guard(_mutex);
        _ap_running = true;
    }

    ESP_LOGI(TAG, "AP started: SSID=%s", ap_ssid);
    logApIp();
}

void WiFiManager::stopAP() {
    bool was_running;
    {
        LockGuard guard(_mutex);
        was_running = _ap_running;
        _ap_running = false;
    }
    if (was_running) {
        // In APSTA, stopping AP means clearing its config; keep STA up
        wifi_mode_t mode;
        if (esp_wifi_get_mode(&mode) == ESP_OK && mode == WIFI_MODE_APSTA) {
            wifi_config_t blank{};
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &blank));
        } else {
            ESP_ERROR_CHECK(esp_wifi_stop());
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
        }
        ESP_LOGI(TAG, "AP stopped");
    }
}

void WiFiManager::logApIp() {
    esp_netif_t* netif = _ap_netif ? _ap_netif : esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
            ESP_LOGI(TAG, "AP IP address: " IPSTR, IP2STR(&ip.ip));
        }
    }
}

// ---------------- STA controls ----------------
void WiFiManager::startSTA() {
    char sta_ssid[SSID_MAX_LEN];

    {
        LockGuard guard(_mutex);
        if (_sta_running) return;
        std::strncpy(sta_ssid, _current_sta_ssid, sizeof(sta_ssid));
        sta_ssid[sizeof(sta_ssid) - 1] = '\0';
    }

    wifi_config_t sta_cfg = {};
    std::strncpy(reinterpret_cast<char*>(sta_cfg.sta.ssid), sta_ssid, sizeof(sta_cfg.sta.ssid));
    sta_cfg.sta.ssid[sizeof(sta_cfg.sta.ssid) - 1] = '\0';

    // Note: password comes from system Wi-Fi settings if WPS/other; since your config doesn’t
    // store STA password, we connect open or credentialless here. If you later add it, set here:
    // std::strncpy((char*)sta_cfg.sta.password, password, sizeof(sta_cfg.sta.password));

    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;  // allow WPA2/WPA3; adjust as needed
    sta_cfg.sta.pmf_cfg.capable = true;
    sta_cfg.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));

    {
        LockGuard guard(_mutex);
        _sta_running = true;
    }

    connectSTA();
}

void WiFiManager::stopSTA() {
    bool was_running;
    {
        LockGuard guard(_mutex);
        was_running = _sta_running;
        _sta_running = false;
    }
    if (was_running) {
        // In APSTA, stopping STA means clearing its config; keep AP up
        wifi_mode_t mode;
        if (esp_wifi_get_mode(&mode) == ESP_OK && mode == WIFI_MODE_APSTA) {
            wifi_config_t blank{};
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &blank));
        } else {
            ESP_ERROR_CHECK(esp_wifi_disconnect());
            ESP_ERROR_CHECK(esp_wifi_stop());
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
        }
        ESP_LOGI(TAG, "STA stopped");
    }
}

void WiFiManager::connectSTA() {
    // safe to call repeatedly; Wi-Fi lib handles state
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "STA connecting...");
    } else if (err != ESP_ERR_WIFI_CONN) {
        ESP_LOGW(TAG, "esp_wifi_connect returned %s", esp_err_to_name(err));
    }
}

void WiFiManager::logStaIp() {
    esp_netif_t* netif = _sta_netif ? _sta_netif : esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
            ESP_LOGI(TAG, "STA IP address: " IPSTR, IP2STR(&ip.ip));
        }
    }
}

// ---------------- Apply network config ----------------
void WiFiManager::updateNetworkConfig(const NetworkConfig& netConfig) {
    bool want_ap = netConfig.ap_enabled != 0;
    bool want_sta = netConfig.sta_enabled != 0;

    bool ap_ssid_changed = false;
    bool ap_pass_changed = false;
    bool sta_ssid_changed = false;

    {
        LockGuard guard(_mutex);
        ap_ssid_changed = std::strcmp(_current_ap_ssid, netConfig.ap_ssid) != 0;
        ap_pass_changed = std::strcmp(_current_ap_pass, netConfig.ap_password) != 0;
        sta_ssid_changed = std::strcmp(_current_sta_ssid, netConfig.ssid) != 0;

        if (ap_ssid_changed) {
            std::strncpy(_current_ap_ssid, netConfig.ap_ssid, sizeof(_current_ap_ssid));
            _current_ap_ssid[sizeof(_current_ap_ssid) - 1] = '\0';
        }
        if (ap_pass_changed) {
            std::strncpy(_current_ap_pass, netConfig.ap_password, sizeof(_current_ap_pass));
            _current_ap_pass[sizeof(_current_ap_pass) - 1] = '\0';
        }
        if (sta_ssid_changed) {
            std::strncpy(_current_sta_ssid, netConfig.ssid, sizeof(_current_sta_ssid));
            _current_sta_ssid[sizeof(_current_sta_ssid) - 1] = '\0';
        }
    }

    // Select mode first (this may start/stop Wi-Fi)
    setModeFromFlags(want_ap, want_sta);

    // Apply AP config if needed
    if (want_ap) {
        if (!_ap_running || ap_ssid_changed || ap_pass_changed) {
            if (_ap_running) stopAP();
            startAP();
        }
    } else if (_ap_running) {
        stopAP();
    }

    // Apply STA config if needed
    if (want_sta) {
        if (!_sta_running || sta_ssid_changed) {
            if (_sta_running) stopSTA();
            startSTA();
        } else {
            connectSTA();  // ensure connected after any mode switch
        }
    } else if (_sta_running) {
        stopSTA();
    }
}

// ---------------- Wi-Fi Event Handler ----------------
void WiFiManager::onWiFiEvent(void* /*arg*/, esp_event_base_t event_base, int32_t event_id,
                              void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_STACONNECTED: {
                auto* e = static_cast<wifi_event_ap_staconnected_t*>(event_data);
                ESP_LOGI(TAG, "AP: device connected: MAC=" MACSTR ", AID=%d", MAC2STR(e->mac),
                         e->aid);
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                auto* e = static_cast<wifi_event_ap_stadisconnected_t*>(event_data);
                ESP_LOGI(TAG, "AP: device disconnected: MAC=" MACSTR ", AID=%d", MAC2STR(e->mac),
                         e->aid);
                break;
            }
            case WIFI_EVENT_STA_START: {
                ESP_LOGI(TAG, "STA started");
                break;
            }
            case WIFI_EVENT_STA_CONNECTED: {
                ESP_LOGI(TAG, "STA connected to AP");
                break;
            }
            case WIFI_EVENT_STA_DISCONNECTED: {
                auto* e = static_cast<wifi_event_sta_disconnected_t*>(event_data);
                ESP_LOGW(TAG, "STA disconnected (reason=%d), reconnecting...", e ? e->reason : -1);
                // simple auto-reconnect
                esp_wifi_connect();
                break;
            }
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            auto* e = static_cast<ip_event_got_ip_t*>(event_data);
            ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        }
    }
}
