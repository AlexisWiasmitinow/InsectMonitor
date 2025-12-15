#include "status_handler.h"

#include "esp_log.h"
#include "esp_wifi.h"

#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "cJSON.h"
#include <string.h>

#include "utils.h"
#include "sensors_handler.h"
#include "sd_storage.h"

#define STATUS_KEY_MOTIONDETECTED "motionDetected"
#define STATUS_KEY_WIFISTATE "wifiState"
#define STATUS_KEY_TEMPERATURE "temperature"
#define STATUS_KEY_HUMIDIDTY "humidity"
#define STATUS_KEY_LASTPHOTO "lastPhotoTS"
#define STATUS_KEY_SYSTEMTIME "systemTime"
#define STATUS_KEY_WIFI_SIGNAL "wifiSignal"
#define STATUS_KEY_LASTMOTION "lastMotionTS"

typedef struct status {
  bool motionDetected;
  float temperature;
  float humidity;
  time_t lastPhotoTS;
  time_t lastMotionTS;
  char systemTime[64];
  bool wifiState;

} status_t;

static status_t system_status = {0};

static const char *TAG = "status_handler";

static SemaphoreHandle_t status_semaphore;

static esp_err_t get_temperature_humidity(float *_temp, float *_hum) {
  esp_err_t ret = ESP_OK;
  ret |= sens_handler_get_temp(_temp);
  ret |= sens_handler_get_humidity(_hum);
  return ret;
}

static esp_err_t get_lates_photo_ts(time_t *_ts) {
  if (_ts) {
    time_t ts = sd_storage_csv_get_newest_photo(NULL);
    *_ts = ts;
    return ESP_OK;
  }
  return ESP_FAIL;
}

static void status_handler_task(void *pvParameters) {
  while (1) {
    if (xSemaphoreTake(status_semaphore, portMAX_DELAY) == pdTRUE) {
      esp_err_t ret = get_temperature_humidity(&system_status.temperature,
                                               &system_status.humidity);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read temperature and humidity");
      }
      xSemaphoreGive(status_semaphore);
    }
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

esp_err_t status_handler_init(void) {
  status_semaphore = xSemaphoreCreateBinary();
  if (!status_semaphore) {
    return ESP_FAIL;
  }
  xSemaphoreGive(status_semaphore);
  xTaskCreate(status_handler_task, "status_handler", 2048, NULL,
              configMAX_PRIORITIES - 10, NULL);
  return ESP_OK;
}

esp_err_t status_handler_motion_detected(bool detected, int64_t timestamp) {
  system_status.motionDetected = detected;
  if (timestamp > 0) {
    system_status.lastMotionTS = timestamp;
  }
  return ESP_OK;
}

esp_err_t status_handler_wifi_set_state(bool state)
{
  system_status.wifiState = state;
  return ESP_OK;
}

esp_err_t status_handler_serialize_status(char *buf, int buf_len) {
  wifi_ap_record_t ap_info = {0};
  esp_wifi_sta_get_ap_info(&ap_info);

  get_timestamp(system_status.systemTime, sizeof(system_status.systemTime));
  get_lates_photo_ts(&system_status.lastPhotoTS);

  cJSON *root = cJSON_CreateObject();
  cJSON_AddBoolToObject(root, STATUS_KEY_MOTIONDETECTED,
                        system_status.motionDetected);
  cJSON_AddBoolToObject(root, STATUS_KEY_WIFISTATE,
                        system_status.wifiState);
  cJSON_AddNumberToObject(root, STATUS_KEY_TEMPERATURE,
                          system_status.temperature);
  cJSON_AddNumberToObject(root, STATUS_KEY_HUMIDIDTY, system_status.humidity);
  cJSON_AddNumberToObject(root, STATUS_KEY_LASTPHOTO,
                          system_status.lastPhotoTS);
  cJSON_AddNumberToObject(root, STATUS_KEY_LASTMOTION,
                          system_status.lastMotionTS);
  cJSON_AddStringToObject(root, STATUS_KEY_SYSTEMTIME,
                          system_status.systemTime);
  cJSON_AddNumberToObject(root, STATUS_KEY_WIFI_SIGNAL,
                          ap_info.rssi);
  esp_err_t ret =
      cJSON_PrintPreallocated(root, buf, buf_len, 0) ? ESP_OK : ESP_FAIL;
  cJSON_Delete(root);

  return ret;
}