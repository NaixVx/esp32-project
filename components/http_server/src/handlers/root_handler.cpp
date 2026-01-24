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
    ESP_LOGI(TAG, "GET /");

    FILE* f = fopen("/www/index.html", "r");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "index.html not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");

    char buf[512];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, read_bytes);
    }

    fclose(f);
    httpd_resp_send_chunk(req, nullptr, 0);
    return ESP_OK;
}

static esp_err_t cssHandler(httpd_req_t* req)
{
    FILE* f = fopen("/www/style.css", "r");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "style.css not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/css");

    char buf[512];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, read_bytes);
    }

    fclose(f);
    httpd_resp_send_chunk(req, nullptr, 0);
    return ESP_OK;
}

static esp_err_t jsHandler(httpd_req_t* req)
{
    FILE* f = fopen("/www/app.js", "r");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "app.js not found");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/javascript");

    char buf[512];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, read_bytes);
    }

    fclose(f);
    httpd_resp_send_chunk(req, nullptr, 0);
    return ESP_OK;
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

    // GET /style.css
    httpd_uri_t css_uri = {
        .uri = "/style.css",
        .method = HTTP_GET,
        .handler = cssHandler,
        .user_ctx = nullptr,
    };
    err = httpd_register_uri_handler(server, &css_uri);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Registered GET /style.css");
    } else {
        ESP_LOGE(TAG, "Failed to register GET /style.css: %s", esp_err_to_name(err));
    }

    // GET /app.js
    httpd_uri_t js_uri = {
        .uri = "/app.js",
        .method = HTTP_GET,
        .handler = jsHandler,
        .user_ctx = nullptr,
    };
    err = httpd_register_uri_handler(server, &js_uri);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Registered GET /app.js");
    } else {
        ESP_LOGE(TAG, "Failed to register GET /app.js: %s", esp_err_to_name(err));
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
