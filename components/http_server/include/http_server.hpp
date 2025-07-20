#pragma once

#include "config_manager.hpp"
#include "esp_http_server.h"

class HttpServer {
   public:
    explicit HttpServer(const DeviceInfo& info);

    void start();
    void stop();

   private:
    httpd_handle_t server_handle = nullptr;
    const DeviceInfo& device_info;

    void registerEndpoints();

    // Handlers
    static esp_err_t infoHandler(httpd_req_t* req);
    static esp_err_t patchDeviceInfoHandler(httpd_req_t* req);
    static esp_err_t postApConfigHandler(httpd_req_t* req);
    static esp_err_t staConnectHandler(httpd_req_t* req);
    static esp_err_t staDisconnectHandler(httpd_req_t* req);
    static esp_err_t networkStatusHandler(httpd_req_t* req);
    static esp_err_t rootHandler(httpd_req_t* req);

    // Wrappers
    static esp_err_t infoHandlerWrapper(httpd_req_t* req);
    static esp_err_t patchDeviceInfoHandlerWrapper(httpd_req_t* req);
    static esp_err_t postApConfigHandlerWrapper(httpd_req_t* req);
    static esp_err_t staConnectHandlerWrapper(httpd_req_t* req);
    static esp_err_t staDisconnectHandlerWrapper(httpd_req_t* req);
    static esp_err_t networkStatusHandlerWrapper(httpd_req_t* req);
    static esp_err_t rootHandlerWrapper(httpd_req_t* req);
};
