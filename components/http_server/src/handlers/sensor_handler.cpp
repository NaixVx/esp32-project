#include "handlers/sensor_handler.hpp"

#include "temperature_sensor.hpp"

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "sensor_handler";

static inline void set_json_headers(httpd_req_t *req) {
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
}

static esp_err_t optionsSensorState(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,OPTIONS");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

  return httpd_resp_send(req, nullptr, 0);
}

// GET /api/sensors/state
static esp_err_t sensorHandler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/sensors/state");

  int16_t temp_multiplied =
      (int16_t)(temperature_sensor::get_last_temperature_C() * 10.0f + 0.5f);

  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "id", 0);
  cJSON_AddNumberToObject(root, "status", temperature_sensor::get_status());
  cJSON_AddNumberToObject(root, "temperature_c_x10", temp_multiplied);

  char *resp = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  set_json_headers(req);

  esp_err_t err = httpd_resp_send(req, resp, strlen(resp));
  free(resp);
  return err;
}

void Handlers::registerSensorEndpoints(httpd_handle_t server, void *ctx) {
  esp_err_t err;

  // GET /api/sensors/state
  httpd_uri_t get_sensors_state_uri = {.uri = "/api/sensors/state",
                                       .method = HTTP_GET,
                                       .handler = sensorHandler,
                                       .user_ctx = ctx};
  err = httpd_register_uri_handler(server, &get_sensors_state_uri);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "Registered GET /api/sensors/state");
  } else {
    ESP_LOGE(TAG, "Failed to register GET /api/sensors/state: %s",
             esp_err_to_name(err));
  }

  // OPTIONS /api/sensors/state
  httpd_uri_t options_sensors_state_uri = {.uri = "/api/sensors/state",
                                           .method = HTTP_OPTIONS,
                                           .handler = optionsSensorState,
                                           .user_ctx = ctx};
  err = httpd_register_uri_handler(server, &options_sensors_state_uri);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "Registered OPTIONS /api/sensors/state");
  } else {
    ESP_LOGE(TAG, "Failed to register OPTIONS /api/sensors/state: %s",
             esp_err_to_name(err));
  }
}
