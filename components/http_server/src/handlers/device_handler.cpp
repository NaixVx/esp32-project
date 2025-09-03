#include "handlers/device_handler.hpp"

#include "cJSON.h"
#include "config_manager.hpp"
#include "esp_log.h"

static const char* TAG = "device_handler";

// GET /api/device/info
static esp_err_t infoHandler(httpd_req_t* req) {
    DeviceInfo device_info = ConfigManager::getInstance().getDeviceInfo();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_name", device_info.device_name);
    cJSON_AddStringToObject(root, "firmware_version", device_info.firmware_version);

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);

    return ret;
}

// PATCH /api/device/info
static esp_err_t patchDeviceInfoHandler(httpd_req_t* req) {
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");

    buf[len] = '\0';
    cJSON* json = cJSON_Parse(buf);
    if (!json) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

    const cJSON* name = cJSON_GetObjectItem(json, "device_name");
    if (!cJSON_IsString(name) || strlen(name->valuestring) >= DEVICE_NAME_MAX_LEN) {
        cJSON_Delete(json);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid device_name");
    }

    DeviceInfo device_info = ConfigManager::getInstance().getDeviceInfo();
    strncpy(device_info.device_name, name->valuestring, DEVICE_NAME_MAX_LEN - 1);
    device_info.device_name[DEVICE_NAME_MAX_LEN - 1] = '\0';
    ConfigManager::getInstance().updateDeviceInfo(device_info);

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "device name updated");

    char* resp_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    cJSON_Delete(json);

    return ret;
}

// Registration
void Handlers::registerDeviceEndpoints(httpd_handle_t server, void* ctx) {
    esp_err_t err;

    // GET /api/device/info
    httpd_uri_t get_device_info_uri = {
        .uri = "/api/device/info",
        .method = HTTP_GET,
        .handler = infoHandler,
        .user_ctx = ctx  // optional: pass extra data if needed
    };
    err = httpd_register_uri_handler(server, &get_device_info_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register GET /api/device/info: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered GET /api/device/info");
    }

    // PATCH /api/device/info
    httpd_uri_t patch_device_info_uri = {.uri = "/api/device/info",
                                         .method = HTTP_PATCH,
                                         .handler = patchDeviceInfoHandler,
                                         .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &patch_device_info_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register PATCH /api/device/info: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registered PATCH /api/device/info");
    }
}