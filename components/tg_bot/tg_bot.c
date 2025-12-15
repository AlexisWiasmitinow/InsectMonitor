/**
 * @file tg_bot.c
 * @author Sergiu Popov (sg.popov@pm.me)
 * @brief
 * @version 0.1
 * @date 2025-04-22
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "tg_bot.h"
#include <esp_log.h>
#include <stdlib.h>
#include <sys/param.h>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "cJSON.h"
#include "mbedtls/base64.h"

#include "sensors_handler.h"
#include "sd_storage.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
#define PATH_LENGTH 256
#define OUTPUT_BUFFER_SIZE (200 * 1024)

typedef struct data {
  char path[PATH_LENGTH];
  float temp;
  float hum;
} data_t;

static const char *TAG = "HTTP_CLIENT";
extern const char servercert_start[] asm("_binary_cert_pem_start");
extern const char servercert_end[] asm("_binary_cert_pem_end");

static QueueHandle_t tg_bot_queue;
static char *out_buffer = NULL;

static int encode_image(char *file_path, char **out) {
  if (!file_path) {
    ESP_LOGE(TAG, "File path is NULL");
    return -1;
  }
  FILE *file = fopen(file_path, "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open image file");
    return -1;
  }

  // Get file size
  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  rewind(file);

  size_t outputbuffer_len = (float)file_size * 1.34f + 1;

  uint8_t *read_buffer =
      heap_caps_malloc(file_size + 1, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  *out =
      heap_caps_malloc(outputbuffer_len, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  if (!read_buffer || !(*out)) {
    fclose(file);
    free(read_buffer);
    return -1;
  }

  // Read file content into output buffer
  size_t bytes_read = fread(read_buffer, 1, file_size, file);
  fclose(file);

  size_t bytes_written = 0;
  if (mbedtls_base64_encode((uint8_t *)*out, outputbuffer_len, &bytes_written,
                            read_buffer, bytes_read)) {
    ESP_LOGE(TAG, "Failed to encode image");
    return -1;
  }
  if (!bytes_written) {
    ESP_LOGE(TAG, "Failed to encode image");
    return -1;
  }

  free(read_buffer);

  return bytes_written;
}

static void tg_bot_task(void *arg) {
  data_t path = {0};
  while (1) {
    if (tg_bot_queue != NULL) {
      if (xQueueReceive(tg_bot_queue, &path, portMAX_DELAY) == pdTRUE) {
        if (path.path[0]) {
          // size_t before_mem = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
          // ESP_LOGI(TAG, "Received path: %s", path.path);

          char *encoded_image = NULL;
          int encoded_image_len = encode_image(path.path, &encoded_image);
          if ((encoded_image_len < 0) || !encoded_image) {
            ESP_LOGE(TAG, "Failed to encode image");
            free(encoded_image);
            continue;
          }

          size_t wrote = snprintf(
              out_buffer, OUTPUT_BUFFER_SIZE,
              "{\"temperature\":%.1f,\"humidity\":%.1f,\"image\":\"%s\"}",
              path.temp, path.hum, encoded_image);
          if (wrote < 1) {
            ESP_LOGE(TAG, "Failed to write to output buffer");
            free(encoded_image);
            continue;
          }

          char token[51] = {0};
          sd_storage_get_string(SETTINGS_API_TOKEN_KEY, token);
          char url[256] = {0};
          sprintf(url, "https://%s/api/upload_data.php?token=%s",
                  CONFIG_TG_BOT_URL, token);

          esp_http_client_config_t config = {
              .url = url,
              .method = HTTP_METHOD_POST,
              .crt_bundle_attach = esp_crt_bundle_attach,
          };

          esp_http_client_handle_t client = esp_http_client_init(&config);
          esp_http_client_set_header(client, "Content-Type",
                                     "application/json");
          esp_http_client_set_post_field(client, out_buffer,
                                         strlen(out_buffer));
          esp_err_t err = esp_http_client_perform(client);
          if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s",
                     esp_err_to_name(err));
          }

          // Clean up
          esp_http_client_cleanup(client);
          free(encoded_image);
          memset(&path, 0, sizeof(data_t));
          memset(out_buffer, 0, OUTPUT_BUFFER_SIZE);
          // size_t after_mem = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
          // ESP_LOGW(TAG, "BEFORE %d | AFTER %d | DIFF %d", before_mem,
          // after_mem, after_mem - before_mem);
        }
      }
    }
  }
}

esp_err_t tg_bot_init(void) {
  out_buffer =
      heap_caps_malloc(OUTPUT_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!out_buffer) {
    ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
    return ESP_FAIL;
  }
  memset(out_buffer, 0, OUTPUT_BUFFER_SIZE);

  tg_bot_queue = xQueueCreateWithCaps(5, sizeof(data_t),
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (tg_bot_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create queue");
    return ESP_FAIL;
  }
  if (xTaskCreateWithCaps(tg_bot_task, "tg_bot_task", 6 * 1024, NULL, 5, NULL,
                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to create task");
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t tg_bot_send_image(const char *path) {
  if (!path) return ESP_ERR_INVALID_ARG;

  data_t p = {0};
  strcpy(p.path, path);
  sens_handler_get_temp(&p.temp);
  sens_handler_get_humidity(&p.hum);

  return xQueueSend(tg_bot_queue, &p, pdMS_TO_TICKS(1000)) == pdTRUE ? ESP_OK
                                                                     : ESP_FAIL;
}
