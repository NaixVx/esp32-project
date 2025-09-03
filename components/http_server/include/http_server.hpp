#pragma once

#include "config_manager.hpp"
#include "esp_http_server.h"

// Forward declarations of handler registration
namespace Handlers {
void registerRootEndpoints(httpd_handle_t server, void* ctx);
void registerDeviceEndpoints(httpd_handle_t server, void* ctx);
void registerNetworkEndpoints(httpd_handle_t server, void* ctx);
}  // namespace Handlers

class HttpServer {
   public:
    explicit HttpServer(const DeviceInfo& info);

    void start();
    void stop();

   private:
    httpd_handle_t server_handle = nullptr;
    const DeviceInfo& device_info;

    void registerAllEndpoints();
};
