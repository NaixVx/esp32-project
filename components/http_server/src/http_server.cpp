#include "http_server.hpp"

#include "esp_log.h"

// Handlers
#include "handlers/device_handler.hpp"
#include "handlers/network_handler.hpp"
#include "handlers/root_handler.hpp"

static const char* TAG = "http_server";

HttpServer::HttpServer() = default;

void HttpServer::registerAllEndpoints()
{
    Handlers::registerRootEndpoints(server_handle, (void*)this);
    Handlers::registerDeviceEndpoints(server_handle, (void*)this);
    Handlers::registerNetworkEndpoints(server_handle, (void*)this);
}

void HttpServer::start()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;

    esp_err_t err = httpd_start(&server_handle, &config);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP server started");
        registerAllEndpoints();
    } else {
        server_handle = nullptr;
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
    }
}

void HttpServer::stop()
{
    if (server_handle) {
        httpd_stop(server_handle);
        server_handle = nullptr;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}
