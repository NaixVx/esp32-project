#include "http_server.hpp"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "sensor_manager.hpp"

static const char* TAG = "http_server";

HttpServer::HttpServer(const DeviceInfo& info) : device_info(info) {}

void HttpServer::registerEndpoints() {
    httpd_uri_t info_uri = {
        .uri = "/info", .method = HTTP_GET, .handler = infoHandlerWrapper, .user_ctx = (void*)this};
    httpd_register_uri_handler(server_handle, &info_uri);

    httpd_uri_t root_uri = {
        .uri = "/", .method = HTTP_GET, .handler = rootHandlerWrapper, .user_ctx = (void*)this};
    httpd_register_uri_handler(server_handle, &root_uri);

    httpd_uri_t favicon_uri = {.uri = "/favicon.ico",
                               .method = HTTP_GET,
                               .handler =
                                   [](httpd_req_t* req) {
                                       httpd_resp_set_type(req, "image/x-icon");
                                       return httpd_resp_send(req, NULL, 0);  // Empty favicon
                                   },
                               .user_ctx = nullptr};
    httpd_register_uri_handler(server_handle, &favicon_uri);
}

esp_err_t HttpServer::infoHandler(httpd_req_t* req) {
    auto* server = static_cast<HttpServer*>(req->user_ctx);

    char resp[256];
    snprintf(resp, sizeof(resp), "{ \"device_name\": \"%s\", \"firmware_version\": \"%s\"}",
             server->device_info.device_name, server->device_info.firmware_version);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

esp_err_t HttpServer::rootHandler(httpd_req_t* req) {
    float temp = DS18B20SensorManager::getLastTemperature();
    bool ok = DS18B20SensorManager::getSensorStatus();

    char resp[128];
    snprintf(resp, sizeof(resp), "{ \"temperature\": %.2f, \"sensor_ok\": %s }", temp,
             ok ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

esp_err_t HttpServer::infoHandlerWrapper(httpd_req_t* req) {
    return static_cast<HttpServer*>(req->user_ctx)->infoHandler(req);
}

esp_err_t HttpServer::rootHandlerWrapper(httpd_req_t* req) {
    return rootHandler(req);
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
