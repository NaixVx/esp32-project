#include "handlers/root_handler.hpp"

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char* TAG = "root_handler";

static inline void set_json_headers(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
}

static esp_err_t optionsRoot(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    return httpd_resp_send(req, nullptr, 0);
}

// GET /
static esp_err_t rootHandler(httpd_req_t* req)
{
    // mock values
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", 20);
    cJSON_AddStringToObject(root, "unit", "C");
    cJSON_AddBoolToObject(root, "sensor_ok", true);

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    set_json_headers(req);

    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    return ret;
}

// Registration
void Handlers::registerRootEndpoints(httpd_handle_t server, void* ctx)
{
    // OPTIONS / (CORS preflight)
    httpd_uri_t options_uri = {
        .uri = "/",
        .method = HTTP_OPTIONS,
        .handler = optionsRoot,
        .user_ctx = ctx,
    };
    httpd_register_uri_handler(server, &options_uri); // ignore error if unsupported

    // GET /
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
                return httpd_resp_send(req, nullptr, 0);
            },
        .user_ctx = nullptr};
    err = httpd_register_uri_handler(server, &favicon_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GET /favicon.ico: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered GET /favicon.ico");
    }
}
