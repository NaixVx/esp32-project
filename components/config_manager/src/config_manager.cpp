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
constexpr const char* NVS_KEY = "cfg_v1";
constexpr const char* NVS_KEY_TMP = "cfg_tmp";

// -----------------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------------

static inline void clamp_cstr(char* buf, size_t max_len)
{
    buf[max_len - 1] = '\0';
}

static inline void sanitize_persisted(PersistedConfig& cfg)
{
    clamp_cstr(cfg.info.id, ID_MAX_LEN);
    clamp_cstr(cfg.info.device_name, DEVICE_NAME_MAX_LEN);
    clamp_cstr(cfg.info.device_type, DEVICE_TYPE_MAX_LEN);
    clamp_cstr(cfg.info.firmware_version, FW_VERSION_MAX_LEN);

    clamp_cstr(cfg.network_private.sta_ssid, SSID_MAX_LEN);
    clamp_cstr(cfg.network_private.sta_password, PASSWORD_MAX_LEN);
    clamp_cstr(cfg.network_private.ap_ssid, SSID_MAX_LEN);
    clamp_cstr(cfg.network_private.ap_password, PASSWORD_MAX_LEN);

    cfg.network_private.ap_enabled = !!cfg.network_private.ap_enabled;
}

// -----------------------------------------------------------------------------
// Singleton
// -----------------------------------------------------------------------------

ConfigManager& ConfigManager::getInstance()
{
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager()
{
    ESP_LOGI(TAG, "Initializing ConfigManager");

    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ESP_LOGE(TAG, "Failed to create mutex");
        abort();
    }

    if (loadFromNVS() != ESP_OK || !isValid()) {
        ESP_LOGW(TAG, "Invalid or missing config, using defaults");
        setDefaults();
        esp_err_t err = saveToNVS();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save defaults: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGI(TAG, "Loaded valid config from NVS");
    }
}

ConfigManager::~ConfigManager()
{
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

// -----------------------------------------------------------------------------
// Getters
// -----------------------------------------------------------------------------

DeviceInfo ConfigManager::getDeviceInfo()
{
    LockGuard guard(mutex_);
    return persisted_.info;
}

NetworkPrivateConfig ConfigManager::getNetworkPrivateConfig()
{
    LockGuard guard(mutex_);
    return persisted_.network_private;
}

NetworkPublicConfig ConfigManager::getNetworkPublicConfig()
{
    LockGuard guard(mutex_);
    return network_public_;
}

// -----------------------------------------------------------------------------
// Updates
// -----------------------------------------------------------------------------

esp_err_t ConfigManager::updateDeviceInfo(const DeviceInfo& info)
{
    DeviceInfo snapshot;
    std::vector<DeviceInfoObserver> observers;

    {
        LockGuard guard(mutex_);
        if (std::memcmp(&persisted_.info, &info, sizeof(info)) == 0) {
            return ESP_OK;
        }
        persisted_.info = info;
        snapshot = persisted_.info;
        observers = info_observers_;
    }

    esp_err_t err = saveToNVS();
    if (err == ESP_OK) {
        for (auto& obs : observers)
            if (obs)
                obs(snapshot);
    }
    return err;
}

esp_err_t ConfigManager::updateNetworkPrivateConfig(const NetworkPrivateConfig& net)
{
    NetworkPrivateConfig snapshot;
    std::vector<NetworkPrivateObserver> observers;

    {
        LockGuard guard(mutex_);
        if (std::memcmp(&persisted_.network_private, &net, sizeof(net)) == 0) {
            return ESP_OK;
        }
        persisted_.network_private = net;
        snapshot = persisted_.network_private;
        observers = network_private_observers_;
    }

    esp_err_t err = saveToNVS();
    if (err == ESP_OK) {
        for (auto& obs : observers)
            if (obs)
                obs(snapshot);
    }
    return err;
}

void ConfigManager::updateNetworkPublicConfig(const NetworkPublicConfig& net)
{
    LockGuard guard(mutex_);
    if (std::memcmp(&network_public_, &net, sizeof(net)) == 0) {
        return;
    }
    network_public_ = net;
}

// -----------------------------------------------------------------------------
// Persistence
// -----------------------------------------------------------------------------

esp_err_t ConfigManager::saveToNVS()
{
    LockGuard guard(mutex_);

    persisted_.header.version = 1;
    persisted_.header.struct_size = static_cast<uint16_t>(sizeof(PersistedConfig));

    sanitize_persisted(persisted_);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK)
        return err;

    err = nvs_set_blob(h, NVS_KEY_TMP, &persisted_, sizeof(PersistedConfig));
    if (err == ESP_OK)
        err = nvs_set_blob(h, NVS_KEY, &persisted_, sizeof(PersistedConfig));

    if (err == ESP_OK) {
        esp_err_t e2 = nvs_erase_key(h, NVS_KEY_TMP);
        if (e2 != ESP_OK && e2 != ESP_ERR_NVS_NOT_FOUND) {
            err = e2;
        }
    }

    if (err == ESP_OK)
        err = nvs_commit(h);

    nvs_close(h);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ConfigManager::loadFromNVS()
{
    LockGuard guard(mutex_);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK)
        return err;

    size_t blob_size = 0;
    err = nvs_get_blob(h, NVS_KEY, nullptr, &blob_size);
    if (err != ESP_OK || blob_size < sizeof(ConfigHeader)) {
        nvs_close(h);
        return err;
    }

    std::vector<uint8_t> buf(blob_size);
    err = nvs_get_blob(h, NVS_KEY, buf.data(), &blob_size);
    nvs_close(h);
    if (err != ESP_OK)
        return err;

    const auto* header = reinterpret_cast<const ConfigHeader*>(buf.data());

    if (header->version != 1) {
        ESP_LOGW(TAG, "Unknown config version: %u", static_cast<unsigned>(header->version));
        return ESP_ERR_INVALID_VERSION;
    }

    std::memset(&persisted_, 0, sizeof(PersistedConfig));
    const size_t copy_len = std::min(static_cast<size_t>(header->struct_size),
        static_cast<size_t>(sizeof(PersistedConfig)));

    std::memcpy(&persisted_, buf.data(), copy_len);

    sanitize_persisted(persisted_);

    persisted_.header.version = 1;
    persisted_.header.struct_size = static_cast<uint16_t>(sizeof(PersistedConfig));

    return ESP_OK;
}

