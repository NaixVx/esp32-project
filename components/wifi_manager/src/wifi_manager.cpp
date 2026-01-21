#include "wifi_manager.hpp"


#include <cstring>


#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"


static const char* TAG = "wifi_manager";


WiFiManager::WiFiManager()
{
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ESP_LOGE(TAG, "Failed to create mutex");
        abort();
    }
}


WiFiManager::~WiFiManager()
{
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}


void WiFiManager::init()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());


    ap_netif_ = esp_netif_create_default_wifi_ap();


    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));


    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiManager::onWiFiEvent, this));


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_start());


    // TODO: add some delay to be able to send http response before reconfiguration
    // like changing ap ssid or password, ap stops first
    ConfigManager::getInstance().registerNetworkPrivateObserver(
        [this](const NetworkPrivateConfig&) { this->scheduleApReconfigure(); });


    syncWithConfig();


    ESP_LOGI(TAG, "Wi-Fi initialized (AP mode)");
}


void WiFiManager::startAP(const NetworkPrivateConfig& cfg)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_start());


    applyApConfig(cfg);


    ESP_LOGI(TAG, "AP started (SSID=%s)", cfg.ap_ssid);
}


void WiFiManager::stopAP()
{
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));


    ap_running_ = false;
    updatePublicState(false, nullptr);


    ESP_LOGI(TAG, "AP stopped");
}


void WiFiManager::applyApConfig(const NetworkPrivateConfig& cfg)
{
    wifi_config_t ap_cfg{};
    std::strncpy(reinterpret_cast<char*>(ap_cfg.ap.ssid), cfg.ap_ssid, sizeof(ap_cfg.ap.ssid) - 1);


    std::strncpy(reinterpret_cast<char*>(ap_cfg.ap.password),
        cfg.ap_password,
        sizeof(ap_cfg.ap.password) - 1);


    ap_cfg.ap.ssid_len = std::strlen(reinterpret_cast<char*>(ap_cfg.ap.ssid));
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.authmode = std::strlen(cfg.ap_password) > 0 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;


    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));


    updatePublicState(true, cfg.ap_ssid);


    ESP_LOGI(TAG, "AP reconfigured (SSID=%s)", cfg.ap_ssid);
}


void WiFiManager::scheduleApReconfigure()
{
    static esp_timer_handle_t timer = nullptr;


    if (!timer) {
        esp_timer_create_args_t args{};
        args.callback = [](void* arg) {
            auto self = static_cast<WiFiManager*>(arg);
            self->syncWithConfig();
        };
        args.arg = this;
        args.dispatch_method = ESP_TIMER_TASK;
        args.name = "ap_reconf";
        args.skip_unhandled_events = false;


        ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
    }


    esp_timer_stop(timer);                                // debounce
    ESP_ERROR_CHECK(esp_timer_start_once(timer, 300000)); // 300 ms
}


void WiFiManager::syncWithConfig()
{
    NetworkPrivateConfig cfg = ConfigManager::getInstance().getNetworkPrivateConfig();


    LockGuard guard(mutex_);


    if (!cfg.ap_enabled) {
        if (ap_running_) {
            stopAP();
        }
        return;
    }


    // AP enabled
    if (!ap_running_) {
        startAP(cfg);
        return;
    }


    // AP already running → update config in place
    applyApConfig(cfg);
}


void WiFiManager::updatePublicState(bool ap_active, const char* ap_ssid)
{
    ConfigManager& cfgMgr = ConfigManager::getInstance();
    NetworkPublicConfig pub = cfgMgr.getNetworkPublicConfig();


    pub.ap_active = ap_active;


    if (ap_active && ap_ssid) {
        std::strncpy(pub.ap_ssid, ap_ssid, SSID_MAX_LEN - 1);
        pub.ap_ssid[SSID_MAX_LEN - 1] = '\0';
    } else {
        pub.ap_ssid[0] = '\0';
        pub.ap_ip[0] = '\0';
    }


    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(pub.mac_address,
        sizeof(pub.mac_address),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);


    if (ap_active && ap_netif_) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(ap_netif_, &ip) == ESP_OK) {
            snprintf(pub.ap_ip, sizeof(pub.ap_ip), IPSTR, IP2STR(&ip.ip));
        }
    }


    cfgMgr.updateNetworkPublicConfig(pub);
}


void WiFiManager::onWiFiEvent(void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "AP interface started");
            break;


        case WIFI_EVENT_AP_STOP:
            ESP_LOGI(TAG, "AP interface stopped");
            break;


        default:
            break;
        }
    }
}
