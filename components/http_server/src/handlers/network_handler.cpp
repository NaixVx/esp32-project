#include "handlers/network_handler.hpp"

#include "cJSON.h"
#include "config_manager.hpp"
#include "esp_log.h"

static const char* TAG = "network_handler";

// POST /api/network/ap/set
static esp_err_t postApConfigHandler(httpd_req_t* req) {
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");

    buf[len] = '\0';
    cJSON* json = cJSON_Parse(buf);
    if (!json) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

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

    const cJSON* password = cJSON_GetObjectItem(json, "ap_password");
    if (cJSON_HasObjectItem(json, "ap_password")) {
        if (cJSON_IsString(password)) {
            if (strlen(password->valuestring) == 0) {
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
        network_config.ap_enabled = cJSON_IsTrue(enabled);
    }

    ConfigManager::getInstance().updateNetworkConfig(network_config);

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "AP config updated");

    char* resp_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    cJSON_Delete(json);

    return ret;
}

// POST /api/network/sta/connect
static esp_err_t staConnectHandler(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "STA connect triggered (mock)");

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);

    return ret;
}

// POST /api/network/sta/disconnect
static esp_err_t staDisconnectHandler(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "STA disconnect triggered (mock)");

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);

    return ret;
}

// GET /api/network/status
static esp_err_t networkStatusHandler(httpd_req_t* req) {
    NetworkConfig network_config = ConfigManager::getInstance().getNetworkConfig();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "ap_ssid", network_config.ap_ssid);
    cJSON_AddBoolToObject(root, "ap_enabled", network_config.ap_enabled);
    cJSON_AddStringToObject(root, "ap_password", network_config.ap_password);
    cJSON_AddBoolToObject(root, "sta_enabled", network_config.sta_enabled);
    cJSON_AddStringToObject(root, "ssid", network_config.ssid);
    cJSON_AddStringToObject(root, "bssid", network_config.bssid);
    cJSON_AddStringToObject(root, "ip_address", network_config.ip_address);
    cJSON_AddStringToObject(root, "mac_address", network_config.mac_address);

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);

    return ret;
}

// Registration
void Handlers::registerNetworkEndpoints(httpd_handle_t server, void* ctx) {
    esp_err_t err;

    // POST /api/network/ap/set
    httpd_uri_t post_ap_config_uri = {.uri = "/api/network/ap/set",
                                      .method = HTTP_POST,
                                      .handler = postApConfigHandler,
                                      .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &post_ap_config_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register POST /api/network/ap/set: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered POST /api/network/ap/set");
    }

    // POST /api/network/sta/connect
    httpd_uri_t post_sta_connect_uri = {.uri = "/api/network/sta/connect",
                                        .method = HTTP_POST,
                                        .handler = staConnectHandler,
                                        .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &post_sta_connect_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register POST /api/network/sta/connect: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered POST /api/network/sta/connect");
    }

    // POST /api/network/sta/disconnect
    httpd_uri_t post_sta_disconnect_uri = {.uri = "/api/network/sta/disconnect",
                                           .method = HTTP_POST,
                                           .handler = staDisconnectHandler,
                                           .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &post_sta_disconnect_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register POST /api/network/sta/disconnect: %s",
                 esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered POST /api/network/sta/disconnect");
    }

    // GET /api/network/status
    httpd_uri_t get_network_status_uri = {.uri = "/api/network/status",
                                          .method = HTTP_GET,
                                          .handler = networkStatusHandler,
                                          .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &get_network_status_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GET /api/network/status: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered GET /api/network/status");
    }
}