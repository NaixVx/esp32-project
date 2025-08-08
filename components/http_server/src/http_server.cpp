#include "http_server.hpp"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "sensor_manager.hpp"

static const char* TAG = "http_server";

HttpServer::HttpServer(const DeviceInfo& info) : device_info(info) {}

void HttpServer::registerEndpoints() {
    // Root
    httpd_uri_t root_uri = {
        .uri = "/", .method = HTTP_GET, .handler = rootHandlerWrapper, .user_ctx = (void*)this};
    esp_err_t err = httpd_register_uri_handler(server_handle, &root_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GET /: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered GET /");
    }

    // Favicon (empty)
    httpd_uri_t favicon_uri = {.uri = "/favicon.ico",
                               .method = HTTP_GET,
                               .handler =
                                   [](httpd_req_t* req) {
                                       httpd_resp_set_type(req, "image/x-icon");
                                       return httpd_resp_send(req, NULL, 0);
                                   },
                               .user_ctx = nullptr};
    err = httpd_register_uri_handler(server_handle, &favicon_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GET /favicon.ico: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered GET /favicon.ico");
    }

    // ───────────── DEVICE INFO ─────────────

    // GET /api/device/info
    httpd_uri_t get_device_info_uri = {.uri = "/api/device/info",
                                       .method = HTTP_GET,
                                       .handler = infoHandlerWrapper,
                                       .user_ctx = (void*)this};
    err = httpd_register_uri_handler(server_handle, &get_device_info_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GET /api/device/info: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered GET /api/device/info");
    }

    // PATCH /api/device/info
    httpd_uri_t patch_device_info_uri = {.uri = "/api/device/info",
                                         .method = HTTP_PATCH,
                                         .handler = patchDeviceInfoHandlerWrapper,
                                         .user_ctx = (void*)this};
    err = httpd_register_uri_handler(server_handle, &patch_device_info_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register PATCH /api/device/info: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered PATCH /api/device/info");
    }

    // ───────────── NETWORK CONFIG ─────────────

    // POST /api/network/ap/set
    httpd_uri_t post_ap_config_uri = {.uri = "/api/network/ap/set",
                                      .method = HTTP_POST,
                                      .handler = postApConfigHandlerWrapper,
                                      .user_ctx = (void*)this};
    err = httpd_register_uri_handler(server_handle, &post_ap_config_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register POST /api/network/ap/set: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered POST /api/network/ap/set");
    }

    // POST /api/network/sta/connect
    httpd_uri_t post_sta_connect_uri = {.uri = "/api/network/sta/connect",
                                        .method = HTTP_POST,
                                        .handler = staConnectHandlerWrapper,
                                        .user_ctx = (void*)this};
    err = httpd_register_uri_handler(server_handle, &post_sta_connect_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register POST /api/network/sta/connect: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered POST /api/network/sta/connect");
    }

    // POST /api/network/sta/disconnect
    httpd_uri_t post_sta_disconnect_uri = {.uri = "/api/network/sta/disconnect",
                                           .method = HTTP_POST,
                                           .handler = staDisconnectHandlerWrapper,
                                           .user_ctx = (void*)this};
    err = httpd_register_uri_handler(server_handle, &post_sta_disconnect_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register POST /api/network/sta/disconnect: %s",
                 esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered POST /api/network/sta/disconnect");
    }

    // GET /api/network/status
    httpd_uri_t get_network_status_uri = {.uri = "/api/network/status",
                                          .method = HTTP_GET,
                                          .handler = networkStatusHandlerWrapper,
                                          .user_ctx = (void*)this};
    err = httpd_register_uri_handler(server_handle, &get_network_status_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GET /api/network/status: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered GET /api/network/status");
    }
}

esp_err_t HttpServer::rootHandler(httpd_req_t* req) {
    float temp = DS18B20SensorManager::getLastTemperature();
    bool ok = DS18B20SensorManager::getSensorStatus();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", temp);
    cJSON_AddBoolToObject(root, "sensor_ok", ok);

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);

    return ret;
}

// GET /api/device/info
esp_err_t HttpServer::infoHandler(httpd_req_t* req) {
    DeviceInfo device_info = ConfigManager::getInstance().getDeviceInfo();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_name", device_info.device_name);
    cJSON_AddStringToObject(root, "firmware_version", device_info.firmware_version);

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);

    return ret;
}

// PATCH /api/device/info
esp_err_t HttpServer::patchDeviceInfoHandler(httpd_req_t* req) {
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");

    buf[len] = '\0';
    cJSON* json = cJSON_Parse(buf);
    if (!json) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

    const cJSON* name = cJSON_GetObjectItem(json, "device_name");
    if (!cJSON_IsString(name) || strlen(name->valuestring) >= DEVICE_NAME_MAX_LEN) {
        cJSON_Delete(json);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid device_name");
    }

    DeviceInfo device_info = ConfigManager::getInstance().getDeviceInfo();
    strncpy(device_info.device_name, name->valuestring, DEVICE_NAME_MAX_LEN - 1);
    device_info.device_name[DEVICE_NAME_MAX_LEN - 1] = '\0';
    ConfigManager::getInstance().updateDeviceInfo(device_info);

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "device name updated");

    char* resp_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    cJSON_Delete(json);

    return ret;
}

// POST /api/network/ap/set
esp_err_t HttpServer::postApConfigHandler(httpd_req_t* req) {
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
esp_err_t HttpServer::staConnectHandler(httpd_req_t* req) {
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
esp_err_t HttpServer::staDisconnectHandler(httpd_req_t* req) {
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
esp_err_t HttpServer::networkStatusHandler(httpd_req_t* req) {
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

// Wrappers
esp_err_t HttpServer::rootHandlerWrapper(httpd_req_t* req) {
    return rootHandler(req);
}

esp_err_t HttpServer::infoHandlerWrapper(httpd_req_t* req) {
    return static_cast<HttpServer*>(req->user_ctx)->infoHandler(req);
}

esp_err_t HttpServer::patchDeviceInfoHandlerWrapper(httpd_req_t* req) {
    return static_cast<HttpServer*>(req->user_ctx)->patchDeviceInfoHandler(req);
}

esp_err_t HttpServer::postApConfigHandlerWrapper(httpd_req_t* req) {
    return static_cast<HttpServer*>(req->user_ctx)->postApConfigHandler(req);
}

esp_err_t HttpServer::staConnectHandlerWrapper(httpd_req_t* req) {
    return static_cast<HttpServer*>(req->user_ctx)->staConnectHandler(req);
}

esp_err_t HttpServer::staDisconnectHandlerWrapper(httpd_req_t* req) {
    return static_cast<HttpServer*>(req->user_ctx)->staDisconnectHandler(req);
}

esp_err_t HttpServer::networkStatusHandlerWrapper(httpd_req_t* req) {
    return static_cast<HttpServer*>(req->user_ctx)->networkStatusHandler(req);
}

void HttpServer::start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server_handle, &config) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started");
        registerEndpoints();
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}

void HttpServer::stop() {
    if (server_handle) {
        httpd_stop(server_handle);
        server_handle = nullptr;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}
