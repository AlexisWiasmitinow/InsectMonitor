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
#include "sd_storage.h"

#define STATUS_KEY_WIFISTATE "wifiState"
#define STATUS_KEY_SYSTEMTIME "systemTime"
#define STATUS_KEY_WIFI_SIGNAL "wifiSignal"

typedef struct status {
  char systemTime[64];
  bool wifiState;

} status_t;

static status_t system_status = {0};

static SemaphoreHandle_t status_semaphore;

esp_err_t status_handler_init(void) {
  status_semaphore = xSemaphoreCreateBinary();
  if (!status_semaphore) {
    return ESP_FAIL;
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

  cJSON *root = cJSON_CreateObject();
  cJSON_AddBoolToObject(root, STATUS_KEY_WIFISTATE,
                        system_status.wifiState);
  cJSON_AddStringToObject(root, STATUS_KEY_SYSTEMTIME,
                          system_status.systemTime);
  cJSON_AddNumberToObject(root, STATUS_KEY_WIFI_SIGNAL,
                          ap_info.rssi);
  esp_err_t ret =
      cJSON_PrintPreallocated(root, buf, buf_len, 0) ? ESP_OK : ESP_FAIL;
  cJSON_Delete(root);

  return ret;
}