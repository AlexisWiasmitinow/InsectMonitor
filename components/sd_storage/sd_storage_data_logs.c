#include "sd_storage.h"

#include <stdbool.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <esp_vfs_fat.h>

#include <cJSON.h>

#define HUM_LOGS_PATH DATA_LOGS_PATH "/hum"
#define TEMP_LOGS_PATH DATA_LOGS_PATH "/temp"

static void get_path(data_logs_type_t type, char *path, time_t ts_now) {
  if (!path) {
    return;
  }

  struct tm timeinfo = {0};
  time(&ts_now);
  localtime_r(&ts_now, &timeinfo);
  timeinfo.tm_year += 1900;
  if (type == DATA_LOGS_TYPE_TEMPERATURE) {
    sprintf(path, TEMP_LOGS_PATH "_%d_%d_%d" CSV_FILE_EXT, timeinfo.tm_year,
            timeinfo.tm_mon, timeinfo.tm_mday);
  } else if (type == DATA_LOGS_TYPE_HUMIDITY) {
    sprintf(path, HUM_LOGS_PATH "_%d_%d_%d" CSV_FILE_EXT, timeinfo.tm_year,
            timeinfo.tm_mon, timeinfo.tm_mday);
  }
}

esp_err_t sd_storage_data_logs_add_entry(data_logs_type_t type, float value,
                                         char state, long long timestamp) {
  esp_err_t ret = ESP_OK;
  char path[256] = {0};
  char row[50] = {0};

  get_path(type, path, timestamp);
  if (strlen(path) > 5) {
    FILE *f = fopen(path, "a");
    if (f && value > 0) {
      sprintf(row, "%lld,%.1f,%d\n", timestamp, value, state);
      fwrite(row, 1, strlen(row), f);
    }
    fclose(f);
  }

  return ret;
}

esp_err_t sd_storage_data_logs_get_list(char *list, int list_len) {
  const char extension[] = ".csv";

  size_t ext_len = strlen(extension);

  DIR *d = opendir(DATA_LOGS_PATH);
  struct dirent *dir;
  cJSON *_root = cJSON_CreateObject();
  cJSON_AddItemToObject(_root, "humidity", cJSON_CreateArray());
  cJSON_AddItemToObject(_root, "temperature", cJSON_CreateArray());

  if (d) {
    while ((dir = readdir(d)) != NULL) {
      size_t filename_len = strlen(dir->d_name);
      if (filename_len > ext_len &&
          !strcmp(dir->d_name + filename_len - ext_len, extension)) {
        if (strstr(dir->d_name, "hum") > 0) {
          cJSON_AddItemToArray(cJSON_GetObjectItem(_root, "humidity"),
                               cJSON_CreateString(dir->d_name));
        } else if (strstr(dir->d_name, "temp") > 0) {
          cJSON_AddItemToArray(cJSON_GetObjectItem(_root, "temperature"),
                               cJSON_CreateString(dir->d_name));
        }
      }
    }
    closedir(d);

    cJSON_PrintPreallocated(_root, list, list_len, false);
    cJSON_Delete(_root);

    return ESP_OK;
  }
  return ESP_ERR_INVALID_ARG;
}