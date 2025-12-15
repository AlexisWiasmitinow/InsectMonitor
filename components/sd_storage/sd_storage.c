#include "sd_storage.h"
#include <driver/sdmmc_host.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <cJSON.h>

#include <sys/stat.h>
#include <dirent.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define DEFAULT_SETTINGS_FILE_PATH DEFAULT_BASE_PATH "/settings.json"
#define DEFAULT_JSON_BUF_SIZE (512)

static const char default_settings_file[] =
    "{\"ssid\":\"QueenBreeder\",\"password\":\"QueenBreeder\",\"wifiMode\":"
    "\"AP\",\"differenceThreshold\":15,\"motionDifference\":10,"
    "\"temperatureTarget\":25,\"tempCheckInterval\":10,\"humidityTarget\":60,"
    "\"checkMotionInterval\":10,\"pumpMsPerH\":10,\"humidityCheckInterval\":30,"
    "\"tempP\":0.1,\"tempI\":0.1,\"tempD\":0.1,\"maxPhotos\":10,\"timezone\":"
    "\"UTC0\",\"debugLevel\":2,\"cropX\":0,\"cropY\":0,\"cropWidth\":320,"
    "\"cropHeight\":240}";

static const char *TAG = "SD_STORAGE";

// buffer to allocate whole json
static char *sd_storage_json_buf = NULL;
static cJSON *root = NULL;

static SemaphoreHandle_t sd_storage_semph = NULL;

static esp_err_t init_sd_card(void) {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.flags = SDMMC_HOST_FLAG_1BIT;
  host.max_freq_khz = 10000;
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = 1;
#ifdef CONFIG_IDF_TARGET_ESP32S3
  slot_config.cmd = GPIO_NUM_38;
  slot_config.clk = GPIO_NUM_39;
  slot_config.d0 = GPIO_NUM_40;
  slot_config.d1 = GPIO_NUM_NC;
  slot_config.d2 = GPIO_NUM_NC;
  slot_config.d3 = GPIO_NUM_NC;
  slot_config.d4 = GPIO_NUM_NC;
  slot_config.d5 = GPIO_NUM_NC;
  slot_config.d6 = GPIO_NUM_NC;
  slot_config.d7 = GPIO_NUM_NC;
#endif

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = true,
      .max_files = 5,
  };
  sdmmc_card_t *card;
  esp_err_t err = esp_vfs_fat_sdmmc_mount(DEFAULT_BASE_PATH, &host,
                                          &slot_config, &mount_config, &card);
  if (err != ESP_OK) {
    return err;
  }
  sdmmc_card_print_info(stdout, card);
  return ESP_OK;
}

static esp_err_t create_default_settings_file(char *path) {
  esp_err_t err = ESP_OK;
  FILE *file = fopen(path, "w");
  if (!file) {
    return ESP_FAIL;
  }
  size_t len =
      fwrite(default_settings_file, 1, sizeof(default_settings_file), file);
  if (len != sizeof(default_settings_file)) {
    err = ESP_FAIL;
  }
  fclose(file);
  return err;
}

static esp_err_t open_settings(char *path) {
  esp_err_t err = ESP_OK;
  FILE *file = fopen(path, "r");
  if (!file) {
    fclose(file);
    err = create_default_settings_file(path);
    file = fopen(path, "r");
  }
  if ((err == ESP_OK) && file) {
    if (sd_storage_json_buf) {
      // get the settings file length
      size_t file_len = 0;
      fseek(file, 0, SEEK_END);
      file_len = ftell(file);
      rewind(file);

      if (file_len > 0) {
        fread(sd_storage_json_buf, 1, file_len, file);
        root = cJSON_Parse(sd_storage_json_buf);
        fclose(file);
      } else {
        // delete and create a new file
        fclose(file);
        create_default_settings_file(path);

        root = cJSON_Parse(default_settings_file);
      }
    }
  }
  return err;
}

static esp_err_t make_dir(char *path) {
  esp_err_t ret = ESP_FAIL;

  // check if folder exists
  struct stat st = {0};
  if (stat(path, &st) == -1) {  // it doesn't exist
    if (mkdir(path, 0755) > -1) {
      ret = ESP_OK;
    }
  } else {
    ESP_LOGD(TAG, "Folder (%s) already exists", path);
    ret = ESP_OK;
  }

  return ret;
}

