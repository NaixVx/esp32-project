#include "handlers/device_handler.hpp"

#include "utils/http_utils.hpp"

#include "cJSON.h"
#include "config_manager.hpp"
#include "esp_http_server.h"
#include "esp_log.h"
#include <algorithm>

static const char* TAG = "device_handler";

static inline void set_json_headers(httpd_req_t* req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
}

static esp_err_t optionsDeviceInfo(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

    return httpd_resp_send(req, nullptr, 0);
}

// GET /api/device/info
static esp_err_t infoHandler(httpd_req_t* req)
{
    DeviceInfo device_info = ConfigManager::getInstance().getDeviceInfo();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "id", device_info.id);
    cJSON_AddStringToObject(root, "device_name", device_info.device_name);
    cJSON_AddStringToObject(root, "device_type", device_info.device_type);
    cJSON_AddStringToObject(root, "firmware_version", device_info.firmware_version);

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    set_json_headers(req);
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    return ret;
}

// POST /api/device/info
static esp_err_t postDeviceInfoHandler(httpd_req_t* req)
{
    char buf[256];
    int len = 0;
    if (!http_read_body(req, buf, sizeof(buf), &len) || len <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
    }

    cJSON* json = cJSON_Parse(buf);
    if (!json)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

    const cJSON* name = cJSON_GetObjectItem(json, "device_name");
    if (!cJSON_IsString(name) || strlen(name->valuestring) == 0 ||
        strlen(name->valuestring) >= DEVICE_NAME_MAX_LEN) {
        cJSON_Delete(json);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid device_name");
    }

    DeviceInfo device_info = ConfigManager::getInstance().getDeviceInfo();
    strncpy(device_info.device_name, name->valuestring, DEVICE_NAME_MAX_LEN - 1);
    device_info.device_name[DEVICE_NAME_MAX_LEN - 1] = '\0';

    esp_err_t err = ConfigManager::getInstance().updateDeviceInfo(device_info);
    cJSON* resp = cJSON_CreateObject();
    if (err == ESP_OK) {
        cJSON_AddStringToObject(resp, "status", "device name updated");
    } else {
        cJSON_AddStringToObject(resp, "status", "failed to update");
        cJSON_AddStringToObject(resp, "error", esp_err_to_name(err));
    }

    char* resp_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    set_json_headers(req);
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    cJSON_Delete(json);
    return ret;
}

void Handlers::registerDeviceEndpoints(httpd_handle_t server, void* ctx)
{
    esp_err_t err;

    // GET /api/device/info
    httpd_uri_t get_device_info_uri = {.uri = "/api/device/info",
        .method = HTTP_GET,
        .handler = infoHandler,
        .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &get_device_info_uri);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Registered GET /api/device/info");
    } else {
        ESP_LOGE(TAG, "Failed to register GET /api/device/info: %s", esp_err_to_name(err));
    }

    // POST /api/device/info
    httpd_uri_t post_device_info_uri = {.uri = "/api/device/info",
        .method = HTTP_POST,
        .handler = postDeviceInfoHandler,
        .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &post_device_info_uri);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Registered POST /api/device/info");
    } else {
        ESP_LOGE(TAG, "Failed to register POST /api/device/info: %s", esp_err_to_name(err));
    }

    // OPTIONS /api/device/info
    httpd_uri_t options_device_info_uri = {.uri = "/api/device/info",
        .method = HTTP_OPTIONS,
        .handler = optionsDeviceInfo,
        .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &options_device_info_uri);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Registered OPTIONS /api/device/info");
    } else {
        ESP_LOGE(TAG, "Failed to register OPTIONS /api/device/info: %s", esp_err_to_name(err));
    }
}
