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
    httpd_register_uri_handler(server_handle, &root_uri);

    // Favicon (empty)
    httpd_uri_t favicon_uri = {.uri = "/favicon.ico",
                               .method = HTTP_GET,
                               .handler =
                                   [](httpd_req_t* req) {
                                       httpd_resp_set_type(req, "image/x-icon");
                                       return httpd_resp_send(req, NULL, 0);
                                   },
                               .user_ctx = nullptr};
    httpd_register_uri_handler(server_handle, &favicon_uri);

    // ───────────── DEVICE INFO ─────────────

    // GET /api/device/info
    httpd_uri_t get_device_info_uri = {.uri = "/api/device/info",
                                       .method = HTTP_GET,
                                       .handler = infoHandlerWrapper,
                                       .user_ctx = (void*)this};
    httpd_register_uri_handler(server_handle, &get_device_info_uri);

    // PATCH /api/device/info
    httpd_uri_t patch_device_info_uri = {.uri = "/api/device/info",
                                         .method = HTTP_PATCH,
                                         .handler = patchDeviceInfoHandlerWrapper,
                                         .user_ctx = (void*)this};
    httpd_register_uri_handler(server_handle, &patch_device_info_uri);

    // ───────────── NETWORK CONFIG ─────────────

    // POST /api/network/ap
    httpd_uri_t post_ap_config_uri = {.uri = "/api/network/ap",
                                      .method = HTTP_POST,
                                      .handler = postApConfigHandlerWrapper,
                                      .user_ctx = (void*)this};
    httpd_register_uri_handler(server_handle, &post_ap_config_uri);

    // POST /api/network/sta/connect
    httpd_uri_t post_sta_connect_uri = {.uri = "/api/network/sta/connect",
                                        .method = HTTP_POST,
                                        .handler = staConnectHandlerWrapper,
                                        .user_ctx = (void*)this};
    httpd_register_uri_handler(server_handle, &post_sta_connect_uri);

    // POST /api/network/sta/disconnect
    httpd_uri_t post_sta_disconnect_uri = {.uri = "/api/network/sta/disconnect",
                                           .method = HTTP_POST,
                                           .handler = staDisconnectHandlerWrapper,
                                           .user_ctx = (void*)this};
    httpd_register_uri_handler(server_handle, &post_sta_disconnect_uri);

    // GET /api/network/status
    httpd_uri_t get_network_status_uri = {.uri = "/api/network/status",
                                          .method = HTTP_GET,
                                          .handler = networkStatusHandlerWrapper,
                                          .user_ctx = (void*)this};
    httpd_register_uri_handler(server_handle, &get_network_status_uri);
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
    auto* server = static_cast<HttpServer*>(req->user_ctx);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_name", server->device_info.device_name);
    cJSON_AddStringToObject(root, "firmware_version", server->device_info.firmware_version);
    cJSON_AddNumberToObject(root, "last_boot_ts", 123456789);

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);

    return ret;
}

// PATCH /api/device/info
esp_err_t HttpServer::patchDeviceInfoHandler(httpd_req_t* req) {
    // For now just mock success JSON
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "device info updated (mock)");

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);

    return ret;
}

// POST /api/network/ap
esp_err_t HttpServer::postApConfigHandler(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "AP config updated (mock)");

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);

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
    cJSON* root = cJSON_CreateObject();
    // mocked data
    cJSON_AddStringToObject(root, "ssid", "MockSSID");
    cJSON_AddStringToObject(root, "bssid", "AA:BB:CC:DD:EE:FF");
    cJSON_AddStringToObject(root, "ip_address", "192.168.1.100");
    cJSON_AddStringToObject(root, "mac_address", "11:22:33:44:55:66");
    cJSON_AddStringToObject(root, "ap_ssid", "ESP32_default_AP");
    cJSON_AddBoolToObject(root, "ap_enabled", true);
    cJSON_AddBoolToObject(root, "sta_enabled", false);
    cJSON_AddNumberToObject(root, "max_connections", 5);

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
