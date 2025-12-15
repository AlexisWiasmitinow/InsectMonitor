
#include <esp_log.h>
#include "esp_camera.h"

#include "camera.h"
#include <esp_vfs_fat.h>
#include <esp_vfs.h>

#include <string.h>
#include "sd_storage.h"
#include "utils.h"
#include "box_controller.h"

#define CAM_PIN_PWDN GPIO_NUM_NC

#define CAM_PIN_SIOD GPIO_NUM_4
#define CAM_PIN_SIOC GPIO_NUM_5
#define CAM_PIN_HREF GPIO_NUM_18
#define CAM_PIN_PCLK GPIO_NUM_12
#define CAM_PIN_XCLK GPIO_NUM_38
#define CAM_PIN_VSYNC GPIO_NUM_8
#define CAM_PIN_RESET GPIO_NUM_39

#define CAM_PIN_D7 GPIO_NUM_9
#define CAM_PIN_D6 GPIO_NUM_10
#define CAM_PIN_D5 GPIO_NUM_11
#define CAM_PIN_D4 GPIO_NUM_13
#define CAM_PIN_D3 GPIO_NUM_21
#define CAM_PIN_D2 GPIO_NUM_48
#define CAM_PIN_D1 GPIO_NUM_47
#define CAM_PIN_D0 GPIO_NUM_14

#define LIGHT_DELAY 1000

static const char *TAG = "CAMERA";

static char *mem_pool = NULL;

camera_fb_t *camera_take_pic(void) {
  esp_err_t ret;
  camera_fb_t *pic = NULL;

  ret = turn_light_on(true);
  vTaskDelay(pdMS_TO_TICKS(LIGHT_DELAY));
  pic = esp_camera_fb_get();
  ret |= turn_light_on(false);
  return ret == ESP_OK && pic != NULL ? pic : NULL;
}

void camera_release_buf(camera_fb_t *buf) { esp_camera_fb_return(buf); }

esp_err_t camera_save_pic(const camera_fb_t *pic, char *path) {
  FILE *file = fopen(path, "w");
  if (!file) {
    ESP_LOGE(TAG, "Failed to open file:%s", path);
    return ESP_FAIL;
  } else {
    fwrite(pic->buf, 1, pic->len, file);
    fclose(file);
  }
  return ESP_OK;
}

