#include "handlers/network_handler.hpp"

#include "cJSON.h"
#include "config_manager.hpp"
#include "esp_http_server.h"
#include "esp_log.h"
#include <algorithm>

static const char* TAG = "network_handler";

static inline void set_json_headers(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
}

static bool read_body_into(httpd_req_t* req, char* dst, size_t dst_size, int* out_len)
{
    size_t total = req->content_len;
    if (dst_size == 0)
        return false;
    size_t to_read = total;
    size_t written = 0;
    while (to_read > 0) {
        int chunk = httpd_req_recv(req, dst + written, std::min(to_read, dst_size - 1 - written));
        if (chunk <= 0)
            return false;
        written += chunk;
        to_read -= chunk;
        if (written >= dst_size - 1)
            break;
    }
    dst[written] = '\0';
    if (out_len)
        *out_len = (int)written;
    while (to_read > 0) {
        char junk[64];
        int chunk = httpd_req_recv(req, junk, std::min<size_t>(to_read, sizeof(junk)));
        if (chunk <= 0)
            break;
        to_read -= chunk;
    }
    return true;
}

// POST /api/network/ap/set
static esp_err_t postApConfigHandler(httpd_req_t* req)
{
    char buf[384];
    int len = 0;
    if (!read_body_into(req, buf, sizeof(buf), &len) || len <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
    }

    cJSON* json = cJSON_Parse(buf);
    if (!json)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

    NetworkConfig network_config = ConfigManager::getInstance().getNetworkConfig();

    const cJSON* ssid = cJSON_GetObjectItem(json, "ap_ssid");
    if (ssid) {
        if (!cJSON_IsString(ssid) || strlen(ssid->valuestring) >= SSID_MAX_LEN) {
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid ap_ssid");
        }
        strncpy(network_config.ap_ssid, ssid->valuestring, SSID_MAX_LEN - 1);
        network_config.ap_ssid[SSID_MAX_LEN - 1] = '\0';
    }

    if (cJSON_HasObjectItem(json, "ap_password")) {
        const cJSON* password = cJSON_GetObjectItem(json, "ap_password");
        if (cJSON_IsString(password)) {
            size_t pwlen = strlen(password->valuestring);
            if (pwlen == 0) {
                network_config.ap_password[0] = '\0';
            } else {
                strncpy(network_config.ap_password, password->valuestring, PASSWORD_MAX_LEN - 1);
                network_config.ap_password[PASSWORD_MAX_LEN - 1] = '\0';
            }
        } else if (cJSON_IsNull(password)) {
            network_config.ap_password[0] = '\0';
        } else {
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid ap_password");
        }
    }

    const cJSON* enabled = cJSON_GetObjectItem(json, "ap_enabled");
    if (enabled && cJSON_IsBool(enabled)) {
        network_config.ap_enabled = cJSON_IsTrue(enabled) ? 1 : 0; // uint8_t
    }

    esp_err_t err = ConfigManager::getInstance().updateNetworkConfig(network_config);

    cJSON* resp = cJSON_CreateObject();
    if (err == ESP_OK) {
        cJSON_AddStringToObject(resp, "status", "AP config updated");
    } else {
        cJSON_AddStringToObject(resp, "status", "failed to update");
        cJSON_AddStringToObject(resp, "error", esp_err_to_name(err));
    }

    char* resp_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    set_json_headers(req);
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    cJSON_Delete(json);
    return ret;
}

// POST /api/network/sta/connect (enable STA + set SSID optionally)
static esp_err_t staConnectHandler(httpd_req_t* req)
{
    // body is optional: { "ssid": "MyWiFi" }
    char buf[256];
    int len = 0;
    read_body_into(req, buf, sizeof(buf), &len);
    cJSON* json = len > 0 ? cJSON_Parse(buf) : nullptr;

    NetworkConfig cfg = ConfigManager::getInstance().getNetworkConfig();
    if (json) {
        const cJSON* ssid = cJSON_GetObjectItem(json, "ssid");
        if (ssid) {
            if (!cJSON_IsString(ssid) || strlen(ssid->valuestring) >= SSID_MAX_LEN) {
                cJSON_Delete(json);
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid ssid");
            }
            strncpy(cfg.ssid, ssid->valuestring, SSID_MAX_LEN - 1);
            cfg.ssid[SSID_MAX_LEN - 1] = '\0';
        }
        cJSON_Delete(json);
    }
    cfg.sta_enabled = 1;
    esp_err_t err = ConfigManager::getInstance().updateNetworkConfig(cfg);

    cJSON* root = cJSON_CreateObject();
    if (err == ESP_OK)
        cJSON_AddStringToObject(root, "status", "STA connect requested");
    else {
        cJSON_AddStringToObject(root, "status", "failed to update");
        cJSON_AddStringToObject(root, "error", esp_err_to_name(err));
    }
    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    set_json_headers(req);
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    return ret;
}

