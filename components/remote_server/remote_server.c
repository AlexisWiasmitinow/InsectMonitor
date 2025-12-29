/**
 * @file remote_server.c
 * @author Sergiu Popov (sg.popov@pm.me)
 * @brief
 * @version 0.1
 * @date 2025-04-22
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "remote_server.h"
#include <esp_log.h>
#include <stdlib.h>
#include <sys/param.h>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

#include "cJSON.h"
#include "mbedtls/base64.h"

#include "camera.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
#define OUTPUT_BUFFER_SIZE (200 * 1024)

static const char *TAG = "HTTP_CLIENT";
extern const char servercert_start[] asm("_binary_cert_pem_start");
extern const char servercert_end[] asm("_binary_cert_pem_end");

static RingbufHandle_t ring_buf = NULL;
static char *out_buffer = NULL;

static int encode_image(const uint8_t *data, size_t len, char **out) {
  if (!data) {
    ESP_LOGE(TAG, "Data is NULL");
    return -1;
  }
  FILE *file = fmemopen((void*)data, len, "rb");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open image file");
    return -1;
  }

  size_t outputbuffer_len = (float)len * 1.34f + 1;

  *out =
      heap_caps_malloc(outputbuffer_len, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  if (!(*out)) {
    fclose(file);
    return -1;
  }

  size_t bytes_written = 0;
  if (mbedtls_base64_encode((uint8_t *)*out, outputbuffer_len, &bytes_written,
                            data, len)) {
    ESP_LOGE(TAG, "Failed to encode image");
    return -1;
  }
  if (!bytes_written) {
    ESP_LOGE(TAG, "Failed to encode image");
    return -1;
  }
  fclose(file);

  return bytes_written;
}

static void rs_task(void *arg) {
  uint8_t *val = NULL;
  size_t item_size = 0;
  while (1) {
    if (ring_buf != NULL) {
      val = xRingbufferReceive(ring_buf, &item_size, portMAX_DELAY);

      char *encoded_image = NULL;
      int encoded_image_len = encode_image(val, item_size, &encoded_image);
      if ((encoded_image_len < 0) || !encoded_image) {
        ESP_LOGE(TAG, "Failed to encode image");
        free(encoded_image);
        continue;
      }

      size_t wrote = snprintf(
          out_buffer, OUTPUT_BUFFER_SIZE,
          "{\"image\":\"%s\"}", encoded_image);
      if (wrote < 1) {
        ESP_LOGE(TAG, "Failed to write to output buffer");
        free(encoded_image);
        continue;
      }

      char url[256] = {0};
      sprintf(url, "https://%s/api/upload_data.php?token=%s",
              CONFIG_REMOTE_SERVER_URL, CONFIG_REMOTE_SERVER_API_TOKEN_KEY);

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

      vRingbufferReturnItem(ring_buf, (void *) val);

      // Clean up
      esp_http_client_cleanup(client);
      free(encoded_image);
      memset(out_buffer, 0, OUTPUT_BUFFER_SIZE);
    }
  }
}

esp_err_t rs_init(void) {
  out_buffer =
      heap_caps_malloc(OUTPUT_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!out_buffer) {
    ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
    return ESP_FAIL;
  }
  memset(out_buffer, 0, OUTPUT_BUFFER_SIZE);

  ring_buf = xRingbufferCreateWithCaps(IMAGE_SIZE*2, RINGBUF_TYPE_NOSPLIT,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (ring_buf == NULL) {
    ESP_LOGE(TAG, "Failed to create ring buffer");
    return ESP_FAIL;
  }
  if (xTaskCreateWithCaps(rs_task, "rs_task", 6 * 1024, NULL, 5, NULL,
                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to create task");
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t rs_send_image(const uint8_t *data, size_t len) {
  if (!data) return ESP_ERR_INVALID_ARG;
  if (!ring_buf) return ESP_FAIL;

  return xRingbufferSend(ring_buf, data, len, pdMS_TO_TICKS(2000)) == pdTRUE ? ESP_OK
                                                                     : ESP_FAIL;
}
