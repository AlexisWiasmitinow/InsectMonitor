#include "webpage_storage.h"

#include "esp_vfs.h"
// #include "esp_vfs_littlefs.h"
#include "esp_log.h"
// #include "esp_vfs_fat.h"
#include "esp_littlefs.h"

#define STORAGE_BASEPATH "/spiflash"
#define STORAGE_SD_BASEPATH "/sdcard"

static const char *TAG = "WEBPAGE_STORAGE";

#define CHECK_FILE_EXTENSION(filename, ext) \
  (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)
static void get_content_type_from_file(const char *filepath, const char *type) {
  if (CHECK_FILE_EXTENSION(filepath, ".html")) {
    memcpy((char *)type, "text/html", 10);
  } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
    memcpy((char *)type, "application/javascript", 23);
  } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
    memcpy((char *)type, "text/css", 9);
  } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
    memcpy((char *)type, "image/png", 10);
  } else if (CHECK_FILE_EXTENSION(filepath, ".jpg") ||
             CHECK_FILE_EXTENSION(filepath, ".jpeg")) {
    memcpy((char *)type, "image/jpeg", 11);
  } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
    memcpy((char *)type, "text/x-icon", 12);
  } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
    memcpy((char *)type, "text/xml", 9);
  } else if (CHECK_FILE_EXTENSION(filepath, ".json")) {
    memcpy((char *)type, "application/json", 17);
  } else {
    memcpy((char *)type, "text/plain", 11);
  }
}

webpage_file *webpage_storage_open_file(char *uri, char *type, bool local) {
  char filepath[100] = {0};
  strcat(filepath, local ? STORAGE_BASEPATH : STORAGE_SD_BASEPATH);
  if (strcmp(uri, "/") == 0) {
    strcat(filepath, "/index.html");
  } else {
    strcat(filepath, uri);
  }
  get_content_type_from_file(filepath, type);
  return fopen(filepath, "r");
}

void webpage_storage_close_file(webpage_file *file) { fclose(file); }

size_t webpage_storage_read(webpage_file *file, char *buf, size_t chunk_size) {
  return fread(buf, 1, chunk_size, file);
}
#include "esp_littlefs.h"

esp_err_t webpage_storage_init(void) {
  esp_err_t err = ESP_OK;

  const esp_vfs_littlefs_conf_t conf = {
      .base_path = STORAGE_BASEPATH,
      .partition_label = "storage",
      .format_if_mount_failed = false,
      .dont_mount = false,
  };

  err = esp_vfs_littlefs_register(&conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount or format filesystem");
    return err;
  }

  return err;
}