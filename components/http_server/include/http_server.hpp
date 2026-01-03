#pragma once

#include "config_manager.hpp"
#include "esp_http_server.h"

namespace Handlers
{
// void registerRootEndpoints(httpd_handle_t server, void* ctx);
void registerDeviceEndpoints(httpd_handle_t server, void* ctx);
// void registerNetworkEndpoints(httpd_handle_t server, void* ctx);
} // namespace Handlers

class HttpServer
{
  public:
    HttpServer();

    void start();
    void stop();

  private:
    httpd_handle_t server_handle{nullptr};
    void registerAllEndpoints();
};
