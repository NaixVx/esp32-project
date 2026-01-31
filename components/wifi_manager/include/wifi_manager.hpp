#pragma once

#include "config_manager.hpp"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "freertos/semphr.h"
#include "utils/lock_guard.hpp"

class WiFiManager {
public:
  static WiFiManager &getInstance();

  WiFiManager();
  ~WiFiManager();

  void init();
  void syncWithConfig();

  const char *getMacAddress() const;
  const char *getApIp() const;
  const char *getStaIp() const;

private:
  /* --- Event handling --------------------------------------------------- */
  static void onWiFiEvent(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);

  /* --- Wi-Fi mode control ----------------------------------------------- */
  void updateWifiMode(bool ap_enabled, bool sta_enabled);

  /* --- Access Point control --------------------------------------------- */
  void enableAp(const NetworkConfigAP &ap);
  void disableAp();
  void configureAp(const NetworkConfigAP &ap);

  /* --- Station control -------------------------------------------------- */
  void enableSta(const NetworkConfigSTA &sta);
  void disableSta();
  void configureSta(const NetworkConfigSTA &sta);

  /* --- Configuration sync ----------------------------------------------- */
  void scheduleConfigSync();

private:
  /* --- Synchronization -------------------------------------------------- */
  SemaphoreHandle_t mutex_{nullptr};

  /* --- Network interfaces ----------------------------------------------- */
  esp_netif_t *ap_netif_{nullptr};
  esp_netif_t *sta_netif_{nullptr};

  /* --- State ------------------------------------------------------------ */
  bool ap_running_{false};
  bool sta_running_{false};

  /* --- Runtime info ----------------------------------------------------- */
  char mac_address_[MAC_ADDR_LEN]{};
  char ap_ip_[IP_ADDR_LEN]{"0.0.0.0"};
  char sta_ip_[IP_ADDR_LEN]{"0.0.0.0"};
};
