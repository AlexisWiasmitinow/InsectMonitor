
#include "webpage.h"
#include "webpage_storage.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include <sys/time.h>
#include "esp_http_server.h"
#include "status_handler.h"
#include "sd_storage.h"
#include "systime_handler.h"
#include "wifi_handler.h"
#include "box_controller.h"
#include "camera_handler.h"

#include <string.h>

#define BUF_SIZE (512)
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

static const char *TAG = "WEBPAGE";

static httpd_handle_t server = NULL;
volatile static bool pause_requests = false;

static esp_err_t get_handler(httpd_req_t *req, bool local) {
  char type[30] = {0};
  webpage_file *fp = webpage_storage_open_file((char *)req->uri, type, local);

  if (!fp) {
    ESP_LOGE(TAG, "Failed to open file: (%s)", (char *)req->uri);
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Failed to open a file");
    return ESP_FAIL;
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, type));

  char buf[1024];
  size_t len = 0;
  esp_err_t err = ESP_OK;
  do {
    len = webpage_storage_read(fp, buf, sizeof(buf));
    err = httpd_resp_send_chunk(req, buf, len);
    if (err != ESP_OK) break;
  } while (len > 0);

  webpage_storage_close_file(fp);
  return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t get_local_handler(httpd_req_t *req) {
  return get_handler(req, true);
}

static esp_err_t get_sd_handler(httpd_req_t *req) {
  return get_handler(req, false);
}

static esp_err_t get_status_handler(httpd_req_t *req) {
  if (!pause_requests) {
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    // ESP_LOGI("HTTP", "Received request: %s", req->uri);
    ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "application/json"));
    char buf[1024] = {0};
    status_handler_serialize_status(buf, sizeof(buf));
    esp_err_t res = httpd_resp_send(req, buf, strlen(buf));
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                          (end_time.tv_usec - start_time.tv_usec) / 1000000.0;
    /*if (res == ESP_OK) {
      ESP_LOGI("HTTP", "Sent response for: %s, elapsed time: %.2f s", req->uri,
               elapsed_time);
    } else {
      ESP_LOGE("HTTP", "Failed to send response: %d", res);
    }*/
   return res;
  }
  return httpd_resp_send_500(req);
}

static esp_err_t update_settings_handler(httpd_req_t *req) {
  esp_err_t ret = ESP_OK;
  char *query = malloc(BUF_SIZE);
  if (!query) {
    ESP_LOGE(TAG, "Failed to allocate query buffer");
    return httpd_resp_send_500(req);
  }
  memset(query, 0, BUF_SIZE);

  ESP_ERROR_CHECK_WITHOUT_ABORT(
      httpd_req_get_url_query_str(req, query, BUF_SIZE));
  ESP_LOGI(TAG, "query: %s", query);
  char *query_dup = strdup(query);
  char *tokens = query_dup;
  char *p = query_dup;

  bool clean_photos = false;

  while ((p = strsep(&tokens, "&\n"))) {
    char *var = strtok(p, "="), *val = NULL;
    if (var && (val = strtok(NULL, "="))) {
      if (!strcmp(var, SETTINGS_TEMP_TARGET_KEY)) {
        sd_storage_set_float(var, (float)strtof(val, NULL));
      } else if (!strcmp(var, SETTINGS_API_TOKEN_KEY)) {
        sd_storage_set_string(SETTINGS_API_TOKEN_KEY, val, strlen(val));
      } else {
        sd_storage_set_int(var, (int)strtol(val, NULL, 10));
      }

      if (!strcmp(var, SETTINGS_MAX_PHOTOS)) {
        sd_storage_set_max_photos((int)strtol(val, NULL, 10));
        sd_storage_delete_oldest_entries((int)strtol(val, NULL, 10));
      }
    }
  }
  if (sd_storage_update_settings() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to update settings");
    ret = ESP_FAIL;
  }
  if (clean_photos) {
    sd_storage_clear_pics_folder();
  }
  camera_handler_update_settings();

  free(query);
  free(query_dup);

  const char *ok_string = "Settings updated";
  return httpd_resp_send(req, ok_string, sizeof(ok_string));
}