// -----------------------------------------------------------------------------
// Defaults & validation
// -----------------------------------------------------------------------------

void ConfigManager::setDefaults()
{
    LockGuard guard(mutex_);
    std::memset(&persisted_, 0, sizeof(persisted_));

    persisted_.header.version = 1;
    persisted_.header.struct_size = static_cast<uint16_t>(sizeof(PersistedConfig));

    std::strncpy(persisted_.info.id, "0", ID_MAX_LEN - 1);
    std::strncpy(persisted_.info.device_name, "esp32-demo", DEVICE_NAME_MAX_LEN - 1);
    std::strncpy(persisted_.info.device_type, "generic", DEVICE_TYPE_MAX_LEN - 1);
    std::strncpy(persisted_.info.firmware_version, "0.0.1", FW_VERSION_MAX_LEN - 1);

    persisted_.network_private.ap_enabled = 1;
    std::strncpy(persisted_.network_private.ap_ssid, "esp32-demo", SSID_MAX_LEN - 1);

    sanitize_persisted(persisted_);
}

esp_err_t ConfigManager::resetToDefaults()
{
    setDefaults();
    return saveToNVS();
}

bool ConfigManager::isValid()
{
    LockGuard guard(mutex_);

    auto within = [](const char* s, size_t n) {
        return strnlen(s, n) < n;
    };

    if (!within(persisted_.info.device_name, DEVICE_NAME_MAX_LEN))
        return false;
    if (!within(persisted_.info.device_type, DEVICE_TYPE_MAX_LEN))
        return false;
    if (!within(persisted_.info.firmware_version, FW_VERSION_MAX_LEN))
        return false;

    if (!within(persisted_.network_private.sta_ssid, SSID_MAX_LEN))
        return false;
    if (!within(persisted_.network_private.sta_password, PASSWORD_MAX_LEN))
        return false;
    if (!within(persisted_.network_private.ap_ssid, SSID_MAX_LEN))
        return false;
    if (!within(persisted_.network_private.ap_password, PASSWORD_MAX_LEN))
        return false;

    return true;
}

// -----------------------------------------------------------------------------
// Observer registration
// -----------------------------------------------------------------------------

void ConfigManager::registerNetworkPrivateObserver(NetworkPrivateObserver obs)
{
    LockGuard guard(mutex_);
    network_private_observers_.push_back(std::move(obs));
}

void ConfigManager::registerDeviceInfoObserver(DeviceInfoObserver obs)
{
    LockGuard guard(mutex_);
    info_observers_.push_back(std::move(obs));
}