esp_err_t sd_storage_update_settings(void) {
  esp_err_t ret = ESP_FAIL;
  if (xSemaphoreTake(sd_storage_semph, pdMS_TO_TICKS(1000))) {
    FILE *file = fopen(DEFAULT_SETTINGS_FILE_PATH, "w");
    if (file && (root != NULL)) {
      char *buf = cJSON_PrintUnformatted(root);
      if (buf) {
        size_t written = fwrite(buf, 1, strlen(buf), file);
        ESP_LOGI(TAG, "Written bytes: (%d)\n JSON: %s", written, buf);
        ret = ESP_OK;
      }
      cJSON_free(buf);
    }
    fclose(file);
    xSemaphoreGive(sd_storage_semph);
  }
  return ret;
}

esp_err_t sd_storage_get_float(char *key, float *val) {
  esp_err_t ret = ESP_FAIL;
  if (xSemaphoreTake(sd_storage_semph, pdMS_TO_TICKS(1000))) {
    if (root != NULL) {
      cJSON *value = cJSON_GetObjectItem(root, key);
      if (!value || !cJSON_IsNumber(value)) {
        ESP_LOGE(TAG, "Invalid JSON object for key: %s", key);
      } else {
        *val = value->valuedouble;
        ESP_LOGI(TAG, "GET FLOAT (key: %s) (val:%.2f)", key,
                 value->valuedouble);
        ret = ESP_OK;
      }
    }
    xSemaphoreGive(sd_storage_semph);
  }
  return ret;
}

esp_err_t sd_storage_set_float(char *key, float val) {
  esp_err_t ret = ESP_FAIL;
  if (xSemaphoreTake(sd_storage_semph, pdMS_TO_TICKS(1000))) {
    if (root != NULL || key != NULL) {
      cJSON *obj = cJSON_GetObjectItem(root, key);
      if (!obj) {
        obj = cJSON_AddNumberToObject(root, key, val);
      } else {
        cJSON_SetNumberValue(obj, val);
      }
      ESP_LOGI(TAG, "SET FLOAT (key: %s) (val:%.2f)", key, val);
      ret = ESP_OK;
    }
    xSemaphoreGive(sd_storage_semph);
  }
  return ret;
}

esp_err_t sd_storage_get_int(char *key, int *val) {
  esp_err_t ret = ESP_FAIL;
  if (xSemaphoreTake(sd_storage_semph, pdMS_TO_TICKS(1000))) {
    if (root != NULL) {
      cJSON *value = cJSON_GetObjectItem(root, key);
      if (!value || !cJSON_IsNumber(value)) {
        ESP_LOGE(TAG, "Invalid JSON object for key: %s", key);
      } else {
        *val = value->valueint;
        ESP_LOGI(TAG, "GET INT (key: %s) (val:%d)", key, value->valueint);
        ret = ESP_OK;
      }
    }
    xSemaphoreGive(sd_storage_semph);
  }
  return ret;
}

esp_err_t sd_storage_set_int(char *key, int val) {
  esp_err_t ret = ESP_FAIL;
  if (xSemaphoreTake(sd_storage_semph, pdMS_TO_TICKS(1000))) {
    if (root != NULL || key != NULL) {
      cJSON *obj = cJSON_GetObjectItem(root, key);
      if (!obj) {
        obj = cJSON_AddNumberToObject(root, key, val);
      } else {
        cJSON_SetNumberValue(obj, val);
      }
      ESP_LOGI(TAG, "SET INT (key: %s) (val:%d)", key, val);
      ret = ESP_OK;
    }
    xSemaphoreGive(sd_storage_semph);
  }
  return ret;
}

esp_err_t sd_storage_get_string(char *key, char *val) {
  esp_err_t ret = ESP_FAIL;
  if (xSemaphoreTake(sd_storage_semph, pdMS_TO_TICKS(1000))) {
    if (root != NULL) {
      cJSON *value = cJSON_GetObjectItem(root, key);
      if (value == NULL || !cJSON_IsString(value)) {
        ESP_LOGE(TAG, "Invalid JSON object for key: %s", key);
      } else {
        size_t len = strlen(value->valuestring);
        if (len > 0) {
          ESP_LOGI(TAG, "GET VALUE: (%s) (%d)", value->valuestring, len);
          memcpy(val, value->valuestring, strlen(value->valuestring));
          ret = ESP_OK;
        }
      }
    }
    xSemaphoreGive(sd_storage_semph);
  }
  return ret;
}