static void replace_chars(const char *original, const char *toReplace,
                          const char *replacement, char *result,
                          size_t resultSize) {
  if (!original || !toReplace || !replacement || !result) return;

  const char *pos;
  const char *start = original;
  size_t toReplaceLen = strlen(toReplace);
  size_t replacementLen = strlen(replacement);
  size_t currentLength = 0;

  result[0] = '\0';

  // Iterate through the original string to replace substrings
  while ((pos = strstr(start, toReplace))) {
    size_t segmentLen = pos - start;

    // Check if the result buffer can hold the next segment and replacement
    if (currentLength + segmentLen + replacementLen >= resultSize - 1) {
      fprintf(stderr, "Error: Result buffer too small.\n");
      result[0] = '\0';
      return;
    }

    // Copy the part before the `toReplace`
    strncat(result, start, segmentLen);
    currentLength += segmentLen;

    // Add the replacement
    strncat(result, replacement, replacementLen);
    currentLength += replacementLen;

    start = pos + toReplaceLen;
  }

  if (currentLength + strlen(start) >= resultSize - 1) {
    fprintf(stderr, "Error: Result buffer too small.\n");
    result[0] = '\0';  // Indicate failure
    return;
  }
  strcat(result, start);
}

static esp_err_t save_network_handler(httpd_req_t *req) {
  char *query = malloc(BUF_SIZE);
  if (!query) {
    ESP_LOGE(TAG, "Failed to allocate query buffer");
    return httpd_resp_send_500(req);
  }
  memset(query, 0, BUF_SIZE);

  ESP_ERROR_CHECK_WITHOUT_ABORT(
      httpd_req_get_url_query_str(req, query, BUF_SIZE));
  ESP_LOGI(TAG, "query: %s", query);
  char *query_dup = strdup(query);
  char *tokens = query_dup;
  char *p = query_dup;

  char *result = malloc(BUF_SIZE);
  char *intermediate = malloc(BUF_SIZE);
  if (!result || !intermediate) {
    ESP_LOGE(TAG, "Failed to allocate result and intermediate buffers");
    return httpd_resp_send_500(req);
  }
  memset(result, 0, BUF_SIZE);
  memset(intermediate, 0, BUF_SIZE);

  while ((p = strsep(&tokens, "&\n"))) {
    strncpy(intermediate, p, BUF_SIZE - 1);
    intermediate[BUF_SIZE - 1] = '\0';

    replace_chars(intermediate, "\%2C", ",", result, BUF_SIZE);
    strncpy(intermediate, result, BUF_SIZE - 1);
    intermediate[BUF_SIZE - 1] = '\0';

    replace_chars(intermediate, "\%2F", "/", result, BUF_SIZE);
    strncpy(intermediate, result, BUF_SIZE - 1);
    intermediate[BUF_SIZE - 1] = '\0';

    char *var = strtok(result, "="), *val = NULL;
    if (var && (val = strtok(NULL, "="))) {
      sd_storage_set_string(var, val, strlen(val));
    }
  }
  if (sd_storage_update_settings() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to update settings");
  }

  free(query_dup);
  free(query);
  free(result);
  free(intermediate);

  const char *ok_string = "Network settings updated";

  char tz[40] = {0};
  sh_get_timezone(tz);
  sh_set_timezone(tz);

  httpd_resp_send(req, ok_string, sizeof(ok_string));
  return restart_wifi();
}

static esp_err_t get_timezone_handler(httpd_req_t *req) {
  ESP_ERROR_CHECK_WITHOUT_ABORT(httpd_resp_set_type(req, "application/json"));
  char *buf = malloc(1024);
  sh_get_timezone_list_json(buf, 1024);
  esp_err_t ret = httpd_resp_send(req, buf, strlen(buf));
  free(buf);
  return ret;
}

static esp_err_t fota_update(httpd_req_t *req) {
  esp_ota_handle_t ota_handle;
  int remaining = req->content_len;
  esp_err_t ret = ESP_OK;

  char *buf = malloc(1024);
  if (!buf) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Failed to allocate buffer");
    return ESP_ERR_NO_MEM;
  }
  const esp_partition_t *ota_part = esp_ota_get_next_update_partition(NULL);

  ret = esp_ota_begin(ota_part, OTA_SIZE_UNKNOWN, &ota_handle);
  if (ret != ESP_OK) {
    goto fota_end;
  }

  while (remaining > 0) {
    int recv_len = httpd_req_recv(req, buf, MIN(remaining, 1024));
    if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
      continue;
    } else if (recv_len <= 0) {
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                          "Protocol error");
      ret = ESP_FAIL;
      break;
    }

    if (esp_ota_write(ota_handle, (const void *)buf, recv_len) != ESP_OK) {
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                          "Failed to flash buffer");
      ret = ESP_FAIL;
      break;
    }

    remaining -= recv_len;
  }

  if (ret == ESP_OK) {
    if ((esp_ota_end(ota_handle) != ESP_OK) ||
        (esp_ota_set_boot_partition(ota_part) != ESP_OK)) {
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                          "Failed to update");
      ret = ESP_FAIL;
      goto fota_end;
    }
    httpd_resp_sendstr(req, "Firmware update complete, rebooting...\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
  }

