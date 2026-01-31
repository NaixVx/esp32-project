#include "wifi_manager.hpp"
#include "config_manager.hpp"

#include <cstring>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"

static const char *TAG = "wifi_manager";

/* -------------------------------------------------------------------------- */
/* Construction / Lifetime */
/* -------------------------------------------------------------------------- */

WiFiManager::WiFiManager() {
  mutex_ = xSemaphoreCreateMutex();
  if (mutex_ == nullptr) {
    ESP_LOGE(TAG, "Mutex creation failed");
    abort();
  }
}

WiFiManager::~WiFiManager() {
  if (mutex_ != nullptr) {
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
  }
}

WiFiManager &WiFiManager::getInstance() {
  static WiFiManager instance;
  return instance;
}

/* -------------------------------------------------------------------------- */
/* Initialization */
/* -------------------------------------------------------------------------- */

void WiFiManager::init() {
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);

  snprintf(mac_address_, sizeof(mac_address_), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  ap_netif_ = esp_netif_create_default_wifi_ap();
  sta_netif_ = esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &WiFiManager::onWiFiEvent, this));

  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                             &WiFiManager::onWiFiEvent, this));

  ConfigManager::getInstance().registerNetworkObserver(
      [this](const NetworkConfig &) { scheduleConfigSync(); });

  syncWithConfig();

  ESP_LOGI(TAG, "Wi-Fi manager initialized");
}

/* -------------------------------------------------------------------------- */
/* Public Getters */
/* -------------------------------------------------------------------------- */

const char *WiFiManager::getMacAddress() const { return mac_address_; }

const char *WiFiManager::getApIp() const {
  LockGuard lock(mutex_);
  return ap_ip_;
}

const char *WiFiManager::getStaIp() const {
  LockGuard lock(mutex_);
  return sta_ip_;
}

/* -------------------------------------------------------------------------- */
/* Configuration Synchronization */
/* -------------------------------------------------------------------------- */

void WiFiManager::syncWithConfig() {
  NetworkConfig net = ConfigManager::getInstance().getNetworkConfig();

  const bool ap_enabled = net.ap.enabled;
  const bool sta_enabled = std::strlen(net.sta.ssid) > 0;

  LockGuard lock(mutex_);

  updateWifiMode(ap_enabled, sta_enabled);

  if (ap_enabled) {
    if (!ap_running_) {
      enableAp(net.ap);
    } else {
      configureAp(net.ap);
    }
  } else if (ap_running_) {
    disableAp();
  }

  if (sta_enabled) {
    if (!sta_running_) {
      enableSta(net.sta);
    } else {
      configureSta(net.sta);
    }
  } else if (sta_running_) {
    disableSta();
  }
}

void WiFiManager::scheduleConfigSync() {
  static esp_timer_handle_t timer = nullptr;

  if (timer == nullptr) {
    esp_timer_create_args_t args = {};
    args.callback = [](void *arg) {
      static_cast<WiFiManager *>(arg)->syncWithConfig();
    };
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "wifi_cfg_sync";

    ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
  }

  esp_timer_stop(timer);
  ESP_ERROR_CHECK(esp_timer_start_once(timer, 300000)); /* 300 ms */
}

/* -------------------------------------------------------------------------- */
/* Wi-Fi Mode Control */
/* -------------------------------------------------------------------------- */

void WiFiManager::updateWifiMode(bool ap_enabled, bool sta_enabled) {
  wifi_mode_t mode = WIFI_MODE_NULL;

  if (ap_enabled && sta_enabled) {
    mode = WIFI_MODE_APSTA;
  } else if (ap_enabled) {
    mode = WIFI_MODE_AP;
  } else if (sta_enabled) {
    mode = WIFI_MODE_STA;
  }

  ESP_ERROR_CHECK(esp_wifi_set_mode(mode));

  if (mode == WIFI_MODE_NULL) {
    ESP_ERROR_CHECK(esp_wifi_stop());
  } else {
    ESP_ERROR_CHECK(esp_wifi_start());
  }
}