// POST /api/network/sta/disconnect (disable STA)
static esp_err_t staDisconnectHandler(httpd_req_t* req)
{
    NetworkConfig cfg = ConfigManager::getInstance().getNetworkConfig();
    cfg.sta_enabled = 0;
    esp_err_t err = ConfigManager::getInstance().updateNetworkConfig(cfg);

    cJSON* root = cJSON_CreateObject();
    if (err == ESP_OK)
        cJSON_AddStringToObject(root, "status", "STA disconnect requested");
    else {
        cJSON_AddStringToObject(root, "status", "failed to update");
        cJSON_AddStringToObject(root, "error", esp_err_to_name(err));
    }
    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    set_json_headers(req);
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    return ret;
}

// GET /api/network/status
static esp_err_t networkStatusHandler(httpd_req_t* req)
{
    NetworkConfig n = ConfigManager::getInstance().getNetworkConfig();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ap_ssid", n.ap_ssid);
    cJSON_AddBoolToObject(root, "ap_enabled", n.ap_enabled != 0);
    cJSON_AddBoolToObject(root, "sta_enabled", n.sta_enabled != 0);
    cJSON_AddStringToObject(root, "ssid", n.ssid);
    cJSON_AddStringToObject(root, "bssid", n.bssid);
    cJSON_AddStringToObject(root, "ip_address", n.ip_address);
    cJSON_AddStringToObject(root, "mac_address", n.mac_address);

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    set_json_headers(req);
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    return ret;
}

// Optional: CORS preflight for all endpoints under /api/*
static esp_err_t optionsAny(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,POST,PATCH,OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    return httpd_resp_send(req, nullptr, 0);
}

// Registration
void Handlers::registerNetworkEndpoints(httpd_handle_t server, void* ctx)
{
    esp_err_t err;

    // CORS preflight
    httpd_uri_t options_api = {.uri = "/api/*",
        .method = HTTP_OPTIONS,
        .handler = optionsAny,
        .user_ctx = ctx};
    httpd_register_uri_handler(server,
        &options_api); // ignore result; wildcard may not match on old IDF

    // POST /api/network/ap/set
    httpd_uri_t post_ap_config_uri = {.uri = "/api/network/ap/set",
        .method = HTTP_POST,
        .handler = postApConfigHandler,
        .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &post_ap_config_uri);
    if (err == ESP_OK)
        ESP_LOGI(TAG, "Registered POST /api/network/ap/set");
    else
        ESP_LOGE(TAG, "Failed to register POST /api/network/ap/set: %s", esp_err_to_name(err));

    // POST /api/network/sta/connect
    httpd_uri_t post_sta_connect_uri = {.uri = "/api/network/sta/connect",
        .method = HTTP_POST,
        .handler = staConnectHandler,
        .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &post_sta_connect_uri);
    if (err == ESP_OK)
        ESP_LOGI(TAG, "Registered POST /api/network/sta/connect");
    else
        ESP_LOGE(TAG, "Failed to register POST /api/network/sta/connect: %s", esp_err_to_name(err));

    // POST /api/network/sta/disconnect
    httpd_uri_t post_sta_disconnect_uri = {.uri = "/api/network/sta/disconnect",
        .method = HTTP_POST,
        .handler = staDisconnectHandler,
        .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &post_sta_disconnect_uri);
    if (err == ESP_OK)
        ESP_LOGI(TAG, "Registered POST /api/network/sta/disconnect");
    else
        ESP_LOGE(TAG,
            "Failed to register POST /api/network/sta/disconnect: %s",
            esp_err_to_name(err));

    // GET /api/network/status
    httpd_uri_t get_network_status_uri = {.uri = "/api/network/status",
        .method = HTTP_GET,
        .handler = networkStatusHandler,
        .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &get_network_status_uri);
    if (err == ESP_OK)
        ESP_LOGI(TAG, "Registered GET /api/network/status");
    else
        ESP_LOGE(TAG, "Failed to register GET /api/network/status: %s", esp_err_to_name(err));
}
