#include "http_server.hpp"

#include "esp_log.h"

// Handlers
#include "handlers/device_handler.hpp"
#include "handlers/network_handler.hpp"
#include "handlers/root_handler.hpp"

static const char* TAG = "http_server";

HttpServer::HttpServer(const DeviceInfo& info) : device_info(info) {}

void HttpServer::registerAllEndpoints() {
    Handlers::registerRootEndpoints(server_handle, (void*)this);
    Handlers::registerDeviceEndpoints(server_handle, (void*)this);
    Handlers::registerNetworkEndpoints(server_handle, (void*)this);
}

void HttpServer::start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server_handle, &config) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started");
        registerAllEndpoints();
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