/* -------------------------------------------------------------------------- */
/* Access Point Control */
/* -------------------------------------------------------------------------- */

void WiFiManager::enableAp(const NetworkConfigAP &ap) {
  configureAp(ap);
  ap_running_ = true;

  ESP_LOGI(TAG, "AP enabled (SSID=%s)", ap.ssid);
}

void WiFiManager::disableAp() {
  ap_running_ = false;
  ESP_LOGI(TAG, "AP disabled");
}

void WiFiManager::configureAp(const NetworkConfigAP &ap) {
  wifi_config_t cfg = {};

  std::strncpy(reinterpret_cast<char *>(cfg.ap.ssid), ap.ssid,
               sizeof(cfg.ap.ssid) - 1);

  std::strncpy(reinterpret_cast<char *>(cfg.ap.password), ap.password,
               sizeof(cfg.ap.password) - 1);

  cfg.ap.ssid_len = std::strlen(reinterpret_cast<char *>(cfg.ap.ssid));
  cfg.ap.max_connection = 4;
  cfg.ap.channel = 1;
  cfg.ap.authmode =
      (std::strlen(ap.password) > 0) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
}

/* -------------------------------------------------------------------------- */
/* Station Control */
/* -------------------------------------------------------------------------- */

void WiFiManager::enableSta(const NetworkConfigSTA &sta) {
  configureSta(sta);
  ESP_ERROR_CHECK(esp_wifi_connect());
  sta_running_ = true;
}

void WiFiManager::disableSta() {
  esp_wifi_disconnect();
  std::strcpy(sta_ip_, "0.0.0.0");
  sta_running_ = false;
}

void WiFiManager::configureSta(const NetworkConfigSTA &sta) {
  wifi_config_t cfg = {};

  std::strncpy(reinterpret_cast<char *>(cfg.sta.ssid), sta.ssid,
               sizeof(cfg.sta.ssid) - 1);

  std::strncpy(reinterpret_cast<char *>(cfg.sta.password), sta.password,
               sizeof(cfg.sta.password) - 1);

  cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  cfg.sta.pmf_cfg.capable = true;
  cfg.sta.pmf_cfg.required = false;

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
}

/* -------------------------------------------------------------------------- */
/* Event Handling */
/* -------------------------------------------------------------------------- */

void WiFiManager::onWiFiEvent(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
  auto *self = static_cast<WiFiManager *>(arg);

  if (event_base == WIFI_EVENT) {

    switch (event_id) {

    case WIFI_EVENT_AP_START: {
      LockGuard lock(self->mutex_);

      if (self->ap_netif_ != nullptr) {
        esp_netif_ip_info_t ip = {};
        if (esp_netif_get_ip_info(self->ap_netif_, &ip) == ESP_OK) {
          snprintf(self->ap_ip_, sizeof(self->ap_ip_), IPSTR, IP2STR(&ip.ip));
        }
      }
      break;
    }

    case WIFI_EVENT_AP_STOP:
      LockGuard(self->mutex_);
      std::strcpy(self->ap_ip_, "0.0.0.0");
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
      ESP_LOGW(TAG, "STA disconnected, retrying");
      esp_wifi_connect();
      break;

    default:
      break;
    }
  }

  if (event_base == IP_EVENT) {

    switch (event_id) {

    case IP_EVENT_STA_GOT_IP: {
      auto *event = static_cast<ip_event_got_ip_t *>(event_data);
      LockGuard lock(self->mutex_);

      snprintf(self->sta_ip_, sizeof(self->sta_ip_), IPSTR,
               IP2STR(&event->ip_info.ip));
      break;
    }

    case IP_EVENT_STA_LOST_IP:
      LockGuard(self->mutex_);
      std::strcpy(self->sta_ip_, "0.0.0.0");
      break;

    default:
      break;
    }
  }
}
