#include "handlers/network_handler.hpp"

#include "utils/http_utils.hpp"

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

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "mac_address", net.mac_address);
    cJSON_AddBoolToObject(root, "ap_enabled", net.ap.enabled != 0);
    cJSON_AddStringToObject(root, "ap_ssid", net.ap.ssid);
    cJSON_AddStringToObject(root, "ap_ip", "0.0.0.0"); // retrieve current ip
    cJSON_AddStringToObject(root, "sta_ssid", net.sta.ssid);
    cJSON_AddStringToObject(root, "sta_ip", "0.0.0.0");

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

// // POST /api/network/sta/set (enable STA + set SSID optionally)
// static esp_err_t staConnectHandler(httpd_req_t* req)
// {
//     // body is optional: { "ssid": "MyWiFi" }
//     char buf[256];
//     int len = 0;
//     read_body_into(req, buf, sizeof(buf), &len);
//     cJSON* json = len > 0 ? cJSON_Parse(buf) : nullptr;

//     NetworkConfig cfg = ConfigManager::getInstance().getNetworkConfig();
//     if (json) {
//         const cJSON* ssid = cJSON_GetObjectItem(json, "ssid");
//         if (ssid) {
//             if (!cJSON_IsString(ssid) || strlen(ssid->valuestring) >= SSID_MAX_LEN) {
//                 cJSON_Delete(json);
//                 return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid ssid");
//             }
//             strncpy(cfg.ssid, ssid->valuestring, SSID_MAX_LEN - 1);
//             cfg.ssid[SSID_MAX_LEN - 1] = '\0';
//         }
//         cJSON_Delete(json);
//     }
//     cfg.sta_enabled = 1;
//     esp_err_t err = ConfigManager::getInstance().updateNetworkConfig(cfg);

//     cJSON* root = cJSON_CreateObject();
//     if (err == ESP_OK)
//         cJSON_AddStringToObject(root, "status", "STA connect requested");
//     else {
//         cJSON_AddStringToObject(root, "status", "failed to update");
//         cJSON_AddStringToObject(root, "error", esp_err_to_name(err));
//     }
//     char* resp = cJSON_PrintUnformatted(root);
//     cJSON_Delete(root);

//     set_json_headers(req);
//     esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
//     free(resp);
//     return ret;
// }

// // POST /api/network/sta/disconnect (disable STA)
// static esp_err_t staDisconnectHandler(httpd_req_t* req)
// {
//     NetworkConfig cfg = ConfigManager::getInstance().getNetworkConfig();
//     cfg.sta_enabled = 0;
//     esp_err_t err = ConfigManager::getInstance().updateNetworkConfig(cfg);

//     cJSON* root = cJSON_CreateObject();
//     if (err == ESP_OK)
//         cJSON_AddStringToObject(root, "status", "STA disconnect requested");
//     else {
//         cJSON_AddStringToObject(root, "status", "failed to update");
//         cJSON_AddStringToObject(root, "error", esp_err_to_name(err));
//     }
//     char* resp = cJSON_PrintUnformatted(root);
//     cJSON_Delete(root);

//     set_json_headers(req);
//     esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
//     free(resp);
//     return ret;
// }

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

    // // POST /api/network/sta/connect
    // httpd_uri_t post_sta_connect_uri = {.uri = "/api/network/sta/connect",
    //     .method = HTTP_POST,
    //     .handler = staConnectHandler,
    //     .user_ctx = ctx};
    // err = httpd_register_uri_handler(server, &post_sta_connect_uri);
    // if (err == ESP_OK)
    //     ESP_LOGI(TAG, "Registered POST /api/network/sta/connect");
    // else
    //     ESP_LOGE(TAG, "Failed to register POST /api/network/sta/connect: %s", esp_err_to_name(err));

    // // POST /api/network/sta/disconnect
    // httpd_uri_t post_sta_disconnect_uri = {.uri = "/api/network/sta/disconnect",
    //     .method = HTTP_POST,
    //     .handler = staDisconnectHandler,
    //     .user_ctx = ctx};
    // err = httpd_register_uri_handler(server, &post_sta_disconnect_uri);
    // if (err == ESP_OK)
    //     ESP_LOGI(TAG, "Registered POST /api/network/sta/disconnect");
    // else
    //     ESP_LOGE(TAG,
    //         "Failed to register POST /api/network/sta/disconnect: %s",
    //         esp_err_to_name(err));
}
