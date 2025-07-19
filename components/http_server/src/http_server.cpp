#include "http_server.hpp"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"

static const char* TAG = "http_server";

HttpServer::HttpServer(const DeviceInfo& info) : device_info(info) {}

void HttpServer::registerEndpoints() {
    httpd_uri_t info_uri = {
        .uri = "/info", .method = HTTP_GET, .handler = infoHandlerWrapper, .user_ctx = (void*)this};
    httpd_register_uri_handler(server_handle, &info_uri);
}

esp_err_t HttpServer::infoHandler(httpd_req_t* req) {
    auto* server = static_cast<HttpServer*>(req->user_ctx);

    char resp[256];
    snprintf(resp, sizeof(resp),
             "{ \"device_name\": \"%s\", \"firmware_version\": \"%s\", \"max_connections\": %d }",
             server->device_info.device_name, server->device_info.firmware_version,
             server->device_info.max_connections);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

esp_err_t HttpServer::infoHandlerWrapper(httpd_req_t* req) {
    return static_cast<HttpServer*>(req->user_ctx)->infoHandler(req);
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