esp_err_t camera_crop_image(const camera_fb_t *pic, camera_fb_t *croppedPic,
                            uint16_t startX, uint16_t startY, uint16_t width,
                            uint16_t height) {
  if (!croppedPic) {
    ESP_LOGE(TAG, "Output buffer is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  if ((height <= 0) || (width <= 0)) {
    ESP_LOGE(
        TAG,
        "Can't crop the image: height or width is 0 or below 0: (w:%d)(h:%d)",
        width, height);
    return ESP_ERR_INVALID_ARG;
  }

  size_t bpp = pic->len / (pic->width * pic->height);
  size_t target_size = width * height * bpp;

  croppedPic->height = height;
  croppedPic->width = width;
  croppedPic->len = target_size;
  croppedPic->format = pic->format;
  croppedPic->timestamp = pic->timestamp;
  croppedPic->buf = (uint8_t *)heap_caps_malloc(
      target_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
  if (!croppedPic->buf) {
    ESP_LOGE(TAG, "Failed to allocate cropped buffer");
    return ESP_ERR_NO_MEM;
  }

  for (unsigned short y = startY, i = 0; y < startY + height; y++) {
    for (unsigned short x = startX; x < startX + width; x++, i++) {
      uint32_t index = y * pic->width + x;
      memcpy(&croppedPic->buf[i * bpp], &pic->buf[index * bpp], bpp);
    }
  }
  return ESP_OK;
}

void camera_image_free_buf(camera_fb_t *pic) {
  if (pic) {
    if (pic->buf) {
      free(pic->buf);
    }
  }
}

int camera_get_prev(camera_fb_t *pic) {
  char last_image_filename[50] = {0};
  char last_image_path[100] = {0};
  if (pic) {
    sd_storage_csv_get_newest_photo(last_image_filename);
    sprintf(last_image_path, PICS_PATH "/%s", last_image_filename);

    FILE *last_image = fopen(last_image_path, "rb");
    if (last_image) {
      size_t last_image_size = 0;
      fseek(last_image, 0, SEEK_END);
      last_image_size = ftell(last_image);
      fseek(last_image, 0, SEEK_SET);

      pic->buf = heap_caps_malloc(last_image_size,
                                  MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
      if (!pic->buf) {
        ESP_LOGE(TAG, "Failed to allocate buffer for last image");
        fclose(last_image);
        return -1;
      }
      pic->len = last_image_size;
      size_t read = fread(pic->buf, 1, last_image_size, last_image);
      if (read != last_image_size) {
        ESP_LOGE(TAG,
                 "Failed to read whole image (expected size: %d) (got: %d)",
                 last_image_size, read);
      }
    } else {
      ESP_LOGE(TAG, "Can't open last frame file");
      return -1;
    }
    fclose(last_image);
  } else {
    ESP_LOGE(TAG, "pic is NULL");
    return -1;
  }
  return 1;
}

int camera_get_diff_prev(const camera_fb_t *pic, int difference_threshold) {
  char last_image_filename[50] = {0};
  char last_image_path[100] = {0};
  int diff = 0;
  if (mem_pool) {
    if (pic) {
      sd_storage_csv_get_newest_photo(last_image_filename);
      sprintf(last_image_path, PICS_PATH "/%s", last_image_filename);

      FILE *last_image = fopen(last_image_path, "rb");
      if (last_image) {
        size_t last_image_size = 0;
        fseek(last_image, 0, SEEK_END);
        last_image_size = ftell(last_image);
        fseek(last_image, 0, SEEK_SET);

        float last_image_avg = 0.f, current_image_avg = 0.f;
        if ((last_image_size <= IMAGE_SIZE) && (pic->len == last_image_size)) {
          size_t read = fread(mem_pool, 1, last_image_size, last_image);
          for (int i = 0; i < last_image_size; i++) {
            last_image_avg += mem_pool[i];
            current_image_avg += pic->buf[i];
          }

          last_image_avg /= last_image_size;
          current_image_avg /= pic->len;

          float brightness_ratio = last_image_avg / current_image_avg;
          ESP_LOGD(TAG,
                   "(last_image_avg:%f) (current_image_avg:%f) "
                   "(brightness_ratio:%f)",
                   last_image_avg, current_image_avg, brightness_ratio);
          for (int i = 0; i < read; i++) {
            int adj_pixel = pic->buf[i] * brightness_ratio;
            if (abs(mem_pool[i] - adj_pixel) > difference_threshold) {
              diff++;
            }
          }
        }

        ESP_LOGI(TAG, "Real diff (%d)", diff);
        diff = convert_range(diff, 0, 100000, 0, 100);
        memset(mem_pool, 0, IMAGE_SIZE);
      } else {
        ESP_LOGE(TAG, "Can't open last frame file");
      }
      fclose(last_image);
    } else {
      ESP_LOGE(TAG, "pic is NULL");
    }
  } else {
    ESP_LOGE(TAG, "Memory pool is NULL");
  }
  return diff;
}

esp_err_t camera_init(void) {
  static camera_config_t camera_config = {
      .pin_pwdn = CAM_PIN_PWDN,
      .pin_reset = CAM_PIN_RESET,
      .pin_xclk = CAM_PIN_XCLK,
      .pin_sccb_sda = CAM_PIN_SIOD,
      .pin_sccb_scl = CAM_PIN_SIOC,

      .pin_d7 = CAM_PIN_D7,
      .pin_d6 = CAM_PIN_D6,
      .pin_d5 = CAM_PIN_D5,
      .pin_d4 = CAM_PIN_D4,
      .pin_d3 = CAM_PIN_D3,
      .pin_d2 = CAM_PIN_D2,
      .pin_d1 = CAM_PIN_D1,
      .pin_d0 = CAM_PIN_D0,
      .pin_vsync = CAM_PIN_VSYNC,
      .pin_href = CAM_PIN_HREF,
      .pin_pclk = CAM_PIN_PCLK,

      // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
      .xclk_freq_hz = 4000000,
      .ledc_timer = LEDC_TIMER_0,
      .ledc_channel = LEDC_CHANNEL_0,

      .pixel_format = PIXFORMAT_JPEG,  // YUV422,GRAYSCALE,RGB565,JPEG
      .frame_size =
          IMAGE_FRAMESIZE,  // QQVGA-UXGA, For ESP32, do not use sizes above
                            // QVGA when not JPEG. The performance of the
                            // ESP32-S series has improved a lot, but JPEG mode
                            // always gives better frame rates.

      .jpeg_quality = 12,  // 0-63, for OV series camera sensors, lower number
                           // means higher quality
      .fb_count = 4,  // When jpeg mode is used, if fb_count more than one, the
                      // driver will work in continuous mode.
      .fb_location = CAMERA_FB_IN_PSRAM,
      .grab_mode = CAMERA_GRAB_LATEST,
  };
  // initialize the camera
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Camera Init Failed");
    return err;
  }

  sensor_t *s = esp_camera_sensor_get();

  s->set_brightness(s, 0);  // -2 to 2
  s->set_contrast(s, 0);    // -2 to 2
  s->set_saturation(s, 0);  // -2 to 2
  s->set_special_effect(s, 0);
  s->set_whitebal(s, 0);  // 0 = disable , 1 = enable
  s->set_awb_gain(s, 0);  // 0 = disable , 1 = enable
  s->set_wb_mode(s, 0);  // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2
                         // - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 0);               // 0 = disable , 1 = enable
  s->set_aec2(s, 1);                        // 0 = disable , 1 = enable
  s->set_ae_level(s, 0);                    // -2 to 2
  s->set_aec_value(s, 50);                  // 0 to 1200
  s->set_gain_ctrl(s, 0);                   // 0 = disable , 1 = enable
  s->set_agc_gain(s, 1);                    // 0 to 30
  s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
  s->set_bpc(s, 0);                         // 0 = disable , 1 = enable
  s->set_wpc(s, 1);                         // 0 = disable , 1 = enable
  s->set_raw_gma(s, 1);                     // 0 = disable , 1 = enable
  s->set_lenc(s, 1);                        // 0 = disable , 1 = enable
  s->set_hmirror(s, 0);                     // 0 = disable , 1 = enable
  s->set_vflip(s, 0);                       // 0 = disable , 1 = enable
  s->set_dcw(s, 1);                         // 0 = disable , 1 = enable
  s->set_colorbar(s, 0);                    // 0 = disable , 1 = enable

  // mem_pool = heap_caps_malloc(IMAGE_SIZE, MALLOC_CAP_8BIT |
  // MALLOC_CAP_SPIRAM);
  return ESP_OK;
}
