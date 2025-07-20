#pragma once

#include "config_manager.hpp"  // You can create this to define your DeviceConfig struct or just include necessary headers
#include "esp_http_server.h"

class HttpServer {
   public:
    HttpServer(const DeviceInfo& info);

    void start();
    void stop();

   private:
    httpd_handle_t server_handle = nullptr;
    const DeviceInfo& device_info;

    void registerEndpoints();

    static esp_err_t infoHandler(httpd_req_t* req);
    static esp_err_t rootHandler(httpd_req_t* req);
    static esp_err_t infoHandlerWrapper(httpd_req_t* req);
    static esp_err_t rootHandlerWrapper(httpd_req_t* req);
};
