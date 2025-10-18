#include "handlers/device_handler.hpp"

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

// TODO: move to utils component
// Utility: read request body fully and null-terminate into dst (truncates if too long)
static bool read_body_into(httpd_req_t* req, char* dst, size_t dst_size, int* out_len)
{
    size_t total = req->content_len;
    if (dst_size == 0)
        return false;
    size_t to_read = total;
    size_t written = 0;
    while (to_read > 0) {
        int chunk = httpd_req_recv(req, dst + written, std::min(to_read, dst_size - 1 - written));
        if (chunk <= 0)
            return false;
        written += chunk;
        to_read -= chunk;
        if (written >= dst_size - 1)
            break; // truncate extra
    }
    dst[written] = '\0';
    if (out_len)
        *out_len = (int)written;
    // Drain leftovers if truncated
    while (to_read > 0) {
        char junk[64];
        int chunk = httpd_req_recv(req, junk, std::min<size_t>(to_read, sizeof(junk)));
        if (chunk <= 0)
            break;
        to_read -= chunk;
    }
    return true;
}

// GET /api/device/info
static esp_err_t infoHandler(httpd_req_t* req)
{
    DeviceInfo device_info = ConfigManager::getInstance().getDeviceInfo();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_name", device_info.device_name);
    cJSON_AddStringToObject(root, "firmware_version", device_info.firmware_version);

    char* resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    set_json_headers(req);
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    return ret;
}

// PATCH /api/device/info
static esp_err_t patchDeviceInfoHandler(httpd_req_t* req)
{
    char buf[256];
    int len = 0;
    if (!read_body_into(req, buf, sizeof(buf), &len) || len <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
    }

    cJSON* json = cJSON_Parse(buf);
    if (!json)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");

    const cJSON* name = cJSON_GetObjectItem(json, "device_name");
    if (!cJSON_IsString(name) || strlen(name->valuestring) >= DEVICE_NAME_MAX_LEN) {
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

static esp_err_t optionsDeviceInfo(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,PATCH,OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    // 204-style empty response is fine
    return httpd_resp_send(req, nullptr, 0);
}

// Registration
void Handlers::registerDeviceEndpoints(httpd_handle_t server, void* ctx)
{
    esp_err_t err;

    httpd_uri_t get_device_info_uri = {.uri = "/api/device/info",
        .method = HTTP_GET,
        .handler = infoHandler,
        .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &get_device_info_uri);

    httpd_uri_t patch_device_info_uri = {.uri = "/api/device/info",
        .method = HTTP_PATCH,
        .handler = patchDeviceInfoHandler,
        .user_ctx = ctx};
    err = httpd_register_uri_handler(server, &patch_device_info_uri);

    // NEW: CORS preflight for PATCH/GET from browsers
    httpd_uri_t options_device_info_uri = {.uri = "/api/device/info",
        .method = HTTP_OPTIONS,
        .handler = optionsDeviceInfo,
        .user_ctx = ctx};
    httpd_register_uri_handler(server, &options_device_info_uri); // ignore error if duplicate
}
