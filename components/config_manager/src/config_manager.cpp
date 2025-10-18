#include "config_manager.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "utils/lock_guard.hpp"

static const char* TAG = "config_manager";

constexpr const char* NVS_NAMESPACE = "cfg";
constexpr const char* NVS_KEY = "dev_cfg_v1";
constexpr const char* NVS_KEY_TMP = "dev_cfg_tmp";

// --- Singleton ---
ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    ESP_LOGI(TAG, "Initializing ConfigManager");

    // Create mutex
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ESP_LOGE(TAG, "Failed to create mutex!");
        abort();
    }

    // Try load; if invalid or missing, fall back to defaults and persist.
    if (loadFromNVS() != ESP_OK || !isValid()) {
        ESP_LOGW(TAG, "Invalid or missing config, using defaults");
        setDefaults();
        esp_err_t err = saveToNVS();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save defaults to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGI(TAG, "Loaded valid config from NVS");
    }
}

ConfigManager::~ConfigManager() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

// --- Internal helpers ---
// TODO: move to utils
static inline void clamp_cstr(char* buf, size_t max_len) {
    buf[max_len - 1] = '\0';
}

static inline void sanitize_strings(DeviceConfig& cfg) {
    clamp_cstr(cfg.info.device_name, DEVICE_NAME_MAX_LEN);
    clamp_cstr(cfg.info.firmware_version, FW_VERSION_MAX_LEN);

    clamp_cstr(cfg.network.ap_ssid, SSID_MAX_LEN);
    clamp_cstr(cfg.network.ap_password, PASSWORD_MAX_LEN);
    clamp_cstr(cfg.network.ssid, SSID_MAX_LEN);
    clamp_cstr(cfg.network.bssid, MAC_ADDR_LEN);
    clamp_cstr(cfg.network.ip_address, IP_ADDR_LEN);
    clamp_cstr(cfg.network.mac_address, MAC_ADDR_LEN);
}

static inline void clamp_flags(DeviceConfig& cfg) {
    cfg.network.ap_enabled = !!cfg.network.ap_enabled;
    cfg.network.sta_enabled = !!cfg.network.sta_enabled;
}

// --- Getters ---
DeviceInfo ConfigManager::getDeviceInfo() {
    LockGuard guard(mutex_);
    return config_.info;
}

NetworkConfig ConfigManager::getNetworkConfig() {
    LockGuard guard(mutex_);
    return config_.network;
}

DeviceConfig ConfigManager::getConfig() {
    LockGuard guard(mutex_);
    return config_;
}

// --- Mutators (return status; notify observers on success) ---
esp_err_t ConfigManager::updateDeviceInfo(const DeviceInfo& info) {
    DeviceInfo snapshot;
    std::vector<DeviceInfoObserver> observers_copy;

    {
        LockGuard guard(mutex_);
        if (std::memcmp(&config_.info, &info, sizeof(info)) == 0) {
            return ESP_OK;  // no change
        }
        config_.info = info;
        snapshot = config_.info;
        observers_copy = info_observers_;
    }

    esp_err_t err = saveToNVS();
    if (err == ESP_OK) {
        for (auto& obs : observers_copy)
            if (obs) obs(snapshot);
    }
    return err;
}

esp_err_t ConfigManager::updateNetworkConfig(const NetworkConfig& netConfig) {
    NetworkConfig snapshot;
    std::vector<NetworkObserver> observers_copy;

    {
        LockGuard guard(mutex_);
        if (std::memcmp(&config_.network, &netConfig, sizeof(netConfig)) == 0) {
            return ESP_OK;  // no change
        }
        config_.network = netConfig;
        snapshot = config_.network;
        observers_copy = network_observers_;
    }

    esp_err_t err = saveToNVS();
    if (err == ESP_OK) {
        for (auto& obs : observers_copy)
            if (obs) obs(snapshot);
    }
    return err;
}

esp_err_t ConfigManager::updateConfig(const DeviceConfig& newConfig) {
    DeviceConfig snapshot;
    std::vector<NetworkObserver> net_copy;
    std::vector<DeviceInfoObserver> info_copy;

    {
        LockGuard guard(mutex_);
        if (std::memcmp(&config_, &newConfig, sizeof(DeviceConfig)) == 0) {
            return ESP_OK;  // no change
        }
        config_ = newConfig;
        snapshot = config_;
        net_copy = network_observers_;
        info_copy = info_observers_;
    }

    esp_err_t err = saveToNVS();
    if (err == ESP_OK) {
        for (auto& obs : info_copy)
            if (obs) obs(snapshot.info);
        for (auto& obs : net_copy)
            if (obs) obs(snapshot.network);
    }
    return err;
}