fota_end:
  free(buf);
  return ret;
}

static esp_err_t filesystem_update(httpd_req_t *req) {
  esp_err_t ret = ESP_OK;
  char *buf = malloc(1024);
  if (!buf) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Failed to allocate buffer");
    return ESP_ERR_NO_MEM;
  }
  const esp_partition_t *part = esp_partition_get(esp_partition_find(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS, "storage"));
  if (!part) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Failed to get storage partition");
    ret = ESP_FAIL;
    goto filesystem_end;
  }

  int remaining = req->content_len;
  size_t offset = 0x0;
  while (remaining > 0) {
    int recv_len = httpd_req_recv(req, buf, MIN(remaining, 1024));
    if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
      continue;
    } else if (recv_len <= 0) {
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                          "Protocol error");
      ret = ESP_FAIL;
      break;
    }
    esp_partition_erase_range(part, offset, recv_len);
    esp_partition_write(part, offset, buf, recv_len);

    remaining -= recv_len;
    offset += recv_len;
  }

  if (ret == ESP_OK) {
    httpd_resp_sendstr(req, "Filesystem update complete, rebooting...\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
  }

filesystem_end:
  free(buf);
  return ret;
}

static esp_err_t update_handler(httpd_req_t *req) {
  esp_err_t ret = ESP_OK;
  char *query = malloc(BUF_SIZE);
  if (!query) {
    ESP_LOGE(TAG, "Failed to allocate query buffer");
    return httpd_resp_send_500(req);
  }
  memset(query, 0, BUF_SIZE);

  ESP_ERROR_CHECK_WITHOUT_ABORT(
      httpd_req_get_url_query_str(req, query, BUF_SIZE));
  ESP_LOGI(TAG, "query: %s", query);
  char *query_dup = strdup(query);
  char *tokens = query_dup;
  char *p = query_dup;

  while ((p = strsep(&tokens, "&\n"))) {
    char *var = strtok(p, "="), *val = NULL;
    if (var && (val = strtok(NULL, "="))) {
      if (!strcmp("filename", var)) {
        camera_pause_thread(true);
        webpage_pause(true);
        if (!strcmp("storage.bin", val)) {
          ret = filesystem_update(req);
        } else {
          ret = fota_update(req);
        }
      }
    }
  }

  free(query);
  free(query_dup);

  return ret;
}

static esp_err_t setio_handler(httpd_req_t *req) {
  char response[30] = {0};
  bool state = false;

  esp_err_t ret = ESP_OK;
  char *query = malloc(BUF_SIZE);
  if (!query) {
    ESP_LOGE(TAG, "Failed to allocate query buffer");
    return httpd_resp_send_500(req);
  }
  memset(query, 0, BUF_SIZE);

  ESP_ERROR_CHECK_WITHOUT_ABORT(
      httpd_req_get_url_query_str(req, query, BUF_SIZE));
  ESP_LOGI(TAG, "query: %s", query);
  char *query_dup = strdup(query);
  char *tokens = query_dup;
  char *p = query_dup;

  while ((p = strsep(&tokens, "&\n"))) {
    char *var = strtok(p, "="), *val = NULL;
    if (var && (val = strtok(NULL, "="))) {
      if (!strcmp("io", var)) {
        if (box_controller_test_pin(val, &state) != ESP_OK) {
          ret = ESP_FAIL;
        } else {
          sprintf(response, "IO=%s,state=%d\n", val, state);
        }
        break;
      }
    }
  }

  free(query);
  free(query_dup);

  return ret == ESP_OK ? httpd_resp_sendstr(req, response)
                       : httpd_resp_send_500(req);
}

static esp_err_t capture_handler(httpd_req_t *req) {
  esp_err_t ret = ESP_OK;
  if (camera_capture() == ESP_OK) {
    return httpd_resp_send(req, "Picture taken!", 15);
  }
  return httpd_resp_send_500(req);
}

static esp_err_t data_logs_get_list_handler(httpd_req_t *req)
{
  char buf[1024] = {0};
  sd_storage_data_logs_get_list(buf, sizeof(buf));
  return httpd_resp_send(req, buf, strlen(buf));
}

esp_err_t webpage_pause(bool pause)
{
  pause_requests = pause;
  return ESP_OK;
}

