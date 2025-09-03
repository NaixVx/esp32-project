#include "handlers/root_handler.hpp"

#include "cJSON.h"
#include "esp_log.h"
#include "sensor_manager.hpp"

static const char* TAG = "root_handler";

static esp_err_t rootHandler(httpd_req_t* req) {
    float temp = DS18B20SensorManager::getLastTemperature();
    bool ok = DS18B20SensorManager::getSensorStatus();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", temp);
    cJSON_AddBoolToObject(root, "sensor_ok", ok);

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);

    return ret;
}

// Registration
void Handlers::registerRootEndpoints(httpd_handle_t server, void* ctx) {
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = rootHandler,
        .user_ctx = ctx,
    };

    esp_err_t err = httpd_register_uri_handler(server, &root_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GET /: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered GET /");
    }

    // Favicon (empty)
    httpd_uri_t favicon_uri = {.uri = "/favicon.ico",
                               .method = HTTP_GET,
                               .handler =
                                   [](httpd_req_t* req) {
                                       httpd_resp_set_type(req, "image/x-icon");
                                       return httpd_resp_send(req, NULL, 0);
                                   },
                               .user_ctx = nullptr};
    err = httpd_register_uri_handler(server, &favicon_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GET /favicon.ico: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered GET /favicon.ico");
    }
}