// --- NVS operations ---
esp_err_t ConfigManager::saveToNVS() {
    LockGuard guard(mutex_);
    // keep header current before persisting
    config_.header.version = 1;
    config_.header.struct_size = static_cast<uint16_t>(sizeof(DeviceConfig));
    clamp_flags(config_);
    sanitize_strings(config_);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    // belt-and-suspenders: write a tmp key, then active, then erase tmp, then commit
    err = nvs_set_blob(h, NVS_KEY_TMP, &config_, sizeof(DeviceConfig));
    if (err == ESP_OK) err = nvs_set_blob(h, NVS_KEY, &config_, sizeof(DeviceConfig));
    if (err == ESP_OK) {
        esp_err_t e2 = nvs_erase_key(h, NVS_KEY_TMP);
        if (e2 != ESP_OK && e2 != ESP_ERR_NVS_NOT_FOUND) err = e2;
    }
    if (err == ESP_OK) err = nvs_commit(h);

    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save config to NVS: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ConfigManager::loadFromNVS() {
    LockGuard guard(mutex_);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    size_t blob_size = 0;
    err = nvs_get_blob(h, NVS_KEY, nullptr, &blob_size);
    if (err != ESP_OK || blob_size < sizeof(ConfigHeader)) {
        nvs_close(h);
        return (err == ESP_OK) ? ESP_ERR_NVS_INVALID_LENGTH : err;
    }

    std::vector<uint8_t> buf(blob_size);
    err = nvs_get_blob(h, NVS_KEY, buf.data(), &blob_size);
    nvs_close(h);
    if (err != ESP_OK) return err;

    // interpret header
    const auto* header = reinterpret_cast<const ConfigHeader*>(buf.data());

    // accept known versions; allow smaller historical struct_size
    if (header->version == 1) {
        std::memset(&config_, 0, sizeof(DeviceConfig));
        const size_t to_copy =
            std::min(static_cast<size_t>(header->struct_size), static_cast<size_t>(buf.size()));
        size_t copy_len = std::min(to_copy, static_cast<size_t>(sizeof(DeviceConfig)));
        std::memcpy(&config_, buf.data(), copy_len);

        // sanitize
        clamp_flags(config_);
        sanitize_strings(config_);

        // if a future field was added, header might not match current sizeof; fix it in memory
        config_.header.version = 1;
        config_.header.struct_size = static_cast<uint16_t>(sizeof(DeviceConfig));
        return ESP_OK;
    }

    // unknown version
    ESP_LOGW(TAG, "Unknown config version: %u", static_cast<unsigned>(header->version));
    return ESP_ERR_INVALID_VERSION;
}

// --- Defaults & validation ---
void ConfigManager::setDefaults() {
    LockGuard guard(mutex_);
    std::memset(&config_, 0, sizeof(config_));
    config_.header.version = 1;
    config_.header.struct_size = static_cast<uint16_t>(sizeof(DeviceConfig));

    std::strncpy(config_.info.device_name, "esp32-project", DEVICE_NAME_MAX_LEN - 1);
    std::strncpy(config_.info.firmware_version, "0.001", FW_VERSION_MAX_LEN - 1);

    std::strncpy(config_.network.ap_ssid, "ESP32-PROJECT", SSID_MAX_LEN - 1);
    config_.network.ap_password[0] = '\0';

    config_.network.ap_enabled = 1;
    config_.network.sta_enabled = 0;

    // optional fields default empty
    config_.network.ssid[0] = '\0';
    config_.network.bssid[0] = '\0';
    config_.network.ip_address[0] = '\0';
    config_.network.mac_address[0] = '\0';

    sanitize_strings(config_);
}

esp_err_t ConfigManager::resetToDefaults() {
    setDefaults();
    return saveToNVS();
}

bool ConfigManager::isValid() {
    LockGuard guard(mutex_);

    auto within = [](const char* s, size_t n) {
        return strnlen(s, n) < n;  // must be null-terminated within bounds
    };
    if (!within(config_.info.device_name, DEVICE_NAME_MAX_LEN)) return false;
    if (!within(config_.info.firmware_version, FW_VERSION_MAX_LEN)) return false;
    if (!within(config_.network.ap_ssid, SSID_MAX_LEN)) return false;
    if (!within(config_.network.ap_password, PASSWORD_MAX_LEN)) return false;
    if (!within(config_.network.ssid, SSID_MAX_LEN)) return false;
    if (!within(config_.network.bssid, MAC_ADDR_LEN)) return false;
    if (!within(config_.network.ip_address, IP_ADDR_LEN)) return false;
    if (!within(config_.network.mac_address, MAC_ADDR_LEN)) return false;

    // policy checks
    if (config_.network.ap_enabled) {
        size_t pwlen = std::strlen(config_.network.ap_password);
        if (pwlen > 0 && pwlen < 8) return false;  // WPA2 minimum if set
    }
    if (config_.network.sta_enabled) {
        if (config_.network.ssid[0] == '\0') return false;
    }

    // clamp flags to 0/1
    clamp_flags(config_);
    return true;
}

// --- Observer registration ---
void ConfigManager::registerNetworkObserver(NetworkObserver obs) {
    LockGuard guard(mutex_);
    network_observers_.push_back(std::move(obs));
}

void ConfigManager::registerDeviceInfoObserver(DeviceInfoObserver obs) {
    LockGuard guard(mutex_);
    info_observers_.push_back(std::move(obs));
}
