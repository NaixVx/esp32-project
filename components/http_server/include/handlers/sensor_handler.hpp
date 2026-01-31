#pragma once

#include "esp_http_server.h"

namespace Handlers
{
void registerSensorEndpoints(httpd_handle_t server, void* ctx);
}