esp_err_t sd_storage_set_string(char *key, char *val, size_t len) {
  esp_err_t ret = ESP_FAIL;
  if (xSemaphoreTake(sd_storage_semph, pdMS_TO_TICKS(1000))) {
    if (root != NULL || key != NULL || val != NULL) {
      ESP_LOGI(TAG, "(key: %s) (val:%s)", key, val);
      cJSON *obj = cJSON_GetObjectItem(root, key);
      if (!obj) {
        obj = cJSON_AddStringToObject(root, key, val);
      } else {
        cJSON_SetValuestring(obj, val);
      }
      ret = ESP_OK;
    }
    xSemaphoreGive(sd_storage_semph);
  }
  return ret;
}

esp_err_t sd_storage_clear_pics_folder(void) {
  esp_err_t ret = ESP_OK;
  DIR *d = opendir(PICS_PATH);
  struct dirent *dir;
  char path[288] = {0};

  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (dir->d_type == DT_REG) {
        sprintf(path, PICS_PATH "/%s", dir->d_name);
        remove(path);
      }
    }
    closedir(d);
  }
  return ret;
}

int sd_storage_count_files_with_ext(const char *extension) {
  int count = 0;
  size_t ext_len = strlen(extension);
  DIR *d = opendir(PICS_PATH);
  struct dirent *dir;

  if (d) {
    while ((dir = readdir(d)) != NULL) {
      size_t filename_len = strlen(dir->d_name);
      if (filename_len > ext_len &&
          !strcmp(dir->d_name + filename_len - ext_len, extension)) {
        count++;
      }
    }
    closedir(d);
  }

  return count;
}

int sd_storage_get_list_of_files_with_ext(char *list, int list_len,
                                          const char *path,
                                          const char *extension) {
  int count = 0;
  size_t ext_len = strlen(extension);
  DIR *d = opendir(path);
  struct dirent *dir;
  cJSON *_root = cJSON_CreateObject();
  cJSON_AddItemToObject(_root, "files", cJSON_CreateArray());

  if (d) {
    while ((dir = readdir(d)) != NULL) {
      size_t filename_len = strlen(dir->d_name);
      if (filename_len > ext_len &&
          !strcmp(dir->d_name + filename_len - ext_len, extension)) {
        count++;
        cJSON_AddItemToArray(cJSON_GetObjectItem(_root, "files"),
                             cJSON_CreateString(dir->d_name));
      }
    }
    closedir(d);

    cJSON_PrintPreallocated(_root, list, list_len, false);
    cJSON_Delete(_root);
  }

  return count;
}

esp_err_t sd_storage_delete_oldest_entries(int max_photos) {
  esp_err_t ret = ESP_OK;
  int photos_in_dir = sd_storage_count_files_with_ext(".raw");

  DIR *d = opendir(PICS_PATH);
  struct dirent *dir;

  if (d) {
    while ((dir = readdir(d)) != NULL && photos_in_dir-- > max_photos) {
      char oldest_entry_fn[100] = {0};
      char path[256] = {0};

      sd_storage_csv_get_oldest_photo(oldest_entry_fn);
      sprintf(path, PICS_PATH "/%s", oldest_entry_fn);
      remove(path);
      sd_storage_csv_remove_entry(oldest_entry_fn);
    }

    closedir(d);
  }
  return ret;
}

esp_err_t sd_storage_init(void) {
  esp_err_t err = ESP_OK;
  err = sd_storage_csv_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to allocate buffer for csv file: (%d)", err);
    return err;
  }

  err = init_sd_card();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize sd card: (%d)", err);
    return err;
  }

  if (sd_storage_json_buf != NULL) {
    ESP_LOGD(TAG, "json_buf is not NULL");
    free(sd_storage_json_buf);
  }
  sd_storage_json_buf = heap_caps_malloc(DEFAULT_JSON_BUF_SIZE,
                                         MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  if (!sd_storage_json_buf) {
    ESP_LOGE(TAG, "Can't allocate json_buf buffer");
    return ESP_ERR_NO_MEM;
  }
  memset(sd_storage_json_buf, 0, DEFAULT_JSON_BUF_SIZE * sizeof(char));

  sd_storage_semph = xSemaphoreCreateBinary();
  xSemaphoreGive(sd_storage_semph);

  err = open_settings(DEFAULT_SETTINGS_FILE_PATH);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open settings file: (%d)", err);
    return err;
  }

  err = make_dir(PICS_PATH);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create PICS (%s) folder: (%d)", PICS_PATH, err);
    return err;
  }

  err = make_dir(DATA_LOGS_PATH);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create PICS (%s) folder: (%d)", DATA_LOGS_PATH,
             err);
    return err;
  }

  return err;
}