esp_err_t webpage_init(void) {
  esp_err_t ret = ESP_FAIL;
  if (!server) {
    ret = webpage_storage_init();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize webstorage");
      return ret;
    }
  }
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.stack_size = 6 * 1024;
  // config.task_caps &= ~MALLOC_CAP_INTERNAL;
  // config.task_caps |= MALLOC_CAP_SPIRAM;
  config.uri_match_fn = httpd_uri_match_wildcard;
  config.max_uri_handlers = 16;
  config.lru_purge_enable = true;

  ret = httpd_start(&server, &config);
  if (ret == ESP_OK) {
    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");

    static const httpd_uri_t index_uri = {.uri = "/",
                                          .method = HTTP_GET,
                                          .handler = get_local_handler,
                                          .user_ctx = NULL};
    static const httpd_uri_t css_uri = {.uri = "/css/*",
                                        .method = HTTP_GET,
                                        .handler = get_local_handler,
                                        .user_ctx = NULL};
    static const httpd_uri_t img_uri = {.uri = "/img/*",
                                        .method = HTTP_GET,
                                        .handler = get_local_handler,
                                        .user_ctx = NULL};
    static const httpd_uri_t js_uri = {.uri = "/js/*",
                                       .method = HTTP_GET,
                                       .handler = get_local_handler,
                                       .user_ctx = NULL};

    static const httpd_uri_t get_status_uri = {.uri = "/getStatus",
                                               .method = HTTP_GET,
                                               .handler = get_status_handler,
                                               .user_ctx = NULL};
    static const httpd_uri_t settings_uri = {.uri = "/settings.json",
                                             .method = HTTP_GET,
                                             .handler = get_sd_handler,
                                             .user_ctx = NULL};
    static const httpd_uri_t pics_list_uri = {.uri = "/pics/*",
                                              .method = HTTP_GET,
                                              .handler = get_sd_handler,
                                              .user_ctx = NULL};
    static const httpd_uri_t update_settings_uri = {
        .uri = "/updateSettings",
        .method = HTTP_GET,
        .handler = update_settings_handler,
        .user_ctx = NULL};
    static const httpd_uri_t save_network_settings_uri = {
        .uri = "/savenetwork",
        .method = HTTP_GET,
        .handler = save_network_handler,
        .user_ctx = NULL};
    static const httpd_uri_t gettz_uri = {.uri = "/gettz",
                                          .method = HTTP_GET,
                                          .handler = get_timezone_handler,
                                          .user_ctx = NULL};
    static const httpd_uri_t update_uri = {.uri = "/update",
                                           .method = HTTP_POST,
                                           .handler = update_handler,
                                           .user_ctx = NULL};
    static const httpd_uri_t setio_uri = {.uri = "/setio",
                                          .method = HTTP_GET,
                                          .handler = setio_handler,
                                          .user_ctx = NULL};
    static const httpd_uri_t savecrop_uri = {.uri = "/savecrop",
                                             .method = HTTP_GET,
                                             .handler = update_settings_handler,
                                             .user_ctx = NULL};
    static const httpd_uri_t capture_uri = {.uri = "/capture",
                                            .method = HTTP_GET,
                                            .handler = capture_handler,
                                            .user_ctx = NULL};
    static const httpd_uri_t get_data_logs_list_uri = {.uri = "/historicDataList",
                                            .method = HTTP_GET,
                                            .handler = data_logs_get_list_handler,
                                            .user_ctx = NULL};
    static const httpd_uri_t data_logs_uri = {.uri = "/logs/*",
                                            .method = HTTP_GET,
                                            .handler = get_sd_handler,
                                            .user_ctx = NULL};

    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &img_uri);
    httpd_register_uri_handler(server, &get_status_uri);
    httpd_register_uri_handler(server, &css_uri);
    httpd_register_uri_handler(server, &js_uri);
    httpd_register_uri_handler(server, &settings_uri);
    httpd_register_uri_handler(server, &pics_list_uri);
    httpd_register_uri_handler(server, &update_settings_uri);
    httpd_register_uri_handler(server, &save_network_settings_uri);
    httpd_register_uri_handler(server, &gettz_uri);
    httpd_register_uri_handler(server, &update_uri);
    httpd_register_uri_handler(server, &setio_uri);
    httpd_register_uri_handler(server, &savecrop_uri);
    httpd_register_uri_handler(server, &capture_uri);
    httpd_register_uri_handler(server, &get_data_logs_list_uri);
    httpd_register_uri_handler(server, &data_logs_uri);
  }

  return ret;
}