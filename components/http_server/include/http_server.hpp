#pragma once

#include "esp_http_server.h"

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
