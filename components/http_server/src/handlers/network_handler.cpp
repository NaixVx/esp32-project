#include "handlers/network_handler.hpp"

#include "utils/http_utils.hpp"
#include "wifi_manager.hpp"

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

// Optional: CORS preflight for all endpoints under /api/*
static esp_err_t optionsAny(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,POST,PATCH,OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    return httpd_resp_send(req, nullptr, 0);
}

// GET /api/network/status
static esp_err_t networkStatusHandler(httpd_req_t* req)
{
    ESP_LOGI(TAG, "GET /api/network/status");

    NetworkConfig net = ConfigManager::getInstance().getNetworkConfig();
    WiFiManager& wifi = WiFiManager::getInstance();

    cJSON* root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "mac_address", wifi.getMacAddress());

    cJSON* ap = cJSON_CreateObject();
    cJSON_AddBoolToObject(ap, "enabled", net.ap.enabled != 0);
    cJSON_AddStringToObject(ap, "ssid", net.ap.ssid);
    cJSON_AddStringToObject(ap, "ip", wifi.getApIp());
    cJSON_AddItemToObject(root, "ap", ap);

    cJSON* sta = cJSON_CreateObject();
    cJSON_AddStringToObject(sta, "ssid", net.sta.ssid);
    cJSON_AddStringToObject(sta, "ip", wifi.getStaIp());
    cJSON_AddItemToObject(root, "sta", sta);

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    set_json_headers(req);
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    return ret;
}

// POST /api/network/ap/set
static esp_err_t postApConfigHandler(httpd_req_t* req)
{
    ESP_LOGI(TAG, "POST /api/network/ap/set");

    char buf[256];
    int len = 0;

    if (!http_read_body(req, buf, sizeof(buf), &len) || len <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
    }

    cJSON* json = cJSON_Parse(buf);
    if (!json) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    NetworkConfigAP ap = ConfigManager::getInstance().getNetworkConfig().ap;

    // --- ap_ssid ---
    const cJSON* ssid = cJSON_GetObjectItem(json, "ap_ssid");
    if (ssid) {
        if (!cJSON_IsString(ssid)) {
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid ap_ssid");
        }

        size_t ssid_len = strlen(ssid->valuestring);
        if (ssid_len == 0 || ssid_len >= SSID_MAX_LEN) {
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid ap_ssid length");
        }

        strncpy(ap.ssid, ssid->valuestring, SSID_MAX_LEN - 1);
        ap.ssid[SSID_MAX_LEN - 1] = '\0';
        // ESP_LOGI(TAG, "Updated ap_ssid");
    }

    // --- ap_password ---
    if (cJSON_HasObjectItem(json, "ap_password")) {
        const cJSON* password = cJSON_GetObjectItem(json, "ap_password");

        if (cJSON_IsNull(password)) {
            ap.password[0] = '\0';
        } else if (cJSON_IsString(password)) {
            size_t pwlen = strlen(password->valuestring);

            // WPA2 minimum length enforcement
            if (pwlen > 0 && pwlen < 8) {
                cJSON_Delete(json);
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ap_password too short");
            }

            if (pwlen == 0) {
                ap.password[0] = '\0';
            } else {
                strncpy(ap.password, password->valuestring, PASSWORD_MAX_LEN - 1);
                ap.password[PASSWORD_MAX_LEN - 1] = '\0';
            }
            // ESP_LOGI(TAG, "Updated ap_password");
        } else {
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid ap_password");
        }
    }

    // --- ap_enabled ---
    const cJSON* enabled = cJSON_GetObjectItem(json, "ap_enabled");
    if (enabled && cJSON_IsBool(enabled)) {
        ap.enabled = cJSON_IsTrue(enabled) ? 1 : 0;
        // ESP_LOGI(TAG, "Updated ap_enabled");
    }

    esp_err_t err = ConfigManager::getInstance().setNetworkConfigAP(ap);

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

// POST /api/network/sta/set
static esp_err_t postStaConfigHandler(httpd_req_t* req)
{
    ESP_LOGI(TAG, "POST /api/network/sta/set");

    char buf[256];
    int len = 0;

    if (!http_read_body(req, buf, sizeof(buf), &len) || len <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
    }

    cJSON* json = cJSON_Parse(buf);
    if (!json) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    }

    NetworkConfigSTA sta = ConfigManager::getInstance().getNetworkConfig().sta;

    // --- sta_ssid ---
    if (cJSON_HasObjectItem(json, "sta_ssid")) {
        const cJSON* ssid = cJSON_GetObjectItem(json, "sta_ssid");

        if (cJSON_IsNull(ssid)) {
            // explicit disable
            sta.ssid[0] = '\0';
            sta.password[0] = '\0';
        } else if (cJSON_IsString(ssid)) {
            size_t ssid_len = strlen(ssid->valuestring);
            if (ssid_len == 0 || ssid_len >= SSID_MAX_LEN) {
                cJSON_Delete(json);
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid sta_ssid length");
            }

            strncpy(sta.ssid, ssid->valuestring, SSID_MAX_LEN - 1);
            sta.ssid[SSID_MAX_LEN - 1] = '\0';
        } else {
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid sta_ssid");
        }
    }

    // --- sta_password ---
    if (cJSON_HasObjectItem(json, "sta_password")) {
        const cJSON* password = cJSON_GetObjectItem(json, "sta_password");

        if (cJSON_IsNull(password)) {
            sta.password[0] = '\0';
        } else if (cJSON_IsString(password)) {
            size_t pwlen = strlen(password->valuestring);

            if (pwlen > 0 && pwlen < 8) {
                cJSON_Delete(json);
                return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "sta_password too short");
            }

            if (pwlen == 0) {
                sta.password[0] = '\0';
            } else {
                strncpy(sta.password, password->valuestring, PASSWORD_MAX_LEN - 1);
                sta.password[PASSWORD_MAX_LEN - 1] = '\0';
            }
        } else {
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid sta_password");
        }
    }

    esp_err_t err = ConfigManager::getInstance().setNetworkConfigSTA(sta);

    cJSON* resp = cJSON_CreateObject();
    if (err == ESP_OK) {
        cJSON_AddStringToObject(resp, "status", "STA config updated");
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

// Registration
void Handlers::registerNetworkEndpoints(httpd_handle_t server, void* ctx)
{
    esp_err_t err;

    // CORS preflight
    httpd_uri_t options_api = {.uri = "/api/*",
        .method = HTTP_OPTIONS,
        .handler = optionsAny,
        .user_ctx = ctx};
    httpd_register_uri_handler(server, &options_api);

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

    // POST /api/network/sta/set
    httpd_uri_t post_sta_config_uri = {.uri = "/api/network/sta/set",
        .method = HTTP_POST,
        .handler = postStaConfigHandler,
        .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &post_sta_config_uri);
    if (err == ESP_OK)
        ESP_LOGI(TAG, "Registered POST /api/network/sta/set");
    else
        ESP_LOGE(TAG, "Failed to register POST /api/network/sta/set: %s", esp_err_to_name(err));
}
