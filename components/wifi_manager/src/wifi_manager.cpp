#include "wifi_manager.hpp"
#include "config_manager.hpp"
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

WiFiManager& WiFiManager::getInstance()
{
    static WiFiManager instance;
    return instance;
}

void WiFiManager::init()
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(mac_address_,
        sizeof(mac_address_),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ap_netif_ = esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiManager::onWiFiEvent, this));

    ConfigManager::getInstance().registerNetworkObserver(
        [this](const NetworkConfig&) { this->scheduleReconfigureAP(); });

    syncWithConfig();

    ESP_LOGI(TAG, "Wi-Fi config initialized");
}

const char* WiFiManager::getMacAddress() const
{
    return mac_address_;
}

const char* WiFiManager::getApIp() const
{
    LockGuard guard(mutex_);
    return ap_ip_;
}

void WiFiManager::startAP(const NetworkConfigAP& ap)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_start());

    applyConfigAP(ap);

    ESP_LOGI(TAG, "AP started (SSID=%s)", ap.ssid);
}

void WiFiManager::stopAP()
{
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));

    ap_running_ = false;

    ESP_LOGI(TAG, "AP stopped");
}

void WiFiManager::applyConfigAP(const NetworkConfigAP& ap)
{
    wifi_config_t wifi_cfg{};
    std::strncpy(reinterpret_cast<char*>(wifi_cfg.ap.ssid), ap.ssid, sizeof(wifi_cfg.ap.ssid) - 1);

    std::strncpy(reinterpret_cast<char*>(wifi_cfg.ap.password),
        ap.password,
        sizeof(wifi_cfg.ap.password) - 1);

    wifi_cfg.ap.ssid_len = std::strlen(reinterpret_cast<char*>(wifi_cfg.ap.ssid));
    wifi_cfg.ap.max_connection = 4;
    wifi_cfg.ap.channel = 1;
    wifi_cfg.ap.authmode = std::strlen(ap.password) > 0 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));

    ESP_LOGI(TAG, "AP reconfigured (SSID=%s)", ap.ssid);
}

void WiFiManager::scheduleReconfigureAP()
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
    NetworkConfig net = ConfigManager::getInstance().getNetworkConfig();

    LockGuard guard(mutex_);

    if (!net.ap.enabled) {
        if (ap_running_) {
            stopAP();
        }
        return;
    }

    // AP enabled
    if (!ap_running_) {
        startAP(net.ap);
        ap_running_ = true;
        return;
    }

    // AP already running → update config in place
    applyConfigAP(net.ap);
}

void WiFiManager::onWiFiEvent(void* arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void* event_data)
{
    auto self = static_cast<WiFiManager*>(arg);

    if (event_base == WIFI_EVENT) {
        switch (event_id) {

        case WIFI_EVENT_AP_START: {
            ESP_LOGI(TAG, "AP interface started");

            LockGuard guard(self->mutex_);

            if (self->ap_netif_) {
                esp_netif_ip_info_t ip;
                if (esp_netif_get_ip_info(self->ap_netif_, &ip) == ESP_OK) {
                    snprintf(self->ap_ip_, sizeof(self->ap_ip_), IPSTR, IP2STR(&ip.ip));
                }
            }
            break;
        }

        case WIFI_EVENT_AP_STOP: {
            ESP_LOGI(TAG, "AP interface stopped");

            LockGuard guard(self->mutex_);
            std::strcpy(self->ap_ip_, "null");

            break;
        }

        default:
            break;
        }
    }
}
